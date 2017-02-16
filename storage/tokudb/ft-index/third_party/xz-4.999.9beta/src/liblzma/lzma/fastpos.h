/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       fastpos.h
/// \brief      Kind of two-bit version of bit scan reverse
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_FASTPOS_H
#define LZMA_FASTPOS_H

// LZMA encodes match distances (positions) by storing the highest two
// bits using a six-bit value [0, 63], and then the missing lower bits.
// Dictionary size is also stored using this encoding in the new .lzma
// file format header.
//
// fastpos.h provides a way to quickly find out the correct six-bit
// values. The following table gives some examples of this encoding:
//
//      pos   return
//       0       0
//       1       1
//       2       2
//       3       3
//       4       4
//       5       4
//       6       5
//       7       5
//       8       6
//      11       6
//      12       7
//     ...      ...
//      15       7
//      16       8
//      17       8
//     ...      ...
//      23       8
//      24       9
//      25       9
//     ...      ...
//
//
// Provided functions or macros
// ----------------------------
//
// get_pos_slot(pos) is the basic version. get_pos_slot_2(pos)
// assumes that pos >= FULL_DISTANCES, thus the result is at least
// FULL_DISTANCES_BITS * 2. Using get_pos_slot(pos) instead of
// get_pos_slot_2(pos) would give the same result, but get_pos_slot_2(pos)
// should be tiny bit faster due to the assumption being made.
//
//
// Size vs. speed
// --------------
//
// With some CPUs that have fast BSR (bit scan reverse) instruction, the
// size optimized version is slightly faster than the bigger table based
// approach. Such CPUs include Intel Pentium Pro, Pentium II, Pentium III
// and Core 2 (possibly others). AMD K7 seems to have slower BSR, but that
// would still have speed roughly comparable to the table version. Older
// x86 CPUs like the original Pentium have very slow BSR; on those systems
// the table version is a lot faster.
//
// On some CPUs, the table version is a lot faster when using position
// dependent code, but with position independent code the size optimized
// version is slightly faster. This occurs at least on 32-bit SPARC (no
// ASM optimizations).
//
// I'm making the table version the default, because that has good speed
// on all systems I have tried. The size optimized version is sometimes
// slightly faster, but sometimes it is a lot slower.

#ifdef HAVE_SMALL
#	include "bsr.h"

#	define get_pos_slot(pos) ((pos) <= 4 ? (pos) : get_pos_slot_2(pos))

static inline uint32_t
get_pos_slot_2(uint32_t pos)
{
	uint32_t i;
	lzma_bsr(i, pos);
	return (i + i) + ((pos >> (i - 1)) & 1);
}


#else

#define FASTPOS_BITS 13

extern const uint8_t lzma_fastpos[1 << FASTPOS_BITS];


#define fastpos_shift(extra, n) \
	((extra) + (n) * (FASTPOS_BITS - 1))

#define fastpos_limit(extra, n) \
	(UINT32_C(1) << (FASTPOS_BITS + fastpos_shift(extra, n)))

#define fastpos_result(pos, extra, n) \
	lzma_fastpos[(pos) >> fastpos_shift(extra, n)] \
			+ 2 * fastpos_shift(extra, n)


static inline uint32_t
get_pos_slot(uint32_t pos)
{
	// If it is small enough, we can pick the result directly from
	// the precalculated table.
	if (pos < fastpos_limit(0, 0))
		return lzma_fastpos[pos];

	if (pos < fastpos_limit(0, 1))
		return fastpos_result(pos, 0, 1);

	return fastpos_result(pos, 0, 2);
}


#ifdef FULL_DISTANCES_BITS
static inline uint32_t
get_pos_slot_2(uint32_t pos)
{
	assert(pos >= FULL_DISTANCES);

	if (pos < fastpos_limit(FULL_DISTANCES_BITS - 1, 0))
		return fastpos_result(pos, FULL_DISTANCES_BITS - 1, 0);

	if (pos < fastpos_limit(FULL_DISTANCES_BITS - 1, 1))
		return fastpos_result(pos, FULL_DISTANCES_BITS - 1, 1);

	return fastpos_result(pos, FULL_DISTANCES_BITS - 1, 2);
}
#endif

#endif

#endif
