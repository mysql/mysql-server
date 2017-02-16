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

#include <ft/comparator.h>

namespace toku {

// A keyrange has a left and right key as endpoints.
//
// When a keyrange is created it owns no memory, but when it copies
// or extends another keyrange, it copies memory as necessary. This
// means it is cheap in the common case.

class keyrange {
public:

    // effect: constructor that borrows left and right key pointers.
    //         no memory is allocated or copied.
    void create(const DBT *left_key, const DBT *right_key);

    // effect: constructor that allocates and copies another keyrange's points.
    void create_copy(const keyrange &range);

    // effect: destroys the keyrange, freeing any allocated memory
    void destroy(void);

    // effect: extends the keyrange by choosing the leftmost and rightmost
    //         endpoints from this range and the given range.
    //         replaced keys in this range are freed, new keys are copied.
    void extend(const comparator &cmp, const keyrange &range);

    // returns: the amount of memory this keyrange takes. does not account
    //          for point optimizations or malloc overhead.
    uint64_t get_memory_size(void) const;

    // returns: pointer to the left key of this range
    const DBT *get_left_key(void) const;

    // returns: pointer to the right key of this range
    const DBT *get_right_key(void) const;

    // two ranges are either equal, lt, gt, or overlapping
    enum comparison {
        EQUALS,
        LESS_THAN,
        GREATER_THAN,
        OVERLAPS
    };

    // effect: compares this range to the given range
    // returns: LESS_THAN    if given range is strictly to the left
    //          GREATER_THAN if given range is strictly to the right
    //          EQUALS       if given range has the same left and right endpoints
    //          OVERLAPS     if at least one of the given range's endpoints falls
    //                       between this range's endpoints
    comparison compare(const comparator &cmp, const keyrange &range) const;

    // returns: true if the range and the given range are equal or overlapping
    bool overlaps(const comparator &cmp, const keyrange &range) const;

    // returns: a keyrange representing -inf, +inf
    static keyrange get_infinite_range(void);

private:
    // some keys should be copied, some keys should not be.
    //
    // to support both, we use two DBTs for copies and two pointers
    // for temporaries. the access rule is:
    // - if a pointer is non-null, then it reprsents the key.
    // - otherwise the pointer is null, and the key is in the copy.
    DBT m_left_key_copy;
    DBT m_right_key_copy;
    const DBT *m_left_key;
    const DBT *m_right_key;

    // if this range is a point range, then m_left_key == m_right_key
    // and the actual data is stored exactly once in m_left_key_copy.
    bool m_point_range;

    // effect: initializes a keyrange to be empty
    void init_empty(void);

    // effect: copies the given key once into the left key copy
    //         and sets the right key copy to share the left.
    // rationale: optimization for point ranges to only do one malloc
    void set_both_keys(const DBT *key);

    // effect: destroys the current left key. sets and copies the new one.
    void replace_left_key(const DBT *key);

    // effect: destroys the current right key. sets and copies the new one.
    void replace_right_key(const DBT *key);
};

} /* namespace toku */
