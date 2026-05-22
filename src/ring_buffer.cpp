#include "ring_buffer.hpp"

bool RingBuffer::push(uint8_t index)
{
    if(((head+1) & (RING_BUFFER_SIZE-1)) == tail)
    {
        return false;
    } else {
        ring_buffer[head] = index;
        head = (head + 1) & (RING_BUFFER_SIZE - 1);
    }
    return true;
}

bool RingBuffer::pop(uint8_t& index)
{
    if(head == tail)
    {
        return false;
    } else {
        index = ring_buffer[tail];
        tail = (tail + 1) & (RING_BUFFER_SIZE-1);
    }
    return true;
}