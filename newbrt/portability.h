#ifndef PORTABILITY_H
#define PORTABILITY_H

// Portability layer


#if defined(__ICC)

// Intel compiler.
//  Define ntohl using bswap.
//  Define __attribute__ to be null

#include <sys/types.h>

static inline
u_int32_t
ntohl(u_int32_t x) {
    return _bswap(x);
}
static inline
u_int32_t
htonl(u_int32_t x) {
    return _bswap(x);
}

#define __attribute__(x)

#elif defined __GNUC__

// Gcc:
//   Define ntohl using arpa/inet.h
#include <arpa/inet.h>
#else
#error Not ICC and not GNUC.  What compiler?
#endif


#endif
