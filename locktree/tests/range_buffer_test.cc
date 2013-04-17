/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include <locktree/range_buffer.h>

namespace toku {

const size_t num_points = 60;

static const DBT *get_dbt_by_iteration(size_t i) {
    if (i == 0) {
        return toku_dbt_negative_infinity();
    } else if (i < num_points) {
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
    range_buffer::iterator iter;
    range_buffer::iterator::record rec;
    iter.create(&buffer);
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
    range_buffer::iterator iter;
    range_buffer::iterator::record rec;
    iter.create(&buffer);
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
    range_buffer::iterator iter;
    range_buffer::iterator::record rec;
    iter.create(&buffer);
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

} /* namespace toku */

int main(void) {
    toku::test_points();
    toku::test_ranges();
    toku::test_mixed();
    return 0;
}
