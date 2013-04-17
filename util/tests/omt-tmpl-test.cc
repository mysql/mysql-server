/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <type_traits>
#include <memory.h>
#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_time.h>
#include <util/omt.h>

namespace toku {

namespace test {

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
    return 0;
}
