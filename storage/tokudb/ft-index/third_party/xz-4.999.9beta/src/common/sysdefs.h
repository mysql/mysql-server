/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       sysdefs.h
/// \brief      Common includes, definitions, system-specific things etc.
///
/// This file is used also by the lzma command line tool, that's why this
/// file is separate from common.h.
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_SYSDEFS_H
#define LZMA_SYSDEFS_H

//////////////
// Includes //
//////////////

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

// size_t and NULL
#include <stddef.h>

#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif

// C99 says that inttypes.h always includes stdint.h, but some systems
// don't do that, and require including stdint.h separately.
#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif

// Some pre-C99 systems have SIZE_MAX in limits.h instead of stdint.h. The
// limits are also used to figure out some macros missing from pre-C99 systems.
#ifdef HAVE_LIMITS_H
#	include <limits.h>
#endif

// Be more compatible with systems that have non-conforming inttypes.h.
// We assume that int is 32-bit and that long is either 32-bit or 64-bit.
// Full Autoconf test could be more correct, but this should work well enough.
// Note that this duplicates some code from lzma.h, but this is better since
// we can work without inttypes.h thanks to Autoconf tests.
#ifndef UINT32_C
#	if UINT_MAX != 4294967295U
#		error UINT32_C is not defined and unsiged int is not 32-bit.
#	endif
#	define UINT32_C(n) n ## U
#endif
#ifndef UINT32_MAX
#	define UINT32_MAX UINT32_C(4294967295)
#endif
#ifndef PRIu32
#	define PRIu32 "u"
#endif
#ifndef PRIX32
#	define PRIX32 "X"
#endif

#if ULONG_MAX == 4294967295UL
#	ifndef UINT64_C
#		define UINT64_C(n) n ## ULL
#	endif
#	ifndef PRIu64
#		define PRIu64 "llu"
#	endif
#	ifndef PRIX64
#		define PRIX64 "llX"
#	endif
#else
#	ifndef UINT64_C
#		define UINT64_C(n) n ## UL
#	endif
#	ifndef PRIu64
#		define PRIu64 "lu"
#	endif
#	ifndef PRIX64
#		define PRIX64 "lX"
#	endif
#endif
#ifndef UINT64_MAX
#	define UINT64_MAX UINT64_C(18446744073709551615)
#endif

// The code currently assumes that size_t is either 32-bit or 64-bit.
#ifndef SIZE_MAX
#	if SIZEOF_SIZE_T == 4
#		define SIZE_MAX UINT32_MAX
#	elif SIZEOF_SIZE_T == 8
#		define SIZE_MAX UINT64_MAX
#	else
#		error sizeof(size_t) is not 32-bit or 64-bit
#	endif
#endif
#if SIZE_MAX != UINT32_MAX && SIZE_MAX != UINT64_MAX
#	error sizeof(size_t) is not 32-bit or 64-bit
#endif

#include <stdlib.h>
#include <assert.h>

// Pre-C99 systems lack stdbool.h. All the code in LZMA Utils must be written
// so that it works with fake bool type, for example:
//
//    bool foo = (flags & 0x100) != 0;
//    bool bar = !!(flags & 0x100);
//
// This works with the real C99 bool but breaks with fake bool:
//
//    bool baz = (flags & 0x100);
//
#ifdef HAVE_STDBOOL_H
#	include <stdbool.h>
#else
#	if ! HAVE__BOOL
typedef unsigned char _Bool;
#	endif
#	define bool _Bool
#	define false 0
#	define true 1
#	define __bool_true_false_are_defined 1
#endif

// string.h should be enough but let's include strings.h and memory.h too if
// they exists, since that shouldn't do any harm, but may improve portability.
#ifdef HAVE_STRING_H
#	include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#	include <strings.h>
#endif

#ifdef HAVE_MEMORY_H
#	include <memory.h>
#endif


////////////
// Macros //
////////////

#if defined(_WIN32) || defined(__MSDOS__) || defined(__OS2__)
#	define DOSLIKE 1
#endif

#undef memzero
#define memzero(s, n) memset(s, 0, n)

#ifndef MIN
#	define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#	define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef ARRAY_SIZE
#	define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#endif
