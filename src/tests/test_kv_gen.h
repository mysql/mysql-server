
#ifndef __TEST_KV_GEN_H
#define __TEST_KV_GEN_H

#if defined(__cilkplusplus) || defined(__cplusplus)
extern "C" {
#endif

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "test.h"
//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
enum {MAX_DBS=256};
enum {MAGIC=311};
static int  aa[MAX_DBS][32] UU();
static int inv[MAX_DBS][32] UU();

// rotate right and left functionsp
static inline unsigned int UU() 
rotr32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x >> n) | ( x << (32 - n));
}
static inline unsigned int UU() 
rotl32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x << n) | ( x >> (32 - n));
}

static void UU() 
generate_permute_tables(void) {
    srandom(1);
    int i, j, tmp;
    for(int db=0;db<MAX_DBS;db++) {
        for(i=0;i<32;i++) {
            aa[db][i] = i;
        }
        for(i=0;i<32;i++) {
            j = random() % (i + 1);
            tmp = aa[db][j];
            aa[db][j] = aa[db][i];
            aa[db][i] = tmp;
        }
        for(i=0;i<32;i++) {
            inv[db][aa[db][i]] = i;
        }
    }
}

// permute bits of x based on permute table bitmap
static unsigned int UU() 
twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << aa[db][i];
    }
    return b;
}

// permute bits of x based on inverse permute table bitmap
static unsigned int UU() 
inv_twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << inv[db][i];
    }
    return b;
}

// generate val from key, index
static unsigned int UU() 
generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
}
static unsigned int UU() 
pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}


  // don't check first n rows (expect to have been deleted)
static void UU()
check_results_after_row_n(DB_ENV *env, DB **dbs, const int num_dbs, const int num_rows, const int first_row_to_check) {

    for(int j=0;j<num_dbs;j++){
        DBT key, val;
        unsigned int k=0, v=0;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        int r;
        unsigned int pkey_for_db_key;

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        DBC *cursor;
        r = dbs[j]->cursor(dbs[j], txn, &cursor, 0);
        CKERR(r);
        for(int i=first_row_to_check; i<num_rows; i++) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);    
            CKERR(r);
            k = *(unsigned int*)key.data;
            pkey_for_db_key = (j == 0) ? k : inv_twiddle32(k, j);
            v = *(unsigned int*)val.data;
            // test that we have the expected keys and values
            if ((unsigned int)pkey_for_db_key != (unsigned int)pkey_for_val(v, j))
                printf(" DB[%d] key = %10u, val = %10u, pkey_for_db_key = %10u, pkey_for_val=%10d\n", j, v, k, pkey_for_db_key, pkey_for_val(v, j));
            assert((unsigned int)pkey_for_db_key == (unsigned int)pkey_for_val(v, j));
            dbt_init(&key, NULL, sizeof(unsigned int));
            dbt_init(&val, NULL, sizeof(unsigned int));
        }
        if ( verbose ) {printf("."); fflush(stdout);}
        r = cursor->c_close(cursor);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
    if ( verbose ) {printf("ok");fflush(stdout);}
}

static void UU()
check_results(DB_ENV *env, DB **dbs, const int num_dbs, const int num_rows)
{
  check_results_after_row_n(env, dbs, num_dbs, num_rows, 0);
}


static int UU() 
put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {

    src_db = src_db;
    extra = extra;

    uint32_t which = *(uint32_t*)dest_db->app_private;

    if ( which == 0 ) {
        if (dest_key->flags==DB_DBT_REALLOC) {
            if (dest_key->data) toku_free(dest_key->data);
            dest_key->flags = 0;
            dest_key->ulen  = 0;
        }
        if (dest_val->flags==DB_DBT_REALLOC) {
            if (dest_val->data) toku_free(dest_val->data);
            dest_val->flags = 0;
            dest_val->ulen  = 0;
        }
        dbt_init(dest_key, src_key->data, src_key->size);
        dbt_init(dest_val, src_val->data, src_val->size);
    }
    else {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(unsigned int)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(unsigned int));
            dest_key->ulen = sizeof(unsigned int);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < sizeof(unsigned int)) {
            dest_val->data = toku_xrealloc(dest_val->data, sizeof(unsigned int));
            dest_val->ulen = sizeof(unsigned int);
        }
        unsigned int *new_key = (unsigned int *)dest_key->data;
        unsigned int *new_val = (unsigned int *)dest_val->data;

        *new_key = twiddle32(*(unsigned int*)src_key->data, which);
        *new_val = generate_val(*(unsigned int*)src_key->data, which);

        dest_key->size = sizeof(unsigned int);
        dest_val->size = sizeof(unsigned int);
        //data is already set above
    }
    return 0;
}

#if defined(__cilkplusplus) || defined(__cplusplus)
}
#endif

#endif // __TEST_KV_GEN_H
