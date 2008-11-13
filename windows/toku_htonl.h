#ifndef _TOKU_HTONL_H
#define _TOKU_HTONL_H

#if !defined(_WIN32)
#error
#endif

#if defined __cplusplus
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

#endif

#if defined(_MSVC_VER)
#include <winsock.h>

static inline uint32_t toku_htonl(uint32_t i) {
    return htonl(i);
}

static inline uint32_t toku_ntohl(uint32_t i) {
    return ntonl(i);
}

#endif

#if defined __cplusplus
};
#endif

#endif
