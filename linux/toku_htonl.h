#ifndef _TOKU_HTONL_H
#define _TOKU_HTONL_H

#if !__linux__
#error
#endif

#if __cplusplus
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

#if __cplusplus
};
#endif

#endif
