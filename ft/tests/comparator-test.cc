/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <stdlib.h>
#include <ft/comparator.h>

static int MAGIC = 49;
static DBT dbt_a;
static DBT dbt_b;
static DESCRIPTOR expected_desc;

static int magic_compare(DB *db, const DBT *a, const DBT *b) {
    invariant(db && a && b);
    invariant(db->cmp_descriptor == expected_desc);
    invariant(a == &dbt_a);
    invariant(b == &dbt_b);
    return MAGIC;
}

static void test_desc(void) {
    int c;
    toku::comparator cmp;
    DESCRIPTOR_S d1, d2;

    // create with d1, make sure it gets used
    cmp.create(magic_compare, &d1);
    expected_desc = &d1;
    c = cmp.compare(&dbt_a, &dbt_b);
    invariant(c == MAGIC);

    // set desc to d2, make sure it gets used
    cmp.set_descriptor(&d2);
    expected_desc = &d2;
    c = cmp.compare(&dbt_a, &dbt_b);
    invariant(c == MAGIC);
}

static int dont_compare_me_bro(DB *db, const DBT *a, const DBT *b) {
    abort();
    return db && a && b;
}

static void test_infinity(void) {
    int c;
    toku::comparator cmp;
    cmp.create(dont_compare_me_bro, nullptr);

    // make sure infinity-valued end points compare as expected
    // to an arbitrary (uninitialized!) dbt. the comparison function
    // should never be called and thus the dbt never actually read.
    DBT arbitrary_dbt;

    c = cmp.compare(&arbitrary_dbt, toku_dbt_positive_infinity());
    invariant(c < 0);
    c = cmp.compare(toku_dbt_negative_infinity(), &arbitrary_dbt);
    invariant(c < 0);

    c = cmp.compare(toku_dbt_positive_infinity(), &arbitrary_dbt);
    invariant(c > 0);
    c = cmp.compare(&arbitrary_dbt, toku_dbt_negative_infinity());
    invariant(c > 0);

    c = cmp.compare(toku_dbt_negative_infinity(), toku_dbt_negative_infinity());
    invariant(c == 0);
    c = cmp.compare(toku_dbt_positive_infinity(), toku_dbt_positive_infinity());
    invariant(c == 0);
}

int main(void) {
    test_desc();
    test_infinity();
    return 0;
}
