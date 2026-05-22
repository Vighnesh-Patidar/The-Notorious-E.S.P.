#include "memory_pool.hpp"
#include "filter_engine.hpp"
#include "frame_parser.hpp"
#include "ring_buffer.hpp"
#include <string.h>  // memcpy

extern "C" int ets_printf(const char* fmt, ...);
extern "C" {
    #include "os_type.h"
    #include "user_interface.h"
    #include "eagle_soc.h"
    #include "gpio.h"
    // ets_timer_* are normally in osapi.h, but osapi.h includes <string.h>
    // which closes our extern "C" block early. Declare them manually here
    // so they keep C linkage and the linker finds the symbols in libmain.a.
    void ets_timer_setfn(ETSTimer *ptimer, void (*pfunction)(void *), void *parg);
    void ets_timer_arm_new(ETSTimer *ptimer, uint32_t time_ms, bool repeat, bool is_ms);
    void ets_timer_disarm(ETSTimer *ptimer);
}

// Onboard blue LED on ESP-12/NodeMCU is GPIO2, active LOW.
#define LED_GPIO 2

extern "C" void user_rf_pre_init(void) {}

// 4MB flash, 512+512 map (eagle.app.v6.ld). SDK v3+ requires
// system_partition_table_regist in user_pre_init; user_rf_cal_sector_set
// alone is ignored.
static const partition_item_t at_partition_table[] = {
    { SYSTEM_PARTITION_RF_CAL,           0x3FB000, 0x1000 },
    { SYSTEM_PARTITION_PHY_DATA,         0x3FC000, 0x1000 },
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, 0x3FD000, 0x3000 },
};

MemoryPool memory_pool;
RingBuffer ring_buffer;

// User task queue lives in priority band 0..2. Pick 0 (lowest) so WiFi/lwIP preempt us.
#define CONSUMER_TASK_PRIO   0
#define CONSUMER_QUEUE_LEN   8
#define CONSUMER_SIG_WAKE    1

static os_event_t consumer_queue[CONSUMER_QUEUE_LEN];

static void consumer_task(os_event_t* e)
{
    // Drain as much as we can per dispatch, but don't block the task loop forever.
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
                (frame->src_ip >> 8) & 0xFF, frame->src_ip & 0xFF,
                frame->src_port,
                (frame->dst_ip >> 24) & 0xFF, (frame->dst_ip >> 16) & 0xFF,
                (frame->dst_ip >> 8) & 0xFF, frame->dst_ip & 0xFF,
                frame->dst_port,
                frame->payload_length);
        }
        FilterEngine::apply(frame);
        memory_pool.release(ptr);
    }
}

extern "C" void promisc_cb(uint8_t* buf, uint16_t len)
{
    // Toggle the onboard LED on every received frame (visible activity).
    static uint8_t led_state = 1;
    led_state ^= 1;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), led_state);

    // Periodic log so we can see promisc is alive and what kind of frames
    // are coming in. For len>=13 (RxControl + at least frame-control byte),
    // decode 802.11 type/subtype and the Protected bit so we can tell open
    // from encrypted traffic at a glance.
    static uint32_t rx_count = 0;
    rx_count++;
    if ((rx_count & 0x3F) == 0) {
        if (len >= 14) {
            uint8_t fc0 = buf[12];        // frame control byte 0
            uint8_t fc1 = buf[13];        // frame control byte 1
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
    memcpy(slot, buf, len);
    uint8_t index = memory_pool.get_index(slot);
    memory_pool.set_length(index, len);
    if (!ring_buffer.push(index))
    {
        memory_pool.release(slot);
        return;
    }
    system_os_post(CONSUMER_TASK_PRIO, CONSUMER_SIG_WAKE, 0);
}

extern "C" void user_pre_init(void)
{
    // eagle.app.v6.ld is the 512+512 layout, so flash map = 4
    // (FLASH_SIZE_32M_MAP_512_512). Must match what the binary actually is,
    // or system_partition_table_regist returns false.
    if (!system_partition_table_regist(at_partition_table,
            sizeof(at_partition_table) / sizeof(at_partition_table[0]),
            FLASH_SIZE_32M_MAP_512_512))
    {
        ets_printf("partition table regist fail\n");
        while (1) {}
    }
}

// Channel-hopping so the parser has a chance of seeing an open AP's data
// traffic. Dwell on each channel ~400 ms, walk 1..13.
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
    ets_timer_arm_new(&channel_timer, 400, true, true);
}

extern "C" void user_init()
{
    // GPIO2 as output, LED off (active LOW).
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_GPIO), 1);

    system_os_task(consumer_task, CONSUMER_TASK_PRIO, consumer_queue, CONSUMER_QUEUE_LEN);

    wifi_set_opmode(STATION_MODE);
    // Defer promisc setup until SDK signals it's done initializing,
    // otherwise our channel/promisc settings get overwritten by async STA init.
    system_init_done_cb(wifi_init_done_cb);
}
