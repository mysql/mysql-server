/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef TOKU_ATOMIC_H
#define TOKU_ATOMIC_H

#include "toku_config.h"
#include <toku_assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

__attribute__((const, always_inline))
static inline intptr_t which_cache_line(intptr_t addr) {
    static const size_t assumed_cache_line_size = 64;
    return addr / assumed_cache_line_size;
}
template <typename T> __attribute__((const, always_inline))
static inline bool crosses_boundary(T *addr, size_t width) {
    const intptr_t int_addr = reinterpret_cast<intptr_t>(addr);
    const intptr_t last_byte = int_addr + width - 1;
    return which_cache_line(int_addr) != which_cache_line(last_byte);
}

template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_fetch_and_add(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_fetch_and_add(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_add_and_fetch(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_add_and_fetch(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_fetch_and_sub(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_fetch_and_sub(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_sub_and_fetch(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_sub_and_fetch(addr, diff);
}
template <typename T, typename U, typename V> __attribute__((always_inline))
static inline T toku_sync_val_compare_and_swap(T *addr, U oldval, V newval) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_val_compare_and_swap(addr, oldval, newval);
}
template <typename T, typename U, typename V> __attribute__((always_inline))
static inline bool toku_sync_bool_compare_and_swap(T *addr, U oldval, V newval) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_bool_compare_and_swap(addr, oldval, newval);
}

// in case you include this but not toku_portability.h
#pragma GCC poison __sync_fetch_and_add
#pragma GCC poison __sync_fetch_and_sub
#pragma GCC poison __sync_fetch_and_or
#pragma GCC poison __sync_fetch_and_and
#pragma GCC poison __sync_fetch_and_xor
#pragma GCC poison __sync_fetch_and_nand
#pragma GCC poison __sync_add_and_fetch
#pragma GCC poison __sync_sub_and_fetch
#pragma GCC poison __sync_or_and_fetch
#pragma GCC poison __sync_and_and_fetch
#pragma GCC poison __sync_xor_and_fetch
#pragma GCC poison __sync_nand_and_fetch
#pragma GCC poison __sync_bool_compare_and_swap
#pragma GCC poison __sync_val_compare_and_swap
#pragma GCC poison __sync_synchronize
#pragma GCC poison __sync_lock_test_and_set
#pragma GCC poison __sync_release

#endif // TOKU_ATOMIC_H
