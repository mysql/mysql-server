/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Test for a memory leak from just closing the lock tree manager (should close
   all lock trees. */

#include <toku_portability.h>
#include <fcntl.h>
#include "test.h"
#include <unistd.h>

static void initial_setup(void);

static int r;
static uint32_t       lt_refs[100];
static toku_lock_tree* lts   [100];
static toku_ltm*       ltm = NULL;
static DICTIONARY_ID   dict_ids[100];
enum { MAX_LT_LOCKS = 10 };
uint32_t max_locks = MAX_LT_LOCKS;
uint64_t max_lock_memory = MAX_LT_LOCKS*64;
int  nums[10000];

static void setup_ltm(void) {
    assert(!ltm);
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    CKERR(r);
    assert(ltm);
}

static void db_open_tree(size_t index, size_t db_id_index) {
    assert((lt_refs[index] == 0 && !lts[index]) ||
           (lt_refs[index] > 0 && lts[index]));
    assert(ltm);
    lt_refs[index]++;
    r = toku_ltm_get_lt(ltm, &lts[index], dict_ids[db_id_index], NULL, intcmp, NULL, NULL, NULL);
    CKERR(r);
    assert(lts[index]);
}

static void db_close_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    r = toku_lt_remove_ref(lts[index]); CKERR(r);
    lt_refs[index]--;
    if (lt_refs[index] == 0) { lts[index] = NULL; }
}

static void txn_open_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    toku_lt_add_ref(lts[index]);
    lt_refs[index]++;
}

static void txn_close_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    r = toku_lt_remove_ref(lts[index]); CKERR(r);
    lt_refs[index]--;
    if (lt_refs[index] == 0) { lts[index] = NULL; }
}

static void close_ltm(void) {
    assert(ltm);
    r = toku_ltm_close(ltm);
    CKERR(r);
    initial_setup();
    ltm = NULL;
}

static void run_test(void) {
    setup_ltm();
    //Start:

    /* ********************************************************************** */
    //Open and close.
    db_open_tree(0, 0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with db and transaction, db closes first.
    db_open_tree(0, 0);
    txn_open_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */
    //Open with db and transaction, txn closes first.
    db_open_tree(0, 0);
    txn_open_tree(0);
    txn_close_tree(0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles.
    db_open_tree(0, 0);
    db_open_tree(0, 0);
    db_close_tree(0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles and txns.
    db_open_tree(0, 0);
    txn_open_tree(0);
    db_open_tree(0, 0);
    db_close_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles and txns.
    db_open_tree(0, 0);
    db_open_tree(0, 0);
    txn_open_tree(0);
    db_close_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */

    //End:
    close_ltm();
}

static void initial_setup(void) {
    uint32_t i;

    ltm = NULL;
    assert(sizeof(dict_ids) / sizeof(dict_ids[0]) == sizeof(lts) / sizeof(lts[0]));
    for (i = 0; i < sizeof(lts) / sizeof(lts[0]); i++) {
        lts[i] = NULL;
        char name[sizeof(TESTDIR) + 256];
        sprintf(name, TESTDIR "/file%05x.db", i);
        dict_ids[i].dictid = i+1;
        assert(dict_ids[i].dictid != DICTIONARY_ID_NONE.dictid);
        lt_refs[i] = 0;
    }
}

static void close_test(void) {
    uint32_t i;
    for (i = 0; i < sizeof(lts) / sizeof(lts[0]); i++) {
        assert(lt_refs[i]==0); //The internal reference isn't counted.
        assert(dict_ids[i].dictid != DICTIONARY_ID_NONE.dictid);
    }
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    r = system("rm -rf " TESTDIR);
    CKERR(r);
    toku_os_mkdir(TESTDIR, S_IRWXU|S_IRWXG|S_IRWXO);

    initial_setup();

    run_test();

    close_test();
    return 0;
}
