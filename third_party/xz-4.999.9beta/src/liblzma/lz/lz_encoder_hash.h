/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_encoder_hash.h
/// \brief      Hash macros for match finders
//
//  Author:     Igor Pavlov
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZ_ENCODER_HASH_H
#define LZMA_LZ_ENCODER_HASH_H

#define HASH_2_SIZE (UINT32_C(1) << 10)
#define HASH_3_SIZE (UINT32_C(1) << 16)
#define HASH_4_SIZE (UINT32_C(1) << 20)

#define HASH_2_MASK (HASH_2_SIZE - 1)
#define HASH_3_MASK (HASH_3_SIZE - 1)
#define HASH_4_MASK (HASH_4_SIZE - 1)

#define FIX_3_HASH_SIZE (HASH_2_SIZE)
#define FIX_4_HASH_SIZE (HASH_2_SIZE + HASH_3_SIZE)
#define FIX_5_HASH_SIZE (HASH_2_SIZE + HASH_3_SIZE + HASH_4_SIZE)

// TODO Benchmark, and probably doesn't need to be endian dependent.
#if !defined(WORDS_BIGENDIAN) && defined(HAVE_FAST_UNALIGNED_ACCESS)
#	define hash_2_calc() \
		const uint32_t hash_value = *(const uint16_t *)(cur);
#else
#	define hash_2_calc() \
		const uint32_t hash_value \
			= (uint32_t)(cur[0]) | ((uint32_t)(cur[1]) << 8)
#endif

#define hash_3_calc() \
	const uint32_t temp = lzma_crc32_table[0][cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & mf->hash_mask

#define hash_4_calc() \
	const uint32_t temp = lzma_crc32_table[0][cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	const uint32_t hash_value = (temp ^ ((uint32_t)(cur[2]) << 8) \
			^ (lzma_crc32_table[0][cur[3]] << 5)) & mf->hash_mask


// The following are not currently used.

#define hash_5_calc() \
	const uint32_t temp = lzma_crc32_table[0][cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	uint32_t hash_4_value = (temp ^ ((uint32_t)(cur[2]) << 8) ^ \
			^ lzma_crc32_table[0][cur[3]] << 5); \
	const uint32_t hash_value \
			= (hash_4_value ^ (lzma_crc32_table[0][cur[4]] << 3)) \
				& mf->hash_mask; \
	hash_4_value &= HASH_4_MASK

/*
#define hash_zip_calc() \
	const uint32_t hash_value \
			= (((uint32_t)(cur[0]) | ((uint32_t)(cur[1]) << 8)) \
				^ lzma_crc32_table[0][cur[2]]) & 0xFFFF
*/

#define hash_zip_calc() \
	const uint32_t hash_value \
			= (((uint32_t)(cur[2]) | ((uint32_t)(cur[0]) << 8)) \
				^ lzma_crc32_table[0][cur[1]]) & 0xFFFF

#define mt_hash_2_calc() \
	const uint32_t hash_2_value \
			= (lzma_crc32_table[0][cur[0]] ^ cur[1]) & HASH_2_MASK

#define mt_hash_3_calc() \
	const uint32_t temp = lzma_crc32_table[0][cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK

#define mt_hash_4_calc() \
	const uint32_t temp = lzma_crc32_table[0][cur[0]] ^ cur[1]; \
	const uint32_t hash_2_value = temp & HASH_2_MASK; \
	const uint32_t hash_3_value \
			= (temp ^ ((uint32_t)(cur[2]) << 8)) & HASH_3_MASK; \
	const uint32_t hash_4_value = (temp ^ ((uint32_t)(cur[2]) << 8) ^ \
			(lzma_crc32_table[0][cur[3]] << 5)) & HASH_4_MASK

#endif
