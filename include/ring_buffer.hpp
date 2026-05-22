#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "c_types.h"


constexpr size_t RING_BUFFER_SIZE = 16;

class RingBuffer
{
    volatile uint8_t head = 0;
    volatile uint8_t tail = 0;
    uint8_t ring_buffer[RING_BUFFER_SIZE] = {};
public:
    bool push(uint8_t index);
    bool pop(uint8_t& index);

};



#endif