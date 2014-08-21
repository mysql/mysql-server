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
enum {MAX_DBS=16};
enum {MAX_ROW_LEN=1024};
static int NUM_DBS=10;
static int DISALLOW_PUTS=0;
static int COMPRESS=0;
static int USE_REGION=0;
static const char *envdir = TOKU_TEST_FILENAME;

static int generate_rows_for_region(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) __attribute__((unused)); 
static int generate_rows_for_lineitem(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) __attribute__((unused));

// linenumber,orderkey form a unique, primary key
// key is a potentially duplicate secondary key
struct tpch_key {
    uint32_t linenumber; 
    uint32_t orderkey;
    uint32_t key;
};

static __attribute__((__unused__)) int
tpch_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
    assert(db && a && b);
    assert(a->size == sizeof(struct tpch_key));
    assert(b->size == sizeof(struct tpch_key));

    unsigned int xl = (*((struct tpch_key *) a->data)).linenumber;
    unsigned int xo = (*((struct tpch_key *) a->data)).orderkey;
    unsigned int xk = (*((struct tpch_key *) a->data)).key;

    unsigned int yl = (*((struct tpch_key *) b->data)).linenumber;
    unsigned int yo = (*((struct tpch_key *) b->data)).orderkey;
    unsigned int yk = (*((struct tpch_key *) b->data)).key;

//    printf("tpch_dbt_cmp xl:%d, yl:%d, xo:%d, yo:%d, xk:%d, yk:%d\n", xl, yl, xo, yo, xk, yk);

    if (xk<yk) return -1;
    if (xk>yk) return 1;

    if (xl<yl) return -1;
    if (xl>yl) return 1;

    if (xo>yo) return -1;
    if (xo<yo) return 1;
    return 0;
}


static int lineno = 0;
static char *tpch_read_row(FILE *fp, int *key, char *val)
{
    *key = lineno++;
    return fgets(val, MAX_ROW_LEN , fp);
}


/*
 *   split '|' separated fields into fields array
 */
static void tpch_parse_row(char *row, char *fields[], int fields_N) 
{
    int field = 0;
    int i = 0;
    int p = 0;
    char c = row[p];

    while(c != '\0')
    {
        if ( c == '|') {
            fields[field][i] = '\0';
            //printf("field : <%s>\n", fields[field]);
            field++;
            i = 0;
        }
        else 
            fields[field][i++] = c;
        c = row[++p];
    }
    assert(field == fields_N);
}

/*
 *     region table
 */

static int generate_rows_for_region(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) 
{
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    // not used
    (void) src_db;
    (void) src_key;
    assert(*(uint32_t*)dest_db->app_private == 0);

    // region fields
    char regionkey[8];
    char name[32];
    char comment[160];
    char row[8+32+160+8];
    sprintf(row, "%s", (char*)src_val->data);

    const uint32_t fields_N = 3;
    char *fields[3] = {regionkey, name, comment};
    tpch_parse_row(row, fields, fields_N);

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
    
    struct tpch_key *XMALLOC(key);
    key->orderkey   = atoi(regionkey);
    key->linenumber = atoi(regionkey);
    key->key        = atoi(regionkey);

    char *XMALLOC_N(sizeof(row), val);
    sprintf(val, "%s|%s", name, comment);

    dbt_init(dest_key, key, sizeof(struct tpch_key));
    dest_key->flags = DB_DBT_REALLOC;

    dbt_init(dest_val, val, strlen(val)+1);
    dest_val->flags = DB_DBT_REALLOC;

    return 0;
}

/*
 *      lineitem table
 */


static int generate_rows_for_lineitem(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) 
{
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];
    // not used
    (void) src_db;
    (void) src_key;

    // lineitem fields
    char orderkey[16];   
    char partkey[16];
    char suppkey[16];
    char linenumber[8];
    char quantity[8];
    char extendedprice[16];
    char discount[8];
    char tax[8];
    char returnflag[8];
    char linestatus[8];
    char shipdate[16];
    char commitdate[16];
    char receiptdate[16];
    char shipinstruct[32];
    char shipmode[16];
    char comment[48];
    char row[16+16+16+8+8+16+8+8+8+8+16+16+16+32+16+48 + 8];
    sprintf(row, "%s", (char*)src_val->data);

    const uint32_t fields_N = 16;
    char *fields[16] = {orderkey,
                        partkey,
                        suppkey,
                        linenumber,
                        quantity,
                        extendedprice,
                        discount,
                        tax,
                        returnflag,
                        linestatus,
                        shipdate,
                        commitdate,
                        receiptdate,
                        shipinstruct,
                        shipmode,
                        comment};
    tpch_parse_row(row, fields, fields_N);

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
    
    struct tpch_key *XMALLOC(key);
    key->orderkey   = atoi(linenumber);
    key->linenumber = atoi(orderkey);
    
    char *val;
    uint32_t which = *(uint32_t*)dest_db->app_private;

    if ( which == 0 ) {
        val = toku_xstrdup(row);
    }
    else {
        val = toku_xstrdup(orderkey);
    }            
    
    switch(which) {
    case 0:
        key->key = atoi(linenumber);
        break;
    case 1:
        // lineitem_fk1
        key->key = atoi(orderkey);
        break;
    case 2:
        // lineitem_fk2
        key->key = atoi(suppkey);
        break;
    case 3:
        // lineitem_fk3
        key->key = atoi(partkey);// not really, ...
        break;
    case 4:
        // lineitem_fk4
        key->key = atoi(partkey);
        break;
    case 5:
        // li_shp_dt_idx
        key->key = atoi(linenumber) + atoi(suppkey); // not really ...
        break;
    case 6:
        key->key = atoi(linenumber) +atoi(partkey); // not really ...
        break;
    case 7:
        // li_rcpt_dt_idx
        key->key = atoi(suppkey) + atoi(partkey); // not really ...
        break;
    default:
        assert(0);
    }

    dbt_init(dest_key, key, sizeof(struct tpch_key));
    dest_key->flags = DB_DBT_REALLOC;

    dbt_init(dest_val, val, strlen(val)+1);
    dest_val->flags = DB_DBT_REALLOC;
        
    return 0;
}


static void *expect_poll_void = &expect_poll_void;
static int poll_count=0;
static int poll_function (void *extra, float progress) {
    if (0) {
	static int did_one=0;
	static struct timeval start;
	struct timeval now;
	gettimeofday(&now, 0);
	if (!did_one) {
	    start=now;
	    did_one=1;
	}
	printf("%6.6f %5.1f%%\n", now.tv_sec - start.tv_sec + 1e-6*(now.tv_usec - start.tv_usec), progress*100);
    }
    assert(extra==expect_poll_void);
    assert(0.0<=progress && progress<=1.0);
    poll_count++;
    return 0;
}

static int test_loader(DB **dbs)
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

    FILE *fp;
    // select which table to loader
    if ( USE_REGION ) {
        fp = fopen("./region.tbl", "r");    
        if (fp == NULL) {
            fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
            return 1;
        }
        assert(fp != NULL);
    } else {
        fp = fopen("./lineitem.tbl", "r");  
        if (fp == NULL) {
            fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
            return 1;
        }
        assert(fp != NULL);
    }

    // create and initialize loader

    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    r = loader->set_error_callback(loader, NULL, NULL);
    CKERR(r);
    r = loader->set_poll_function(loader, poll_function, expect_poll_void);
    CKERR(r);

    // using loader->put, put values into DB
    printf("puts "); fflush(stdout);
    DBT key, val;
    int k;
    char v[MAX_ROW_LEN];
    char *c;
    c = tpch_read_row(fp, &k, v);
    int i = 1;
    while ( c != NULL ) {
        v[strlen(v)-1] = '\0';  // remove trailing \n
        dbt_init(&key, &k, sizeof(int));
        dbt_init(&val, v, strlen(v)+1);
        r = loader->put(loader, &key, &val);
        if (DISALLOW_PUTS) {
            CKERR2(r, EINVAL);
        } else {
            CKERR(r);
        }
        if (verbose) { if((i++%10000) == 0){printf("."); fflush(stdout);} }
        c = tpch_read_row(fp, &k, v);
    }
    if(verbose) {printf("\n"); fflush(stdout);}        
    fclose(fp);
        
    poll_count=0;

    // close the loader
    printf("closing"); fflush(stdout);
    r = loader->close(loader);
    printf(" done\n");
    CKERR(r);

    if ( DISALLOW_PUTS == 0 ) assert(poll_count>0);

    r = txn->commit(txn, 0);
    CKERR(r);

    return 0;
}

static int run_test(void) 
{
    int r;
    char rmcmd[32 + strlen(envdir)];
    snprintf(rmcmd, sizeof rmcmd, "rm -rf %s", envdir);
    r = system(rmcmd);                                                                             CKERR(r);
    r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    db_env_enable_engine_status(0);  // disable engine status on crash because test is expected to fail
    r = env->set_default_bt_compare(env, tpch_dbt_cmp);                                                       CKERR(r);
    // select which TPC-H table to load
    if ( USE_REGION ) {
        r = env->set_generate_row_callback_for_put(env, generate_rows_for_region);                            CKERR(r);
        NUM_DBS=1;
    }
    else {
        r = env->set_generate_row_callback_for_put(env, generate_rows_for_lineitem);                          CKERR(r);
        NUM_DBS=8;
    }

    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, envdir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
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

    // -------------------------- //
    int testr = test_loader(dbs);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);

    return testr;
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    int r = run_test();
    return r;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage: -h -p -g\n%s\n", cmd);
	    exit(resultcode);
        } else if (strcmp(argv[0], "-p")==0) {
            DISALLOW_PUTS = LOADER_DISALLOW_PUTS;
        } else if (strcmp(argv[0], "-z")==0) {
            COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
        } else if (strcmp(argv[0], "-g")==0) {
            USE_REGION = 1;
        } else if (strcmp(argv[0], "-e") == 0) {
            argc--; argv++;
            if (argc > 0)
                envdir = argv[0];
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
