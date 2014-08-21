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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <zlib.h>
#include <db.h>

// The following provides an abstraction of quicklz and zlib.
// We offer three compression methods: ZLIB, QUICKLZ, and LZMA, as well as a "no compression" option.  These options are declared in make_tdb.c.
// The resulting byte string includes enough information for us to decompress it.  That is, we can tell whether it's z-compressed or qz-compressed or xz-compressed.

size_t toku_compress_bound (enum toku_compression_method a, size_t size);
// Effect:  Return the number of bytes needed to compress a buffer of size SIZE using compression method A.
//  Typically, the result is a little bit larger than SIZE, since some data cannot be compressed.
// Usage note: It may help to know roughly how much space is involved.
//    zlib's bound is something like (size + (size>>12) + (size>>14) + (size>>25) + 13.
//    quicklz's bound is something like size+400.

void toku_compress (enum toku_compression_method a,
		    // the following types and naming conventions come from zlib.h
		    Bytef       *dest,   uLongf *destLen,
		    const Bytef *source, uLong   sourceLen);
// Effect: Using compression method A, compress SOURCE into DEST.   The number of bytes to compress is passed in SOURCELEN.
//  On input: *destLen is the size of the buffer.
//  On output: *destLen is the size of the actual compressed data.
// Usage note: sourceLen may be be zero (unlike for quicklz, which requires sourceLen>0).
// Requires: The buffer must be big enough to hold the compressed data.  (That is *destLen >= compressBound(a, sourceLen))
// Requires: sourceLen < 2^32.
// Usage note: Although we *try* to assert if the DESTLEN isn't big enough, it's possible that it's too late by then (in the case of quicklz which offers
//   no way to avoid a buffer overrun.)  So we require that that DESTLEN is big enough.
// Rationale:  zlib's argument order is DEST then SOURCE with the size of the buffer passed in *destLen, and the size of the result returned in *destLen.
//             quicklz's argument order is SOURCE then DEST with the size returned (and it has no way to verify that an overright didn't happen).
//     We use zlib's calling conventions partly because it is safer, and partly because it is more established.
//     We also use zlib's ugly camel case convention for destLen and sourceLen.
//     Unlike zlib, we return no error codes.  Instead, we require that the data be OK and the size of the buffers is OK, and assert if there's a problem.

void toku_decompress (Bytef       *dest,   uLongf destLen,
		      const Bytef *source, uLongf sourceLen);
// Effect: Decompress source (length sourceLen) into dest (length destLen)
//  This function can decompress data compressed with either zlib or quicklz compression methods (calling toku_compress(), which puts an appropriate header on so we know which it is.)
// Requires: destLen is equal to the actual decompressed size of the data.
// Requires: The source must have been properly compressed.
