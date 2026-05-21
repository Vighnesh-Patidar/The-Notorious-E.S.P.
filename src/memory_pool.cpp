#include "memory_pool.hpp"

bool MemoryPool::bitmap[POOL_DEPTH];
uint8_t MemoryPool::pool_array[POOL_DEPTH][FRAME_SLOT_SIZE];


uint8_t* MemoryPool::acquire()
{
    for(int i = 0; i < POOL_DEPTH; i++)
    {
        if(!bitmap[i])
        {
            bitmap[i] = true;
            return pool_array[i];
        }
    }
    return nullptr;
}

void MemoryPool::release(uint8_t* slot)
{
    int offset = slot - pool_array[0];
    int index = offset / FRAME_SLOT_SIZE;
    if(index > POOL_DEPTH) return;
    bitmap[index] = false;

}

uint8_t MemoryPool::get_index(uint8_t* slot)
{
    int offset = slot - pool_array[0];
    return offset / FRAME_SLOT_SIZE;
}

uint8_t* MemoryPool::get_slot(uint8_t index)
{
    return pool_array[index];
}