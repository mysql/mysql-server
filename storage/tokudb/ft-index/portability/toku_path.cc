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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_path.h"
#include <toku_assert.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

const char *toku_test_filename(const char *default_filename) {
    const char *filename = getenv("TOKU_TEST_FILENAME");
    if (filename == nullptr) {
        filename = basename((char *) default_filename);
        assert(filename != nullptr);
    }
    return filename;
}

// Guarantees NUL termination (unless siz == 0)
// siz is full size of dst (including NUL terminator)
// Appends src to end of dst, (truncating if necessary) to use no more than siz bytes (including NUL terminator)
// Returns strnlen(dst, siz) (size (excluding NUL) of string we tried to create)
size_t toku_strlcat(char *dst, const char *src, size_t siz)
{
    if (siz == 0) {
        return 0;
    }
    dst[siz-1] = '\0'; //Guarantee NUL termination.

    const size_t old_dst_len = strnlen(dst, siz - 1);
    paranoid_invariant(old_dst_len <= siz - 1);
    if (old_dst_len == siz - 1) {
        // No room for anything more.
        return old_dst_len;
    }
    char *d = &dst[old_dst_len];  //Points to null ptr at end of old string
    const size_t remaining_space = siz-old_dst_len-1;
    const size_t allowed_src_len = strnlen(src, remaining_space);  // Limit to remaining space (leave space for NUL)
    paranoid_invariant(allowed_src_len <= remaining_space);
    paranoid_invariant(old_dst_len + allowed_src_len < siz);
    memcpy(d, src, allowed_src_len);
    d[allowed_src_len] = '\0';  // NUL terminate (may be redundant with previous NUL termination)

    return old_dst_len + allowed_src_len;
}

// Guarantees NUL termination (unless siz == 0)
// siz is full size of dst (including NUL terminator)
// Appends src to end of dst, (truncating if necessary) to use no more than siz bytes (including NUL terminator)
// Returns strnlen(dst, siz) (size (excluding NUL) of string we tried to create)
//
// Implementation note: implemented for simplicity as oppsed to performance
size_t toku_strlcpy(char *dst, const char *src, size_t siz)
{
    if (siz == 0) {
        return 0;
    }
    *dst = '\0';
    return toku_strlcat(dst, src, siz);
}

char *toku_path_join(char *dest, int n, const char *base, ...) {
    static const char PATHSEP = '/';
    size_t written;
    written = toku_strlcpy(dest, base, TOKU_PATH_MAX);
    paranoid_invariant(written < TOKU_PATH_MAX);
    paranoid_invariant(dest[written] == '\0');

    va_list ap;
    va_start(ap, base);
    for (int i = 1; written < TOKU_PATH_MAX && i < n; ++i) {
        if (dest[written - 1] != PATHSEP) {
            if (written+2 >= TOKU_PATH_MAX) {
                // No room.
                break;
            }
            dest[written++] = PATHSEP;
            dest[written] = '\0';
        }
        const char *next = va_arg(ap, const char *);
        written = toku_strlcat(dest, next, TOKU_PATH_MAX);
        paranoid_invariant(written < TOKU_PATH_MAX);
        paranoid_invariant(dest[written] == '\0');
    }
    va_end(ap);

    // Zero out rest of buffer for security
    memset(&dest[written], 0, TOKU_PATH_MAX - written);
    return dest;
}
