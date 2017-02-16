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


/* NOTE:
 *
 * Someday figure out a better way to verify inames that should not be 
 * in data dir after recovery.  Currently, they are just hard-coded in
 * the new_iname_str[] array.  This will break when something changes,
 * such as the xid of the transaction that creates the loader.
 */


/* Purpose is to verify that when a loader crashes:
 *  - there are no temp files remaining
 *  - the loader-generated iname file is not present
 *
 * In the event of a crash, the verification of no temp files and 
 * no loader-generated iname file is done after recovery.
 *
 * Mechanism:
 * This test is derived from loader-cleanup-test, which was derived from loader-stress-test.
 *
 * The outline of the test is as follows:
 *  - use loader to create table
 *  - verify presence of temp files
 *  - crash
 *  - recover
 *  - verify absence of temp files
 *  - verify absence of unwanted iname files (new inames) - how?
 *
 *  
 */


#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <dirent.h>

#include "ydb-internal.h"

static const int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;

#define NUM_DBS 5

static bool do_test=false, do_recover=false;

static DB_ENV *env;
static int NUM_ROWS=50000000;
static int COMPRESS=0;

enum {MAX_NAME=128};
enum {MAGIC=311};

static DBT old_inames[NUM_DBS];
static DBT new_inames[NUM_DBS];


static char const * const new_iname_str[NUM_DBS] = {"qo_0000_35_c_L_0.tokudb",
						    "qo_0001_35_c_L_1.tokudb",
						    "qo_0002_35_c_L_2.tokudb",
						    "qo_0003_35_c_L_3.tokudb",
						    "qo_0004_35_c_L_4.tokudb"};

static const char *loader_temp_prefix = "tokuld"; // 2536
static int count_temp(char * dirname);
static void get_inames(DBT* inames, DB** dbs);
static int verify_file(char const * const dirname, char const * const filename);
static int print_dir(char * dirname);

// return number of temp files
int
count_temp(char * dirname) {
    int n = 0;
    
    DIR * dir = opendir(dirname);

    struct dirent *ent;
    while ((ent = readdir(dir)))
	if ((ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) && strncmp(ent->d_name, loader_temp_prefix, 6) == 0)
	    n++;
    closedir(dir);
    return n;
}

// print contents of directory
int
print_dir(char * dirname) {
    int n = 0;

    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent = readdir(dir))) {
	if (ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) {
	    n++;
	    printf("File: %s\n", ent->d_name);
	}
    }
    closedir(dir);
    return n;
}



// return non-zero if file exists
int 
verify_file(char const * const dirname, char const * const filename) {
    int n = 0;
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if ((ent->d_type==DT_REG || ent->d_type==DT_UNKNOWN) && strcmp(ent->d_name, filename)==0) {
	    n++;
	}
    }
    closedir(dir);
    return n;
}

void
get_inames(DBT* inames, DB** dbs) {
    int i;
    for (i = 0; i < NUM_DBS; i++) {
	DBT dname;
	char * dname_str = dbs[i]->i->dname;
	dbt_init(&dname, dname_str, strlen(dname_str)+1);
	dbt_init(&(inames[i]), NULL, 0);
	inames[i].flags |= DB_DBT_MALLOC;
	int r = env->get_iname(env, &dname, &inames[i]);
	CKERR(r);
	char * iname_str = (char*) (inames[i].data);
	//	if (verbose) 
	    printf("dname = %s, iname = %s\n", dname_str, iname_str);
    }
}


#if 0
void print_inames(DB** dbs);
void
print_inames(DB** dbs) {
    int i;
    for (i = 0; i < NUM_DBS; i++) {
	DBT dname;
	DBT iname;
	char * dname_str = dbs[i]->i->dname;
	dbt_init(&dname, dname_str, sizeof(dname_str));
	dbt_init(&iname, NULL, 0);
	iname.flags |= DB_DBT_MALLOC;
	int r = env->get_iname(env, &dname, &iname);
	CKERR(r);
	char * iname_str = (char*)iname.data;
	if (verbose) printf("dname = %s, iname = %s\n", dname_str, iname_str);
	int n = verify_file(env->i->real_data_dir, iname_str);
	assert(n == 1);
	toku_free(iname.data);
    }
}
#endif


//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
int   a[NUM_DBS][32];
int inv[NUM_DBS][32];


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
    for(int db=0;db<NUM_DBS;db++) {
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

// generate val from key, index
static unsigned int generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
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


static void *expect_poll_void = &expect_poll_void;

static int poll_function (void *UU(extra), float UU(progress)) {
    toku_hard_crash_on_purpose();
    return -1;
}

static void test_loader(DB **dbs)
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
    uint32_t loader_flags = COMPRESS; // set with -p option

    int n = count_temp(env->i->real_data_dir);
    assert(n == 0);  // Must be no temp files before loader is run
    
    if (verbose) printf("old inames:\n");
    get_inames(old_inames, dbs);
    
    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    r = loader->set_error_callback(loader, NULL, NULL);
    CKERR(r);
    r = loader->set_poll_function(loader, poll_function, expect_poll_void);
    CKERR(r);

    printf("COMPRESS = %d\n", COMPRESS);
    if (verbose) printf("new inames:\n");
    get_inames(new_inames, dbs);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(int i=1;i<=NUM_ROWS;i++) {
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
	CKERR(r);
        if (verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if( verbose) {printf("\n"); fflush(stdout);}        
        
    printf("Data dir is %s\n", env->i->real_data_dir);
    n = count_temp(env->i->real_data_dir);
    printf("Num temp files = %d\n", n);
    assert(n);  // test is useless unless at least one temp file is created
    if (verbose) {
	printf("Contents of data dir:\n");
	print_dir(env->i->real_data_dir);
    }
    printf("closing, will crash\n"); fflush(stdout);
    r = loader->close(loader);
    printf("Should never return from loader->close()\n");  fflush(stdout);
    assert(0);

}


static void run_test(void)
{
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
//    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[NUM_DBS];
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

    test_loader(dbs);
    printf("Should never return from test_loader\n");  fflush(stdout);
    assert(0);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);



static void run_recover (void) {
    int i;

    // Recovery starts from oldest_living_txn, which is older than any inserts done in run_test,
    // so recovery always runs over the entire log.

    // run recovery
    int r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r); 

    // now verify contents of data_dir, should be no temp files, no loader-created iname files
    if (verbose)
	print_dir(env->i->real_data_dir);

    int n = count_temp(env->i->real_data_dir);
    printf("Num temp files = %d\n", n);
    assert(n==0);  // There should be no temp files remaining after recovery

    for (i = 0; i < NUM_DBS; i++) {
	char const * const iname = new_iname_str[i];
	r = verify_file(env->i->real_data_dir, iname);
	if (r) {
	    printf("File %s exists, but it should not\n", iname);
	}
	assert(r == 0);
	if (verbose) 
	    printf("File has been properly deleted: %s\n", iname);	
    }

    r = env->close(env, 0);                                                             CKERR(r);
    exit(0);

}

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);

    if (do_test) {
	printf("\n\n perform test, crash\n");
	fflush(stdout);
	run_test();
    }
    else if (do_recover) {
	printf("\n\n perform recovery\n");
	run_recover();
    }
    else {
	printf("\n\n BOGUS!\n");
	assert(0);
    }

    return 0;
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
	    fprintf(stderr, "Usage: -h -c -d <num_dbs> -r <num_rows>\n%s\n", cmd);
	    exit(resultcode);
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-z")==0) {
            COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
	    printf("Compressing\n");
	} else if (strcmp(argv[0], "--test")==0) {
	    do_test=true;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=true;

	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
