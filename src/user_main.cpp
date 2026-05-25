#include "memory_pool.hpp"
#include "filter_engine.hpp"
#include "frame_parser.hpp"
#include "ring_buffer.hpp"
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

MemoryPool memory_pool;
RingBuffer ring_buffer;


#define CONSUMER_TASK_PRIO   0
#define CONSUMER_QUEUE_LEN   8
#define CONSUMER_SIG_WAKE    1

static os_event_t consumer_queue[CONSUMER_QUEUE_LEN];

static void consumer_task(os_event_t* e)
{
    for (int i = 0; i < 8; ++i)
    {
        uint8_t index;
        if (!ring_buffer.pop(index)) break;
        uint8_t* ptr = memory_pool.get_slot(index);
        uint16_t plen = memory_pool.get_length(index);
        ParsedFrame* frame = FrameParser::parse(ptr, plen);
        if (frame) {
            ets_printf("rx proto=%u %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u len=%u\n",
                frame->protocol,
                (frame->src_ip >> 24) & 0xFF, (frame->src_ip >> 16) & 0xFF,
                (frame->src_ip >> 8)  & 0xFF,  frame->src_ip & 0xFF,
                frame->src_port,
                (frame->dst_ip >> 24) & 0xFF, (frame->dst_ip >> 16) & 0xFF,
                (frame->dst_ip >> 8)  & 0xFF,  frame->dst_ip & 0xFF,
                frame->dst_port,
                frame->payload_length);
        
            uint16_t print_len = frame->payload_length > 64 ? 64 : frame->payload_length;
            for (uint16_t i = 0; i < print_len; i += 16) {
                ets_printf("%04x  ", i);
                for (uint16_t j = i; j < i + 16; j++) {
                    if (j < print_len) ets_printf("%02x ", frame->payload[j]);
                    else               ets_printf("   ");
                }
                ets_printf(" |");
                for (uint16_t j = i; j < i + 16 && j < print_len; j++) {
                    uint8_t c = frame->payload[j];
                    ets_printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
                }
                ets_printf("|\n");
            }
            if (frame->payload_length > 64)
                ets_printf("  ...+%u bytes\n", frame->payload_length - 64);
        } 
        FilterEngine::apply(frame);
        memory_pool.release(ptr);
    }
}

extern "C" void promisc_cb(uint8_t* buf, uint16_t len)
{
    static uint8_t led_state = 1;
    led_state ^= 1;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), led_state);

    
    static uint32_t rx_count = 0;
    rx_count++;
    if ((rx_count & 0x3F) == 0) {
        if (len >= 14) {
            uint8_t fc0 = buf[12];        
            uint8_t fc1 = buf[13];        
            uint8_t type = (fc0 >> 2) & 0x3;
            uint8_t sub  = (fc0 >> 4) & 0xF;
            uint8_t prot = (fc1 >> 6) & 0x1;
            ets_printf("rx n=%u len=%u type=%u sub=%u prot=%u\n",
                rx_count, len, type, sub, prot);
        } else {
            ets_printf("rx n=%u len=%u (ctrl-only)\n", rx_count, len);
        }
    }

    uint8_t* slot = memory_pool.acquire();
    if (slot == nullptr) return;
    if (len > FRAME_SLOT_SIZE) len = FRAME_SLOT_SIZE;
    if(len <= 12) { memory_pool.release(slot); return; }
    memcpy(slot, buf + 12, len - 12);
    uint8_t index = memory_pool.get_index(slot);
    memory_pool.set_length(index, len-12);
    if (!ring_buffer.push(index))
    {
        memory_pool.release(slot);
        return;
    }
    system_os_post(CONSUMER_TASK_PRIO, CONSUMER_SIG_WAKE, 0);
}

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
static volatile uint8_t current_channel = 1;

static void hop_channel(void* arg)
{
    (void)arg;
    current_channel++;
    if (current_channel > 13) current_channel = 1;
    wifi_set_channel(current_channel);
    ets_printf("ch=%u\n", current_channel);
}

static void wifi_init_done_cb(void)
{
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);
    wifi_set_channel(current_channel);
    ets_printf("promisc up, hopping channels every 400ms\n");

    ets_timer_disarm(&channel_timer);
    ets_timer_setfn(&channel_timer, hop_channel, nullptr);
    ets_timer_arm_new(&channel_timer, 2000, true, true);
}

extern "C" void user_init()
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), 1);

    system_os_task(consumer_task, CONSUMER_TASK_PRIO, consumer_queue, CONSUMER_QUEUE_LEN);

    wifi_set_opmode(STATION_MODE);
    system_init_done_cb(wifi_init_done_cb);
}
