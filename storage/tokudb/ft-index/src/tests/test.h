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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>

#include <string.h>
#include <stdlib.h>
#include <toku_stdint.h>
#include <stdio.h>
#include <db.h>
#include <limits.h>
#include <errno.h>
#include <toku_htonl.h>
#include <portability/toku_path.h>
#include <portability/toku_crash.h>
#include "toku_assert.h"
#include <signal.h>
#include <time.h>

#include "ydb.h"
//TDB uses DB_NOTFOUND for c_del and DB_CURRENT errors.
#ifdef DB_KEYEMPTY
#error
#endif
#define DB_KEYEMPTY DB_NOTFOUND

// Certain tests fail when row locks taken for read are not shared.
// This switch prevents them from failing so long as read locks are not shared.
#define BLOCKING_ROW_LOCKS_READS_NOT_SHARED

int verbose=0;

#define UU(x) x __attribute__((__unused__))

#define CKERR(r) ({ int __r = r; if (__r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, __r, db_strerror(r)); assert(__r==0); })
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, db_strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, db_strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

/*
 * Helpers for defining pseudo-hygienic macros using a (gensym)-like
 * technique.
 */
#define _CONCAT(x, y) x ## y
#define CONCAT(x, y) _CONCAT(x, y)
#define GS(symbol) CONCAT(CONCAT(__gensym_, __LINE__), CONCAT(_, symbol))

#define DEBUG_LINE do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

static __attribute__((__unused__)) void
parse_args (int argc, char * const argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-h]\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

static __attribute__((__unused__)) void 
print_engine_status(DB_ENV * UU(env)) {
    if (verbose) {  // verbose declared statically in this file
        uint64_t nrows;
        env->get_engine_status_num_rows(env, &nrows);
        int bufsiz = nrows * 128;   // assume 128 characters per row
        char buff[bufsiz];  
        env->get_engine_status_text(env, buff, bufsiz);
        printf("Engine status:\n");
        printf("%s", buff);
    }
}

static __attribute__((__unused__)) uint64_t
get_engine_status_val(DB_ENV * UU(env), const char * keyname) {
    uint64_t rval = 0;
    uint64_t nrows;
    uint64_t max_rows;
    env->get_engine_status_num_rows(env, &max_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[max_rows];
    fs_redzone_state redzone_state;
    uint64_t panic;
    uint32_t panic_string_len = 1024;
    char panic_string[panic_string_len];
    int r = env->get_engine_status (env, mystat, max_rows, &nrows, &redzone_state, &panic, panic_string, panic_string_len, TOKU_ENGINE_STATUS);
    CKERR(r);
    int found = 0;
    for (uint64_t i = 0; i < nrows && !found; i++) {
        if (strcmp(keyname, mystat[i].keyname) == 0) {
            found++;
            rval = mystat[i].value.num;
        }
    }
    CKERR2(found, 1);
    return rval;
}

static __attribute__((__unused__)) DBT *
dbt_init(DBT *dbt, const void *data, uint32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = (void*)data;
    dbt->size = size;
    return dbt;
}

static __attribute__((__unused__)) DBT *
dbt_init_malloc (DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

static __attribute__((__unused__)) DBT *
dbt_init_realloc (DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_REALLOC;
    return dbt;
}

// Simple LCG random number generator.  Not high quality, but good enough.
static uint32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline uint32_t myrandom (void) {
    rstate = (279470275ull*(uint64_t)rstate)%4294967291ull;
    return rstate;
}

static __attribute__((__unused__)) int
int64_dbt_cmp (DB *db UU(), const DBT *a, const DBT *b) {
//    assert(db && a && b);
    assert(a);
    assert(b);
//    assert(db);

    assert(a->size == sizeof(int64_t));
    assert(b->size == sizeof(int64_t));

    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static __attribute__((__unused__)) int
int_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size == sizeof(int));
  assert(b->size == sizeof(int));

  int x = *(int *) a->data;
  int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static __attribute__((__unused__)) int
uint_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size == sizeof(unsigned int));
  assert(b->size == sizeof(unsigned int));

  unsigned int x = *(unsigned int *) a->data;
  unsigned int y = *(unsigned int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

#define SET_TRACE_FILE(x) toku_set_trace_file(x)
#define CLOSE_TRACE_FILE(x) toku_close_trace_file()

#include <memory.h>

static uint64_t __attribute__((__unused__))
random64(void) {
    const unsigned int seed = 0xFEEDFACE;
    static int seeded = 0;
    if (!seeded) {
        seeded = 1;
        srandom(seed);
    }
    //random() generates 31 bits of randomness (low order)
    uint64_t low     = random();
    uint64_t high    = random();
    uint64_t twobits = random();
    uint64_t ret     = low | (high<<31) | (twobits<<62); 
    return ret;
}

static __attribute__((__unused__))
double get_tdiff(void) {
    static struct timeval prev={0,0};
    if (prev.tv_sec==0) {
	gettimeofday(&prev, 0);
	return 0.0;
    } else {
	struct timeval now;
	gettimeofday(&now, 0);
	double diff = now.tv_sec - prev.tv_sec + 1e-6*(now.tv_usec - prev.tv_usec);
	prev = now;
	return diff;
    }
}

static __attribute__((__unused__))
void format_time(const time_t *timer, char *buf) {
    ctime_r(timer, buf);
    size_t len = strlen(buf);
    assert(len < 26);
    char end;

    assert(len>=1);
    end = buf[len-1];
    while (end == '\n' || end == '\r') {
        buf[len-1] = '\0';
        len--;
        assert(len>=1);
        end = buf[len-1];
    }
}

static __attribute__((__unused__))
void print_time_now(void) {
    char timestr[80];
    time_t now = time(NULL);
    format_time(&now, timestr);
    printf("%s", timestr);
}

static void UU()
multiply_locks_for_n_dbs(DB_ENV *env, int num_dbs) {
    uint64_t current_max_lock_memory;
    int r = env->get_lk_max_memory(env, &current_max_lock_memory);
    CKERR(r);
    r = env->set_lk_max_memory(env, current_max_lock_memory * num_dbs);
    CKERR(r);
}

static inline void
default_parse_args (int argc, char * const argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    ++verbose;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

UU()
static void copy_dbt(DBT *dest, const DBT *src) {
    assert(dest->flags & DB_DBT_REALLOC);
    dest->data = toku_xrealloc(dest->data, src->size);
    dest->size = src->size;
    memcpy(dest->data, src->data, src->size);
}

// DBT_ARRAY is a toku-specific type
UU()
static int
env_update_multiple_test_no_array(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    DBT *old_src_key, DBT *old_src_data,
    DBT *new_src_key, DBT *new_src_data,
    uint32_t num_dbs, DB **db_array, uint32_t* flags_array,
    uint32_t num_keys, DBT keys[],
    uint32_t num_vals, DBT vals[]) {
    int r;
    DBT_ARRAY key_arrays[num_keys];
    DBT_ARRAY val_arrays[num_vals];
    for (uint32_t i = 0; i < num_keys; i++) {
        toku_dbt_array_init(&key_arrays[i], 1);
        key_arrays[i].dbts[0] = keys[i];
    }
    for (uint32_t i = 0; i < num_vals; i++) {
        toku_dbt_array_init(&val_arrays[i], 1);
        val_arrays[i].dbts[0] = vals[i];
    }
    r = env->update_multiple(env, src_db, txn, old_src_key, old_src_data, new_src_key, new_src_data,
                          num_dbs, db_array, flags_array,
                          num_keys, &key_arrays[0],
                          num_vals, &val_arrays[0]);
    for (uint32_t i = 0; i < num_keys; i++) {
        invariant(key_arrays[i].size == 1);
        invariant(key_arrays[i].capacity == 1);
        keys[i] = key_arrays[i].dbts[0];
        toku_dbt_array_destroy_shallow(&key_arrays[i]);
    }
    for (uint32_t i = 0; i < num_vals; i++) {
        invariant(val_arrays[i].size == 1);
        invariant(val_arrays[i].capacity == 1);
        vals[i] = val_arrays[i].dbts[0];
        toku_dbt_array_destroy_shallow(&val_arrays[i]);
    }
    return r;
}

UU()
static int env_put_multiple_test_no_array(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *src_key, 
    const DBT *src_val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys,
    DBT *vals,
    uint32_t *flags_array) 
{
    int r;
    DBT_ARRAY key_arrays[num_dbs];
    DBT_ARRAY val_arrays[num_dbs];
    for (uint32_t i = 0; i < num_dbs; i++) {
        toku_dbt_array_init(&key_arrays[i], 1);
        toku_dbt_array_init(&val_arrays[i], 1);
        key_arrays[i].dbts[0] = keys[i];
        val_arrays[i].dbts[0] = vals[i];
    }
    r = env->put_multiple(env, src_db, txn, src_key, src_val, num_dbs, db_array, &key_arrays[0], &val_arrays[0], flags_array);
    for (uint32_t i = 0; i < num_dbs; i++) {
        invariant(key_arrays[i].size == 1);
        invariant(key_arrays[i].capacity == 1);
        invariant(val_arrays[i].size == 1);
        invariant(val_arrays[i].capacity == 1);
        keys[i] = key_arrays[i].dbts[0];
        vals[i] = val_arrays[i].dbts[0];
        toku_dbt_array_destroy_shallow(&key_arrays[i]);
        toku_dbt_array_destroy_shallow(&val_arrays[i]);
    }
    return r;
}

UU()
static int env_del_multiple_test_no_array(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *src_key, 
    const DBT *src_val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys,
    uint32_t *flags_array) 
{
    int r;
    DBT_ARRAY key_arrays[num_dbs];
    for (uint32_t i = 0; i < num_dbs; i++) {
        toku_dbt_array_init(&key_arrays[i], 1);
        key_arrays[i].dbts[0] = keys[i];
    }
    r = env->del_multiple(env, src_db, txn, src_key, src_val, num_dbs, db_array, &key_arrays[0], flags_array);
    for (uint32_t i = 0; i < num_dbs; i++) {
        invariant(key_arrays[i].size == 1);
        invariant(key_arrays[i].capacity == 1);
        keys[i] = key_arrays[i].dbts[0];
        toku_dbt_array_destroy_shallow(&key_arrays[i]);
    }
    return r;
}

/* Some macros for evaluating blocks or functions within the scope of a
 * transaction. */
#define IN_TXN_COMMIT(env, parent, txn, flags, expr) ({                 \
            DB_TXN *(txn);                                              \
            { int chk_r = (env)->txn_begin((env), (parent), &(txn), (flags)); CKERR(chk_r); } \
            (expr);                                                     \
            { int chk_r = (txn)->commit((txn), 0); CKERR(chk_r); }      \
        })

#define IN_TXN_ABORT(env, parent, txn, flags, expr) ({                  \
            DB_TXN *(txn);                                              \
            { int chk_r = (env)->txn_begin((env), (parent), &(txn), (flags)); CKERR(chk_r); } \
            (expr);                                                     \
            { int chk_r = (txn)->abort(txn); CKERR(chk_r); }            \
        })

int test_main(int argc, char *const argv[]);
int main(int argc, char *const argv[]) {
    int r;
    toku_os_initialize_settings(1);
    r = test_main(argc, argv);
    return r;
}

#ifndef DB_GID_SIZE
#define	DB_GID_SIZE	DB_XIDDATASIZE
#endif
