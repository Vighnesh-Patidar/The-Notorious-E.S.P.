#include "memory_pool.hpp"
#include "filter_engine.hpp"
#include "frame_parser.hpp"
#include "ring_buffer.hpp"
#include "user_interface.h"

MemoryPool memory_pool;
RingBuffer ring_buffer;

void promisc_cb(uint8_t* buf, uint16_t len)
{
    uint8_t* slot = memory_pool.acquire();
    if(slot == nullptr) return;
    memcpy(slot, buf, len);
    uint8_t index = memory_pool.get_index(slot);
    ring_buffer.push(index);
}


void user_init()
{
    wifi_set_opmode(STATION_MODE);


    wifi_set_promiscuous_recv_cb(promisc_cb);
    wifi_promiscuous_enable(1);

    while(true)
    {
        uint8_t index;
        if(ring_buffer.pop(index))
        {
            uint8_t* ptr = memory_pool.get_slot(index);
            ParsedFrame* frame = FrameParser::parse(ptr, FRAME_SLOT_SIZE);
            FilterEngine::apply(frame);
            memory_pool.release(ptr);   
    }
}
}