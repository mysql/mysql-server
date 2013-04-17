/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <string.h>
#include "test.h"

// verify that updating a lock tree's descriptor works properly.

static toku_ltm *ltm;
static toku_lock_tree *tree;

enum { MAX_LOCKS = 1000, MAX_LOCK_MEMORY = MAX_LOCKS * 64 };

#define verbose_printf(...)     \
    do {                        \
        if (verbose) {          \
            printf(__VA_ARGS__);\
        }                       \
    } while (0)

#define MAKE_DESCRIPTOR(buf) \
    { .dbt = { .data = (char *) buf, .size = sizeof(buf) } }
static DESCRIPTOR_S descriptors[] = {
    MAKE_DESCRIPTOR("cats"),
    MAKE_DESCRIPTOR("elephants"),
    MAKE_DESCRIPTOR("snakes"),
    MAKE_DESCRIPTOR("catsarecute"),
    MAKE_DESCRIPTOR("elephantsarelarge"),
    MAKE_DESCRIPTOR("snakesonaplane")
};
static const int num_descriptors = sizeof(descriptors) / sizeof(DESCRIPTOR_S);
static DESCRIPTOR current_descriptor;

static bool same_descriptor(DESCRIPTOR a, DESCRIPTOR b)
{
    verbose_printf("a %p b %p\n", a, b);
    if (a == NULL && b == NULL) {
        return true;
    }
    if (a != NULL && b != NULL) {
        verbose_printf("a.size %d b.size %d\n", a->dbt.size, b->dbt.size);
        verbose_printf("a.data %s b.data %s\n", (char*)a->dbt.data, (char*)b->dbt.data);
        return a->dbt.size == b->dbt.size && 
            memcmp(a->dbt.data, b->dbt.data, a->dbt.size) == 0;
    }
    return false;
}

static int cmp_function(DB * db, const DBT * a, const DBT * b)
{
    // clearly the db should not be null.
    assert(db);
    // the descriptor in the DB should be the descriptor we expect
    assert(same_descriptor(current_descriptor, db->cmp_descriptor));
    // doesn't really matter what we return here
    (void) a; (void) b;
    return 0;
}

int main(int argc, const char *argv[]) 
{
    int r;
    parse_args(argc, argv);

    // get a lock manager and a lock tree.
    r = toku_ltm_create(&ltm, MAX_LOCKS, MAX_LOCK_MEMORY, dbpanic);
    CKERR(r);
    current_descriptor = NULL;
    toku_ltm_get_lt(ltm, &tree, (DICTIONARY_ID) {1}, current_descriptor, cmp_function, NULL, NULL, NULL);
    CKERR(r);

    for (int d = 0; d < num_descriptors; d++) {
        current_descriptor = &descriptors[d];
        toku_lt_update_descriptor(tree, current_descriptor);
        // check that we can call this point comparison a couple
        // of times and pass the comparison function's assertion
        for (int i = 0; i < 10; i++) {
            const toku_point x = { .lt = tree, .key_payload = (void *) "" };
            const toku_point y = { .lt = tree, .key_payload = (void *) "" };
            toku_lt_point_cmp(&x, &y);
        }
    }

    // cleanup
    toku_ltm_close(ltm);

    return 0;
}
