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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// measure the performance of insertions into multiple dictionaries using ENV->put_multiple
// the table schema is t(a bigint, b bigint, c bigint, d bigint, primary key(a), key(b), key(c,d), clustering key(d))
// the primary key(a) is represented with key=a and value=b,c,d
// the key(b) index is represented with key=b,a and no value
// the key(c,d) index is represented with key=c,d,a and no value
// the clustering key(d) is represented with key=d,a and value=b,c
// a is auto increment
// b, c and d are random

#include "../include/toku_config.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#if defined(HAVE_BYTESWAP_H)
# include <byteswap.h>
#elif defined(HAVE_LIBKERN_OSBYTEORDER_H)
# include <libkern/OSByteOrder.h>
# define bswap_64 OSSwapInt64
#endif
#include <arpa/inet.h>
#include "db.h"

static int force_multiple = 1;

struct table {
    int ndbs;
    DB **dbs;
#if defined(TOKUDB)
    DBT *mult_keys;
    DBT *mult_vals;
    uint32_t *mult_flags;
#endif
};

#if defined(TOKUDB)
static void table_init_dbt(DBT *dbt, size_t length) {
    dbt->flags = DB_DBT_USERMEM;
    dbt->data = malloc(length);
    dbt->ulen = length;
    dbt->size = 0;
}

static void table_destroy_dbt(DBT *dbt) {
    free(dbt->data);
}
#endif

static void table_init(struct table *t, int ndbs, DB **dbs, size_t key_length __attribute__((unused)), size_t val_length __attribute__((unused))) {
    t->ndbs = ndbs;
    t->dbs = dbs;
#if defined(TOKUDB)
    t->mult_keys = calloc(ndbs, sizeof (DBT));
    int i;
    for (i = 0; i < ndbs; i++) 
        table_init_dbt(&t->mult_keys[i], key_length);
    t->mult_vals = calloc(ndbs, sizeof (DBT));
    for (i = 0; i < ndbs; i++) 
        table_init_dbt(&t->mult_vals[i], val_length);
    t->mult_flags = calloc(ndbs, sizeof (uint32_t));
    for (i = 0; i < ndbs; i++) 
        t->mult_flags[i] = 0;
#endif
}

static void table_destroy(struct table *t) {
#if defined(TOKUDB)
    int i;
    for (i = 0; i < t->ndbs; i++)
        table_destroy_dbt(&t->mult_keys[i]);
    free(t->mult_keys);
    for (i = 0; i < t->ndbs; i++)
        table_destroy_dbt(&t->mult_vals[i]);
    free(t->mult_vals);
    free(t->mult_flags);
#else
    assert(t);
#endif
}

static int verbose = 0;

static long random64(void) {
    return ((long)random() << 32LL) + (long)random();
}

static long htonl64(long x) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64(x);
#else
#error
#endif
}

#if defined(TOKUDB)
static int my_generate_row_for_put(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {
    assert(src_db);
    assert(dest_key->flags == DB_DBT_USERMEM && dest_key->ulen >= 4 * 8);
    assert(dest_val->flags == DB_DBT_USERMEM && dest_val->ulen >= 4 * 8);
    int index_num; 
    assert(dest_db->descriptor->dbt.size == sizeof index_num);
    memcpy(&index_num, dest_db->descriptor->dbt.data, sizeof index_num);
    switch (htonl(index_num) % 4) {
    case 0:
        // dest_key = src_key
        dest_key->size = src_key->size;
        memcpy(dest_key->data, src_key->data, src_key->size);
        // dest_val = src_val
        dest_val->size = src_val->size;
        memcpy(dest_val->data, src_val->data, src_val->size);
        break;
    case 1:
        // dest_key = b,a
        dest_key->size = 2 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 0, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_key->data + 0, 8);
        // dest_val = null
        dest_val->size = 0;
        break;
    case 2:
        // dest_key = c,d,a
        dest_key->size = 3 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 8, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_val->data + 16, 8);
        memcpy((char *)dest_key->data + 16, (char *)src_key->data + 0, 8);
        // dest_val = null
        dest_val->size = 0;
        break;
    case 3:
        // dest_key = d,a
        dest_key->size = 2 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 16, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_key->data + 0, 8);
        // dest_val = b,c
        dest_val->size = 2 * 8;
        memcpy((char *)dest_val->data + 0, (char *)src_val->data + 0, 8);
        memcpy((char *)dest_val->data + 8, (char *)src_val->data + 8, 8);
        break;
    default:
        assert(0);
    }
    return 0;
}

#else

static int my_secondary_key(DB *db, const DBT *src_key, const DBT *src_val, DBT *dest_key) {
    assert(dest_key->flags == 0 && dest_key->data == NULL);
    dest_key->flags = DB_DBT_APPMALLOC;
    dest_key->data = malloc(4 * 8); assert(dest_key->data);
    switch ((intptr_t)db->app_private % 4) {
    case 0:
        // dest_key = src_key
        dest_key->size = src_key->size;
        memcpy(dest_key->data, src_key->data, src_key->size);
        break;
    case 1:
        // dest_key = b,a
        dest_key->size = 2 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 0, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_key->data + 0, 8);
        break;
    case 2:
        // dest_key = c,d,a
        dest_key->size = 3 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 8, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_val->data + 16, 8);
        memcpy((char *)dest_key->data + 16, (char *)src_key->data + 0, 8);
        break;
    case 3:
        // dest_key = d,a,b,c
        dest_key->size = 4 * 8;
        memcpy((char *)dest_key->data + 0, (char *)src_val->data + 16, 8);
        memcpy((char *)dest_key->data + 8, (char *)src_key->data + 0, 8);
        memcpy((char *)dest_key->data + 16, (char *)src_val->data + 0, 8);
        memcpy((char *)dest_key->data + 24, (char *)src_val->data + 8, 8);
        break;
    default:
        assert(0);
    }
    return 0;
}
#endif

static void insert_row(DB_ENV *db_env, struct table *t, DB_TXN *txn, long a, long b, long c, long d) {
    int r;

    // generate the primary key
    char key_buffer[8];
    a = htonl64(a);
    memcpy(key_buffer, &a, sizeof a);

    // generate the primary value
    char val_buffer[3*8];
    b = htonl64(b);
    memcpy(val_buffer+0, &b, sizeof b);
    c = htonl64(c);
    memcpy(val_buffer+8, &c, sizeof c);
    d = htonl64(d);
    memcpy(val_buffer+16, &d, sizeof d);

    DBT key = { .data = key_buffer, .size = sizeof key_buffer };
    DBT value = { .data = val_buffer, .size = sizeof val_buffer };
#if defined(TOKUDB)
    if (!force_multiple && t->ndbs == 1) {
        r = t->dbs[0]->put(t->dbs[0], txn, &key, &value, t->mult_flags[0]); assert(r == 0);
    } else {
        r = db_env->put_multiple(db_env, t->dbs[0], txn, &key, &value, t->ndbs, &t->dbs[0], t->mult_keys, t->mult_vals, t->mult_flags); assert(r == 0);
    }
#else
    assert(db_env);
    r = t->dbs[0]->put(t->dbs[0], txn, &key, &value, 0); assert(r == 0);
#endif
}

static inline float tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

static void insert_all(DB_ENV *db_env, struct table *t, long nrows, long max_rows_per_txn, long key_range, long rows_per_report, bool do_txn) {
    int r;

    struct timeval tstart;
    r = gettimeofday(&tstart, NULL); assert(r == 0);
    struct timeval tlast = tstart;
    DB_TXN *txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
    }
    long n_rows_per_txn = 0;
    long rowi;
    for (rowi = 0; rowi < nrows; rowi++) {
        long a = rowi;
        long b = random64() % key_range;
        long c = random64() % key_range;
        long d = random64() % key_range;
        insert_row(db_env, t, txn, a, b, c, d);
        n_rows_per_txn++;
        
        // maybe commit
        if (do_txn && n_rows_per_txn == max_rows_per_txn) {
            r = txn->commit(txn, 0); assert(r == 0);
            r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
            n_rows_per_txn = 0;
        }

        // maybe report performance
        if (((rowi + 1) % rows_per_report) == 0) {
            struct timeval tnow;
            r = gettimeofday(&tnow, NULL); assert(r == 0);
            float last_time = tdiff(&tnow, &tlast);
            float total_time = tdiff(&tnow, &tstart);
            printf("%ld %.3f %.0f/s %.0f/s\n", rowi + 1, last_time, rows_per_report/last_time, rowi/total_time); fflush(stdout);
            tlast = tnow;
        }
    }

    if (do_txn) {
        r = txn->commit(txn, 0); assert(r == 0);
    }
    struct timeval tnow;
    r = gettimeofday(&tnow, NULL); assert(r == 0);
    printf("total %ld %.3f %.0f/s\n", nrows, tdiff(&tnow, &tstart), nrows/tdiff(&tnow, &tstart)); fflush(stdout);
}

int main(int argc, char *argv[]) {
#if defined(TOKDUB)
    char *db_env_dir = "insertm.env.tokudb";
#else
    char *db_env_dir = "insertm.env.bdb";
#endif
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    long rows = 100000000;
    long rows_per_txn = 1000;
    long rows_per_report = 100000;
    long key_range = 100000;
    bool do_txn = true;
    u_int32_t pagesize = 0;
    u_int64_t cachesize = 1000000000;
    int ndbs = 4;
#if defined(TOKUDB)
    u_int32_t checkpoint_period = 60;
#endif

    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "--ndbs") == 0 && i+1 < argc) {
            ndbs = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows") == 0 && i+1 < argc) {
            rows = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_txn") == 0 && i+1 < argc) {
            rows_per_txn = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_report") == 0 && i+1 < argc) {
            rows_per_report = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--key_range") == 0 && i+1 < argc) {
            key_range = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--txn") == 0 && i+1 < argc) {
            do_txn = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--force_multiple") == 0 && i+1 < argc) {
            force_multiple = atoi(argv[++i]);
            continue;
        }
#if defined(TOKUDB)
        if (strcmp(arg, "--checkpoint_period") == 0 && i+1 < argc) {
            checkpoint_period = atoi(argv[++i]);
            continue;
        }
#endif

        assert(0);
    }

    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    // create and open the env
    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    if (cachesize) {
        const u_int64_t gig = 1 << 30;
        r = db_env->set_cachesize(db_env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
#if defined(TOKUDB)
    r = db_env->set_generate_row_callback_for_put(db_env, my_generate_row_for_put); assert(r == 0);
#endif
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
#if defined(TOKUDB)
    if (checkpoint_period) {
        r = db_env->checkpointing_set_period(db_env, checkpoint_period); assert(r == 0);
        u_int32_t period;
        r = db_env->checkpointing_get_period(db_env, &period); assert(r == 0 && period == checkpoint_period);
    }
#endif


    // create the db
    DB *dbs[ndbs];
    for (i = 0; i < ndbs; i++) {
        DB *db = NULL;
        r = db_create(&db, db_env, 0); assert(r == 0);
        DB_TXN *create_txn = NULL;
        if (do_txn) {
            r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
        }
        if (pagesize) {
            r = db->set_pagesize(db, pagesize); assert(r == 0);
        }
        char db_filename[32]; sprintf(db_filename, "test%d", i);
        r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

#if defined(TOKUDB)
        DESCRIPTOR_S new_descriptor;
        int index_num = htonl(i);
        new_descriptor.dbt.data = &index_num;
        new_descriptor.dbt.size = sizeof i;
        r = db->change_descriptor(db, create_txn, &new_descriptor.dbt, 0); assert(r == 0);
#else
        db->app_private = (void *) (intptr_t) i;
        if (i > 0) {
            r = dbs[0]->associate(dbs[0], create_txn, db, my_secondary_key, 0); assert(r == 0);
        }
#endif
        if (do_txn) {
            r = create_txn->commit(create_txn, 0); assert(r == 0);
        }
        dbs[i] = db;
    }

    // insert all rows
    struct table table;
    table_init(&table, ndbs, dbs, 4 * 8, 4 * 8);

    insert_all(db_env, &table, rows, rows_per_txn, key_range, rows_per_report, do_txn);

    table_destroy(&table);

    // shutdown
    for (i = 0; i < ndbs; i++) {
        DB *db = dbs[i];
        r = db->close(db, 0); assert(r == 0); db = NULL;
    }
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
