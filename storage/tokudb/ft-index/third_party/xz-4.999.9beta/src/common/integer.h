/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       integer.h
/// \brief      Reading and writing integers from and to buffers
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_INTEGER_H
#define LZMA_INTEGER_H

// On big endian, we need byte swapping. These macros may be used outside
// this file, so don't put these inside HAVE_FAST_UNALIGNED_ACCESS.
#ifdef WORDS_BIGENDIAN
#	include "bswap.h"
#	define integer_le_16(n) bswap_16(n)
#	define integer_le_32(n) bswap_32(n)
#	define integer_le_64(n) bswap_64(n)
#else
#	define integer_le_16(n) (n)
#	define integer_le_32(n) (n)
#	define integer_le_64(n) (n)
#endif


// I'm aware of AC_CHECK_ALIGNED_ACCESS_REQUIRED from Autoconf archive, but
// it's not useful here. We don't care if unaligned access is supported,
// we care if it is fast. Some systems can emulate unaligned access in
// software, which is horribly slow; we want to use byte-by-byte access on
// such systems but the Autoconf test would detect such a system as
// supporting unaligned access.
//
// NOTE: HAVE_FAST_UNALIGNED_ACCESS indicates only support for 16-bit and
// 32-bit integer loads and stores. 64-bit integers may or may not work.
// That's why 64-bit functions are commented out.
//
// TODO: Big endian PowerPC supports byte swapping load and store instructions
// that also allow unaligned access. Inline assembler could be OK for that.
//
// Performance of these functions isn't that important until LZMA3, but it
// doesn't hurt to have these ready already.
#ifdef HAVE_FAST_UNALIGNED_ACCESS

static inline uint16_t
integer_read_16(const uint8_t buf[static 2])
{
	uint16_t ret = *(const uint16_t *)(buf);
	return integer_le_16(ret);
}


static inline uint32_t
integer_read_32(const uint8_t buf[static 4])
{
	uint32_t ret = *(const uint32_t *)(buf);
	return integer_le_32(ret);
}


/*
static inline uint64_t
integer_read_64(const uint8_t buf[static 8])
{
	uint64_t ret = *(const uint64_t *)(buf);
	return integer_le_64(ret);
}
*/


static inline void
integer_write_16(uint8_t buf[static 2], uint16_t num)
{
	*(uint16_t *)(buf) = integer_le_16(num);
}


static inline void
integer_write_32(uint8_t buf[static 4], uint32_t num)
{
	*(uint32_t *)(buf) = integer_le_32(num);
}


/*
static inline void
integer_write_64(uint8_t buf[static 8], uint64_t num)
{
	*(uint64_t *)(buf) = integer_le_64(num);
}
*/


#else

static inline uint16_t
integer_read_16(const uint8_t buf[static 2])
{
	uint16_t ret = buf[0] | (buf[1] << 8);
	return ret;
}


static inline uint32_t
integer_read_32(const uint8_t buf[static 4])
{
	uint32_t ret = buf[0];
	ret |= (uint32_t)(buf[1]) << 8;
	ret |= (uint32_t)(buf[2]) << 16;
	ret |= (uint32_t)(buf[3]) << 24;
	return ret;
}


/*
static inline uint64_t
integer_read_64(const uint8_t buf[static 8])
{
	uint64_t ret = buf[0];
	ret |= (uint64_t)(buf[1]) << 8;
	ret |= (uint64_t)(buf[2]) << 16;
	ret |= (uint64_t)(buf[3]) << 24;
	ret |= (uint64_t)(buf[4]) << 32;
	ret |= (uint64_t)(buf[5]) << 40;
	ret |= (uint64_t)(buf[6]) << 48;
	ret |= (uint64_t)(buf[7]) << 56;
	return ret;
}
*/


static inline void
integer_write_16(uint8_t buf[static 2], uint16_t num)
{
	buf[0] = (uint8_t)(num);
	buf[1] = (uint8_t)(num >> 8);
}


static inline void
integer_write_32(uint8_t buf[static 4], uint32_t num)
{
	buf[0] = (uint8_t)(num);
	buf[1] = (uint8_t)(num >> 8);
	buf[2] = (uint8_t)(num >> 16);
	buf[3] = (uint8_t)(num >> 24);
}


/*
static inline void
integer_write_64(uint8_t buf[static 8], uint64_t num)
{
	buf[0] = (uint8_t)(num);
	buf[1] = (uint8_t)(num >> 8);
	buf[2] = (uint8_t)(num >> 16);
	buf[3] = (uint8_t)(num >> 24);
	buf[4] = (uint8_t)(num >> 32);
	buf[5] = (uint8_t)(num >> 40);
	buf[6] = (uint8_t)(num >> 48);
	buf[7] = (uint8_t)(num >> 56);
}
*/

#endif

#endif
