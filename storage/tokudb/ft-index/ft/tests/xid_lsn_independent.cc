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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include "toku_os.h"
#include "checkpoint.h"

#define ENVDIR TOKU_TEST_FILENAME
#include "test-ft-txns.h"

static void do_txn(TOKULOGGER logger, bool readonly) {
    int r;
    TOKUTXN txn;
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_NONE, false);
    CKERR(r);

    if (!readonly) {
        toku_maybe_log_begin_txn_for_write_operation(txn);
    }
    r = toku_txn_commit_txn(txn, false, NULL, NULL);
    CKERR(r);

    toku_txn_close_txn(txn);
}

static void test_xid_lsn_independent(int N) {
    TOKULOGGER logger;
    CACHETABLE ct;
    int r;

    test_setup(TOKU_TEST_FILENAME, &logger, &ct);

    FT_HANDLE brt;

    TOKUTXN txn;
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_NONE, false);
    CKERR(r);

    r = toku_open_ft_handle("ftfile", 1, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);
    CKERR(r);

    r = toku_txn_commit_txn(txn, false, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(txn);

    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_NONE, false);
    CKERR(r);
    TXNID xid_first = txn->txnid.parent_id64;
    unsigned int rands[N];
    for (int i=0; i<N; i++) {
        char key[100],val[300];
        DBT k, v;
        rands[i] = random();
        snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
        memset(val, 'v', sizeof(val));
        val[sizeof(val)-1]=0;
        toku_ft_insert(brt, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), txn);
    }
    {
        TOKUTXN txn2;
        r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn2, logger, TXN_SNAPSHOT_NONE, false);
    CKERR(r);
        // Verify the txnid has gone up only by one (even though many log entries were done)
        invariant(txn2->txnid.parent_id64 == xid_first + 1);
        r = toku_txn_commit_txn(txn2, false, NULL, NULL);
    CKERR(r);
        toku_txn_close_txn(txn2);
    }
    r = toku_txn_commit_txn(txn, false, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(txn);
    {
        //TODO(yoni) #5067 will break this portion of the test. (End ids are also assigned, so it would increase by 4 instead of 2.)
        // Verify the txnid has gone up only by two (even though many log entries were done)
        TOKUTXN txn3;
        r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn3, logger, TXN_SNAPSHOT_NONE, false);
    CKERR(r);
        invariant(txn3->txnid.parent_id64 == xid_first + 2);
        r = toku_txn_commit_txn(txn3, false, NULL, NULL);
    CKERR(r);
        toku_txn_close_txn(txn3);
    }
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    CKERR(r);
    r = toku_close_ft_handle_nolsn(brt, NULL);
    CKERR(r);

    clean_shutdown(&logger, &ct);
}

static TXNID
logger_get_last_xid(TOKULOGGER logger) {
    TXN_MANAGER mgr = toku_logger_get_txn_manager(logger);
    return toku_txn_manager_get_last_xid(mgr);
}

static void test_xid_lsn_independent_crash_recovery(int N) {
    TOKULOGGER logger;
    CACHETABLE ct;
    int r;

    test_setup(TOKU_TEST_FILENAME, &logger, &ct);

    for (int i=0; i < N - 1; i++) {
        do_txn(logger, true);
    }
    do_txn(logger, false);

    TXNID last_xid_before = logger_get_last_xid(logger);

    toku_logger_close_rollback(logger);

    toku_cachetable_close(&ct);
    // "Crash"
    r = toku_logger_close(&logger);
    CKERR(r);
    ct = NULL;
    logger = NULL;

    // "Recover"
    test_setup_and_recover(TOKU_TEST_FILENAME, &logger, &ct);

    TXNID last_xid_after = logger_get_last_xid(logger);

    invariant(last_xid_after == last_xid_before);

    shutdown_after_recovery(&logger, &ct);
}

static void test_xid_lsn_independent_shutdown_recovery(int N) {
    TOKULOGGER logger;
    CACHETABLE ct;
    test_setup(TOKU_TEST_FILENAME, &logger, &ct);

    for (int i=0; i < N - 1; i++) {
        do_txn(logger, true);
    }
    do_txn(logger, false);

    TXNID last_xid_before = logger_get_last_xid(logger);

    clean_shutdown(&logger, &ct);

    // Did a clean shutdown.

    // "Recover"
    test_setup_and_recover(TOKU_TEST_FILENAME, &logger, &ct);

    TXNID last_xid_after = logger_get_last_xid(logger);

    invariant(last_xid_after == last_xid_before);

    shutdown_after_recovery(&logger, &ct);
}

static void test_xid_lsn_independent_parents(int N) {
    TOKULOGGER logger;
    CACHETABLE ct;
    int r;

    // Lets txns[-1] be NULL
    TOKUTXN txns_hack[N+1];
    TOKUTXN *txns=&txns_hack[1];

    int num_non_cascade = N;
    do {
        test_setup(TOKU_TEST_FILENAME, &logger, &ct);
        ZERO_ARRAY(txns_hack);

        for (int i = 0; i < N; i++) {
            r = toku_txn_begin_txn((DB_TXN*)NULL, txns[i-1], &txns[i], logger, TXN_SNAPSHOT_NONE, false);
            CKERR(r);

            if (i < num_non_cascade) {
                toku_maybe_log_begin_txn_for_write_operation(txns[i]);
                invariant(txns[i]->begin_was_logged);
            }
            else {
                invariant(!txns[i]->begin_was_logged);
            }
        }
        for (int i = 0; i < N; i++) {
            if (i < num_non_cascade) {
                toku_maybe_log_begin_txn_for_write_operation(txns[i]);
                invariant(txns[i]->begin_was_logged);
            }
            else {
                invariant(!txns[i]->begin_was_logged);
            }
        }
        toku_maybe_log_begin_txn_for_write_operation(txns[N-1]);
        for (int i = 0; i < N; i++) {
            invariant(txns[i]->begin_was_logged);
        }
        for (int i = N-1; i >= 0; i--) {
            r = toku_txn_commit_txn(txns[i], false, NULL, NULL);
            CKERR(r);

            toku_txn_close_txn(txns[i]);
        }
        clean_shutdown(&logger, &ct);

        num_non_cascade /= 2;
    } while (num_non_cascade > 0);
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    for (int i=1; i<=128; i *= 2) {
        test_xid_lsn_independent(i);
        test_xid_lsn_independent_crash_recovery(i);
        test_xid_lsn_independent_shutdown_recovery(i);
        test_xid_lsn_independent_parents(i);
    }
    return 0;
}
