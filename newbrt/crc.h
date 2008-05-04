#ifndef TOKU_CRC_H
#define TOKU_CRC_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <sys/types.h>
#include <zlib.h>

// zlib crc32 has a bug:  If len==0 then it should return oldcrc32, but crc32 returns 0.
inline u_int32_t toku_crc32 (u_int32_t oldcrc32, const void *data, u_int32_t len);

static const u_int32_t toku_null_crc = 0;

// Don't use crc32, use toku_crc32 to avoid that bug.
ZEXTERN uLong ZEXPORT crc32   OF((uLong crc, const Bytef *buf, uInt len)) __attribute__((deprecated));
#endif
