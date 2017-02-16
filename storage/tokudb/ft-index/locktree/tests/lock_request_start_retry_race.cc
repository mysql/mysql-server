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

#ident "Copyright (c) 2014 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <iostream>
#include "test.h"
#include "locktree.h"
#include "lock_request.h"

// Test FT-633, the data race on the lock request between ::start and ::retry
// This test is non-deterministic.  It uses sleeps at 2 critical places to
// expose the data race on the lock requests state.

namespace toku {

struct locker_arg {
    locktree *_lt;
    TXNID _id;
    const DBT *_key;

    locker_arg(locktree *lt, TXNID id, const DBT *key) : _lt(lt), _id(id), _key(key) {
    }
};

static void locker_callback(void) {
    usleep(10000);
}

static void run_locker(locktree *lt, TXNID txnid, const DBT *key) {
    int i;
    for (i = 0; i < 1000; i++) {

        lock_request request;
        request.create();

        request.set(lt, txnid, key, key, lock_request::type::WRITE, false);

        // set the test callbacks
        request.set_start_test_callback(locker_callback);
        request.set_retry_test_callback(locker_callback);

        // try to acquire the lock
        int r = request.start();
        if (r == DB_LOCK_NOTGRANTED) {
            // wait for the lock to be granted
            r = request.wait(10 * 1000);
        }

        if (r == 0) {
            // release the lock
            range_buffer buffer;
            buffer.create();
            buffer.append(key, key);
            lt->release_locks(txnid, &buffer);
            buffer.destroy();

            // retry pending lock requests
            lock_request::retry_all_lock_requests(lt);
        }

        request.destroy();
        memset(&request, 0xab, sizeof request);

        toku_pthread_yield();
        if ((i % 10) == 0)
            std::cout << toku_pthread_self() << " " << i << std::endl;
    }
}

static void *locker(void *v_arg) {
    locker_arg *arg = static_cast<locker_arg *>(v_arg);
    run_locker(arg->_lt, arg->_id, arg->_key);
    return arg;
}

} /* namespace toku */

int main(void) {
    int r;

    toku::locktree lt;
    DICTIONARY_ID dict_id = { 1 };
    lt.create(nullptr, dict_id, toku::dbt_comparator);

    const DBT *one = toku::get_dbt(1);

    const int n_workers = 2;
    toku_pthread_t ids[n_workers];
    for (int i = 0; i < n_workers; i++) {
        toku::locker_arg *arg = new toku::locker_arg(&lt, i, one);
        r = toku_pthread_create(&ids[i], nullptr, toku::locker, arg);
        assert_zero(r);
    }
    for (int i = 0; i < n_workers; i++) {
        void *ret;
        r = toku_pthread_join(ids[i], &ret);
        assert_zero(r);
        toku::locker_arg *arg = static_cast<toku::locker_arg *>(ret);
        delete arg;
    }

    lt.release_reference();
    lt.destroy();
    return 0;
}

