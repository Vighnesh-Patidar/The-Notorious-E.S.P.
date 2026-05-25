#include "beacon.hpp"
#include <string.h>

extern "C" int ets_printf(const char* fmt, ...);
extern "C" {
    #include "os_type.h"
    #include "user_interface.h"
    #include "eagle_soc.h"
    #include "gpio.h"

    void ets_timer_setfn(ETSTimer *ptimer, void (*pfunction)(void *), void *parg);
    void ets_timer_arm_new(ETSTimer *ptimer, uint32_t time_ms, bool repeat, bool is_ms);
    void ets_timer_disarm(ETSTimer *ptimer);
}

#define LED_GPIO 2

extern "C" void user_rf_pre_init(void) {}

static const partition_item_t at_partition_table[] = {
    { SYSTEM_PARTITION_RF_CAL,           0x3FB000, 0x1000 },
    { SYSTEM_PARTITION_PHY_DATA,         0x3FC000, 0x1000 },
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, 0x3FD000, 0x3000 },
};

extern "C" void user_pre_init(void)
{
    if (!system_partition_table_regist(at_partition_table,
            sizeof(at_partition_table) / sizeof(at_partition_table[0]),
            FLASH_SIZE_32M_MAP_512_512))
    {
        ets_printf("partition table regist fail\n");
        while (1) {}
    }
}

static ETSTimer channel_timer;
static ETSTimer data_frame_timer;
static volatile uint8_t current_channel = 1;

// --- NEW: TRAFFIC TRACKING VARIABLES ---
static uint8_t my_mac[6];
static volatile uint32_t external_traffic_count = 0;

// --- NEW: PROMISCUOUS SNIFFER ---
extern "C" void promisc_cb(uint8_t* buf, uint16_t len)
{
    if (len >= (12 + 24)) { // Ensure the packet is large enough to contain a Source MAC
        uint8_t* src_mac = buf + 22;
        
        // Check if the packet came from our own injector.
        if (memcmp(src_mac, my_mac, 6) == 0) {
            return; // Ignore our own frames!
        }
    }
    
    // If we reach here, a device other than us is talking on this channel.
    external_traffic_count++;
}

// --- UPDATED: HUNTER-SEEKER CHANNEL LOGIC ---
static void evaluate_channel(void* arg)
{
    (void)arg;
    
    if (external_traffic_count > 2) {
        ets_printf("Ch %u is ACTIVE (Heard %u pkts). Staying and spamming...\n", current_channel, external_traffic_count);
    } else {
        current_channel++;
        if (current_channel > 13) current_channel = 1;
        wifi_set_channel(current_channel);
    }
    
    external_traffic_count = 0;
}

static void inject_data_frame(void* arg) 
{
    (void)arg;
    static uint8_t led_state = 1;
    led_state ^= 1;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), led_state);

    uint8_t payload[64];
    memset(payload, 0xAA, sizeof(payload)); 
    Beacon::send(payload, sizeof(payload));
}

static void wifi_init_done_cb(void)
{
    // Fetch our MAC address so the sniffer knows what to ignore
    wifi_get_macaddr(STATION_IF, my_mac);

    // Boot up the promiscuous sniffer to listen for traffic
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);

    wifi_set_channel(current_channel);
    ets_printf("Hunter-Seeker active. Looking for traffic to spam...\n");

    // Evaluate the channel every 250ms
    ets_timer_disarm(&channel_timer);
    ets_timer_setfn(&channel_timer, evaluate_channel, nullptr);
    ets_timer_arm_new(&channel_timer, 250, true, true);

    // Spam data frames every 10ms
    ets_timer_disarm(&data_frame_timer);
    ets_timer_setfn(&data_frame_timer, inject_data_frame, nullptr);
    ets_timer_arm_new(&data_frame_timer, 1, true, true);
}

extern "C" void user_init()
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), 1); 

    // Force Station Mode into RAM ONLY to kill the ghost AP
    wifi_set_opmode_current(STATION_MODE);
    
    system_init_done_cb(wifi_init_done_cb);
}