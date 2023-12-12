#pragma once

#include "region_allocator.h"

/**
 * RDMABufferAllocator is used to cache temporary data structures that is writing into remote StateNode
 * Different from the RDMARegionAllocator that is used to manage global_data_buffer, 
 * the RDMABufferAllocator is to manager each thread's local data_buffer.
 * Each thread has a __thread_local RDMABufferAllocator.
*/
class RDMABufferAllocator {
public:
    RDMABufferAllocator(char* _start, char* _end)
        : start(_start), end(_end), curr_offset(0) {}
    
    ALWAYS_INLINE
    char* Alloc(size_t size) {
        if(unlikely(start + curr_offset + size > end)) {
            curr_offset = 0;
        }

        char* res = start + curr_offset;
        curr_offset += size;
        return res;
    }

    ALWAYS_INLINE
    void Free(void* p) {
        
    }

private:
    char* start;
    char* end;
    uint64_t curr_offset;
};