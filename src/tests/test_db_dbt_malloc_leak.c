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
    int32_t skey;
} DATA;

int callback_set_malloc;
DB* db;
DB* sdb;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv;

int nummallocced = 0;

void* my_malloc(size_t size) {
    void* p = malloc(size);
    if (size != 0) {
        nummallocced++;
//        if (verbose) printf("Malloc [%d] %p.\n", (int)size, p);
    }
    return p;
}

void* my_realloc(void *p, size_t size) {
    void* newp = realloc(p, size);
//    if (verbose) printf("realloc [%d] %p.\n", (int)size, newp);
    return newp;
}

void my_free(void * p) {
    if (p) {
        nummallocced--;
//        if (verbose) printf("Free %p.\n", p);
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
	memset(skey, 0, sizeof(DBT));
	entry = (DATA*)pdata->data;

    skey->size = sizeof(entry->skey);
    if (callback_set_malloc) {
        skey->flags = DB_DBT_APPMALLOC;
        skey->data = my_malloc(skey->size);
    	memcpy(skey->data, &entry->skey, skey->size);
    }
    else skey->data = &entry->skey;

    return 0;
}

void second_setup(u_int32_t dupflags) {
    int r;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);
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
    if (dupflags) {
        r = sdb->set_flags(sdb, dupflags);                                      CKERR(r);
    }
    r = sdb->open(sdb, null_txn, DIR "/second.db", NULL, DB_BTREE, DB_CREATE, 0600); CKERR(r);

    /* Associate the secondary with the primary. */
    r = db->associate(db, null_txn, sdb, getskey, 0);                           CKERR(r);
}

void insert_test(int pkey, int skey) {
    int r;
    DATA entry;
    DBT data;
    DBT key;

    memset(&entry, 0, sizeof(entry));
    entry.pkey = pkey;
    entry.skey = skey;
    
    dbt_init(&key, &entry.pkey, sizeof(entry.pkey));
    dbt_init(&data, &entry, sizeof(entry));
    r = db->put(db, null_txn, &key, &data, 0);  CKERR(r);
}

DBT* dbt_init_malloc_and_copy(DBT* dbt, int something) {
    dbt_init_malloc(dbt);
    dbt->size = sizeof(something);
    dbt->data = my_malloc(dbt->size);
    memcpy(dbt->data, &something, dbt->size);
    return dbt;
}

void pget_test_set_skey_pkey(DBC* dbc, u_int32_t flag, u_int32_t expect, int set_skey, int skey_set, int set_pkey, int pkey_set) {
    int r;
    DBT skey;
    DBT pkey;
    DBT data;

    if (set_skey) dbt_init_malloc_and_copy(&skey, skey_set);
    else dbt_init_malloc(&skey);
    if (set_pkey) dbt_init_malloc_and_copy(&pkey, pkey_set);
    else dbt_init_malloc(&pkey);
    dbt_init_malloc(&data);

    r = dbc->c_pget(dbc, &skey, &pkey, &data, flag);        CKERR2(r, expect);
    my_free(pkey.data);
    my_free(skey.data);
    my_free(data.data);
}

void pget_test(DBC* dbc, u_int32_t flag, u_int32_t expect) {
    pget_test_set_skey_pkey(dbc, flag, expect, 0, 0, 0, 0);
}

void close_dbs() {
    int r;

    r = db->close(db, 0);       CKERR(r);
    r = sdb->close(sdb, 0);     CKERR(r);
}


u_int32_t get_dupflags(u_int32_t flag) {
    if (flag == DB_NEXT_DUP) {
        return DB_DUP | DB_DUPSORT;
    }
    return 0;
}

const int keysmall = 2;
const int keymid = 3;
const int keybig = 5;
const int skeysmall = 11;
const int skeymid = 13;
const int skeybig = 17;

void insert_setup(u_int32_t flag) {
    switch (flag) {
        case (DB_SET_RANGE):    //Must be inserted in descending
        case (DB_SET):          //Just insert any two.
        case (DB_GET_BOTH):     //Just insert any two.
#ifdef DB_NEXT_NODUP
        case (DB_NEXT_NODUP):   //Must be inserted in descending
#endif        
        case (DB_NEXT):         //Must be inserted in descending
        case (DB_FIRST): {      //Must be inserted in descending
            insert_test(keysmall, skeybig);
            insert_test(keysmall, skeysmall);
            break;
        }
#ifdef DB_PREV_NODUP
        case (DB_PREV_NODUP):   //Must be inserted in ascending
#endif
        case (DB_PREV):         //Must be inserted in ascending
        case (DB_LAST): {       //Must be inserted in ascending
            insert_test(keysmall, skeysmall);
            insert_test(keysmall, skeybig);
            break;
        }
        case (DB_CURRENT): {
            //Must insert one initially and then do more.
            insert_test(keysmall, skeysmall);
            break;
        }
        case (DB_NEXT_DUP): {
            //Must have two entries with same skey, instead of same p1key.
            insert_test(keysmall, skeysmall);
            insert_test(keybig,   skeysmall);
            break;
        }
        default: {
            printf("Not yet ready for flag %u\n", flag);
            exit(0);
        }
    }
    
}

void cursor_setup(DBC* dbc, u_int32_t flag) {
    switch (flag) {
#ifdef DB_NEXT_NODUP
        case (DB_NEXT_NODUP):
#endif        
#ifdef DB_PREV_NODUP
        case (DB_PREV_NODUP):
#endif
        case (DB_NEXT):
        case (DB_FIRST):
        case (DB_PREV):
        case (DB_LAST): {
            pget_test(dbc, flag, 0);
            break;
        }
        case (DB_CURRENT): {
            pget_test(dbc, DB_FIRST, 0);
            insert_test(keysmall, skeybig);
            pget_test(dbc, flag, DB_KEYEMPTY);
            break;
        }
        case (DB_GET_BOTH): {
            pget_test_set_skey_pkey(dbc, flag, DB_NOTFOUND, 1, skeybig, 1, keysmall);
            break;
        }
        case (DB_SET): {
            pget_test_set_skey_pkey(dbc, flag, DB_NOTFOUND, 1, skeybig, 0, 0);
            break;
        }
        case (DB_NEXT_DUP): {
            pget_test(dbc, DB_FIRST, 0);
            insert_test(keybig, skeybig);
            pget_test(dbc, flag, DB_NOTFOUND);
            break;
        }
        case (DB_SET_RANGE): {
            pget_test_set_skey_pkey(dbc, flag, DB_NOTFOUND, 1, skeymid, 0, 0);
            break;
        }
        default: {
            printf("Not yet ready for flag %u\n", flag);
            exit(0);
        }
    }
    
}

int main(int argc, const char *argv[]) {
    int i;
    int r;
    DBC* dbc;
    
    parse_args(argc, argv);
//Simple flags that require minimal setup.
    u_int32_t flags[] = {
        DB_NEXT,
        DB_PREV,
        DB_FIRST,
        DB_LAST,
        DB_CURRENT, //noparam, but must be set
        DB_GET_BOTH,
#ifdef DB_NEXT_NODUP
        DB_NEXT_NODUP,
#endif        
#ifdef DB_PREV_NODUP
        DB_PREV_NODUP,
#endif
        DB_SET,
        DB_NEXT_DUP,
        DB_SET_RANGE,
    };
    int num_flags = sizeof(flags) / sizeof(flags[0]);

    int j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < num_flags; j++) {
            u_int32_t flag = flags[j];
            u_int32_t dupflags = get_dupflags(flag);
            second_setup(dupflags);
            callback_set_malloc = i & 0x1;

            insert_setup(flag);
            r = sdb->cursor(sdb, null_txn, &dbc, 0);                CKERR(r);
            cursor_setup(dbc, flag);        
            r = dbc->c_close(dbc);                                  CKERR(r);
            close_dbs();
#ifndef USE_TDB
            //if (flag != DB_CURRENT)
            if (nummallocced != 0) {
                printf("Nummallocced = %d\n", nummallocced);
                assert(nummallocced == 0);
            }
#endif
        }
    }
    return 0;
}

