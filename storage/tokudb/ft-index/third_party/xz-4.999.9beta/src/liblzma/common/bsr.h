/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       bsr.h
/// \brief      Bit scan reverse
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_BSR_H
#define LZMA_BSR_H

// NOTE: Both input and output variables for lzma_bsr must be uint32_t.

#if defined(__GNUC__) && (defined (HAVE_ASM_X86) || defined(HAVE_ASM_X86_64))
#	define lzma_bsr(dest, n) \
		__asm__("bsrl %1, %0" : "=r" (dest) : "rm" (n))

#else
#	define lzma_bsr(dest, n) dest = lzma_bsr_helper(n)

static inline uint32_t
lzma_bsr_helper(uint32_t n)
{
	assert(n != 0);

	uint32_t i = 31;

	if ((n & UINT32_C(0xFFFF0000)) == 0) {
		n <<= 16;
		i = 15;
	}

	if ((n & UINT32_C(0xFF000000)) == 0) {
		n <<= 8;
		i -= 8;
	}

	if ((n & UINT32_C(0xF0000000)) == 0) {
		n <<= 4;
		i -= 4;
	}

	if ((n & UINT32_C(0xC0000000)) == 0) {
		n <<= 2;
		i -= 2;
	}

	if ((n & UINT32_C(0x80000000)) == 0)
		--i;

	return i;
}

#endif

#endif
