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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "toku_random.h"

bool fast = false;

DB_ENV *env;
enum {NUM_DBS=2};
uint32_t USE_COMPRESS=0;

bool do_check = false;
uint32_t num_rows = 1;
uint32_t which_db_to_fail = (uint32_t) -1;
uint32_t which_row_to_fail = (uint32_t) -1;
enum how_to_fail { FAIL_NONE, FAIL_KSIZE, FAIL_VSIZE } how_to_fail = FAIL_NONE;

static struct random_data random_data[NUM_DBS];
char random_buf[NUM_DBS][8];

static int put_multiple_generate(DB *dest_db,
				 DB *src_db __attribute__((__unused__)),
				 DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals,
				 const DBT *src_key, const DBT *src_val __attribute__((__unused__))) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    uint32_t which = *(uint32_t*)dest_db->app_private;
    assert(src_key->size==4);
    uint32_t rownum = *(uint32_t*)src_key->data;

    uint32_t ksize, vsize;
    const uint32_t kmax=32*1024, vmax=32*1024*1024;
    if (which==which_db_to_fail && rownum==which_row_to_fail) {
	switch (how_to_fail) {
	case FAIL_NONE:  ksize=kmax;   vsize=vmax;   goto gotsize;
	case FAIL_KSIZE: ksize=kmax+1; vsize=vmax;   goto gotsize;
	case FAIL_VSIZE: ksize=kmax;   vsize=vmax+1; goto gotsize;
	}
	assert(0);
    gotsize:;
    } else {
	ksize=4; vsize=100;
    }
    assert(dest_key->flags==DB_DBT_REALLOC);
    if (dest_key->ulen < ksize) {
	dest_key->data = toku_xrealloc(dest_key->data, ksize);
	dest_key->ulen = ksize;
    }
    assert(dest_val->flags==DB_DBT_REALLOC);
    if (dest_val->ulen < vsize) {
	dest_val->data = toku_xrealloc(dest_val->data, vsize);
	dest_val->ulen = vsize;
    }
    assert(ksize>=sizeof(uint32_t));
    for (uint32_t i=0; i<ksize; i++) ((char*)dest_key->data)[i] = myrandom_r(&random_data[which]);
    for (uint32_t i=0; i<vsize; i++) ((char*)dest_val->data)[i] = myrandom_r(&random_data[which]);
    *(uint32_t*)dest_key->data = rownum;
    dest_key->size = ksize;
    dest_val->size = vsize;

    return 0;
}

struct error_extra {
    int bad_i;
    int error_count;
};

static void error_callback (DB *db __attribute__((__unused__)), int which_db, int err, DBT *key __attribute__((__unused__)), DBT *val __attribute__((__unused__)), void *extra) {
    struct error_extra *e =(struct error_extra *)extra;
    assert(which_db==(int)which_db_to_fail);
    assert(err==EINVAL);
    assert(e->error_count==0);
    e->error_count++;
}

static void reset_random(void) {
    int r;

    for (int i = 0; i < NUM_DBS; i++) {
        ZERO_STRUCT(random_data[i]);
        ZERO_ARRAY(random_buf[i]);
        r = myinitstate_r(i, random_buf[i], 8, &random_data[i]);
        assert(r==0);
    }
}

static void test_loader_maxsize(DB **dbs, DB **check_dbs)
{
    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[NUM_DBS];
    uint32_t dbt_flags[NUM_DBS];
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = USE_COMPRESS; // set with -p option

    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
    r = env->create_loader(env, txn, &loader, nullptr, NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    assert(which_db_to_fail != 0);
    CKERR(r);
    struct error_extra error_extra = {.bad_i=0,.error_count=0};
    r = loader->set_error_callback(loader, error_callback, (void*)&error_extra);
    CKERR(r);
    r = loader->set_poll_function(loader, NULL, NULL);
    CKERR(r);

    reset_random();
    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(uint32_t i=0;i<num_rows;i++) {
        k = i;
        v = i;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        CKERR(r);
    }

    // close the loader
    if (verbose) { printf("closing"); fflush(stdout); }
    r = loader->close(loader);
    if (verbose) {  printf(" done\n"); }
    switch(how_to_fail) {
    case FAIL_NONE:  assert(r==0);      assert(error_extra.error_count==0); goto checked;
    case FAIL_KSIZE: assert(r==EINVAL); assert(error_extra.error_count==1); goto checked;
    case FAIL_VSIZE: assert(r==EINVAL); assert(error_extra.error_count==1); goto checked;
    }
    assert(0);
 checked:
    r = txn->commit(txn, 0);
    CKERR(r);

    if (do_check && how_to_fail==FAIL_NONE) {
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);
        reset_random();
        DBT keys[NUM_DBS];
        DBT vals[NUM_DBS];
        uint32_t flags[NUM_DBS];
        for (int i = 0; i < NUM_DBS; i++) {
            dbt_init_realloc(&keys[i]);
            dbt_init_realloc(&vals[i]);
            flags[i] = 0;
        }

        for(uint32_t i=0;i<num_rows;i++) {
            k = i;
            v = i;
            dbt_init(&key, &k, sizeof(unsigned int));
            dbt_init(&val, &v, sizeof(unsigned int));
            r = env_put_multiple_test_no_array(env, nullptr, txn, &key, &val, NUM_DBS, check_dbs, keys, vals, flags);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
        CKERR(r);
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        for (int i = 0; i < NUM_DBS; i++) {
            DBC *loader_cursor;
            DBC *check_cursor;
            r = dbs[i]->cursor(dbs[i], txn, &loader_cursor, 0);
            CKERR(r);
            r = dbs[i]->cursor(check_dbs[i], txn, &check_cursor, 0);
            CKERR(r);
            DBT loader_key;
            DBT loader_val;
            DBT check_key;
            DBT check_val;
            dbt_init_realloc(&loader_key);
            dbt_init_realloc(&loader_val);
            dbt_init_realloc(&check_key);
            dbt_init_realloc(&check_val);
            for (uint32_t x = 0; x <= num_rows; x++) {
                int r_loader = loader_cursor->c_get(loader_cursor, &loader_key, &loader_val, DB_NEXT);
                int r_check = check_cursor->c_get(check_cursor, &check_key, &check_val, DB_NEXT);
                assert(r_loader == r_check);
                if (x == num_rows) {
                    CKERR2(r_loader, DB_NOTFOUND);
                    CKERR2(r_check, DB_NOTFOUND);
                } else {
                    CKERR(r_loader);
                    CKERR(r_check);
                }
                assert(loader_key.size == check_key.size);
                assert(loader_val.size == check_val.size);
                assert(memcmp(loader_key.data, check_key.data, loader_key.size) == 0);
                assert(memcmp(loader_val.data, check_val.data, loader_val.size) == 0);
            }
            toku_free(loader_key.data);
            toku_free(loader_val.data);
            toku_free(check_key.data);
            toku_free(check_val.data);
            loader_cursor->c_close(loader_cursor);
            check_cursor->c_close(check_cursor);
        }

        for (int i = 0; i < NUM_DBS; i++) {
            toku_free(keys[i].data);
            toku_free(vals[i].data);
            dbt_init_realloc(&keys[i]);
            dbt_init_realloc(&vals[i]);
        }
        r = txn->commit(txn, 0);
        CKERR(r);
    }


}

char *free_me = NULL;
const char *env_dir = TOKU_TEST_FILENAME; // the default env_dir

static void create_and_open_dbs(DB **dbs, const char *suffix, int *idx) {
    int r;
    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    enum {MAX_NAME=128};
    char name[MAX_NAME*2];

    for(int i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x_%s", i, suffix);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0); CKERR(chk_r); }
        });
    }
}

static int
uint_or_size_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  if (a->size == sizeof(unsigned int) && b->size == sizeof(unsigned int)) {
      return uint_dbt_cmp(db, a, b);
  }
  return a->size - b->size;
}

static void run_test(uint32_t nr, uint32_t wdb, uint32_t wrow, enum how_to_fail htf) {
    num_rows = nr; which_db_to_fail = wdb; which_row_to_fail = wrow; how_to_fail = htf;

    int r;
    toku_os_recursive_delete(env_dir);
    r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_or_size_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOG | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DB **XMALLOC_N(NUM_DBS, dbs);
    DB **XMALLOC_N(NUM_DBS, check_dbs);
    int idx[NUM_DBS];

    create_and_open_dbs(dbs, "loader", &idx[0]);
    if (do_check && how_to_fail==FAIL_NONE) {
        create_and_open_dbs(check_dbs, "check", &idx[0]);
    }

    if (verbose) printf("running test_loader()\n");
    // -------------------------- //
    test_loader_maxsize(dbs, check_dbs);
    // -------------------------- //
    if (verbose) printf("done    test_loader()\n");

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
        if (do_check && how_to_fail==FAIL_NONE) {
            check_dbs[i]->close(check_dbs[i], 0);                                                                 CKERR(r);
            check_dbs[i] = NULL;
        }
    }
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
    toku_free(check_dbs);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int num_rows_set = false;

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);

    run_test(1, (uint32_t) -1, (uint32_t) -1, FAIL_NONE);
    run_test(1,  1,  0, FAIL_NONE);
    run_test(1,  1,  0, FAIL_KSIZE);
    run_test(1,  1,  0, FAIL_VSIZE);
    if (!fast) {
	run_test(1000000, 1, 500000, FAIL_KSIZE);
	run_test(1000000, 1, 500000, FAIL_VSIZE);
    }
    toku_free(free_me);
    return 0;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0], "-h")==0) {
            resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage: %s [-h] [-v] [-q] [-p] [-f]\n", cmd);
	    fprintf(stderr, " where -e <env>         uses <env> to construct the directory (so that different tests can run concurrently)\n");
	    fprintf(stderr, "       -h               help\n");
	    fprintf(stderr, "       -v               verbose\n");
	    fprintf(stderr, "       -q               quiet\n");
	    fprintf(stderr, "       -z               compress intermediates\n");
	    fprintf(stderr, "       -c               compare with regular dbs\n");
	    fprintf(stderr, "       -f               fast (suitable for vgrind)\n");
	    exit(resultcode);
	} else if (strcmp(argv[0], "-c")==0) {
	    do_check = true;
	} else if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-z")==0) {
            USE_COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
        } else if (strcmp(argv[0], "-f")==0) {
	    fast     = true;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
