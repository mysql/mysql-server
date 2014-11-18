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
// test tokudb cardinality in status dictionary
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>
typedef unsigned long long ulonglong;
#include <tokudb_status.h>
#include <tokudb_buffer.h>

#include "fake_mysql.h"

#if __APPLE__
typedef unsigned long ulong;
#endif
#include <tokudb_card.h>

static void test_no_keys() {
    TABLE_SHARE s = { 0, 0, 0, NULL };
    TABLE t = { &s, NULL };
    assert(tokudb::compute_total_key_parts(&s) == 0);
    tokudb::set_card_in_key_info(&t, 0, NULL);
}

static void test_simple_pk() {
    const uint keys = 1;
    const uint key_parts = 1;
    uint64_t pk_rec_per_key[keys] = { 0 };
    KEY_INFO pk = { 0, key_parts, pk_rec_per_key, (char *) "PRIMARY" };
    TABLE_SHARE s = { 0, keys, key_parts, &pk };
    TABLE t = { &s, &pk };
    assert(tokudb::compute_total_key_parts(&s) == key_parts);
    uint64_t computed_rec_per_key[keys] = { 2 };
    tokudb::set_card_in_key_info(&t, keys, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 1);
}

static void test_pk_2() {
    const uint keys = 1;
    const uint key_parts = 2;
    uint64_t pk_rec_per_key[keys * key_parts] = { 0 };
    KEY_INFO pk = { 0, key_parts, pk_rec_per_key, (char *) "PRIMARY" };
    TABLE_SHARE s = { 0, keys, key_parts, &pk };
    TABLE t = { &s, &pk };
    assert(tokudb::compute_total_key_parts(&s) == key_parts);
    uint64_t computed_rec_per_key[keys * key_parts] = { 2, 3 };
    tokudb::set_card_in_key_info(&t, keys * key_parts, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 2);
    assert(t.key_info[0].rec_per_key[1] == 1);
}

static void test_simple_sk() {
    const uint keys = 1;
    const uint key_parts = 1;
    uint64_t sk_rec_per_key[keys] = { 0 };
    KEY_INFO sk = { 0, keys, sk_rec_per_key, (char *) "KEY" };
    TABLE_SHARE s = { MAX_KEY, keys, key_parts, &sk };
    TABLE t = { &s, &sk };
    assert(tokudb::compute_total_key_parts(&s) == 1);
    uint64_t computed_rec_per_key[keys] = { 2 };
    tokudb::set_card_in_key_info(&t, keys, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 2);
}

static void test_simple_unique_sk() {
    const uint keys = 1;
    uint64_t sk_rec_per_key[keys] = { 0 };
    KEY_INFO sk = { HA_NOSAME, keys, sk_rec_per_key, (char *) "KEY" };
    TABLE_SHARE s = { MAX_KEY, keys, keys, &sk };
    TABLE t = { &s, &sk };
    assert(tokudb::compute_total_key_parts(&s) == 1);
    uint64_t computed_rec_per_key[keys] = { 2 };
    tokudb::set_card_in_key_info(&t, keys, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 1);
}

static void test_simple_pk_sk() {
    const uint keys = 2;
    uint64_t rec_per_key[keys] = { 0 };
    KEY_INFO key_info[keys] = {
        { 0, 1, &rec_per_key[0], (char *) "PRIMARY" },
        { 0, 1, &rec_per_key[1], (char *) "KEY" },
    };
    TABLE_SHARE s = { 0, keys, keys, key_info };
    TABLE t = { &s, key_info };
    assert(tokudb::compute_total_key_parts(&s) == 2);
    uint64_t computed_rec_per_key[keys] = { 100, 200 };
    tokudb::set_card_in_key_info(&t, keys, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 1);
    assert(t.key_info[1].rec_per_key[0] == 200);
}

static void test_simple_sk_pk() {
    const uint keys = 2;
    uint64_t rec_per_key[keys] = { 0 };
    KEY_INFO key_info[keys] = {
        { 0, 1, &rec_per_key[0], (char *) "KEY" },
        { 0, 1, &rec_per_key[1], (char *) "PRIMARY" },
    };
    TABLE_SHARE s = { 1, keys, keys, key_info };
    TABLE t = { &s, key_info };
    assert(tokudb::compute_total_key_parts(&s) == 2);
    uint64_t computed_rec_per_key[keys] = { 100, 200 };
    tokudb::set_card_in_key_info(&t, keys, computed_rec_per_key);
    assert(t.key_info[0].rec_per_key[0] == 100);
    assert(t.key_info[1].rec_per_key[0] == 1);
}

int main() {
    test_no_keys();
    test_simple_pk();
    test_pk_2();
    test_simple_sk();
    test_simple_unique_sk();
    test_simple_pk_sk();
    test_simple_sk_pk();
    return 0;
}
