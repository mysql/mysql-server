///////////////////////////////////////////////////////////////////////////////
//
/// \file       bswap.h
/// \brief      Byte swapping
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_BSWAP_H
#define LZMA_BSWAP_H

// NOTE: We assume that config.h is already #included.

// At least glibc has byteswap.h which contains inline assembly code for
// byteswapping. Some systems have byteswap.h but lack one or more of the
// bswap_xx macros/functions, which is why we check them separately even
// if byteswap.h is available.

#ifdef HAVE_BYTESWAP_H
#	include <byteswap.h>
#endif

#ifndef HAVE_BSWAP_16
#	define bswap_16(num) \
		(((num) << 8) | ((num) >> 8))
#endif

#ifndef HAVE_BSWAP_32
#	define bswap_32(num) \
		( (((num) << 24)                       ) \
		| (((num) <<  8) & UINT32_C(0x00FF0000)) \
		| (((num) >>  8) & UINT32_C(0x0000FF00)) \
		| (((num) >> 24)                       ) )
#endif

#ifndef HAVE_BSWAP_64
#	define bswap_64(num) \
		( (((num) << 56)                               ) \
		| (((num) << 40) & UINT64_C(0x00FF000000000000)) \
		| (((num) << 24) & UINT64_C(0x0000FF0000000000)) \
		| (((num) <<  8) & UINT64_C(0x000000FF00000000)) \
		| (((num) >>  8) & UINT64_C(0x00000000FF000000)) \
		| (((num) >> 24) & UINT64_C(0x0000000000FF0000)) \
		| (((num) >> 40) & UINT64_C(0x000000000000FF00)) \
		| (((num) >> 56)                               ) )
#endif

#endif
