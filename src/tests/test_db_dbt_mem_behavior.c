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
    char    waste[1024];
} DATA;

DB* db;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv;
u_int32_t set_ulen;
int32_t key_1 = 1;

void setup(void) {
    int r;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    dbenv = 0;
    /* Open/create primary */
    r = db_create(&db, dbenv, 0);                                               CKERR(r);
    r = db->open(db, null_txn, DIR "/primary.db", NULL, DB_BTREE, DB_CREATE, 0600);  CKERR(r);
}

void insert_test(void) {
    int r;
    DATA entry;
    DBT data;
    DBT key;

    memset(&entry, 0xFF, sizeof(entry));
    entry.pkey = key_1;
    
    dbt_init(&key, &entry.pkey, sizeof(entry.pkey));
    dbt_init(&data, &entry, sizeof(entry));
    r = db->put(db, null_txn, &key, &data, 0);  CKERR(r);
}

void close_dbs() {
    int r;

    r = db->close(db, 0);       CKERR(r);
}

int main(int argc, const char *argv[]) {
    int i;
    int r;
    
    parse_args(argc, argv);
//Simple flags that require minimal setup.
    u_int32_t flags[] = {
        0,
        DB_DBT_USERMEM,
        DB_DBT_MALLOC,
        DB_DBT_REALLOC,
    };
    int num_flags = sizeof(flags) / sizeof(flags[0]);

    int j;
    setup();
    insert_test();
    DBT key;
    DBT data;
    void* oldmem;
    
    for (j = 0; j < num_flags; j++) {
        for (i = 0; i < 2; i++) {
            if (i) set_ulen = sizeof(DATA) / 2;
            else   set_ulen = sizeof(DATA);

            int old_ulen;
            int was_truncated = 0;
            int ulen_changed;
            int size_full;
            int clone = 0;
            DATA fake;
            int small_buffer = 0;
            
            memset(&fake, 0xFF, sizeof(DATA));
            fake.pkey = key_1;
             
            
            dbt_init(&key, &key_1, sizeof(key_1));
            dbt_init(&data, 0, 0);
            data.flags = flags[j];
            oldmem = malloc(set_ulen);
            data.data = oldmem;
            memset(oldmem, 0, set_ulen);
            if (flags[j] == DB_DBT_USERMEM) {
                data.ulen = set_ulen;
            }
            old_ulen = data.ulen;
            r = db->get(db, null_txn, &key, &data, 0);
            if (flags[j] == DB_DBT_USERMEM && set_ulen < sizeof(DATA)) CKERR2(r, DB_BUFFER_SMALL);
            else CKERR(r);
            
            if (r == DB_BUFFER_SMALL) {
                //The entire 'waste' is full of 0xFFs
                DATA* entry = data.data;
                was_truncated = entry->waste[0] != 0;
                small_buffer = 1;
            }
            ulen_changed = data.ulen != old_ulen;
            size_full = data.size == sizeof(DATA);
            
            int min = data.ulen < data.size ? data.ulen : data.size;
            min = min < sizeof(DATA) ? min : sizeof(DATA);
            //assert(min == sizeof(DATA));
            r = memcmp((DATA*)data.data, &fake, min);
            clone = r == 0;

            if (flags[j] != 0) {
                free(data.data);
            }
            if (flags[j] == 0 || flags[j] == DB_DBT_MALLOC) {
                free(oldmem);
            }
                
            assert(!was_truncated);
            assert(!ulen_changed);
            assert(size_full);
            assert(clone == !small_buffer);
        }
    }
    oldmem = 0;
    dbt_init(&key, 0, 0);
    dbt_init(&data, 0, 0);
    close_dbs();
    return 0;
}

