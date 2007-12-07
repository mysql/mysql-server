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
    char waste[10240];
} DATA;

int callback_set_malloc;
DB* db;
DB* sdb;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv;



void* lastmalloced;

void* my_malloc(size_t size) {
    void* p = malloc(size);
    return p;
}

void* my_realloc(void *p, size_t size) {
    return realloc(p, size);
}

void my_free(void * p) {
    if (lastmalloced == p) {
        if (verbose) printf("Freeing %p.\n", p);
        lastmalloced = NULL;
    }
    free(p);    
}

/*
 * getname -- extracts a secondary key (the last name) from a primary
 * 	key/data pair
 */
int getskey(DB *secondary, const DBT *pkey, const DBT *pdata, DBT *skey)
{
    DATA* entry;

    if (verbose) {
        //printf("callback: init[%d],malloc[%d]\n", callback_init_data, callback_set_malloc);
        //fflush(stdout);
    }
    lastmalloced = NULL;
	memset(skey, 0, sizeof(DBT));
	entry = (DATA*)pdata->data;

    skey->size = sizeof(entry->skey);
    if (callback_set_malloc) {
        skey->flags = DB_DBT_APPMALLOC;
        skey->data = lastmalloced = my_malloc(skey->size);
    	memcpy(skey->data, &entry->skey, skey->size);
    }
    else skey->data = &entry->skey;

    return 0;
}

void second_setup() {
    int r;

    dbenv = 0;
    /* Open/create primary */
    r = db_create(&db, dbenv, 0);                                               CKERR(r);
#ifndef USE_TDB
    r = db->set_alloc(db, my_malloc, my_realloc, my_free);                      CKERR(r);
#endif
    r = db->open(db, null_txn, DIR "/primary.db", NULL, DB_BTREE, DB_CREATE, 0600);  CKERR(r);

    r = db_create(&sdb, dbenv, 0);                                              CKERR(r);
#ifndef USE_TDB
    r = sdb->set_alloc(sdb, my_malloc, my_realloc, my_free);                    CKERR(r);
#endif
    r = sdb->open(sdb, null_txn, DIR "/second.db", NULL, DB_BTREE, DB_CREATE, 0600); CKERR(r);

    /* Associate the secondary with the primary. */
    r = db->associate(db, null_txn, sdb, getskey, 0);                           CKERR(r);
}

void insert_test() {
    int r;
    static DATA entry;
    DBT data;
    DBT key;

    entry.pkey += 2;
    entry.junk += 3;
    entry.skey += 5;
    
    dbt_init(&key, &entry.pkey, sizeof(entry.pkey));
    dbt_init(&data, &entry, sizeof(entry));
    r = db->put(db, null_txn, &key, &data, 0);  CKERR(r);
}

void delete_test() {
    int r;
    static DATA entry;
    DBT key;

    entry.pkey += 2;
    entry.junk += 3;
    entry.skey += 5;
    
    dbt_init(&key, &entry.pkey, sizeof(entry.pkey));
    r = db->del(db, null_txn, &key, 0);  CKERR(r);
}

void close_dbs() {
    int r;

    r = db->close(db, 0);       CKERR(r);
    r = sdb->close(sdb, 0);     CKERR(r);
}

int main(int argc, const char *argv[]) {
    int i;
    
    parse_args(argc, argv);
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    second_setup();
    for (i = 0; i < 2; i++) {
        callback_set_malloc = i & 0x1;

#ifndef USE_TDB
        assert(lastmalloced == NULL);
#endif    
        insert_test();
#ifndef USE_TDB
        assert(lastmalloced == NULL);
#endif    
        delete_test();
#ifndef USE_TDB
        assert(lastmalloced == NULL);
#endif    
    }
    close_dbs();
    lastmalloced = NULL;
    return 0;
}

