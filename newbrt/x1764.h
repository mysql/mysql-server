#ifndef X1764_H
#define X1764_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <sys/types.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// The x1764 hash is
//   $s = \sum_i a_i*17^i$  where $a_i$ is the $i$th 64-bit number (represented in little-endian format)
// The final 32-bit result is the xor of the high- and low-order bits of s.
// If any odd bytes numbers are left at the end, they are filled in at the low end.


u_int32_t x1764_memory (const void *buf, int len);
// Effect: Compute x1764 on the bytes of buf.  Return the 32 bit answer.

u_int32_t x1764_memory_simple (const void *buf, int len);
// Effect: Same as x1764_memory, but not highly optimized (more likely to be correct).  Useful for testing the optimized version.


// For incrementally computing an x1764, use the following interfaces.
struct x1764 {
    u_int64_t sum;
    u_int64_t input;
    int n_input_bytes;
};

void x1764_init(struct x1764 *l);
// Effect: Initialize *l.

void x1764_add (struct x1764 *l, const void *vbuf, int len);
// Effect: Add more bytes to *l.

u_int32_t x1764_finish (struct x1764 *l);
// Effect: Return the final 32-bit result.

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
