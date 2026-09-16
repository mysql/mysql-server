#include "c4/memory_util.hpp"
#include "c4/error.hpp"

namespace c4 {


/** Fills 'dest' with the first 'pattern_size' bytes at 'pattern', 'num_times'. */
void mem_repeat(void* dest, void const* pattern, size_t pattern_size, size_t num_times)
{
    C4_ASSERT( ! mem_overlaps(dest, pattern, num_times*pattern_size, pattern_size));
    if C4_UNLIKELY(num_times == 0)
        return;
    char *C4_RESTRICT begin = static_cast<char*>(dest);
    char *C4_RESTRICT end   = begin + (num_times * pattern_size);
    // copy the pattern once
    ::memcpy(begin, pattern, pattern_size);
    // now copy from dest to itself, doubling up every time
    size_t n = pattern_size;
    size_t n2 = n * 2;
    while(begin + n2 < end)
    {
        ::memcpy(begin + n, begin, n);
        n = n2;
        n2 *= 2u;
    }
    // copy the missing part
    if(begin + n < end)
    {
        ::memcpy(begin + n, begin, static_cast<size_t>(end - (begin + n)));
    }
}


} // namespace c4
