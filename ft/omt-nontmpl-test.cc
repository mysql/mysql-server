/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_time.h>
#include <memory.h>
#include "fttypes.h"
#include "log-internal.h"
#include "omt.h"

/*static int intcmp(OMTVALUE onev, void *twov) {
    int64_t one = (int64_t) onev;
    int64_t two = (int64_t) twov;
    return two - one;
    }*/

static int find_by_xid(OMTVALUE txnv, void *findidv) {
    TOKUTXN txn = (TOKUTXN) txnv;
    TXNID findid = (TXNID) findidv;
    if (txn->txnid64 > findid) {
        return 1;
    }
    if (txn->txnid64 < findid) {
        return -1;
    }
    return 0;
}

static int txn_iter(OMTVALUE UU(txnv), uint32_t UU(idx), void *UU(v)) {
    return 0;
}

const int NTXNS = 1<<23;

void runit(void)
{
    {
        srandom(0);
        double inserttime = 0.0, querytime = 0.0, itertime = 0.0;
        size_t overhead = 0;

        for (int trial = 0; trial < 100; ++trial) {
            OMT txn_omt;
            toku_omt_create(&txn_omt);

            TOKUTXN XMALLOC_N(NTXNS, txns);
            for (int i = 0; i < NTXNS; ++i) {
                TOKUTXN txn = &txns[i];
                txn->txnid64 = ((random() << 32) | random());
            }
            tokutime_t t0 = get_tokutime();
            for (int i = 0; i < NTXNS; ++i) {
                TOKUTXN txn = &txns[i];
                int r = toku_omt_insert(txn_omt, (OMTVALUE) txn, find_by_xid, (void *) txn->txnid64, NULL);
                invariant_zero(r);
                //invariant(r == 0 || r == DB_KEYEXIST);
            }
            tokutime_t t1 = get_tokutime();
            for (int i = 0; i < NTXNS; ++i) {
                TOKUTXN txn;
                int r = toku_omt_find_zero(txn_omt, find_by_xid, (void *) txns[i].txnid64, (OMTVALUE *) &txn, NULL);
                invariant_zero(r);
                invariant(txn == &txns[i]);
            }
            tokutime_t t2 = get_tokutime();
            toku_omt_iterate(txn_omt, txn_iter, NULL);
            tokutime_t t3 = get_tokutime();

            inserttime += tokutime_to_seconds(t1-t0);
            querytime += tokutime_to_seconds(t2-t1);
            itertime += tokutime_to_seconds(t3-t2);
            if (overhead == 0) {
                overhead = toku_omt_memory_size(txn_omt);
            }

            toku_omt_destroy(&txn_omt);
            invariant_null(txn_omt);
            toku_free(txns);
        }

        printf("inserts: %.03lf\nqueries: %.03lf\niterate: %.03lf\noverhead: %lu\n",
               inserttime, querytime, itertime, overhead);
    }
    int64_t maxrss;
    toku_os_get_max_rss(&maxrss);
    printf("memused: %" PRId64 "\n", maxrss);

    /* {
        srand(0);
        OMT int_omt;
        toku_omt_create(&int_omt);

        int64_t *XMALLOC_N(NTXNS, ints);
        for (int i = 0; i < NTXNS; ++i) {
            ints[i] = rand() >> 8;
        }
        tokutime_t t0 = get_tokutime();
        for (int i = 0; i < NTXNS; ++i) {
            //int r =
            toku_omt_insert(int_omt, (OMTVALUE) ints[i], intcmp, (void *) ints[i], NULL);
            //invariant(r == 0 || r == DB_KEYEXIST);
        }
        tokutime_t t1 = get_tokutime();
        OMT clone;
        toku_omt_clone_noptr(&clone, int_omt);
        tokutime_t t2 = get_tokutime();
        for (int i = 0; i < NTXNS; ++i) {
            //int r =
            toku_omt_find_zero(clone, intcmp, (void *) ints[i], NULL, NULL);
            //invariant_zero(r);
        }
        tokutime_t t3 = get_tokutime();

        printf("omtsize: %" PRIu32 "\ninserts: %.03lf\nqueries: %.03lf\n",
               toku_omt_size(clone), tokutime_to_seconds(t1-t0), tokutime_to_seconds(t3-t2));

        toku_omt_destroy(&int_omt);
        invariant_null(int_omt);
        toku_omt_destroy(&clone);
        invariant_null(clone);
        toku_free(ints);
        }*/
}

int main(void)
{
    runit();
    return 0;
}
