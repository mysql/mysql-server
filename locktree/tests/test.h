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

#include <limits.h>

#include "ft/comparator.h"
#include "util/dbt.h"

namespace toku {

    __attribute__((__unused__))
    static DBT min_dbt(void) {
        static int64_t min = INT_MIN;
        DBT dbt;
        toku_fill_dbt(&dbt, &min, sizeof(int64_t));
        dbt.flags = DB_DBT_USERMEM;
        return dbt;
    }

    __attribute__((__unused__))
    static DBT max_dbt(void) {
        static int64_t max = INT_MAX;
        DBT dbt;
        toku_fill_dbt(&dbt, &max, sizeof(int64_t));
        dbt.flags = DB_DBT_USERMEM;
        return dbt;
    }

    __attribute__((__unused__))
    static const DBT *get_dbt(int64_t key) {
        static const int NUM_DBTS = 1000;
        static bool initialized;
        static int64_t static_ints[NUM_DBTS];
        static DBT static_dbts[NUM_DBTS];
        invariant(key < NUM_DBTS);
        if (!initialized) {
            for (int i = 0; i < NUM_DBTS; i++) {
                static_ints[i] = i;
                toku_fill_dbt(&static_dbts[i],
                        &static_ints[i],
                        sizeof(int64_t));
                static_dbts[i].flags = DB_DBT_USERMEM;
            }
            initialized = true;
        }

        invariant(key < NUM_DBTS);
        return &static_dbts[key];
    }

    __attribute__((__unused__))
    static int compare_dbts(DB *db, const DBT *key1, const DBT *key2) {
        (void) db;

        // this emulates what a "infinity-aware" comparator object does
        if (toku_dbt_is_infinite(key1) || toku_dbt_is_infinite(key2)) {
            return toku_dbt_infinite_compare(key1, key2);
        } else {
            invariant(key1->size == sizeof(int64_t));
            invariant(key2->size == sizeof(int64_t));
            int64_t a = *(int64_t*) key1->data;
            int64_t b = *(int64_t*) key2->data;
            if (a < b) {
                return -1;
            } else if (a == b) {
                return 0;
            } else {
                return 1;
            }
        }
    }

    __attribute__((__unused__)) comparator dbt_comparator;

    __attribute__((__constructor__))
    static void construct_dbt_comparator(void) {
        dbt_comparator.create(compare_dbts, nullptr); 
    }

    __attribute__((__destructor__))
    static void destruct_dbt_comparator(void) {
        dbt_comparator.destroy();
    }

} /* namespace toku */
