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

DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=256};
int NUM_DBS=5;
int NUM_ROWS=100000;
int CHECK_RESULTS=0;
int DISALLOW_PUTS=0;
int COMPRESS=0;
enum {MAGIC=311};

bool dup_row_at_end = false; // false: duplicate at the begining.  true: duplicate at the end.   The duplicated row is row 0.
int  dup_row_id     = 0;     // 0 means to use row 1 if inserting at the end, row NUM_ROWS if inserting at the beginning.  Otherwise insert the row specified here.

//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
int   a[MAX_DBS][32];
int inv[MAX_DBS][32];


// rotate right and left functions
static inline unsigned int rotr32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x >> n) | ( x << (32 - n));
}
static inline unsigned int rotl32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x << n) | ( x >> (32 - n));
}

static void generate_permute_tables(void) {
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
static unsigned int twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << a[db][i];
    }
    return b;
}

// permute bits of x based on inverse permute table bitmap
static unsigned int inv_twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << inv[db][i];
    }
    return b;
}

// generate val from key, index
static unsigned int generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
}
static unsigned int pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}

// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int put_multiple_generate(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    (void) src_db;

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

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}


static void check_results(DB **dbs)
{
    for(int j=0;j<NUM_DBS;j++){
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
        for(int i=0;i<NUM_ROWS;i++) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            if (DISALLOW_PUTS) {
                CKERR2(r, EINVAL);
            } else {
                CKERR(r);
                k = *(unsigned int*)key.data;
                pkey_for_db_key = (j == 0) ? k : inv_twiddle32(k, j);
                v = *(unsigned int*)val.data;
                // test that we have the expected keys and values
                assert((unsigned int)pkey_for_db_key == (unsigned int)pkey_for_val(v, j));
//            printf(" DB[%d] key = %10u, val = %10u, pkey_for_db_key = %10u, pkey_for_val=%10d\n", j, v, k, pkey_for_db_key, pkey_for_val(v, j));
            }
        }
        {printf("."); fflush(stdout);}
        r = cursor->c_close(cursor);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
    printf("\nCheck OK\n");
}

struct error_extra {
    int bad_i;
    int error_count;
};

static void error_callback (DB *db, int which_db, int err, DBT *key, DBT *val, void *extra) {
    assert(db);
    assert(extra);
    assert(err==DB_KEYEXIST);
    assert(which_db>=0);
    assert(key->size==4);
    assert(which_db==0);
    struct error_extra *e =(struct error_extra *)extra;
    assert(e->bad_i == *(int*)key->data);
    (void)val;
    assert(e->error_count==0);
    e->error_count++;
}

static void test_loader(DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    for(int i=0;i<MAX_DBS;i++) {
        db_flags[i] = DB_NOOVERWRITE;
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = DISALLOW_PUTS | COMPRESS; // set with -p option

    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    struct error_extra error_extra = {.bad_i = 0, .error_count=0};
    r = loader->set_error_callback(loader, error_callback, (void*)&error_extra);
    CKERR(r);
    r = loader->set_poll_function(loader, NULL, NULL);
    CKERR(r);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    if (!dup_row_at_end) {
	// put a duplicate row in.
	int i = dup_row_id==0 ? NUM_ROWS : dup_row_id;
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        CKERR(r);
        if ( CHECK_RESULTS || verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
	error_extra.bad_i = i;
    }
    for(int i=1;i<=NUM_ROWS;i++) {
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        if (DISALLOW_PUTS) {
            CKERR2(r, EINVAL);
        } else {
            CKERR(r);
        }
        if ( CHECK_RESULTS || verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if (dup_row_at_end) {
	// put a duplicate row in.
	int i = dup_row_id==0 ? 1 : dup_row_id;
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        CKERR(r);
        if ( CHECK_RESULTS || verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
	error_extra.bad_i = i;
    }

    if( CHECK_RESULTS || verbose ) {printf("\n"); fflush(stdout);}        
        
    // close the loader
    if (verbose) { printf("closing"); fflush(stdout); }
    r = loader->close(loader);
    if (verbose) {  printf(" done\n"); }
    if (NUM_ROWS > 0) {
        assert(r==DB_KEYEXIST);
        assert(error_extra.error_count==1);
    }

    r = txn->commit(txn, 0);
    CKERR(r);

    // verify the DBs
    if ( CHECK_RESULTS ) {
        check_results(dbs);
    }
}

char *free_me = NULL;
const char *env_dir = TOKU_TEST_FILENAME; // the default env_dir

static void run_test(void) 
{
    int r;
    toku_os_recursive_delete(env_dir);
    r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOG | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[MAX_DBS];
    for(int i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0); CKERR(chk_r); }
        });
    }

    generate_permute_tables();

    if (verbose) printf("running test_loader()\n");
    // -------------------------- //
    test_loader(dbs);
    // -------------------------- //
    if (verbose) printf("done    test_loader()\n");

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int num_rows_set = false;

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    if (num_rows_set)
	run_test();
    else {
	int sizes[]={1,4000000,-1};
        //Make PUT loader take about the same amount of time:
        if (DISALLOW_PUTS) sizes[1] /= 25;
	for (int i=0; sizes[i]>=0; i++) {
	    if (verbose) printf("Doing %d\n", sizes[i]);
	    NUM_ROWS = sizes[i];
	    run_test();
	}
    }
    if (free_me) toku_free(free_me);
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
	    fprintf(stderr, "Usage: %s -h -c -d %d -r %d\n", cmd, NUM_DBS, NUM_ROWS);
	    fprintf(stderr, " where -e <env>         uses <env> to construct the directory (so that different tests can run concurrently)\n");
	    fprintf(stderr, "       -s               use size factor of 1 (makes internal loader buffers small so certain cases are easier to test)\n");
	    fprintf(stderr, "       -E               duplicate the first row at the end (not the beginning).\n");
	    fprintf(stderr, "       -D <rid>         use row id <rid> when duplicating.  (Default is 1 if inserting at end, <numrows> if inserting at beginning\n");
	    exit(resultcode);
	} else if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-d")==0) {
            argc--; argv++;
            NUM_DBS = atoi(argv[0]);
            if ( NUM_DBS > MAX_DBS ) {
                fprintf(stderr, "max value for -d field is %d\n", MAX_DBS);
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
	    num_rows_set = true;
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-z")==0) {
            COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
        } else if (strcmp(argv[0], "-p")==0) {
            DISALLOW_PUTS = LOADER_DISALLOW_PUTS;
        } else if (strcmp(argv[0], "-s")==0) {
	    db_env_set_loader_size_factor(1);            
	} else if (strcmp(argv[0], "-E")==0) {
	    dup_row_at_end = true;
	} else if (strcmp(argv[0], "-D")==0) {
            argc--; argv++;
	    dup_row_id = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
    assert(0<=dup_row_id && dup_row_id<=NUM_ROWS);
}
