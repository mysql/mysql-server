/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef _TOKU_HTONL_H
#define _TOKU_HTONL_H

#if !TOKU_WINDOWS
#error
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__INTEL_COMPILER)

// assume endian == LITTLE_ENDIAN

static inline uint32_t toku_htonl(uint32_t i) {
    return _bswap(i);
}

static inline uint32_t toku_ntohl(uint32_t i) {
    return _bswap(i);
}

#elif defined(_MSVC_VER)
#include <winsock.h>

static inline uint32_t toku_htonl(uint32_t i) {
    return htonl(i);
}

static inline uint32_t toku_ntohl(uint32_t i) {
    return ntonl(i);
}

#endif

#if defined(__cplusplus)
};
#endif

#endif
