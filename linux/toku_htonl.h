#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#ifndef _TOKU_HTONL_H
#define _TOKU_HTONL_H

#if !__linux__ && !__FreeBSD__ && !__sun__
#error
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <arpa/inet.h>

static inline uint32_t toku_htonl(uint32_t i) {
    return htonl(i);
}

static inline uint32_t toku_ntohl(uint32_t i) {
    return ntohl(i);
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
