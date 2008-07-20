#ifndef MURMUR_H
#define MURMUR_H
#include <sys/types.h>
u_int32_t murmur_string (void *data, int len);


// This structure is designed to allow us to incrementally compute the murmur function.
// The murmur function operates on 4-byte values, and then if there are few bytes left at the end it handles them specially.
// Thus to perform the computation incrementally, we may end up with a few extra bytes.  We must hang on to those extra bytes
// until we either get 4 byte (in which case murmur can run a little further) or until we ge to the end. 
struct murmur {
    int n_bytes_in_k;  // How many bytes in k
    u_int32_t k;       // These are the extra bytes.   Bytes are shifted into the low-order bits.
    u_int32_t h;       // The hash so far (up to the most recent 4-byte boundary)
};
void murmur_init (struct murmur *mm);
void murmur_add (struct murmur *mm, const void * key, unsigned int len);
u_int32_t murmur_finish (struct murmur *mm);
#endif
