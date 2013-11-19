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
#ident "$Id$"


/* TODO:
 *
 * When ready, add simulated errors on calls to malloc()
 *
 */


/* Purpose is to verify that when a loader fails:
 *  - there are no temp files remaining
 *  - the loader-generated iname file is not present
 *
 * A loader can fail in the following ways:
 *  - user calls loader->abort()
 *  - user aborts transaction
 *  - disk full (ENOSPC)
 *  - crash (not tested in this test program)
 *
 * Mechanism:
 * This test is derived from the loader-stress-test.
 *
 * The outline of the test is as follows:
 *  - use loader to create table
 *  - verify presence of temp files
 *  - commit / abort / inject error (simulated error from system call)
 *  - verify absence of temp files
 *  - verify absence of unwanted iname files (old inames if committed, new inames if aborted)
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

enum test_type {event,                   // any event
		commit,                  // close loader, commit txn
		abort_txn,               // close loader, abort txn
		abort_loader,            // abort loader, abort txn
		abort_via_poll,          // close loader, but poll function returns non-zero, abort txn
		enospc_w,                // close loader, but close fails due to enospc return from toku_os_write
		enospc_f,                // either loader->put() or loader->close() fails due to enospc return from do_fwrite()
		enospc_p,                // loader->close() fails due to enospc return from toku_os_pwrite()
		einval_fdo,              // return einval from fdopen()
		einval_fo,               // return einval from fopen()
		einval_o,                // return einval from open()
		enospc_fc};              // return enospc from fclose()


DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=256};
#define default_NUM_DBS 5
int NUM_DBS=default_NUM_DBS;
#define default_NUM_ROWS 100000
int NUM_ROWS=default_NUM_ROWS;
//static int NUM_ROWS=50000000;
int CHECK_RESULTS=0;
int DISALLOW_PUTS=0;
int COMPRESS=0;
int event_trigger_lo=0;  // what event triggers to use?
int event_trigger_hi =0; // 0 and 0 mean none.
enum {MAGIC=311};


DBT old_inames[MAX_DBS];
DBT new_inames[MAX_DBS];

static const char *loader_temp_prefix = "tokuld"; // #2536
static int count_temp(char * dirname);
static void get_inames(DBT* inames, DB** dbs);
static int verify_file(char * dirname, char * filename);
static void assert_inames_missing(DBT* inames);
static void run_all_tests(void);
static void free_inames(DBT* inames);


// how many different system calls are intercepted with error injection
#define NUM_ERR_TYPES 7+1         // abort_via_poll does not exactly inject errors

int64_t event_count = 0;          // number of calls of all types so far (in this run)
int64_t event_count_nominal = 0;  // number of calls of all types in the nominally error-free run.
int64_t event_count_trigger = 0;  // which call will we complain about

int fwrite_count = 0;
int fwrite_count_nominal = 0;  // number of fwrite calls for normal operation, initially zero
int fwrite_count_trigger = 0;  // sequence number of fwrite call that will fail (zero disables induced failure)

int write_count = 0;
int write_count_nominal = 0;  // number of write calls for normal operation, initially zero
int write_count_trigger = 0;  // sequence number of write call that will fail (zero disables induced failure)

int pwrite_count = 0;
int pwrite_count_nominal = 0;  // number of pwrite calls for normal operation, initially zero
int pwrite_count_trigger = 0;  // sequence number of pwrite call that will fail (zero disables induced failure)

int fdopen_count = 0;
int fdopen_count_nominal = 0;  // number of fdopen calls for normal operation, initially zero
int fdopen_count_trigger = 0;  // sequence number of fdopen call that will fail (zero disables induced failure)

int fopen_count = 0;
int fopen_count_nominal = 0;  // number of fopen calls for normal operation, initially zero
int fopen_count_trigger = 0;  // sequence number of fopen call that will fail (zero disables induced failure)

int open_count = 0;
int open_count_nominal = 0;  // number of open calls for normal operation, initially zero
int open_count_trigger = 0;  // sequence number of open call that will fail (zero disables induced failure)

int fclose_count = 0;
int fclose_count_nominal = 0;  // number of fclose calls for normal operation, initially zero
int fclose_count_trigger = 0;  // sequence number of fclose call that will fail (zero disables induced failure)

int poll_count = 0;
int poll_count_nominal = 0;    // number of fclose calls for normal operation, initially zero
int poll_count_trigger = 0;    // sequence number of fclose call that will fail (zero disables induced failure)

int error_injected = 0;

static const char *
err_type_str (enum test_type t) {
    switch(t) {
    case event:          return "anyevent";
    case enospc_f:       return "fwrite";
    case enospc_w:       return "write";
    case enospc_p:       return "pwrite";
    case einval_fdo:     return "fdopen";
    case einval_fo:      return "fopen";
    case einval_o:       return "open";
    case enospc_fc:      return "fclose";
    case abort_via_poll: return "abort_via_poll";
    case commit:         assert(0);
    case abort_txn:      assert(0);
    case abort_loader:   assert(0);
    }
    // I know that Barry prefers the single-return case, but writing the code this way means that the compiler will complain if I forget something in the enum. -Bradley
    assert(0);
    return NULL;
}

static const char *
err_msg_type_str (enum test_type t) {
    switch(t) {
    case event:          return "ENOSPC/EINVAL/POLL";
    case enospc_f:       return "ENOSPC";
    case enospc_w:       return "ENOSPC";
    case enospc_p:       return "ENOSPC";
    case einval_fdo:     return "EINVAL";
    case einval_fo:      return "EINVAL";
    case einval_o:       return "EINVAL";
    case enospc_fc:      return "ENOSPC";
    case abort_via_poll: return "non-zero";
    case commit:         assert(0);
    case abort_txn:      assert(0);
    case abort_loader:   assert(0);
    }
    // I know that Barry prefers the single-return case, but writing the code this way means that the compiler will complain if I forget something in the enum. -Bradley
    assert(0);
    return NULL;
}

static size_t bad_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    fwrite_count++;
    event_count++;
    size_t r;
    if (fwrite_count_trigger == fwrite_count || event_count == event_count_trigger) {
	error_injected++;
	errno = ENOSPC;
	r = (size_t) -1;
    } else {
	r = fwrite(ptr, size, nmemb, stream);
	if (r!=nmemb) {
	    errno = ferror(stream);
	}
    }
    return r;
}


static ssize_t 
bad_write(int fd, const void * bp, size_t len) {
    ssize_t r;
    write_count++;
    event_count++;
    if (write_count_trigger == write_count || event_count == event_count_trigger) {
	error_injected++;
	errno = ENOSPC;
	r = -1;
    } else {
	r = write(fd, bp, len);
    }
    return r;
}

static ssize_t
bad_pwrite (int fd, const void *buf, size_t len, toku_off_t off) {
    int r;
    pwrite_count++;
    event_count++;
    if (pwrite_count_trigger == pwrite_count || event_count == event_count_trigger) {
	error_injected++;
	errno = ENOSPC;
	r = -1;
    } else {
	r = pwrite(fd, buf, len, off);
    }
    return r;
}



static FILE * 
bad_fdopen(int fd, const char * mode) {
    FILE * rval;
    fdopen_count++;
    event_count++;
    if (fdopen_count_trigger == fdopen_count || event_count == event_count_trigger) {
	error_injected++;
	errno = EINVAL;
	rval  = NULL;
    } else {
	rval = fdopen(fd, mode);
    }
    return rval;
}

static FILE * 
bad_fopen(const char *filename, const char *mode) {
    FILE * rval;
    fopen_count++;
    event_count++;
    if (fopen_count_trigger == fopen_count || event_count == event_count_trigger) {
	error_injected++;
	errno = EINVAL;
	rval  = NULL;
    } else {
	rval = fopen(filename, mode);
    }
    return rval;
}


static int
bad_open(const char *path, int oflag, int mode) {
    int rval;
    open_count++;
    event_count++;
    if (open_count_trigger == open_count || event_count == event_count_trigger) {
	error_injected++;
	errno = EINVAL;
	rval = -1;
    } else {
	rval = open(path, oflag, mode);
    }
    return rval;
}



static int
bad_fclose(FILE * stream) {
    int rval;
    fclose_count++;
    event_count++;
    // Must close the stream even in the "error case" because otherwise there is no way to get the memory back.
    rval = fclose(stream);
    if (rval==0) {
	if (fclose_count_trigger == fclose_count || event_count == event_count_trigger) {
            error_injected++;
	    errno = ENOSPC;
	    rval = -1;
	}
    }
    return rval;
}



///////////////


// return number of temp files
static int
count_temp(char * dirname) {
    int n = 0;
    
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if ((ent->d_type==DT_REG || ent->d_type==DT_UNKNOWN) && strncmp(ent->d_name, loader_temp_prefix, 6)==0) {
	    n++;
	    if (verbose >= 3) {
		printf("Temp files\n");
		printf("  %s/%s\n", dirname, ent->d_name);
	    } 
	}
    }
    closedir(dir);
    return n;
}



// return non-zero if file exists
static int 
verify_file(char * dirname, char * filename) {
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

static void
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
	if (verbose >= 2) printf("dname = %s, iname = %s\n", dname_str, iname_str);
    }
}


static void 
assert_inames_missing(DBT* inames) {
    int i;
    char * dir = env->i->real_data_dir;
    for (i=0; i<NUM_DBS; i++) {
	char * CAST_FROM_VOIDP(iname, inames[i].data);
	int r = verify_file(dir, iname);
	if (r) {
	    printf("File %s exists, but it should not\n", iname);
	}
	assert(r == 0);
	if (verbose) printf("File has been properly deleted: %s\n", iname);
    }
}

static 
void free_inames(DBT* inames) {
    int i;
    for (i=0; i<NUM_DBS; i++) {
	toku_free(inames[i].data);
    }
}

#if 0
static void
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

static void *expect_poll_void = &expect_poll_void;
static int poll_function (void *extra, float progress) {
    int r;
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
    event_count++;
    if (poll_count_trigger == poll_count || event_count == event_count_trigger) {
	r = 1;
    }
    else {
	r = 0;
    }
    return r;
}

static void test_loader(enum test_type t, DB **dbs, int trigger)
{
    int failed_put = 0;
    int error_injection;  // are we expecting simulated errors from system calls?
    error_injected = 0;   // number of errors actually injected

    if (t == commit        ||
	t == abort_txn     ||
	t == abort_loader  ||
	t == abort_via_poll)
	error_injection = 0;
    else
	error_injection = 1;
    

    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = DISALLOW_PUTS | COMPRESS; // set with -p/-z option

    if (verbose >= 2) 
	printf("old inames:\n");
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

    if (verbose) {
	printf("DISALLOW_PUTS = %d\n", DISALLOW_PUTS);
	printf("COMPRESS = %d\n", COMPRESS);
    }
    if (verbose >= 2) 
	printf("new inames:\n");
    get_inames(new_inames, dbs);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(int i=1;i<=NUM_ROWS && !failed_put;i++) {
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        if (DISALLOW_PUTS) {
            assert(r == EINVAL);
        } else if (r != 0) {
	    assert(error_injection && error_injected);
	    failed_put = r;
	}
        if ( CHECK_RESULTS || verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if( CHECK_RESULTS || verbose ) {printf("\n"); fflush(stdout);}        
        
    assert(poll_count == 0);  // no polling before loader->close() is called

    // You cannot count the temp files here.
    if (verbose) {
	printf("Data dir is %s\n", env->i->real_data_dir);
    }
    if (t == commit || t == abort_txn) {
	// close the loader
	if (verbose) {
	    printf("closing\n"); 
	    fflush(stdout);
	}
	r = loader->close(loader);
	CKERR(r);
	if (!DISALLOW_PUTS) {
	    assert(poll_count>0);
	    // You cannot count temp files here
	}
    }
    else if (t == abort_via_poll) {
	assert(!DISALLOW_PUTS);  // test makes no sense with DISALLOW_PUTS
	if (verbose)
	    printf("closing, but expecting abort via poll\n");
	r = loader->close(loader);
	if (r == 0) {
	    printf("loader->close() returned 0 but should have failed due to non-zero return from polling function.\n");
	    fflush(stdout);
	}
	assert(r);  // not defined what close() returns when poll function returns non-zero
    }
    else if (error_injection && !failed_put) {
	const char * type = err_type_str(t);
	r = loader->close(loader);
	if (verbose) {
	    if (error_injected)
		printf("closing, but expecting failure from simulated error (enospc or einval)%s\n", type);
	    else
		printf("closing, expecting no error because number of system calls was less than predicted (%s)\n", type);
	}
	if (!DISALLOW_PUTS && error_injected) {
	    if (r == 0) {
		printf("loader->close() returned 0 but should have failed due to injected error from %s on call %d\n",
		       err_type_str(t), trigger);
		fflush(stdout);
	    }
	    assert(r);
	}
	else
	    CKERR(r);  // if using puts, "outer" loader should close without error, if no errors injected should also close without error
    }
    else {
	if (verbose)
	    printf("aborting loader"); 
	r = loader->abort(loader);
	CKERR(r);
    }
    int n = count_temp(env->i->real_data_dir);
    if (verbose) printf("Num temp files = %d\n", n);
    fflush(stdout);
    assert(n==0);

    if (verbose)
	printf(" done\n");

    if (t == commit) {
	event_count_nominal  = event_count;
	fwrite_count_nominal = fwrite_count;  // capture how many fwrites were required for normal operation
	write_count_nominal  =  write_count;  // capture how many  writes were required for normal operation
	pwrite_count_nominal = pwrite_count;  // capture how many pwrites were required for normal operation
	fdopen_count_nominal = fdopen_count;  // capture how many fdopens were required for normal operation
	fopen_count_nominal  = fopen_count;   // capture how many fopens were required for normal operation
	open_count_nominal   = open_count;    // capture how many opens were required for normal operation
	fclose_count_nominal = fclose_count;  // capture how many fcloses were required for normal operation
	poll_count_nominal   = poll_count;    // capture how many times the polling function was called
	
	if (verbose) {
	    printf("Nominal calls:  function  calls (number of calls for normal operation)\n");
	    printf("                events    %" PRId64 "\n", event_count_nominal);
	    printf("                fwrite    %d\n", fwrite_count_nominal);
	    printf("                write     %d\n", write_count_nominal);
	    printf("                pwrite    %d\n", pwrite_count_nominal);
	    printf("                fdopen    %d\n", fdopen_count_nominal);
	    printf("                fopen     %d\n", fopen_count_nominal);
	    printf("                open      %d\n", open_count_nominal);
	    printf("                fclose    %d\n", fclose_count_nominal);
	    printf("                poll      %d\n", poll_count_nominal);
	}

	r = txn->commit(txn, 0);
	CKERR(r);
	if (!DISALLOW_PUTS) {
	    assert_inames_missing(old_inames);
	}
	if ( CHECK_RESULTS ) {
	    check_results(dbs);
	}

    }
    else {
	r = txn->abort(txn);
	CKERR(r);
	if (!DISALLOW_PUTS) {
	    assert_inames_missing(new_inames);
	}
    }
    free_inames(old_inames);
    free_inames(new_inames);
}


static int run_test_count = 0;
static const char *envdir = TOKU_TEST_FILENAME;

static void run_test(enum test_type t, int trigger) 
{
    run_test_count++;

    int r;

    if (verbose>0) {  // Don't print anything if verbose is 0.  Use "+" to indicate progress if verbose is positive
	printf("+");
	fflush(stdout);
    }

    toku_os_recursive_delete(envdir);
    r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_redzone(env, 0);                                                                             CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);

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

    generate_permute_tables();

    event_count_trigger  = event_count  = 0;
    fwrite_count_trigger = fwrite_count = 0;
    write_count_trigger  =  write_count = 0;
    pwrite_count_trigger = pwrite_count = 0;
    fdopen_count_trigger = fdopen_count = 0;
    fopen_count_trigger  = fopen_count  = 0;
    open_count_trigger   = open_count   = 0;
    fclose_count_trigger = fclose_count = 0;
    poll_count_trigger   = poll_count   = 0;

    switch(t) {
    case commit:
    case abort_txn:
    case abort_loader:
	break;
    case event:
	event_count_trigger  = trigger;      break;
    case enospc_f:
	fwrite_count_trigger = trigger;      break;
    case enospc_w:
	write_count_trigger = trigger;       break;
    case enospc_p:
	pwrite_count_trigger = trigger;      break;
    case einval_fdo:
	fdopen_count_trigger = trigger;      break;
    case einval_fo:
	fopen_count_trigger = trigger;       break;
    case einval_o:
	open_count_trigger = trigger;        break;
    case enospc_fc:
	fclose_count_trigger = trigger;      break;
    case abort_via_poll:
	poll_count_trigger  = trigger;       break;
    default:
	assert(0);
    }


    db_env_set_func_loader_fwrite(bad_fwrite);
    db_env_set_func_write(bad_write);
    db_env_set_func_pwrite(bad_pwrite);
    db_env_set_func_fdopen(bad_fdopen);
    db_env_set_func_fopen(bad_fopen);
    db_env_set_func_open(bad_open);
    db_env_set_func_fclose(bad_fclose);
	
    test_loader(t, dbs, trigger);

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    if (verbose >= 3)
	print_engine_status(env);

    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

static void run_all_tests(void) {
    int trigger;

    if (verbose) printf("\n\nTesting loader with loader close and txn commit (normal)\n");
    run_test(commit, 0);

    if (verbose) printf("\n\nTesting loader with loader abort and txn abort\n");
    run_test(abort_loader, 0);

    if (verbose) printf("\n\nTesting loader with loader close and txn abort\n");
    run_test(abort_txn, 0);

    if (event_trigger_lo || event_trigger_hi) {
	printf("\n\nDoing events %d-%d\n", event_trigger_lo, event_trigger_hi);
	for (int i=event_trigger_lo; i<=event_trigger_hi; i++) {
	    run_test(event, i);
	}
    } else {

	enum test_type et[NUM_ERR_TYPES] = {enospc_f, enospc_w, enospc_p, einval_fdo, einval_fo, einval_o, enospc_fc, abort_via_poll};
	int * nomp[NUM_ERR_TYPES] = {&fwrite_count_nominal, &write_count_nominal, &pwrite_count_nominal,
				     &fdopen_count_nominal, &fopen_count_nominal, &open_count_nominal, 
				     &fclose_count_nominal, &poll_count_nominal};
	int limit = NUM_DBS * 5;
	int j;
	for (j = 0; j<NUM_ERR_TYPES; j++) {
	    enum test_type t = et[j];
	    const char * err_type = err_type_str(t);
	    const char * err_msg_type = err_msg_type_str(t);
	    
	    int nominal = *(nomp[j]);
	    if (verbose)
		printf("\nNow test with induced %s returned from %s, nominal = %d\n", err_msg_type, err_type, nominal);
	    int i;
	    // induce write error at beginning of process
	    for (i = 1; i < limit && i < nominal+1; i++) {
		trigger = i;
		if (verbose) printf("\n\nTesting loader with %s induced at %s count %d (of %d)\n", 
				    err_msg_type, err_type, trigger, nominal);
		run_test(t, trigger);
	    }
	    if (nominal > limit)  {  // if we didn't already test every possible case
		// induce write error sprinkled through process
		for (i = 2; i < 5; i++) {
		    trigger = nominal / i;
		    if (verbose) printf("\n\nTesting loader with %s induced at %s count %d (of %d)\n", 
					err_msg_type, err_type, trigger, nominal);
		    run_test(t, trigger);
		}
		// induce write error at end of process
		for (i = 0; i < limit; i++) {
		    trigger =  nominal - i;
		    assert(trigger > 0);
		    if (verbose) printf("\n\nTesting loader with %s induced at %s count %d (of %d)\n", 
					err_msg_type, err_type, trigger, nominal);
		    run_test(t, trigger);
		}
	    }
	}
    }
}

static int test_only_abort_via_poll = 0;


int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    if (test_only_abort_via_poll) {
	printf("Testing only normal operation and abort via polling, but test abort_via_polling exhaustively.\n");
	if (verbose) {
	    print_time_now();
	    printf(": Testing loader with loader close and txn commit (normal)\n");
	}
	run_test(commit, 0);
	if (verbose) {
	    printf("\n\nTesting loader with abort_via_polling exhaustively,\n");
	    printf("returning 1 from polling function on each iteration from 1 to %d\n", poll_count_nominal);
	}
	for (int i = 1; i < poll_count_nominal+1; i++) {
	    const char * err_type = err_type_str(abort_via_poll);
	    const char * err_msg_type = err_msg_type_str(abort_via_poll);
	    if (verbose) {
		print_time_now();
		printf(": Testing loader with %s induced at %s count %d (of %d)\n", 
		       err_msg_type, err_type, i, poll_count_nominal);
		print_time_now();
	    }
	    run_test(abort_via_poll, i);
	}
	if (verbose) {
	    print_time_now();
	    printf(": Done.\n");
	}
    }
    else
	run_all_tests();
    printf("run_test_count=%d\n", run_test_count);
    return 0;
}

static void usage(const char *cmd) {
    fprintf(stderr, "Usage: -h -c -s -p -d <num_dbs> -r <num_rows> -t <elow> <ehi> \n%s\n", cmd);
    fprintf(stderr, "  where -h              print this message.\n");
    fprintf(stderr, "        -c              check the results.\n");
    fprintf(stderr, "        -p              LOADER_DISALLOW_PUTS.\n");
    fprintf(stderr, "        -z              LOADER_COMPRESS_INTERMEDIATES.\n");
    fprintf(stderr, "        -k              Test only normal operation and abort_via_poll (but thoroughly).\n");
    fprintf(stderr, "        -s              size_factor=1.\n");
    fprintf(stderr, "        -d <num_dbs>    Number of indexes to create (default=%d).\n", default_NUM_DBS);
    fprintf(stderr, "        -r <num_rows>   Number of rows to put (default=%d).\n", default_NUM_ROWS);
    fprintf(stderr, "        -t <elo> <ehi>  Instrument only events <elo> to <ehi> (default: instrument all).\n");
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
            usage(cmd); exit(resultcode);
        } else if (strcmp(argv[0], "-d")==0 && argc > 1) {
            argc--; argv++;
            NUM_DBS = atoi(argv[0]);
            if ( NUM_DBS > MAX_DBS ) {
                fprintf(stderr, "max value for -d field is %d\n", MAX_DBS);
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-r")==0 && argc > 1) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-z")==0) {
            COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
        } else if (strcmp(argv[0], "-p")==0) {
            DISALLOW_PUTS = LOADER_DISALLOW_PUTS;
	    printf("DISABLED Using puts as part of #4503\n");
        } else if (strcmp(argv[0], "-k")==0) {
	    test_only_abort_via_poll = 1;
	    printf("Perform only abort_via_poll test\n");
	} else if (strcmp(argv[0], "-t")==0 && argc > 2) {
	    argc--; argv++;
	    event_trigger_lo = atoi(argv[0]);
	    argc--; argv++;
	    event_trigger_hi = atoi(argv[0]);
	} else if (strcmp(argv[0], "-s")==0) {
	    db_env_set_loader_size_factor(1);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
