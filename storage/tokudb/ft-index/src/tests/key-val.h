/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."

#ifndef KEY_VAL_H
#define KEY_VAL_H
//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   To use, during initialization:
//     generate_permute_tables();
//     r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
//


enum {MAX_DBS=32};
enum {MAGIC=311};

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
int   a[MAX_DBS][32];
int inv[MAX_DBS][32];

// rotate right and left functions
static inline uint32_t UU() rotr32(const uint32_t x, const uint32_t num) {
    const uint32_t n = num % 32;
    return (x >> n) | ( x << (32 - n));
}
static inline uint64_t UU() rotr64(const uint64_t x, const uint64_t num) {
    const uint64_t n = num % 64;
    return ( x >> n ) | ( x << (64 - n));
}
static inline uint32_t UU() rotl32(const uint32_t x, const uint32_t num) {
    const uint32_t n = num % 32;
    return (x << n) | ( x >> (32 - n));
}
static inline uint64_t UU() rotl64(const uint64_t x, const uint64_t num) {
    const uint64_t n = num % 64;
    return ( x << n ) | ( x >> (64 - n));
}

static void UU() generate_permute_tables(void) {
    int i, j, tmp;
    for(int db=0;db<MAX_DBS;db++) {
        for(i=0;i<32;i++) {
            a[db][i] = i;
        }
        for(i=0;i<32;i++) {
            j = random() % (i + 1);
            tmp = a[db][j];
            a[db][j] = a[db][i];
            a[db][i] = tmp;
        }
//        if(db < NUM_DBS){ printf("a[%d] = ", db); for(i=0;i<32;i++) { printf("%2d ", a[db][i]); } printf("\n");}
        for(i=0;i<32;i++) {
            inv[db][a[db][i]] = i;
        }
    }
}

// permute bits of x based on permute table bitmap
static uint32_t UU() twiddle32(uint32_t x, int db)
{
    uint32_t b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << a[db][i];
    }
    return b;
}

// permute bits of x based on inverse permute table bitmap
static uint32_t UU() inv_twiddle32(uint32_t x, int db)
{
    uint32_t b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << inv[db][i];
    }
    return b;
}

// generate val from key, index
static uint32_t UU() generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
}
static uint32_t UU() pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}

// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int UU() put_multiple_generate(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];
    (void) src_db;
    (void) src_val;

    uint32_t which = *(uint32_t*)dest_db->app_private;

    assert(which != 0);
    assert(dest_db != src_db);
    {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(uint32_t)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(uint32_t));
            dest_key->ulen = sizeof(uint32_t);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < sizeof(uint32_t)) {
            dest_val->data = toku_xrealloc(dest_val->data, sizeof(uint32_t));
            dest_val->ulen = sizeof(uint32_t);
        }
        uint32_t *new_key = (uint32_t *)dest_key->data;
        uint32_t *new_val = (uint32_t *)dest_val->data;

        *new_key = twiddle32(*(uint32_t*)src_key->data, which);
        *new_val = generate_val(*(uint32_t*)src_key->data, which);

        dest_key->size = sizeof(uint32_t);
        dest_val->size = sizeof(uint32_t);
        //data is already set above
    }

//    printf("pmg : dest_key.data = %u, dest_val.data = %u \n", *(unsigned int*)dest_key->data, *(unsigned int*)dest_val->data);

    return 0;
}

UU()
static int put_multiple_generate_switch(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];
    dest_key->flags = 0;
    dest_val->flags = 0;

    (void) src_db;

    uint32_t which = (uint32_t) (intptr_t) dest_db->app_private;
    assert(which == 0);

    // switch the key and val
    dbt_init(dest_key, src_val->data, src_val->size);
    dbt_init(dest_val, src_key->data, src_key->size);

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}

static int UU() uint_cmp(const void *ap, const void *bp) {
    unsigned int an = *(unsigned int *)ap;
    unsigned int bn = *(unsigned int *)bp;
    if (an < bn) 
        return -1;
    if (an > bn)
        return +1;
    return 0;
}

float last_progress = 0.0;
static int UU() poll_print(void *extra, float progress) {
    if ( verbose ) {
        if ( last_progress + 0.01 < progress ) {
            printf("  progress : %3.0f%%\n", progress * 100.0);
            last_progress = progress;
        }
    }    
    (void) extra;
    return 0;
}

enum {MAX_CLIENTS=10};
static inline UU() uint32_t key_to_put(int iter, int offset)
{
    return (uint32_t)(((iter+1) * MAX_CLIENTS) + offset);
}

static int UU() generate_initial_table(DB *db, DB_TXN *txn, uint32_t rows) 
{
    struct timeval start, now;
    if ( verbose ) {
        printf("generate_initial_table\n");
        gettimeofday(&start,0);
    }
    int r = 0;
    DBT key, val;
    uint32_t k, v, i;
    // create keys of stride MAX_CLIENTS
    for (i=0; i<rows; i++)
    {
        k = key_to_put(i, 0);
        v = generate_val(k, 0);
        dbt_init(&key, &k, sizeof(k));
        dbt_init(&val, &v, sizeof(v));
        r = db->put(db, txn, &key, &val, 0);
        if ( r != 0 ) break;
    }
    if ( verbose ) {
        gettimeofday(&now,0);
        int duration = (int)(now.tv_sec - start.tv_sec);
        if ( duration > 0 )
            printf("generate_initial_table : %u rows in %d sec = %d rows/sec\n", rows, duration, rows/duration);
    }
    
    return r;
}




#endif // KEY_VAL_H
