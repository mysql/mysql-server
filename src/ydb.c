/* -*- mode: C; c-basic-offset: 4 -*- */

#include <assert.h>
#include <brt.h>
#include "ydb-internal.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cachetable.h"
#include "log.h"
#include "memory.h"

static inline void *malloc_zero(size_t size) {
    void *vp = toku_malloc(size);
    if (vp)
        memset(vp, 0, size);
    return vp;
}

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    TOKUTXN tokutxn;
};

void __toku_db_env_err (const DB_ENV *env __attribute__((__unused__)), int error, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "YDB Error %d:", error);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

#define barf() ({ fprintf(stderr, "YDB: BARF %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define barff(fmt,...) ({ fprintf(stderr, "YDB: BARF %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })
#define note() ({ fprintf(stderr, "YDB: Note %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define notef(fmt,...) ({ fprintf(stderr, "YDB: Note %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })

void print_flags (u_int32_t flags) {
  u_int32_t gotit=0;
  int doneone=0;
#define doit(flag) if (flag & flags) { if (doneone) fprintf(stderr, " | "); fprintf(stderr, "%s", #flag);  doneone=1; gotit|=flag; }
  printf(" flags=");
  doit(DB_INIT_LOCK);
  doit(DB_INIT_LOG);
  doit(DB_INIT_MPOOL);
  doit(DB_INIT_TXN);
  doit(DB_CREATE);
  doit(DB_THREAD);
  doit(DB_RECOVER);
  doit(DB_PRIVATE);
  if (gotit!=flags) printf("  flags 0x%x not accounted for", flags&~gotit);
  fprintf(stderr, "\n");
}

struct __toku_db_env_internal {
    u_int32_t open_flags;
    int       open_mode;
    void (*errcall)(const char *, char *);
    const char *errpfx;
    char *dir; /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *data_dir;
    //void (*noticecall)(DB_ENV *, db_notices);
    int n_files;
    int files_array_limit; // How big is *files ?
    struct ydb_file **files;
    CACHETABLE cachetable;
    TOKULOGGER logger;
};

int  __toku_db_env_open (DB_ENV *env, const char *home, u_int32_t flags, int mode) {
    int r;
    notef("(%p, \"%s\", 0x%x, 0%o)\n", env, home, flags, mode);
    env->i->dir = strdup(home);  assert(env->i->dir!=0);
    env->i->open_flags = flags;
    env->i->open_mode  = mode;
    
    print_flags(flags);
    assert(DB_PRIVATE & flags); // This means that we don't have to do anything with shared memory.  And that's good enough for mysql. 
    
    r = brt_create_cachetable(&env->i->cachetable, 32);
    assert(r==0);

    if (flags & (DB_INIT_TXN | DB_INIT_LOG)) {
	r = tokulogger_create_and_open_logger(env->i->dir, &env->i->logger);
    }

    return 0;
}

int  __toku_db_env_close (DB_ENV * env, u_int32_t flags) {
    cachetable_close(&env->i->cachetable);
    if (env->i->logger) {
	tokulogger_log_close(&env->i->logger);
    }
    toku_free(env->i->dir);
    toku_free(env->i->files);
    toku_free(env->i);
    toku_free(env);
    return 0;
}
int  __toku_db_env_log_archive (DB_ENV *env, char **list[], u_int32_t flags) {
    *list = NULL;
    return 0;
}
int  __toku_db_env_log_flush (DB_ENV * env, const DB_LSN * lsn) {
  barf();
  return 1;
}
int  __toku_db_env_set_cachesize (DB_ENV * env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    fprintf(stderr, "%s:%d set_cachsize %d %d %d\n", __FILE__, __LINE__, gbytes, bytes, ncache);
    return 1;
}
int  __toku_db_env_set_data_dir (DB_ENV * env, const char *dir) {
    env->i->data_dir = strdup(dir);
    return 1;
}
void __toku_db_env_set_errcall (DB_ENV *env, void (*errcall)(const char *, char *)) {
  env->i->errcall=errcall;
}
void __toku_db_env_set_errpfx (DB_ENV * env, const char *errpfx) {
    env->i->errpfx = strdup(errpfx);
}
int  __toku_db_env_set_flags (DB_ENV *env, u_int32_t flags, int onoff) {
    assert(flags==0);
    return 1;
}
int  __toku_db_env_set_lg_bsize (DB_ENV * env, u_int32_t bsize) {
    barff("lg_bsize=%d\n", bsize);
    return 1;
}
int  __toku_db_env_set_lg_dir (DB_ENV * env, const char * dir) {
  barf();
  return 1;
}
int  __toku_db_env_set_lg_max (DB_ENV *env, u_int32_t lg_max) {
    barff("lg_max=%d\n", lg_max);
    return 1;
}
int  __toku_db_env_set_lk_detect (DB_ENV *env, u_int32_t detect) {
    barff("detect=%d\n", detect);
    return 1;
}
int  __toku_db_env_set_lk_max (DB_ENV *env, u_int32_t lk_max) {
    barff("lk_max=%d\n", lk_max);
    return 0;
}
//void __toku_db_env_set_noticecall (DB_ENV *env, void (*noticecall)(DB_ENV *, db_notices)) {
//    env->i->noticecall = noticecall;
//}
int  __toku_db_env_set_tmp_dir (DB_ENV * env, const char *tmp_dir) {
    env->i->tmp_dir = strdup(tmp_dir);
    return 0;
}
int  __toku_db_env_set_verbose (DB_ENV *env, u_int32_t which, int onoff) {
  barf();
  return 1;
}
int  __toku_db_env_txn_checkpoint (DB_ENV *env, u_int32_t kbyte, u_int32_t min, u_int32_t flags) {
  return 0;
}

int  __toku_db_env_txn_stat (DB_ENV *env, DB_TXN_STAT **statp, u_int32_t flags) {
  barf();
  return 1;
}

void __toku_default_errcall(const char *errpfx, char *msg) {
  fprintf(stderr, "YDB: %s: %s", errpfx, msg);
}

int txn_begin (DB_ENV *env, DB_TXN *stxn, DB_TXN **txn, u_int32_t flags);

int db_env_create (DB_ENV **envp, u_int32_t flags) {
  DB_ENV *result=malloc_zero(sizeof(*result));
  fprintf(stderr, "%s:%d db_env_create flags=%d, returning %p\n", __FILE__, __LINE__, flags, result);
  result->err = __toku_db_env_err;
  result->open = __toku_db_env_open;
  result->close = __toku_db_env_close;
  result->txn_checkpoint = __toku_db_env_txn_checkpoint;
  result->log_flush = __toku_db_env_log_flush;
  result->set_errcall = __toku_db_env_set_errcall;
  result->set_errpfx = __toku_db_env_set_errpfx;
  //result->set_noticecall = __toku_db_env_set_noticecall;
  result->set_flags = __toku_db_env_set_flags;
  result->set_data_dir = __toku_db_env_set_data_dir;
  result->set_tmp_dir = __toku_db_env_set_tmp_dir;
  result->set_verbose = __toku_db_env_set_verbose;
  result->set_lg_bsize = __toku_db_env_set_lg_bsize;
  result->set_lg_dir = __toku_db_env_set_lg_dir;
  result->set_lg_max = __toku_db_env_set_lg_max;
  result->set_cachesize = __toku_db_env_set_cachesize;
  result->set_lk_detect = __toku_db_env_set_lk_detect;
  result->set_lk_max = __toku_db_env_set_lk_max;
  result->log_archive = __toku_db_env_log_archive;
  result->txn_stat = __toku_db_env_txn_stat;
  result->txn_begin = txn_begin;

  result->i = malloc_zero(sizeof(*result->i));
  result->i->dir = 0;
  //result->i->noticecall = 0;
  result->i->tmp_dir = 0;

  result->i->errcall =  __toku_default_errcall;
  result->i->errpfx  =  ""; 

  result->i->n_files = 0;
  result->i->files_array_limit = 4;
  result->i->files = malloc_zero(result->i->files_array_limit * sizeof (*result->i->files));
  *envp = result;
  return 0;
}


int __toku_db_txn_commit (DB_TXN *txn, u_int32_t flags) {
  //notef("flags=%d\n", flags);
  if (!txn) return -1;
  int r = tokulogger_log_commit(txn->i->tokutxn);
  if (r!=0) return r;
  if (txn->i) toku_free(txn->i);
  toku_free(txn);
  return 0;
}

u_int32_t __toku_db_txn_id (DB_TXN *txn) {
  barf();
  abort();
}

static TXNID next_txn=0;

int txn_begin (DB_ENV *env, DB_TXN *stxn, DB_TXN **txn, u_int32_t flags) {
  DB_TXN *result = malloc_zero(sizeof(*result));
  //notef("parent=%p flags=0x%x\n", stxn, flags);
  result->commit = __toku_db_txn_commit;
  result->id     = __toku_db_txn_id;
  result->i      = malloc(sizeof(*result->i));
  int r = tokutxn_begin(&result->i->tokutxn, next_txn++, env->i->logger);
  if (r!=0) return r;
  *txn = result;
  return 0;
}

int txn_abort (DB_TXN *txn) {
  fprintf(stderr, "txn_abort(%p)\n", txn);
  abort();
}

#if 0
int txn_commit (DB_TXN *txn, u_int32_t flags) {
    printf("%s:%d\n", __FILE__, __LINE__);
    return tokulogger_log_commit(txn->i->tokutxn);
}
#endif

int log_compare (const DB_LSN *a, const DB_LSN *b) {
  fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
  abort();
}

int __toku_db_close (DB *db, u_int32_t flags) {
    int r = 0;
    if (db->i->brt) {
	r = close_brt(db->i->brt);
    }
    printf("%s:%d %d=__toku_db_close(%p)\n", __FILE__, __LINE__, r, db);
    db->i->freed = 1;
    toku_free(db->i->database_name);
    toku_free(db->i->full_fname);
    toku_free(db->i);
    toku_free(db);
    return r;
}

struct __toku_dbc_internal {
    BRT_CURSOR c;
    DB *db;
    DB_TXN *txn;
};
			  
int __toku_c_get (DBC *c, DBT *key, DBT *data, u_int32_t flag) {
    int r = brt_cursor_get(c->i->c, key, data, flag, c->i->db, c->i->txn->i->tokutxn);
    return r;
}

int __toku_c_close (DBC *c) {
    int r = brt_cursor_close(c->i->c);
    printf("%s:%d %d=__toku_c_close(%p)\n", __FILE__, __LINE__, r, c);
    toku_free(c->i);
    toku_free(c);
    return r;
}

int __toku_c_del (DBC *c, u_int32_t flags) {
    int r = brt_cursor_delete(c->i->c, flags);
    return r;
}

int __toku_db_cursor (DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    DBC *result=malloc_zero(sizeof(*result));
    int r;
    assert(result);
    result->c_get = __toku_c_get;
    result->c_close = __toku_c_close;
    result->c_del = __toku_c_del;
    result->i = malloc_zero(sizeof(*result->i));
    result->i->db = db;
    result->i->txn = txn;
    r = brt_cursor(db->i->brt, &result->i->c);
    assert(r==0);
    *c = result;
    return 0;
}

int  __toku_db_del (DB *db, DB_TXN *txn __attribute__((unused)), DBT *key, u_int32_t flags __attribute((unused))) {
    int r = brt_delete(db->i->brt, key, db);
    return r;
}
  
int  __toku_db_get (DB *db, DB_TXN *txn __attribute__((unused)), DBT *key, DBT *data, u_int32_t flags) {
    assert(flags == 0);
    int r = brt_lookup(db->i->brt, key, data, db);
    return r;
}

int  __toku_db_key_range (DB *db, DB_TXN *txn, DBT *dbt, DB_KEY_RANGE *kr, u_int32_t flags) {
  barf();
  abort();
}

char *construct_full_name (const char *dir, const char *fname) {
    if (fname[0]=='/')
	dir = "";
    {
	int dirlen = strlen(dir);
	int fnamelen = strlen(fname);
	int len = dirlen+fnamelen+2; // One for the / between (which may not be there).  One for the trailing null.
	char *result = toku_malloc(len);
	int l;
	printf("%s:%d len(%d)=%d+%d+2\n", __FILE__, __LINE__, len, dirlen, fnamelen);
	assert(result);
	l=snprintf(result, len, "%s", dir);
	if (l==0 || result[l-1]!='/') {
	    /* Didn't put a slash down. */
	    if (fname[0]!='/') {
		result[l++]='/';
		result[l]=0;
	    }
	}
	l+=snprintf(result+l, len-l, "%s", fname);
	return result;
    }
}

// The decision to embedded subdatabases in files is a little bit painful.
// My original design was to simply create another file, but it turns out that we 
//  have to inherit mode bits and so forth from the first file that was created.
// Other problems may ensue (who is responsible for deleting the file?  That's not so bad actually.)
// This suggests that we really need to put the multiple databases into one file.
int  __toku_db_open (DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    // Warning.  Should check arguments.  Should check return codes on malloc and open and so forth.

    int openflags=0;
    int r;
    notef("txn=%p fname=%s dbname=%s dbtype=%d flags=0x%x mode=0%o\n", txn, fname, dbname, dbtype, flags, mode);
    print_flags(flags);
    if (db->i->full_fname) return -1; /* It was already open. */
    db->i->full_fname = construct_full_name(db->i->env->i->dir, fname);
    printf("Full name = %s\n", db->i->full_fname);
    db->i->database_name = strdup(dbname ? dbname : "");

    if (flags&DB_RDONLY) openflags |= O_RDONLY;
    else openflags |= O_RDWR;

    if (flags&DB_CREATE) openflags |= O_CREAT;

    {
	struct stat statbuf;
	if (stat(db->i->full_fname, &statbuf)==0) {
	    /* If the database exists at the file level, and we specified no db_name, then complain here. */
	    if (dbname==0 && (flags&DB_CREATE)) return EEXIST;
	} else {
	    if (!(flags&DB_CREATE)) return ENOENT;
	}
    }

    db->i->open_flags = flags;
    db->i->open_mode = mode;
    // Warning:  new_brt has deficienceis:
    //  Each tree has its own cache, instead of a big shared cache.
    //  It doesn't do error checking on insert.
    //  It's tough to do cursors.
    r=open_brt(db->i->full_fname, dbname, (flags&DB_CREATE), &db->i->brt, 1<<20, db->i->env->i->cachetable,
	       db->i->bt_compare);
    assert(r==0);
    return 0;
}

int  __toku_db_put (DB *db, DB_TXN *txn, DBT *key, DBT *data, u_int32_t flags) {
    int r = brt_insert(db->i->brt, key, data, db, txn ? txn->i->tokutxn : 0);
    //printf("%s:%d %d=__toku_db_put(...)\n", __FILE__, __LINE__, r);
    return r;
}
int  __toku_db_remove (DB *db, const char *fname, const char *dbname, u_int32_t flags) {
    int r;
    char ffull[PATH_MAX];
    assert(dbname==0);
    r = snprintf(ffull, PATH_MAX, "%s%s", db->i->env->i->dir, fname);     assert(r<PATH_MAX);
    return unlink(ffull);
}
int  __toku_db_rename (DB *db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
    char afull[PATH_MAX], cfull[PATH_MAX];
    int r;
    assert(nameb==0);
    r = snprintf(afull, PATH_MAX, "%s%s", db->i->env->i->dir, namea);     assert(r<PATH_MAX);
    r = snprintf(cfull, PATH_MAX, "%s%s", db->i->env->i->dir, namec);     assert(r<PATH_MAX);
    return rename(afull, cfull);
}
int  __toku_db_set_bt_compare (DB *db, int (*bt_compare)(DB *, const DBT *, const DBT *)) {
  note();
  db->i->bt_compare = bt_compare;
  return 0;
}
int  __toku_db_set_flags (DB *db, u_int32_t flags) {
    if (flags&DB_DUP) {
	printf("Set DB_DUP\n");
	if (db->i->brt && !db->i->is_db_dup) {
	    printf("Already Not DB_DUP\n");
	    return -1;
	}
	db->i->is_db_dup=1;
	flags&=~DB_DUP;
    }
    assert(flags==0);
    return 0;
}
int __toku_db_stat (DB *db, void *v, u_int32_t flags) {
  barf();
  abort();
}

extern int default_compare_fun(DB *db, const DBT *a, const DBT *b);

int db_create (DB **db, DB_ENV *env, u_int32_t flags) {
  DB *result=malloc_zero(sizeof(*result));
  fprintf(stderr, "%s:%d db_create(%p, %p, 0x%x)\n", __FILE__, __LINE__, db, env, flags);
  print_flags(flags);
  result->app_private = 0;
  result->close = __toku_db_close;
  result->cursor = __toku_db_cursor;
  result->del = __toku_db_del;
  result->get = __toku_db_get;
  result->key_range = __toku_db_key_range;
  result->open = __toku_db_open;
  result->put = __toku_db_put;
  result->remove = __toku_db_remove;
  result->rename = __toku_db_rename;
  result->set_bt_compare = __toku_db_set_bt_compare;
  result->set_flags = __toku_db_set_flags;
  result->stat = __toku_db_stat;
  result->i = malloc_zero(sizeof(*result->i));
  result->i->freed = 0;
  result->i->bt_compare = default_compare_fun;
  result->i->header = 0;
  result->i->database_number = 0;
  result->i->env = env;
  result->i->full_fname = 0;
  result->i->database_name = 0;
  result->i->open_flags = 0;
  result->i->open_mode = 0;
  result->i->brt = 0;
  result->i->is_db_dup = 0;
  *db = result;
  return 0;
}

char *db_strerror (int error) {

    if (error == 0) return "Success: 0";
    if (error > 0) {
	char *result = strerror(error);
	if (result) return result;
    unknown:
	{
	    static char unknown_result[100]; // Race condition if two threads call this at the same time.    However even in a bad case, it should be some sort of nul-terminated string.
	    snprintf(unknown_result, 100, "Unknown error code: %d", error);
	    return unknown_result;
	}
    }
    switch (error) {
    default:
	goto unknown;
    }
}

const char *db_version (int *major, int *minor, int *patch) {
    if (major) *major=DB_VERSION_MAJOR;
    if (minor) *minor=DB_VERSION_MINOR;
    if (patch) *patch=DB_VERSION_PATCH;
    return DB_VERSION_STRING;
}
