/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <type_traits>
#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_time.h>
#include <memory.h>
#include "fttypes.h"
#include "log-internal.h"
#include "omt-tmpl.h"

namespace toku {

namespace test {

    struct four_xids {
        TXNID one, two, three, four;
    };

    inline int find_xid_one(const struct four_xids &xids, const TXNID &find);
    inline int find_xid_one(const struct four_xids &xids, const TXNID &find) {
        if (xids.one > find) {
            return 1;
        }
        if (xids.one < find) {
            return -1;
        }
        return 0;
    }

    inline int find_xid_two(const struct four_xids &xids, const TXNID &find);
    inline int find_xid_two(const struct four_xids &xids, const TXNID &find) {
        if (xids.two > find) {
            return 1;
        }
        if (xids.two < find) {
            return -1;
        }
        return 0;
    }

    inline int fx_iter(const struct four_xids &UU(xids), const uint32_t UU(idx), void *const UU(unused));
    inline int fx_iter(const struct four_xids &UU(xids), const uint32_t UU(idx), void *const UU(unused)) {
        return 0;
    }

    typedef omt<four_xids, four_xids *> fx_omt_t;
    ENSURE_POD(fx_omt_t);
    static_assert(24 == sizeof(fx_omt_t), "fx_omt_t is bigger than 24 bytes");

    inline int find_by_xid(const TOKUTXN &txn, const TXNID &findid);
    inline int find_by_xid(const TOKUTXN &txn, const TXNID &findid) {
        if (txn->txnid64 > findid) {
            return 1;
        }
        if (txn->txnid64 < findid) {
            return -1;
        }
        return 0;
    }

    inline int txn_iter(const TOKUTXN &UU(txn), const uint32_t UU(idx), void *const UU(unused));
    inline int txn_iter(const TOKUTXN &UU(txn), const uint32_t UU(idx), void *const UU(unused)) {
        return 0;
    }

    typedef omt<TOKUTXN> txn_omt_t;
    ENSURE_POD(txn_omt_t);
    static_assert(24 == sizeof(txn_omt_t), "txn_omt_t is bigger than 24 bytes");

    const int NTXNS = 1<<13;

    static void perftest(void)
    {
        if (0) {
            srandom(0);
            double inserttime = 0.0, querytime = 0.0, itertime = 0.0;
            size_t overhead = 0;

            for (int trial = 0; trial < 100; ++trial) {
                fx_omt_t *XMALLOC(fx_omt);
                fx_omt->create();

                struct four_xids *XMALLOC_N(NTXNS, txns);
                for (int i = 0; i < NTXNS; ++i) {
                    txns[i].one = ((random() << 32) | random());
                    txns[i].two = txns[i].one;
                }
                tokutime_t t0 = get_tokutime();
                for (int i = 0; i < NTXNS; ++i) {
                    int r = fx_omt->insert<TXNID, find_xid_one>(txns[i], txns[i].one, nullptr);
                    invariant_zero(r);
                    //invariant(r == 0 || r == DB_KEYEXIST);
                }
                tokutime_t t1 = get_tokutime();
                for (int i = 0; i < NTXNS; ++i) {
                    struct four_xids *v;
                    int r = fx_omt->find_zero<TXNID, find_xid_one>(txns[i].one, &v, nullptr);
                    invariant_zero(r);
                    invariant(v->one == txns[i].one);
                    invariant(v != &txns[i]);
                }
                tokutime_t t2 = get_tokutime();
                fx_omt->iterate<void, fx_iter>(nullptr);
                tokutime_t t3 = get_tokutime();
                for (int i = 0; i < NTXNS; ++i) {
                    struct four_xids *v;
                    int r = fx_omt->find_zero<TXNID, find_xid_two>(txns[i].two, &v, nullptr);
                    invariant_zero(r);
                    invariant(v->two == txns[i].two);
                    invariant(v != &txns[i]);
                }

                inserttime += tokutime_to_seconds(t1-t0);
                querytime += tokutime_to_seconds(t2-t1);
                itertime += tokutime_to_seconds(t3-t2);
                if (overhead == 0) {
                    overhead = fx_omt->memory_size();
                }

                fx_omt->destroy();
                toku_free(fx_omt);
                toku_free(txns);
            }

            printf("inserts: %.03lf\nqueries: %.03lf\niterate: %.03lf\noverhead: %zu\n",
                   inserttime, querytime, itertime, overhead);
        }
        {
            srandom(0);
            double inserttime = 0.0, querytime = 0.0, itertime = 0.0;
            size_t overhead = 0;

            for (int trial = 0; trial < 100; ++trial) {
                txn_omt_t *XMALLOC(txn_omt);
                txn_omt->create();

                TOKUTXN XMALLOC_N(NTXNS, txns);
                for (int i = 0; i < NTXNS; ++i) {
                    TOKUTXN txn = &txns[i];
                    // eww, sorry:
                    *(const_cast<TXNID *>(&txn->txnid64)) = ((random() << 32) | random());
                }
                tokutime_t t0 = get_tokutime();
                for (int i = 0; i < NTXNS; ++i) {
                    TOKUTXN txn = &txns[i];
                    int r = txn_omt->insert<TXNID, find_by_xid>(txn, txn->txnid64, nullptr);
                    invariant_zero(r);
                    //invariant(r == 0 || r == DB_KEYEXIST);
                }
                tokutime_t t1 = get_tokutime();
                for (int i = 0; i < NTXNS; ++i) {
                    TOKUTXN txn;
                    int r = txn_omt->find_zero<TXNID, find_by_xid>(txns[i].txnid64, &txn, nullptr);
                    invariant_zero(r);
                    invariant(txn == &txns[i]);
                }
                tokutime_t t2 = get_tokutime();
                txn_omt->iterate<void, txn_iter>(nullptr);
                tokutime_t t3 = get_tokutime();

                inserttime += tokutime_to_seconds(t1-t0);
                querytime += tokutime_to_seconds(t2-t1);
                itertime += tokutime_to_seconds(t3-t2);
                if (overhead == 0) {
                    overhead = txn_omt->memory_size();
                }

                txn_omt->destroy();
                toku_free(txn_omt);
                toku_free(txns);
            }

            printf("inserts: %.03lf\nqueries: %.03lf\niterate: %.03lf\noverhead: %zu\n",
                   inserttime, querytime, itertime, overhead);
        }
    }

    inline int intcmp(const int &a, const int &b);
    inline int intcmp(const int &a, const int &b) {
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return +1;
        }
        return 0;
    }

    typedef omt<int> int_omt_t;

    static int intiter_magic = 0xdeadbeef;
    inline int intiter(const int &value __attribute__((__unused__)), const uint32_t idx __attribute__((__unused__)), int *const extra);
    inline int intiter(const int &value __attribute__((__unused__)), const uint32_t idx __attribute__((__unused__)), int *const extra) {
        invariant(*extra == intiter_magic);
        return 0;
    }

    struct intiter2extra {
        int count;
        int last;
    };
    inline int intiter2(const int &value, const uint32_t idx __attribute__((__unused__)), struct intiter2extra *const extra);
    inline int intiter2(const int &value, const uint32_t idx __attribute__((__unused__)), struct intiter2extra *const extra) {
        extra->count++;
        invariant(extra->last < value);
        extra->last = value;
        return 0;
    }

    static void unittest(void) {
        int_omt_t o;
        int r;
        o.create();
        invariant(o.size() == 0);

        r = o.insert<int, intcmp>(1, 1, nullptr);
        invariant_zero(r);
        r = o.insert<int, intcmp>(3, 3, nullptr);
        invariant_zero(r);

        invariant(o.size() == 2);

        r = o.insert<int, intcmp>(2, 2, nullptr);
        invariant_zero(r);

        invariant(o.size() == 3);

        int x;
        r = o.fetch(1, &x);
        invariant_zero(r);

        invariant(x == 2);

        r = o.iterate<int, intiter>(&intiter_magic);
        invariant_zero(r);

        struct intiter2extra e = {0, 0};
        r = o.iterate_on_range<struct intiter2extra, intiter2>(0, 2, &e);
        invariant_zero(r);
        invariant(e.count == 2);
        invariant(e.last == 2);

        r = o.set_at(5, 1);
        invariant_zero(r);
        r = o.delete_at(1);
        invariant_zero(r);

        invariant(o.size() == 2);

        o.destroy();

        int *XMALLOC_N(4, intarray);
        for (int i = 0; i < 4; ++i) {
            intarray[i] = i + 1;
        }
        int_omt_t left, right;
        left.create_steal_sorted_array(&intarray, 4, 4);
        invariant_null(intarray);
        right.create();
        r = right.insert<int, intcmp>(8, 8, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(7, 7, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(6, 6, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(5, 5, nullptr);
        invariant_zero(r);

        int_omt_t combined;
        combined.merge(&left, &right);
        invariant(combined.size() == 8);
        invariant(left.size() == 0);
        invariant(right.size() == 0);
        struct intiter2extra e2 = {0, 0};
        r = combined.iterate<struct intiter2extra, intiter2>(&e2);
        invariant_zero(r);
        invariant(e2.count == 8);
        invariant(e2.last == 8);

        combined.destroy();
    }

} // end namespace test

} // end namespace toku

int main(void) {
    toku::test::unittest();
    toku::test::perftest();
    return 0;
}
