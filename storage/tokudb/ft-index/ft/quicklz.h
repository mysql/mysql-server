/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ifndef QLZ_HEADER
#define QLZ_HEADER

// Fast data compression library
// Copyright (C) 2006-2011 Lasse Mikkel Reinhold
// lar@quicklz.com
//
// QuickLZ can be used for free under the GPL 1, 2 or 3 license (where anything 
// released into public must be open source) or under a commercial license if such 
// has been acquired (see http://www.quicklz.com/order.html). The commercial license 
// does not cover derived or ported versions created by third parties under GPL.

// You can edit following user settings. Data must be decompressed with the same 
// setting of QLZ_COMPRESSION_LEVEL and QLZ_STREAMING_BUFFER as it was compressed
// (see manual). If QLZ_STREAMING_BUFFER > 0, scratch buffers must be initially
// zeroed out (see manual). First #ifndef makes it possible to define settings from 
// the outside like the compiler command line.

// 1.5.0 final

#ifndef QLZ_COMPRESSION_LEVEL
	//#define QLZ_COMPRESSION_LEVEL 1
	//#define QLZ_COMPRESSION_LEVEL 2
	#define QLZ_COMPRESSION_LEVEL 3

	#define QLZ_STREAMING_BUFFER 0
	//#define QLZ_STREAMING_BUFFER 100000
	//#define QLZ_STREAMING_BUFFER 1000000

	//#define QLZ_MEMORY_SAFE
#endif

#define QLZ_VERSION_MAJOR 1
#define QLZ_VERSION_MINOR 5
#define QLZ_VERSION_REVISION 0

// Using size_t, memset() and memcpy()
#include <string.h>

// Verify compression level
#if QLZ_COMPRESSION_LEVEL != 1 && QLZ_COMPRESSION_LEVEL != 2 && QLZ_COMPRESSION_LEVEL != 3
#error QLZ_COMPRESSION_LEVEL must be 1, 2 or 3
#endif

typedef unsigned int ui32;
typedef unsigned short int ui16;

// Decrease QLZ_POINTERS for level 3 to increase compression speed. Do not touch any other values!
#if QLZ_COMPRESSION_LEVEL == 1
#define QLZ_POINTERS 1
#define QLZ_HASH_VALUES 4096
#elif QLZ_COMPRESSION_LEVEL == 2
#define QLZ_POINTERS 4
#define QLZ_HASH_VALUES 2048
#elif QLZ_COMPRESSION_LEVEL == 3
#define QLZ_POINTERS 16
#define QLZ_HASH_VALUES 4096
#endif

// Detect if pointer size is 64-bit. It's not fatal if some 64-bit target is not detected because this is only for adding an optional 64-bit optimization.
#if defined _LP64 || defined __LP64__ || defined __64BIT__ || _ADDR64 || defined _WIN64 || defined __arch64__ || __WORDSIZE == 64 || (defined __sparc && defined __sparcv9) || defined __x86_64 || defined __amd64 || defined __x86_64__ || defined _M_X64 || defined _M_IA64 || defined __ia64 || defined __IA64__
	#define QLZ_PTR_64
#endif

// hash entry
typedef struct 
{
#if QLZ_COMPRESSION_LEVEL == 1
	ui32 cache;
#if defined QLZ_PTR_64 && QLZ_STREAMING_BUFFER == 0
	unsigned int offset;
#else
	const unsigned char *offset;
#endif
#else
	const unsigned char *offset[QLZ_POINTERS];
#endif

} qlz_hash_compress;

typedef struct 
{
#if QLZ_COMPRESSION_LEVEL == 1
	const unsigned char *offset;
#else
	const unsigned char *offset[QLZ_POINTERS];
#endif
} qlz_hash_decompress;


// states
typedef struct
{
	#if QLZ_STREAMING_BUFFER > 0
		unsigned char stream_buffer[QLZ_STREAMING_BUFFER];
	#endif
	size_t stream_counter;
	qlz_hash_compress hash[QLZ_HASH_VALUES];
	unsigned char hash_counter[QLZ_HASH_VALUES];
} qlz_state_compress;


#if QLZ_COMPRESSION_LEVEL == 1 || QLZ_COMPRESSION_LEVEL == 2
	typedef struct
	{
#if QLZ_STREAMING_BUFFER > 0
		unsigned char stream_buffer[QLZ_STREAMING_BUFFER];
#endif
		qlz_hash_decompress hash[QLZ_HASH_VALUES];
		unsigned char hash_counter[QLZ_HASH_VALUES];
		size_t stream_counter;
	} qlz_state_decompress;
#elif QLZ_COMPRESSION_LEVEL == 3
	typedef struct
	{
#if QLZ_STREAMING_BUFFER > 0
		unsigned char stream_buffer[QLZ_STREAMING_BUFFER];
#endif
#if QLZ_COMPRESSION_LEVEL <= 2
		qlz_hash_decompress hash[QLZ_HASH_VALUES];
#endif
		size_t stream_counter;
	} qlz_state_decompress;
#endif


#if defined (__cplusplus)
extern "C" {
#endif

// Public functions of QuickLZ
size_t qlz_size_decompressed(const char *source);
size_t qlz_size_compressed(const char *source);
size_t qlz_compress(const void *source, char *destination, size_t size, qlz_state_compress *state);
size_t qlz_decompress(const char *source, void *destination, qlz_state_decompress *state);
int qlz_get_setting(int setting);

#if defined (__cplusplus)
}
#endif

#endif

