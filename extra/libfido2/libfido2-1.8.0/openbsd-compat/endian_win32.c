/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include "openbsd-compat.h"

#if defined(_WIN32) && !defined(HAVE_ENDIAN_H)

/*
 * Hopefully, if the endianness differs from the end result, the compiler
 * optimizes these functions with some type of bswap instruction. Or,
 * otherwise, to just return the input value unmodified. GCC and clang
 * both does these optimization at least. This should be preferred over
 * relying on some BYTE_ORDER macro, which may or may not be defined.
 */

uint32_t
htole32(uint32_t in)
{
	uint32_t	 out = 0;
	uint8_t		*b = (uint8_t *)&out;

	b[0] = (uint8_t)((in >> 0)  & 0xff);
	b[1] = (uint8_t)((in >> 8)  & 0xff);
	b[2] = (uint8_t)((in >> 16) & 0xff);
	b[3] = (uint8_t)((in >> 24) & 0xff);

	return (out);
}

uint64_t
htole64(uint64_t in)
{
	uint64_t	 out = 0;
	uint8_t		*b = (uint8_t *)&out;

	b[0] = (uint8_t)((in >> 0)  & 0xff);
	b[1] = (uint8_t)((in >> 8)  & 0xff);
	b[2] = (uint8_t)((in >> 16) & 0xff);
	b[3] = (uint8_t)((in >> 24) & 0xff);
	b[4] = (uint8_t)((in >> 32) & 0xff);
	b[5] = (uint8_t)((in >> 40) & 0xff);
	b[6] = (uint8_t)((in >> 48) & 0xff);
	b[7] = (uint8_t)((in >> 56) & 0xff);

	return (out);
}

#endif /* WIN32 && !HAVE_ENDIAN_H */
