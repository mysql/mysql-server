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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

/* Purpose of this file is to provide definitions of 
 * Host to Disk byte transposition functions, an abstraction of
 * htod32()/dtoh32() and htod16()/dtoh16() functions.
 *
 * These htod/dtoh functions will only perform the transposition
 * if the disk and host are defined to be in opposite endian-ness.
 * If we define the disk to be in host order, then no byte 
 * transposition is performed.  (We might do this to save the 
 * the time used for byte transposition.) 
 * 
 * This abstraction layer allows us to define the disk to be in
 * any byte order with a single compile-time switch (in htod.c).
 *
 * NOTE: THIS FILE DOES NOT CURRENTLY SUPPORT A BIG-ENDIAN
 *       HOST AND A LITTLE-ENDIAN DISK.
 */

#include <portability/toku_config.h>

#if defined(HAVE_ENDIAN_H)
# include <endian.h>
#elif defined(HAVE_MACHINE_ENDIAN_H)
# include <machine/endian.h>
# define __BYTE_ORDER __DARWIN_BYTE_ORDER
# define __LITTLE_ENDIAN __DARWIN_LITTLE_ENDIAN
# define __BIG_ENDIAN __DARWIN_BIG_ENDIAN
#endif
#if !defined(__BYTE_ORDER) || \
    !defined(__LITTLE_ENDIAN) || \
    !defined(__BIG_ENDIAN)
#error Standard endianness things not all defined
#endif


static const int64_t toku_byte_order_host = 0x0102030405060708LL;

#define NETWORK_BYTE_ORDER  (__BIG_ENDIAN)
#define INTEL_BYTE_ORDER    (__LITTLE_ENDIAN)
#define HOST_BYTE_ORDER     (__BYTE_ORDER)

//DISK_BYTE_ORDER is the byte ordering for integers written to disk.
//If DISK_BYTE_ORDER is the same as HOST_BYTE_ORDER no conversions are necessary.
//Otherwise some structures require conversion to HOST_BYTE_ORDER on loading from disk (HOST_BYTE_ORDER in memory), and
//others require conversion to HOST_BYTE_ORDER on every access/mutate (DISK_BYTE_ORDER in memory).
#define DISK_BYTE_ORDER     (INTEL_BYTE_ORDER)

#if HOST_BYTE_ORDER!=INTEL_BYTE_ORDER
//Even though the functions are noops if DISK==HOST, we do not have the logic to test whether the file was moved from another BYTE_ORDER machine.
#error Only intel byte order supported so far.
#endif

#if DISK_BYTE_ORDER == HOST_BYTE_ORDER
static inline uint64_t
toku_dtoh64(uint64_t i) {
    return i;
}

static inline uint64_t
toku_htod64(uint64_t i) {
    return i;
}

static inline uint32_t
toku_dtoh32(uint32_t i) {
    return i;
}

static inline uint32_t
toku_htod32(uint32_t i) {
    return i;
}
#else
#error Not supported
#endif
