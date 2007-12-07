/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// DIR is defined in the Makefile

typedef struct {
    int32_t pkey;
    int32_t junk;
    int32_t skey;
} DATA;

int callback_init_data;
int callback_set_malloc;
int callback_return_DONOTINDEX;
DB* db;
DB* sdb;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv = 0;


void* lastmalloced = NULL;

/*
 * getname -- extracts a secondary key (the last name) from a primary
 * 	key/data pair
 */
int getskey(DB *secondary, const DBT *pkey, const DBT *pdata, DBT *skey)
{
	/*
	 * Since the secondary key is a simple structure member of the
	 * record, we don't have to do anything fancy to return it.  If
	 * we have composite keys that need to be constructed from the
	 * record, rather than simply pointing into it, then the user's
	 * function might need to allocate space and copy data.  In
	 * this case, the DB_DBT_APPMALLOC flag should be set in the
	 * secondary key DBT.
	 */
    DATA* entry;

    if (verbose) {
        printf("callback: init[%d],malloc[%d],%sINDEX\n", callback_init_data, callback_set_malloc, callback_return_DONOTINDEX ? "DONOT" : "");
        fflush(stdout);
    }
    
	memset(skey, 0, sizeof(DBT));
	entry = (DATA*)pdata->data;

    if (callback_set_malloc) skey->flags = DB_DBT_APPMALLOC;
    if (callback_init_data) {
        skey->size = sizeof(entry->skey);
        if (callback_set_malloc) {
            skey->data = lastmalloced = malloc(skey->size);
        	memcpy(skey->data, &entry->skey, skey->size);
        }
        else skey->data = &entry->skey;
    }
    else {
        skey->data = NULL;
        skey->size = 0;
    }

    if (callback_return_DONOTINDEX) return DB_DONOTINDEX;
    return 0;
}

void second_setup() {
    int r;

    /* Open/create primary */
    r = db_create(&db, dbenv, 0);                                               CKERR(r);
    r = db->open(db, null_txn, DIR "/primary.db", NULL, DB_BTREE, DB_CREATE, 0600); CKERR(r);

    r = db_create(&sdb, dbenv, 0);                                              CKERR(r);
    r = sdb->open(sdb, null_txn, DIR "/secondary.db", NULL, DB_BTREE, DB_CREATE, 0600); CKERR(r);

    /* Associate the secondary with the primary. */
    r = db->associate(db, null_txn, sdb, getskey, 0);                            CKERR(r);
}

void insert() {
    int r;
    DATA entry;
    DBT data;
    DBT key;

    entry.pkey = 2;
    entry.junk = 3;
    entry.skey = 5;
    
    dbt_init(&key, &entry.pkey, sizeof(entry.pkey));
    dbt_init(&data, &entry, sizeof(entry));
    r = db->put(db, null_txn, &key, &data, 0);  CKERR(r);
}

void check_secondary(int expect_r) {
    int r;
    DBC *c;
    DBT skey;
    DBT data;
    
    dbt_init(&skey, 0, 0);
    dbt_init(&data, 0, 0);
    r = sdb->cursor(sdb, null_txn, &c, 0);      CKERR(r);
    r = c->c_get(c, &skey, &data, DB_FIRST);    CKERR2(r, expect_r);
    r = c->c_close(c);                          CKERR(r);
}

void close_dbs() {
    int r;
    
    r = db->close(db, 0);   CKERR(r);
    r = sdb->close(sdb, 0); CKERR(r);
}

int main(int argc, const char *argv[]) {
    int i;
    
    parse_args(argc, argv);
    for (i = 0; i < (1<<3); i++) {
        system("rm -rf " DIR);
        mkdir(DIR, 0777);
        second_setup();

        check_secondary(DB_NOTFOUND);

        callback_init_data = i & (1 << 0);
        callback_set_malloc = i & (1 << 1);
        callback_return_DONOTINDEX = i & (1 << 2);

        lastmalloced = NULL;
        insert();
        check_secondary(callback_return_DONOTINDEX ? DB_NOTFOUND : 0);
        if (callback_return_DONOTINDEX) free(lastmalloced);
        close_dbs();
    }
    return 0;
}
