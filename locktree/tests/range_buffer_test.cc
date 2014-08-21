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

#include "test.h"

#include <string.h>

#include <portability/memory.h>

#include <locktree/range_buffer.h>

namespace toku {

const size_t num_points = 60;

static const DBT *get_dbt_by_iteration(size_t i) {
    if (i == 0) {
        return toku_dbt_negative_infinity();
    } else if (i < (num_points - 1)) {
        return get_dbt(i); 
    } else {
        return toku_dbt_positive_infinity();
    }
}

static void test_points(void) {
    range_buffer buffer;
    buffer.create();

    for (size_t i = 0; i < num_points; i++) {
        const DBT *point = get_dbt_by_iteration(i);
        buffer.append(point, point);
    }

    size_t i = 0;
    range_buffer::iterator iter(&buffer);
    range_buffer::iterator::record rec;
    while (iter.current(&rec)) {
        const DBT *expected_point = get_dbt_by_iteration(i);
        invariant(compare_dbts(nullptr, expected_point, rec.get_left_key()) == 0);
        invariant(compare_dbts(nullptr, expected_point, rec.get_right_key()) == 0);
        iter.next();
        i++;
    }
    invariant(i == num_points);

    buffer.destroy();
}

static void test_ranges(void) {
    range_buffer buffer;
    buffer.create();

    // we are going to store adjacent points as ranges,
    // so make sure there are an even number of points.
    invariant(num_points % 2 == 0);

    for (size_t i = 0; i < num_points; i += 2) {
        const DBT *left = get_dbt_by_iteration(i);
        const DBT *right = get_dbt_by_iteration(i + 1);
        buffer.append(left, right);
    }

    size_t i = 0;
    range_buffer::iterator iter(&buffer);
    range_buffer::iterator::record rec;
    while (iter.current(&rec)) {
        const DBT *expected_left = get_dbt_by_iteration(i);
        const DBT *expected_right = get_dbt_by_iteration(i + 1);
        invariant(compare_dbts(nullptr, expected_left, rec.get_left_key()) == 0);
        invariant(compare_dbts(nullptr, expected_right, rec.get_right_key()) == 0);
        iter.next();
        i += 2;
    }
    invariant(i == num_points);

    buffer.destroy();

}

static void test_mixed(void) {
    range_buffer buffer;
    buffer.create();
    buffer.destroy();

    // we are going to store adjacent points as ranges,
    // followed by a single point, so make sure the
    // number of points is a multiple of 3.
    invariant(num_points % 3 == 0);

    for (size_t i = 0; i < num_points; i += 3) {
        const DBT *left = get_dbt_by_iteration(i);
        const DBT *right = get_dbt_by_iteration(i + 1);
        const DBT *point = get_dbt_by_iteration(i + 2);
        buffer.append(left, right);
        buffer.append(point, point);
    }

    size_t i = 0;
    range_buffer::iterator iter(&buffer);
    range_buffer::iterator::record rec;
    while (iter.current(&rec)) {
        const DBT *expected_left = get_dbt_by_iteration(i);
        const DBT *expected_right = get_dbt_by_iteration(i + 1);
        invariant(compare_dbts(nullptr, expected_left, rec.get_left_key()) == 0);
        invariant(compare_dbts(nullptr, expected_right, rec.get_right_key()) == 0);
        iter.next();

        const DBT *expected_point = get_dbt_by_iteration(i + 2);
        bool had_point = iter.current(&rec);
        invariant(had_point);
        invariant(compare_dbts(nullptr, expected_point, rec.get_left_key()) == 0);
        invariant(compare_dbts(nullptr, expected_point, rec.get_right_key()) == 0);
        iter.next();
        i += 3;
    }
    invariant(i == num_points);

    buffer.destroy();
}

static void test_small_and_large_points(void) {
    range_buffer buffer;
    buffer.create();
    buffer.destroy();

    // Test a bug where a small append would cause
    // the range buffer to not grow properly for
    // a subsequent large append.
    const size_t small_size = 32;
    const size_t large_size = 16 * 1024;
    char *small_buf = (char *) toku_xmalloc(small_size);
    char *large_buf = (char *) toku_xmalloc(large_size);
    DBT small_dbt, large_dbt;
    memset(&small_dbt, 0, sizeof(DBT));
    memset(&large_dbt, 0, sizeof(DBT));
    small_dbt.data = small_buf;
    small_dbt.size = small_size;
    large_dbt.data = large_buf;
    large_dbt.size = large_size;

    // Append a small dbt, the buf should be able to fit it.
    buffer.append(&small_dbt, &small_dbt);
    invariant(buffer.total_memory_size() >= small_dbt.size);
    // Append a large dbt, the buf should be able to fit it.
    buffer.append(&large_dbt, &large_dbt);
    invariant(buffer.total_memory_size() >= (small_dbt.size + large_dbt.size));

    toku_free(small_buf);
    toku_free(large_buf);
    buffer.destroy();
}

} /* namespace toku */

int main(void) {
    toku::test_points();
    toku::test_ranges();
    toku::test_mixed();
    toku::test_small_and_large_points();
    return 0;
}
