/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <db.h>
#include <portability/toku_atomic.h>

#include "test.h"
#include "threaded_stress_test_helpers.h"

//
// This test tries to emulate iibench at the ydb layer.
//
// The schema is simple:
// 8 byte primary key
// 8 byte key A
// 8 byte key B
// 8 byte key C
//
// There's one primary DB for the pk and three secondary DBs.
//
// The primary key stores the other columns as the value.
// The secondary keys have the primary key appended to them.
//

static const size_t iibench_secondary_key_size = 16;

struct iibench_row {
    uint64_t pk;
    int64_t a;
    int64_t b;
    int64_t c;
};

struct iibench_secondary_row {
    int64_t column;
    uint64_t pk;
};

static int64_t hash(uint64_t key) {
    uint64_t hash = 0;
    uint8_t *buf = (uint8_t *) &key;
    for (int i = 0; i < 8; i++) {
        hash += (((buf[i] + 1) * 17) & 0xFF) << (i * 8);
    }
    return hash;
}

static int64_t iibench_generate_column_by_pk(int64_t pk, int db_idx) {
    invariant(db_idx > 0);
    return hash(pk * db_idx);
}

static void iibench_generate_row(int64_t pk, struct iibench_row *row) {
    row->a = iibench_generate_column_by_pk(pk, 1);
    row->b = iibench_generate_column_by_pk(pk, 2);
    row->c = iibench_generate_column_by_pk(pk, 3);
}

static void iibench_parse_row(const DBT *key, const DBT *val, struct iibench_row *row) {
    char *CAST_FROM_VOIDP(val_buf, val->data);
    invariant(key->size == 8);
    invariant(val->size == 24);
    memcpy(&row->pk, key->data, 8);
    memcpy(&row->a, val_buf + 0, 8);
    memcpy(&row->b, val_buf + 8, 8);
    memcpy(&row->c, val_buf + 16, 8);
}

static void UU() iibench_verify_row(const struct iibench_row *row) {
    struct iibench_row expected_row;
    iibench_generate_row(row->pk, &expected_row);
    invariant(row->a == expected_row.a);
    invariant(row->b == expected_row.b);
    invariant(row->c == expected_row.c);
}

static void iibench_parse_secondary_row(const DBT *key, const DBT *val, struct iibench_secondary_row *row) {
    char *CAST_FROM_VOIDP(key_buf, key->data);
    invariant(key->size == iibench_secondary_key_size);
    invariant(val->size == 0);
    memcpy(&row->column, key_buf + 0, 8);
    memcpy(&row->pk, key_buf + 8, 8);
}

static void UU() iibench_verify_secondary_row(const struct iibench_secondary_row *row, int db_idx) {
    int64_t expected = iibench_generate_column_by_pk(row->pk, db_idx);
    invariant(row->column == expected);
}

static void iibench_fill_key_buf(uint64_t pk, int64_t *buf) {
    memcpy(&buf[0], &pk, 8);
}

static void iibench_fill_val_buf(uint64_t pk, int64_t *buf) {
    struct iibench_row row;
    iibench_generate_row(pk, &row);
    memcpy(&buf[0], &row.a, sizeof(row.a));
    memcpy(&buf[1], &row.b, sizeof(row.b));
    memcpy(&buf[2], &row.c, sizeof(row.c));
}

static int iibench_get_db_idx(DB *db) {
    DESCRIPTOR desc = db->cmp_descriptor;
    invariant_notnull(desc->dbt.data);
    invariant(desc->dbt.size == sizeof(int));
    int db_idx;
    memcpy(&db_idx, desc->dbt.data, desc->dbt.size);
    return db_idx;
}

static void iibench_rangequery_cb(DB *db, const DBT *key, const DBT *val, void *extra) {
    invariant_null(extra);
    const int db_idx = iibench_get_db_idx(db);
    if (db_idx == 0) {
        struct iibench_row row;
        iibench_parse_row(key, val, &row);
        iibench_verify_row(&row);
    } else {
        struct iibench_secondary_row row;
        iibench_parse_secondary_row(key, val, &row);
        iibench_verify_secondary_row(&row, db_idx);
    }
}

struct iibench_put_op_extra {
    uint64_t autoincrement;
};

static int UU() iibench_put_op(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra) {
    const int num_dbs = arg->cli->num_DBs;
    DB **dbs = arg->dbp;
    DB_ENV *env = arg->env;
    DBT_ARRAY mult_key_dbt[num_dbs];
    DBT_ARRAY mult_val_dbt[num_dbs];
    uint32_t mult_put_flags[num_dbs];

    // The first index is unique with serial autoincrement keys.
    // The rest are have keys generated with this thread's random data.
    mult_put_flags[0] = get_put_flags(arg->cli) |
        // If the table was already created, don't check for uniqueness.
        (arg->cli->num_elements > 0 ? 0 : DB_NOOVERWRITE);
    for (int i = 0; i < num_dbs; i++) {
        toku_dbt_array_init(&mult_key_dbt[i], 1);
        toku_dbt_array_init(&mult_val_dbt[i], 1);
        mult_put_flags[i] = get_put_flags(arg->cli);
    }
    mult_key_dbt[0].dbts[0].flags = 0;
    mult_val_dbt[0].dbts[0].flags = 0;

    int r = 0;

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        struct iibench_put_op_extra *CAST_FROM_VOIDP(info, operation_extra);

        // Get a random primary key, generate secondary key columns in valbuf
        uint64_t pk = toku_sync_fetch_and_add(&info->autoincrement, 1);
        if (arg->bounded_element_range && arg->cli->num_elements > 0) {
            pk = pk % arg->cli->num_elements;
        }
        int64_t keybuf[1];
        int64_t valbuf[3];
        iibench_fill_key_buf(pk, keybuf);
        iibench_fill_val_buf(pk, valbuf);
        dbt_init(&mult_key_dbt[0].dbts[0], keybuf, sizeof keybuf);
        dbt_init(&mult_val_dbt[0].dbts[0], valbuf, sizeof valbuf);

        r = env->put_multiple(
            env, 
            dbs[0], // source db.
            txn, 
            &mult_key_dbt[0].dbts[0], // source db key
            &mult_val_dbt[0].dbts[0], // source db value
            num_dbs, // total number of dbs
            dbs, // array of dbs
            mult_key_dbt, // array of keys
            mult_val_dbt, // array of values
            mult_put_flags // array of flags
            );
        if (r != 0) {
            goto cleanup;
        }
        puts_to_increment++;
        if (puts_to_increment == 100) {
            increment_counter(stats_extra, PUTS, puts_to_increment);
            puts_to_increment = 0;
        }
    }

cleanup:
    for (int i = 0; i < num_dbs; i++) {
        toku_dbt_array_destroy(&mult_key_dbt[i]);
        toku_dbt_array_destroy(&mult_val_dbt[i]);
    }
    return r;
}

static int iibench_generate_row_for_put(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *UU(src_key), const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    invariant(src_db != dest_db);
    // 8 byte primary key, REALLOC secondary key
    invariant_notnull(src_key->data);
    invariant(src_key->size == 8);
    invariant(dest_key->flags == DB_DBT_REALLOC);
    // Expand the secondary key data buffer if necessary
    if (dest_key->size != iibench_secondary_key_size) {
        dest_key->data = toku_xrealloc(dest_key->data, iibench_secondary_key_size);
        dest_key->size = iibench_secondary_key_size;
    }

    // Get the db index from the descriptor. This is a secondary index
    // so it has to be greater than zero (which would be the pk). Then
    // grab the appropriate secondary key from the source val, which is
    // an array of the 3 columns, so we have to subtract 1 from the index.
    const int db_idx = iibench_get_db_idx(dest_db);
    int64_t *CAST_FROM_VOIDP(columns, src_val->data);
    int64_t secondary_key = columns[db_idx - 1];

    // First write down the secondary key, then the primary key (in src_key)
    int64_t *CAST_FROM_VOIDP(dest_key_buf, dest_key->data);
    memcpy(&dest_key_buf[0], &secondary_key, sizeof(secondary_key));
    memcpy(&dest_key_buf[1], src_key->data, src_key->size);
    dest_val->data = nullptr;
    dest_val->size = 0;
    return 0;
}

// After each DB opens, set the descriptor to store the DB idx value.
// Close and reopen the DB so we can use db->cmp_descriptor during comparisons.
static DB *iibench_set_descriptor_after_db_opens(DB_ENV *env, DB *db, int idx, reopen_db_fn reopen, struct cli_args *cli_args) {
    int r;
    DBT desc_dbt;
    desc_dbt.data = &idx;
    desc_dbt.size = sizeof(idx);
    desc_dbt.ulen = 0;
    desc_dbt.flags = 0;
    r = db->change_descriptor(db, nullptr, &desc_dbt, 0); CKERR(r);
    r = db->close(db, 0); CKERR(r);
    r = db_create(&db, env, 0); CKERR(r);
    reopen(db, idx, cli_args);
    return db;
}

static int iibench_compare_keys(DB *db, const DBT *a, const DBT *b) {
    const int db_idx = iibench_get_db_idx(db);
    if (db_idx == 0) {
        invariant(a->size == 8);
        invariant(b->size == 8);
        uint64_t x = *(uint64_t *) a->data;
        uint64_t y = *(uint64_t *) b->data;
        if (x < y) {
            return -1;
        } else if (x == y) {
            return 0;
        } else {
            return 1;
        }
    } else {
        invariant(a->size == 16);
        invariant(b->size == 16);
        int64_t x = *(int64_t *) a->data;
        int64_t y = *(int64_t *) b->data;
        uint64_t pk_x = *(uint64_t *) (((char *) a->data) + 8);
        uint64_t pk_y = *(uint64_t *) (((char *) b->data) + 8);
        if (x < y) {
            return -1;
        } else if (x == y) {
            if (pk_x < pk_y) {
                return -1;
            } else if (pk_x == pk_y) {
                return 0;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
    }
}

static void iibench_rangequery_db(DB *db, DB_TXN *txn, ARG arg, uint64_t max_pk) {
    const int limit = arg->cli->range_query_limit;

    int r;
    DBC *cursor;

    // Get a random key no greater than max pk
    DBT start_key, end_key;
    uint64_t start_k = myrandom_r(arg->random_data) % (max_pk + 1);
    uint64_t end_k = start_k + limit;
    dbt_init(&start_key, &start_k, 8);
    dbt_init(&end_key, &end_k, 8);

    r = db->cursor(db, txn, &cursor, 0); CKERR(r);
    r = cursor->c_set_bounds(cursor, &start_key, &end_key, true, 0); CKERR(r);
    struct rangequery_cb_extra extra = {
        .rows_read = 0,
        .limit = limit,
        .cb = iibench_rangequery_cb,
        .db = db,
        .cb_extra = nullptr,
    };
    r = cursor->c_getf_set(cursor, 0, &start_key, rangequery_cb, &extra);
    while (r == 0 && extra.rows_read < extra.limit && run_test) {
        r = cursor->c_getf_next(cursor, 0, rangequery_cb, &extra);
    }

    r = cursor->c_close(cursor); CKERR(r);
}

// Do a range query over the primary index, verifying the contents of the rows
static int iibench_rangequery_op(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra) {
    struct iibench_put_op_extra *CAST_FROM_VOIDP(info, operation_extra);
    DB *db = arg->dbp[0];

    // Assume the max PK is the table size. If it isn't specified, do a
    // safe read of the current autoincrement key from the put thread.
    uint64_t max_pk = arg->cli->num_elements;
    if (max_pk == 0) {
        max_pk = toku_sync_fetch_and_add(&info->autoincrement, 0);
    }
    iibench_rangequery_db(db, txn, arg, max_pk);
    increment_counter(stats_extra, PTQUERIES, 1);
    return 0;
}

static int iibench_fill_tables(DB_ENV *env, DB **dbs, struct cli_args *cli_args, bool UU(fill_with_zeroes)) {
    const int num_dbs = cli_args->num_DBs;
    int r = 0;

    DB_TXN *txn;
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

    DB_LOADER *loader;
    uint32_t db_flags[num_dbs];
    uint32_t dbt_flags[num_dbs];
    for (int i = 0; i < num_dbs; i++) {
        db_flags[i] = DB_PRELOCKED_WRITE;
        dbt_flags[i] = DB_DBT_REALLOC;
    }

    r = env->create_loader(env, txn, &loader, dbs[0], num_dbs, dbs, db_flags, dbt_flags, 0); CKERR(r);
    for (int i = 0; i < cli_args->num_elements; i++) {
        DBT key, val;
        uint64_t pk = i;
        int64_t keybuf[1];
        int64_t valbuf[3];
        iibench_fill_key_buf(pk, keybuf);
        iibench_fill_val_buf(pk, valbuf);
        dbt_init(&key, keybuf, sizeof keybuf);
        dbt_init(&val, valbuf, sizeof valbuf);
        r = loader->put(loader, &key, &val); CKERR(r);
        if (verbose && i > 0 && i % 10000 == 0) {
            report_overall_fill_table_progress(cli_args, 10000);
        }
    }
    r = loader->close(loader); CKERR(r);

    r = txn->commit(txn, 0); CKERR(r);
    return 0;
}

static void
stress_table(DB_ENV* env, DB **dbs, struct cli_args *cli_args) {
    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_put_threads + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];

    // Put threads do iibench-like inserts with an auto-increment primary key
    // Query threads do range queries of a certain size, verifying row contents.

    struct iibench_put_op_extra put_extra = {
        .autoincrement = 0
    };
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbs, env, cli_args);
        if (i < cli_args->num_put_threads) {
            myargs[i].operation = iibench_put_op;
            myargs[i].operation_extra = &put_extra;
        } else {
            myargs[i].operation = iibench_rangequery_op;
            myargs[i].operation_extra = &put_extra;
            myargs[i].txn_flags |= DB_TXN_READ_ONLY;
            myargs[i].sleep_ms = 1000; // 1 second between range queries
        }
    }
    const bool crash_at_end = false;
    run_workers(myargs, num_threads, cli_args->num_seconds, crash_at_end, cli_args);
}

int test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    args.num_elements = 0;  // want to start with empty DBs
    // Puts per transaction is configurable. It defaults to 1k.
    args.txn_size = 1000;
    // Default to one writer on 4 indexes (pk + 3 secondaries), no readers.
    args.num_DBs = 4;
    args.num_put_threads = 1;
    args.num_ptquery_threads = 0;
    parse_stress_test_args(argc, argv, &args);
    // The schema is not configurable. Silently ignore whatever was passed in.
    args.key_size = 8;
    args.val_size = 32;
    // when there are multiple threads, its valid for two of them to
    // generate the same key and one of them fail with DB_LOCK_NOTGRANTED
    if (args.num_put_threads > 1) {
        args.crash_on_operation_failure = false;
    }
    args.env_args.generate_put_callback = iibench_generate_row_for_put;
    after_db_open_hook = iibench_set_descriptor_after_db_opens;
    fill_tables = iibench_fill_tables;
    perf_test_main_with_cmp(&args, iibench_compare_keys);
    return 0;
}
