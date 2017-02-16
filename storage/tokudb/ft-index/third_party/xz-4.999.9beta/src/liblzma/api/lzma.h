/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/**
 * \file        api/lzma.h
 * \brief       The public API of liblzma data compression library
 *
 * liblzma is a public domain general-purpose data compression library with
 * a zlib-like API. The native file format is .xz, but also the old .lzma
 * format and raw (no headers) streams are supported. Multiple compression
 * algorithms (filters) are supported. Currently LZMA2 is the primary filter.
 *
 * liblzma is part of XZ Utils <http://tukaani.org/xz/>. XZ Utils includes
 * a gzip-like command line tool named xz and some other tools. XZ Utils
 * is developed and maintained by Lasse Collin.
 *
 * Major parts of liblzma are based on Igor Pavlov's public domain LZMA SDK
 * <http://7-zip.org/sdk.html>.
 *
 * The SHA-256 implementation is based on the public domain code found from
 * 7-Zip <http://7-zip.org/>, which has a modified version of the public
 * domain SHA-256 code found from Crypto++ <http://www.cryptopp.com/>.
 * The SHA-256 code in Crypto++ was written by Kevin Springle and Wei Dai.
 */

/*
 * Author: Lasse Collin
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

#ifndef LZMA_H
#define LZMA_H

/*****************************
 * Required standard headers *
 *****************************/

/*
 * liblzma API headers need some standard types and macros. To allow
 * including lzma.h without requiring the application to include other
 * headers first, lzma.h includes the required standard headers unless
 * they already seem to be included already or if LZMA_MANUAL_HEADERS
 * has been defined.
 *
 * Here's what types and macros are needed and from which headers:
 *  - stddef.h: size_t, NULL
 *  - stdint.h: uint8_t, uint32_t, uint64_t, UINT32_C(n), uint64_C(n),
 *    UINT32_MAX, UINT64_MAX
 *
 * However, inttypes.h is a little more portable than stdint.h, although
 * inttypes.h declares some unneeded things compared to plain stdint.h.
 *
 * The hacks below aren't perfect, specifically they assume that inttypes.h
 * exists and that it typedefs at least uint8_t, uint32_t, and uint64_t,
 * and that, in case of incomplete inttypes.h, unsigned int is 32-bit.
 * If the application already takes care of setting up all the types and
 * macros properly (for example by using gnulib's stdint.h or inttypes.h),
 * we try to detect that the macros are already defined and don't include
 * inttypes.h here again. However, you may define LZMA_MANUAL_HEADERS to
 * force this file to never include any system headers.
 *
 * Some could argue that liblzma API should provide all the required types,
 * for example lzma_uint64, LZMA_UINT64_C(n), and LZMA_UINT64_MAX. This was
 * seen unnecessary mess, since most systems already provide all the necessary
 * types and macros in the standard headers.
 *
 * Note that liblzma API still has lzma_bool, because using stdbool.h would
 * break C89 and C++ programs on many systems. sizeof(bool) in C99 isn't
 * necessarily the same as sizeof(bool) in C++.
 */

#ifndef LZMA_MANUAL_HEADERS
	/*
	 * I suppose this works portably also in C++. Note that in C++,
	 * we need to get size_t into the global namespace.
	 */
#	include <stddef.h>

	/*
	 * Skip inttypes.h if we already have all the required macros. If we
	 * have the macros, we assume that we have the matching typedefs too.
	 */
#	if !defined(UINT32_C) || !defined(UINT64_C) \
			|| !defined(UINT32_MAX) || !defined(UINT64_MAX)
		/*
		 * MSVC has no C99 support, and thus it cannot be used to
		 * compile liblzma. The liblzma API has to still be usable
		 * from MSVC, so we need to define the required standard
		 * integer types here.
		 */
#		if defined(_WIN32) && defined(_MSC_VER)
			typedef unsigned __int8 uint8_t;
			typedef unsigned __int32 uint32_t;
			typedef unsigned __int64 uint64_t;
#		else
			/* Use the standard inttypes.h. */
#			ifdef __cplusplus
				/*
				 * C99 sections 7.18.2 and 7.18.4 specify that
				 * in C++ implementations define the limit
				 * and constant macros only if specifically
				 * requested. Note that if you want the
				 * format macros (PRIu64 etc.) too, you need
				 * to define __STDC_FORMAT_MACROS before
				 * including lzma.h, since re-including
				 * inttypes.h with __STDC_FORMAT_MACROS
				 * defined doesn't necessarily work.
				 */
#				ifndef __STDC_LIMIT_MACROS
#					define __STDC_LIMIT_MACROS 1
#				endif
#				ifndef __STDC_CONSTANT_MACROS
#					define __STDC_CONSTANT_MACROS 1
#				endif
#			endif

#			include <inttypes.h>
#		endif

		/*
		 * Some old systems have only the typedefs in inttypes.h, and
		 * lack all the macros. For those systems, we need a few more
		 * hacks. We assume that unsigned int is 32-bit and unsigned
		 * long is either 32-bit or 64-bit. If these hacks aren't
		 * enough, the application has to setup the types manually
		 * before including lzma.h.
		 */
#		ifndef UINT32_C
#			if defined(_WIN32) && defined(_MSC_VER)
#				define UINT32_C(n) n ## UI32
#			else
#				define UINT32_C(n) n ## U
#			endif
#		endif

#		ifndef UINT64_C
#			if defined(_WIN32) && defined(_MSC_VER)
#				define UINT64_C(n) n ## UI64
#			else
				/* Get ULONG_MAX. */
#				include <limits.h>
#				if ULONG_MAX == 4294967295UL
#					define UINT64_C(n) n ## ULL
#				else
#					define UINT64_C(n) n ## UL
#				endif
#			endif
#		endif

#		ifndef UINT32_MAX
#			define UINT32_MAX (UINT32_C(4294967295))
#		endif

#		ifndef UINT64_MAX
#			define UINT64_MAX (UINT64_C(18446744073709551615))
#		endif
#	endif
#endif /* ifdef LZMA_MANUAL_HEADERS */


/******************
 * LZMA_API macro *
 ******************/

/*
 * Some systems require (or at least recommend) that the functions and
 * function pointers are declared specially in the headers. LZMA_API_IMPORT
 * is for importing symbols and LZMA_API_CALL is to specify calling
 * convention.
 *
 * By default it is assumed that the application will link dynamically
 * against liblzma. #define LZMA_API_STATIC in your application if you
 * want to link against static liblzma. If you don't care about portability
 * to operating systems like Windows, or at least don't care about linking
 * against static liblzma on them, don't worry about LZMA_API_STATIC. That
 * is, most developers will never need to use LZMA_API_STATIC.
 *
 * Cygwin is a special case on Windows. We rely on GCC doing the right thing
 * and thus don't use dllimport and don't specify the calling convention.
 */
#ifndef LZMA_API_IMPORT
#	if !defined(LZMA_API_STATIC) && defined(_WIN32) && !defined(__CYGWIN__)
#		define LZMA_API_IMPORT __declspec(dllimport)
#	else
#		define LZMA_API_IMPORT
#	endif
#endif

#ifndef LZMA_API_CALL
#	if defined(_WIN32) && !defined(__CYGWIN__)
#		define LZMA_API_CALL __cdecl
#	else
#		define LZMA_API_CALL
#	endif
#endif

#ifndef LZMA_API
#	define LZMA_API(type) LZMA_API_IMPORT type LZMA_API_CALL
#endif


/***********
 * nothrow *
 ***********/

/*
 * None of the functions in liblzma may throw an exception. Even
 * the functions that use callback functions won't throw exceptions,
 * because liblzma would break if a callback function threw an exception.
 */
#ifndef lzma_nothrow
#	if defined(__cplusplus)
#		define lzma_nothrow throw()
#	elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#		define lzma_nothrow __attribute__((__nothrow__))
#	else
#		define lzma_nothrow
#	endif
#endif


/********************
 * GNU C extensions *
 ********************/

/*
 * GNU C extensions are used conditionally in the public API. It doesn't
 * break anything if these are sometimes enabled and sometimes not, only
 * affects warnings and optimizations.
 */
#if __GNUC__ >= 3
#	ifndef lzma_attribute
#		define lzma_attribute(attr) __attribute__(attr)
#	endif

#	ifndef lzma_restrict
#		define lzma_restrict __restrict__
#	endif

	/* warn_unused_result was added in GCC 3.4. */
#	ifndef lzma_attr_warn_unused_result
#		if __GNUC__ == 3 && __GNUC_MINOR__ < 4
#			define lzma_attr_warn_unused_result
#		endif
#	endif

#else
#	ifndef lzma_attribute
#		define lzma_attribute(attr)
#	endif

#	ifndef lzma_restrict
#		if __STDC_VERSION__ >= 199901L
#			define lzma_restrict restrict
#		else
#			define lzma_restrict
#		endif
#	endif
#endif


#ifndef lzma_attr_pure
#	define lzma_attr_pure lzma_attribute((__pure__))
#endif

#ifndef lzma_attr_const
#	define lzma_attr_const lzma_attribute((__const__))
#endif

#ifndef lzma_attr_warn_unused_result
#	define lzma_attr_warn_unused_result \
		lzma_attribute((__warn_unused_result__))
#endif


/**************
 * Subheaders *
 **************/

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subheaders check that this is defined. It is to prevent including
 * them directly from applications.
 */
#define LZMA_H_INTERNAL 1

/* Basic features */
#include "lzma/version.h"
#include "lzma/base.h"
#include "lzma/vli.h"
#include "lzma/check.h"

/* Filters */
#include "lzma/filter.h"
#include "lzma/subblock.h"
#include "lzma/bcj.h"
#include "lzma/delta.h"
#include "lzma/lzma.h"

/* Container formats */
#include "lzma/container.h"

/* Advanced features */
#include "lzma/stream_flags.h"
#include "lzma/block.h"
#include "lzma/index.h"
#include "lzma/index_hash.h"

/*
 * All subheaders included. Undefine LZMA_H_INTERNAL to prevent applications
 * re-including the subheaders.
 */
#undef LZMA_H_INTERNAL

#ifdef __cplusplus
}
#endif

#endif /* ifndef LZMA_H */
