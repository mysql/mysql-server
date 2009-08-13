/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#define fname ENVDIR "/each_le_and_msg.tokudb_10"

static void
test_upgrade_from_10 (void) {
    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    system("gunzip -c backwards_10/each_le_and_msg.tokudb_10.gz > " fname);

    const int PAGESIZE=1024;
    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_pagesize(db, PAGESIZE);
    assert(r == 0);
    r = db->open(db, null_txn, fname, NULL, DB_BTREE, 0, 0666);
    assert(r == 0);

    enum {num_leafentries = 7, num_insert_messages=2};
    char *keys[num_leafentries+num_insert_messages] = {
        "le_committed",
        "le_provpair_0",
        "le_provpair_25",
        "le_provdel_0",  //Leafentry should not even exist; query should not find it.
        "le_provdel_25", //Leafentry should exist, but query should not find it.
        "le_both_0",
        "le_both_25",
        "message_0",
        "message_42"
    };
    char *vals[num_leafentries+num_insert_messages] = {
        "val_le_committed",
        "val_le_provpair_0",
        "val_le_provpair_25",
        "val_le_provdel_0",
        "val_le_provdel_25",
        "val_le_both_0_and_padding",
        "val_le_both_25_and_padding",
        "val_message_0",
        "val_message_42"
    };

    int i;

    unsigned int VALSIZE = 256;
    DBT key,val;
    for (i=0; i<num_leafentries+num_insert_messages; i++) {
        char valbuf[VALSIZE];
        assert(VALSIZE > strlen(vals[i]));
        memset(valbuf, 'X', sizeof(valbuf));
        strcpy(valbuf, vals[i]);

        dbt_init(&key, keys[i], strlen(keys[i]));
        if (i<num_leafentries)
            dbt_init(&val, valbuf, sizeof(valbuf));
        else
            dbt_init(&val, vals[i], strlen(vals[i]));
        r = db->get(db, null_txn, &key, &val, DB_GET_BOTH); //DB_GET_BOTH);
        if (!strcmp(keys[i], "le_provdel_0") ||
            !strcmp(keys[i], "le_provdel_25")) {
            //Should not find with key/val pair
            CKERR2(r, DB_NOTFOUND);
            
            //Should not find with key alone
            DBT nothing;
            dbt_init(&nothing, NULL, 0);
            r = db->get(db, null_txn, &key, &nothing, 0);
            CKERR2(r, DB_NOTFOUND);
        }
        else
            CKERR(r);
    } 


    DBC *c;
    r = db->cursor(db, NULL, &c, 0);
        CKERR(r);

    dbt_init(&key, NULL, 0);
    dbt_init(&val, NULL, 0);
    int num_found = 0;
    do {
        r = c->c_get(c, &key, &val, DB_NEXT);
        CKERR2s(r, 0, DB_NOTFOUND);
        if (r==0) num_found++;
    } while (r==0);
    CKERR2(r, DB_NOTFOUND);
    c->c_close(c);
    assert(num_found == num_leafentries + num_insert_messages - 2); //le_provdels should not show up

    r = db->close(db, 0);
    assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
    parse_args(argc, argv);

    test_upgrade_from_10();

    return 0;
}
