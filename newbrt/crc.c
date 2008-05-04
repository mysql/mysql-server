#include <sys/types.h>
#include <zlib.h>
// hack: include crc.h below so we can deprecate the call to crc32

inline u_int32_t toku_crc32 (u_int32_t oldcrc32, const void *data, u_int32_t len) {
    if (len==0) return oldcrc32;
    else return crc32((unsigned long)oldcrc32, data, (uInt)len);
}

// Hack
#include "crc.h"
