

#ifndef MEM_POOL_HPP
#define MEM_POOL_HPP

#include "c_types.h"


constexpr size_t FRAME_SLOT_SIZE = 256;
constexpr size_t POOL_DEPTH = 16;

class MemoryPool
{
    
    static bool bitmap[POOL_DEPTH];
    static uint8_t pool_array[POOL_DEPTH][FRAME_SLOT_SIZE];

public :
    uint8_t* acquire();
    void release(uint8_t*);
    uint8_t get_index(uint8_t* slot);
    uint8_t* get_slot(uint8_t index);


};


#endif