/* Wrapper for bdb.c. */

#include <sys/types.h>
/* This include is to the berkeley-db compiled with --with-uniquename */
#include <db.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <toku_assert.h>
#include <sys/time.h>
#include <time.h>

#undef db_env_create

#define barf() ({ fprintf(stderr, "YDB: BARF %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define barff(fmt,...) ({ fprintf(stderr, "YDB: BARF %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })
#define note() ({ fprintf(stderr, "YDB: Note %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define notef(fmt,...) ({ fprintf(stderr, "YDB: Note %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })

static char *tracefname = "/home/bradley/ydbtrace.c";
static FILE *traceout=0;
unsigned long long objnum=1;
void tracef (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void tracef (const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (traceout==0) {
	struct timeval tv;
	char *ctimes;
	gettimeofday(&tv, 0);
	ctimes = ctime(&tv.tv_sec);
	ctimes[strlen(ctimes)-1]=0;
	traceout=fopen(tracefname, "a");
	assert(traceout);
	fprintf(stderr, "traceout created\n");
	fprintf(traceout, "/* bdbw trace captured %s (%ld.%06ld) */\n",
		ctimes, tv.tv_sec, tv.tv_usec);
    }
    vfprintf(traceout, fmt, ap);
    fflush(traceout);
    va_end(ap);
}

struct wrap_db_env_internal {
    unsigned long long objnum;
    DB_ENV *env;
    //void (*noticecall)(DB_ENV_ydb*, db_notices_ydb);
    char *home;
};

static void wrap_env_err (const DB_ENV *, int error, const char *fmt, ...);
static int  wrap_env_open (DB_ENV *, const char *home, u_int32_t flags, int mode);
static int  wrap_env_close (DB_ENV *, u_int32_t flags);
static int  wrap_env_txn_checkpoint (DB_ENV *, u_int32_t kbyte, u_int32_t min, u_int32_t flags);
static int  wrap_db_env_log_flush (DB_ENV*, const DB_LSN*lsn);


int db_env_create (DB_ENV **envp, u_int32_t flags) {
    DB_ENV *result = malloc(sizeof(*result));
    struct wrap_db_env_internal *i = malloc(sizeof(*i));
    int r;
    //note();
    // use a private field for this
    result->reginfo = i;

    i->objnum = objnum++;
    //i->noticecall = 0;
    i->home = 0;

    result->err            = wrap_env_err;
    result->open           = wrap_env_open;
    result->close          = wrap_env_close;
    result->txn_checkpoint = wrap_env_txn_checkpoint;
    result->log_flush      = wrap_env_log_flush;
    result->set_errcall = ydb_env_set_errcall;
    result->set_errpfx = ydb_env_set_errpfx;
    result->set_noticecall = ydb_env_set_noticecall;
    result->set_flags = ydb_env_set_flags;
    result->set_data_dir = ydb_env_set_data_dir;
    result->set_tmp_dir = ydb_env_set_tmp_dir;
    result->set_verbose = ydb_env_set_verbose;
    result->set_lg_bsize = ydb_env_set_lg_bsize;
    result->set_lg_dir = ydb_env_set_lg_dir;
    result->set_lg_max = ydb_env_set_lg_max;
    result->set_cachesize = ydb_env_set_cachesize;
    result->set_lk_detect = ydb_env_set_lk_detect;
    result->set_lk_max = ydb_env_set_lk_max;
    result->log_archive = ydb_env_log_archive;
    result->txn_stat = ydb_env_txn_stat;
    result->txn_begin = txn_begin_bdbw;

    r = db_env_create_4001(&result->i->env, flags);
    result->i->env->app_private = result;
    *envp = result;

    tracef("r=db_env_create(new_envobj(%lld), %u); assert(r==%d);\n",
	   result->i->objnum, flags, r);

    return r;
}

struct __toku_db_txn_internal {
    long long objnum;
    DB_TXN *txn;
};

static void wrap_env_err (const DB_ENV *env, int error, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "YDB Error %d:", error);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

#define doit(flag) ({ if (flag ## _ydb & flags) { gotit|=flag; flags&=~flag ## _ydb; } })

void doits_internal (u_int32_t flag_ydb, u_int32_t flag_bdb, char *flagname, u_int32_t *flags_ydb, u_int32_t *flags_bdb, char **flagstring, int *flagstringlen) {
    if (flag_ydb & *flags_ydb) {
	int len = strlen(flagname);
	*flags_bdb |=  flag_bdb;
	*flags_ydb &= ~flag_ydb;
	assert(len + 2 < *flagstringlen);
	snprintf(*flagstring, *flagstringlen, "|%s", flagname);
	*flagstring += len+1;
	*flagstringlen -= len+1;
    }
}

//#define doits(flag) doits_internal(flag ## _ydb, flag, #flag, &flags, &gotit, &flagstring, &flagstringlen)
#define doits(flag) doits_internal(flag, flag, #flag, &flags, &gotit, &flagstring, &flagstringlen)


static u_int32_t convert_envopen_flags(u_int32_t flags, char *flagstring, int flagstringlen) {
  u_int32_t gotit=0;
  snprintf(flagstring, flagstringlen, "0"); flagstringlen--; flagstring++;
  doits(DB_INIT_LOCK);
  doits(DB_INIT_LOG);
  doits(DB_INIT_MPOOL);
  doits(DB_INIT_TXN);
  doits(DB_CREATE);
  doits(DB_THREAD);
  doits(DB_RECOVER);
  doits(DB_PRIVATE);
  assert(flags==0);
  return gotit;
}

static u_int32_t open_flags_ydb_2_bdb (u_int32_t flags, char *flagstring, int flagstringlen) {
    u_int32_t gotit=0;
    snprintf(flagstring, flagstringlen, "0"); flagstringlen--; flagstring++;
    doits(DB_CREATE);
    doits(DB_RDONLY);
    doits(DB_RECOVER);
    doits(DB_THREAD);
    assert(flags==0);
    return gotit;
}


u_int32_t convert_db_create_flags(u_int32_t flags) {
    if (flags==0) return 0;
    abort();
}

u_int32_t convert_db_set_flags (u_int32_t flags, char *flagstring, int flagstringlen) {
    u_int32_t gotit=0;
    snprintf(flagstring, flagstringlen, "0"); flagstringlen--; flagstring++;
    doits(DB_DUP);
    assert(flags==0);
    return gotit;
}

//#define retit(flag) ({ if (flag ## _ydb == flags) { strncpy(flagstring, #flag ,flagstringlen); return flag; } })
#define retit(flag) ({ if (flag == flags) { strncpy(flagstring, #flag ,flagstringlen); return flag; } })

u_int32_t convert_c_get_flags(u_int32_t flags, char *flagstring, int flagstringlen) {
    retit(DB_FIRST);
    retit(DB_LAST);
    retit(DB_NEXT);
    abort();
}

int  wrap_env_open (DB_ENV *env_w, const char *home, u_int32_t flags, int mode) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    int r;
    char flagstring[1000];
    u_int32_t bdb_flags = convert_envopen_flags(flags, flagstring, sizeof(flagstring));
    //note();
    r = env->i->env->open(env->i->env, home, bdb_flags, mode);
    env->i->home = strdup(home);
    tracef("r = envobj(%lld)->open(envobj(%lld), \"%s\", %s, 0%o); assert(r==%d);\n",
	   env->i->objnum, env->i->objnum, home, flagstring, mode, r);
    return r;
}

int  bdbw_env_close (DB_ENV * env_w, u_int32_t flags) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    int r;
    notef("flags=%d\n", flags);
    assert(flags==0);
    r = env->i->env->close(env->i->env, 0);
    env->i->env=0;
    // free(env);
    return r;
}

u_int32_t convert_log_archive_flags (u_int32_t flags, char *flagstring, int flagstringlen) {
    retit(DB_ARCH_ABS);
    retit(DB_ARCH_LOG);
    abort();
}

int  ydb_env_log_archive (DB_ENV *env_w, char **list[], u_int32_t flags) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    int r;
    char flagstring[1000];
    int bdbflags = convert_log_archive_flags(flags, flagstring, sizeof(flagstring));
    r = env->i->env->log_archive(env->i->env, list, bdbflags);
    assert(r==0);
    tracef("{ char **list; r = envobj(%lld)->log_archive(envobj(%lld), &list, %s); assert(r==%d); }\n",
	   env->i->objnum, env->i->objnum, flagstring, r);
    return r;
}
int  ydb_env_log_flush (DB_ENV *env, const DB_LSN *lsn) {
  barf();
  return 1;
}
int  ydb_env_set_cachesize (DB_ENV * env_w, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_cachesize(env->i->env, gbytes, bytes, ncache);
}
int  ydb_env_set_data_dir (DB_ENV* env_w, const char *dir) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_data_dir(env->i->env, dir);
}
void ydb_env_set_errcall (DB_ENV *env_w, void (*errcall)(const char *, char *)) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    env->i->env->set_errcall(env->i->env, errcall);
}
void ydb_env_set_errpfx (DB_ENV * env_w, const char *errpfx) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    env->i->env->set_errpfx(env->i->env, errpfx);
}
int  ydb_env_set_flags (DB_ENV *env_w, u_int32_t flags, int onoff) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    assert(flags==0);
    return env->i->env->set_flags(env->i->env, flags, onoff);
}
int  ydb_env_set_lg_bsize (DB_ENV * env_w, u_int32_t bsize) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_lg_bsize(env->i->env, bsize);
}
int  ydb_env_set_lg_dir (DB_ENV *env_w, const char * dir) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    barf();
    return 1;
}
int  ydb_env_set_lg_max (DB_ENV *env_w, u_int32_t lg_max) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_lg_max(env->i->env, lg_max);
}
int  ydb_env_set_lk_detect (DB_ENV *env_w, u_int32_t detect) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_lk_detect(env->i->env, detect);
}
int  ydb_env_set_lk_max (DB_ENV *env_w, u_int32_t lk_max) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    return env->i->env->set_lk_max(env->i->env, lk_max);
}
#if 0
void ydbenv_bdb_noticecall (DB_ENV *bdb_env, db_notices notices) {
    DB_ENV_ydb *ydb_env = bdb_env->app_private;
    tracef("/* Doing noticecall */\n");
    assert(notices==0 || notices==DB_NOTICE_LOGFILE_CHANGED);
    ydb_env->i->noticecall(ydb_env, notices==0 ? 0 : DB_NOTICE_LOGFILE_CHANGED_ydb);
}
extern void berkeley_noticecall (DB_ENV_ydb *, db_notices_ydb);
void ydb_env_set_noticecall (DB_ENV_ydb *env, void (*noticecall)(DB_ENV_ydb *, db_notices_ydb)) {
    env->i->env->set_noticecall(env->i->env, ydbenv_bdb_noticecall);
    env->i->noticecall = noticecall;
    {
	const char *fun_name;
	if (noticecall==berkeley_noticecall) {
	    fun_name = "berkeley_noticecall";
	} else {
	    fun_name = "Unknown_function";
	    barf();
	    abort();
	}
	tracef("envobj(%lld)->set_noticecall(envobj(%lld), %s);\n",
	       env->i->objnum, env->i->objnum, fun_name);
    }
}
#endif
int  ydb_env_set_tmp_dir (DB_ENV * env_w, const char *tmp_dir) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    int r = env->i->env->set_tmp_dir(env->i->env, tmp_dir);
    tracef("r = envobj(%lld)->set_tmp_dir(envobj(%lld), \"%s\"); assert(r==%d);\n",
	   env->i->objnum, env->i->objnum, tmp_dir, r);
    return r;
}
int  ydb_env_set_verbose (DB_ENV *env_w, u_int32_t which, int onoff) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    barf();
    return 1;
}
int  ydb_env_txn_checkpoint (DB_ENV *env_w, u_int32_t kbyte, u_int32_t min, u_int32_t flags) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    int r;
    assert(flags==0);
    r=env->i->env->txn_checkpoint(env->i->env, kbyte, min, 0);
    assert(r==0);
    tracef("r=envobj(%lld)->txn_checkpoint(envobj(%lld), %u, %u, %u); assert(r==0);\n",
	   env->i->objnum, env->i->objnum, kbyte, min, flags);
    return r;
}

int  ydb_env_txn_stat (DB_ENV *env_w, DB_TXN_STAT **statp, u_int32_t flags) {
    struct __toku_db_env *env = bdb2toku_env(env_w);
    barf();
    return 1;
}


int yobi_db_txn_commit (DB_TXN_ydb *txn, u_int32_t flags) {
    int r;
    //notef("flags=%d\n", flags);
    assert(flags==0);
    r =  txn->i->txn->commit(txn->i->txn, 0);
    txn->i->txn = 0;
    assert(flags==0); // need to convert otherwise.
    tracef("r=txnobj(%lld)->commit(txnobj(%lld), %d); assert(r==%d);\n",
	   txn->i->objnum, txn->i->objnum, flags, r);
    // free(txn);
    return r;
}

u_int32_t yobi_db_txn_id (DB_TXN_ydb *txn) {
  barf();
  abort();
}

// There is no txn_begin when generated with --with-uniquename.
int txn_begin_bdbw (struct yobi_db_env *env, struct yobi_db_txn *stxn, struct yobi_db_txn **txn, u_int32_t flags) {
    int r;
    struct yobi_db_txn *result = malloc(sizeof(*result));
    result->commit = yobi_db_txn_commit;
    result->id = yobi_db_txn_id;
    result->i = malloc(sizeof(*result->i));
    result->i->objnum = objnum++;
    //note();
    r = env->i->env->txn_begin(env->i->env,
				   stxn ? stxn->i->txn : 0,
				   &result->i->txn, flags);
    *txn = result;
    tracef("r = envobj(%lld)->txn_begin(envobj(%lld), ", env->i->objnum , env->i->objnum);
    if (!stxn) tracef("0, "); else tracef(" txnobj(%lld), ", stxn->i->objnum);
    tracef("new_txnobj(%lld), 0x%x); ", result->i->objnum, flags);
    tracef(" assert(r==%d);\n", r);
    return r;
}

int txn_abort_bdbw (DB_TXN_ydb *txn) {
    barf();
    abort();
}

int txn_commit_bdbw (DB_TXN_ydb *txn, u_int32_t flags) {
    int r;
    u_int32_t bdbflags = 0;
    char *bdbflagsstring = "0";
    assert(flags==0);
    r = txn->i->txn->commit(txn->i->txn, bdbflags);
    assert(r==0);
    tracef("r=txnobj(%lld)->commit(txnobj(%lld), %s); assert(r==%d);\n",
	   txn->i->objnum, txn->i->objnum, bdbflagsstring, r);
    return r;
}

struct ydb_db_internal {
    long long objnum;
    DB *db;
    int (*bt_compare)(DB_ydb *, const DBT_ydb *, const DBT_ydb *);
    DB_ENV_ydb *env;
};

static int bdbw_db_close (DB_ydb *db, u_int32_t flags) {
    int r;
    //notef("flags=%d\n", flags);
    assert(flags==0);
    r = db->i->db->close(db->i->db, 0);
    tracef("r=dbobj(%lld)->close(dbobj(%lld), 0); assert(r==0);\n",
	   db->i->objnum, db->i->objnum);
    db->i->db = 0;
    // free(db);
    return r;
}

struct yobi_dbc_internal {
    DBC *dbc;
    long long objnum;
};

void dbt_bdb2ydb (DBT *da, DBT_ydb *a, const char *varname) {
    u_int32_t aflags = a->flags;
    memset(da, 0, sizeof(*da));
    tracef("  memset(&%s,0,sizeof(a));\n", varname);
    da->data = a->data;
    if (aflags==DB_DBT_USERMEM_ydb) {
	aflags &= ~DB_DBT_USERMEM_ydb;
	da->flags |= DB_DBT_USERMEM;
	tracef("  %s.flags |= DB_DBT_USERMEM;\n", varname);
	if (a->ulen>0) {
	    tracef("  %s.data = malloc(%d);\n", varname, a->ulen);
	} else {
	    tracef("  %s.data = 0;\n", varname);
	}
	da->ulen = a->ulen;
	tracef("  %s.ulen = %d;\n", varname, a->ulen);
    }
    assert(aflags==0);
}

int yobi_dbc_c_get (DBC_ydb *dbc, DBT_ydb *a, DBT_ydb *b, u_int32_t flags) {
    int r;
    DBT da;
    DBT db;
    const int flagstringlen=100;
    char flagstring[flagstringlen];
    int bdb_flags = convert_c_get_flags(flags, flagstring, flagstringlen);
    tracef("{ DBT a,b; \n");
    dbt_bdb2ydb(&da, a, "a");
    dbt_bdb2ydb(&db, b, "b");
    assert(flags==DB_LAST_ydb || flags==DB_FIRST_ydb || flags==DB_NEXT_ydb);
    r = dbc->i->dbc->c_get(dbc->i->dbc, &da, &db, bdb_flags);
    tracef("  r = dbcobj(%lld)->c_get(dbcobj(%lld), ",
	   dbc->i->objnum, dbc->i->objnum);
    tracef(" &a, &b, ");
    tracef(" %s);\n", flagstring);
    if (r==0) {
	unsigned int i;
	tracef("  assert(r==%d);\n", r);
	tracef("  assert(a.size==%d);\n", da.size);
	//tracef("  assert(memcmp(a.address, ");
	tracef("  assert(b.size==%d);\n", db.size);
	tracef("  { unsigned char adata[%d] = {", da.size);
	for (i=0; i<da.size; i++) {
	    if (i>0) tracef(", ");
	    tracef("%u", ((unsigned char*)(da.data))[i]);
	}
	tracef("};\n    unsigned char bdata[%d] = {", db.size);
	for (i=0; i<db.size; i++) {
	    if (i>0) tracef(", ");
	    tracef("%u", ((unsigned char*)(db.data))[i]);
	}
	tracef("};\n");
	tracef("    assert(memcmp(a.data, adata, sizeof(adata))==0);\n");
	tracef("    assert(memcmp(b.data, bdata, sizeof(bdata))==0);\n");
	a->size = da.size;
	tracef("  }\n");
	a->data = da.data;
	b->size = db.size;
	b->data = db.data;
	assert(r==0);
    } else if (r==DB_PAGE_NOTFOUND) {
	tracef("  assert(r==DB_PAGE_NOTFOUND);\n");
    } else if (r==DB_NOTFOUND) {
	tracef("  assert(r==DB_NOTFOUND);\n");
    } else {
	printf("DB Error r=%d: %s\n", r, db_strerror(r));
	abort();
    }
    tracef("}\n");
    return r;
}

int yobi_dbc_c_close (DBC_ydb *dbc) {
    int r;
    r = dbc->i->dbc->c_close(dbc->i->dbc);
    assert(r==0);
    tracef("r=dbcobj(%lld)->c_close(dbcobj(%lld)); assert(r==%d);\n",
	   dbc->i->objnum, dbc->i->objnum, r);
    dbc->i->dbc = 0;
    // free(dbc->i); free(dbc);
    return r;
}

int yobi_dbc_c_del (DBC_ydb *dbc, u_int32_t flags) {
    barf();
    abort();
}

static int bdbw_db_cursor (DB_ydb *db, DB_TXN_ydb *txn, DBC_ydb **c, u_int32_t flags) {
    struct yobi_dbc *dbc = malloc(sizeof(*dbc));
    int r;
    dbc->c_get = yobi_dbc_c_get;
    dbc->c_close = yobi_dbc_c_close;
    dbc->c_del = yobi_dbc_c_del;
    dbc->i = malloc(sizeof(*dbc->i));
    assert(dbc->i);
    assert(flags==0);
    dbc->i->objnum = objnum++;
    r=db->i->db->cursor(db->i->db, txn ? txn->i->txn : 0, &dbc->i->dbc, flags);
    assert(r==0);
    //note();
    *c = dbc;
    tracef("r=dbobj(%lld)->cursor(dbobj(%lld), txnobj(%lld), new_dbcobj(%lld), %d); assert(r==%d);\n",
	   db->i->objnum, db->i->objnum, txn ? txn->i->objnum : -1, dbc->i->objnum, flags, r);
    return r;
}

static int  bdbw_db_del (DB_ydb *db, DB_TXN_ydb *txn, DBT_ydb *dbt, u_int32_t flags) {
  barf();
  abort();
}
  
static int  bdbw_db_get (DB_ydb *db, DB_TXN_ydb *txn, DBT_ydb *dbta, DBT_ydb *dbtb, u_int32_t flags) {
  barf();
  abort();
}

static int  bdbw_db_key_range (DB_ydb *db, DB_TXN_ydb *txn, DBT_ydb *dbt, DB_KEY_RANGE_ydb *kr, u_int32_t flags) {
  barf();
  abort();
}

static int  bdbw_db_open (DB_ydb *db, DB_TXN_ydb *txn, const char *fname, const char *dbname, DBTYPE_ydb dbtype, u_int32_t flags, int mode) {
    int r;
    char flagstring[1000];
    u_int32_t bdb_flags = open_flags_ydb_2_bdb(flags, flagstring, sizeof(flagstring));
    //notef("txn=%p fname=%s dbname=%s dbtype=%d flags=0x%x (bdb=0x%x) %mode=0%o\n", txn, fname, dbname, dbtype, flags, bdb_flags, mode);
    assert(dbtype == DB_BTREE_ydb);
    r = db->i->db->open(db->i->db,
			txn ? txn->i->txn : 0,
			fname, dbname, DB_BTREE, bdb_flags, mode);
    assert(db->i->db->app_private == db);
    tracef("r=dbobj(%lld)->open(dbobj(%lld), txnobj(%lld), \"%s\", \"%s\",",
	   db->i->objnum, db->i->objnum, txn ? txn->i->objnum : -1, fname, dbname);
    if (dbtype==DB_BTREE_ydb) tracef(" DB_BTREE,");
    else abort();
    tracef(" %s, 0%o);", flagstring, mode);
    assert(r==0);
    tracef(" assert(r==%d);\n", r);
    return r;
}

static int bdbw_bt_compare (DB *db, const DBT *a, const DBT *b) {
    DB_ydb *ydb = db->app_private;
    DBT_ydb a_y, b_y;
    //note();
    assert(ydb);
    a_y.data = a->data;
    a_y.size = a->size;
    a_y.app_private = a->app_private; /* mysql uses app_private for key compares. */
    b_y.data = b->data;
    b_y.size = b->size;
    return ydb->i->bt_compare(ydb, &a_y, &b_y);
}

u_int32_t convert_put_flags(u_int32_t flags, char *flagstring, int flagstringlen) {
    if (flags==0) {
	snprintf(flagstring, flagstringlen, "0");
	return 0;
    }
    retit(DB_NOOVERWRITE);
    abort();
}

int  bdbw_db_put (DB_ydb *db, DB_TXN_ydb *txn, DBT_ydb *dbta, DBT_ydb *dbtb, u_int32_t flags) {
    int r;
    unsigned int i;
    DBT a,b;
    char flagstring[1000];
    u_int32_t bdbflags = convert_put_flags(flags, flagstring, sizeof(flagstring));
    assert(dbta->flags==0);   assert(dbtb->flags==0);
    assert(dbta->ulen==0);    assert(dbtb->ulen==0);
    tracef("{ DBT a,b;\n");
    tracef("  unsigned char adata[%d] = {", dbta->size);
    for (i=0; i<dbta->size; i++) {
	if (i>0) tracef(", ");
	tracef("%u", ((unsigned char*)(dbta->data))[i]);
    }
    tracef("};\n  unsigned char bdata[%d] = {", dbtb->size);
    for (i=0; i<dbtb->size; i++) {
	if (i>0) tracef(", ");
	tracef("%u", ((unsigned char*)(dbtb->data))[i]);
    }
    tracef("};\n  memset(&a,0,sizeof(a)); memset(&b,0,sizeof(b));\n");
    tracef("  a.data = adata; b.data=bdata;\n");
    tracef("  a.flags= 0;     b.flags=0;\n");
    tracef("  a.ulen=0;       b.ulen=0;\n");
    tracef("  a.size=%d;      b.size=%d;\n", dbta->size, dbtb->size);
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.data = dbta->data;    b.data = dbtb->data;
    a.flags = 0;            b.flags = 0;
    a.ulen = 0;             b.ulen = 0;
    a.size = dbta->size;    b.size = dbtb->size;
    a.app_private = dbta->app_private; /* mysql uses app_private to inform bt_compare of the key structure, so comparisons can be done properly.   This is needed because mysql does not write the keys to disk in a format that memcmp() could be used. */
    r=db->i->db->put(db->i->db, txn ? txn->i->txn : 0, &a, &b, flags);
    assert(r==0);
    tracef("  r=dbobj(%lld)->put(dbobj(%lld), txnobj(%lld), &a, &b, %s); assert(r==%d);\n}\n",
	   db->i->objnum, db->i->objnum, txn ? txn->i->objnum : -1, flagstring, r);
    return r;
}
int  bdbw_db_remove (DB_ydb *db, const char *fname, const char *dbname, u_int32_t flags) {
    int r;
    assert(dbname==0);
    assert(flags==0);
    tracef(" r =dbobj(%lld)->remove(dbobj(%lld), \"%s\", 0, 0);", db->i->objnum, db->i->objnum, fname);
    r = db->i->db->remove(db->i->db, fname, dbname, flags);
    assert(r==0);
    tracef(" assert(r==%d);\n", r);
    return r;
}
int  bdbw_db_rename (DB_ydb *db, const char *namea, const char *database, const char *namec, u_int32_t flags) {
    int r;
    assert(database==0);
    assert(flags==0);
    tracef(" r = dbobj(%lld)->rename(dbobj(%lld), \"%s\", ", db->i->objnum, db->i->objnum, namea);
    if (database) tracef("\"%s\"", database);
    else tracef("0");
    tracef(", \"%s\", 0); ", namec);
    r=db->i->db->rename(db->i->db, namea, database, namec, 0);
    tracef(" assert(r==%d);\n", r);
    assert(r==0);
    return r;
}

extern int berkeley_cmp_hidden_key(DB_ydb *, const DBT_ydb *, const DBT_ydb *);
extern int berkeley_cmp_packed_key(DB_ydb *, const DBT_ydb *, const DBT_ydb *);

static int  bdbw_db_set_bt_compare (DB_ydb *db, int (*bt_compare)(DB_ydb *, const DBT_ydb *, const DBT_ydb *)) {
    int r;
    r = db->i->db->set_bt_compare(db->i->db, bdbw_bt_compare);
    db->i->bt_compare = bt_compare;
    {
	const char *fun_name;
	if (bt_compare==berkeley_cmp_hidden_key) {
	    fun_name = "berkeley_cmp_hidden_key";
	} else if (bt_compare==berkeley_cmp_packed_key) {
	    fun_name = "berkeley_cmp_packed_key";
	} else {
	    fun_name = "Unknown_function";
	    barf();
	    abort();
	}
	tracef("r = dbobj(%lld)->set_bt_compare(dbobj(%lld), %s); assert(r==%d);\n",
	       db->i->objnum, db->i->objnum, fun_name, r);
    }
    return r;
}

int  bdbw_db_set_flags (DB_ydb *db, u_int32_t flags) {
    int r;
    char flagsstring[1000];
    u_int32_t bdb_flags = convert_db_set_flags (flags, flagsstring, sizeof(flagsstring));
    r = db->i->db->set_flags(db->i->db, bdb_flags);
    assert(r==0);
    tracef("r=dbobj(%lld)->set_flags(dbobj(%lld), %s); assert(r==0);\n",
	   db->i->objnum, db->i->objnum, flagsstring);
    return r;
}
int bdbw_db_stat (DB_ydb *db, void *v, u_int32_t flags) {
  barf();
  abort();
}

int db_create_bdbw (DB_ydb **db, DB_ENV_ydb *env, u_int32_t flags) {
  DB_ydb *result=malloc(sizeof(*result));
  int r;
  result->app_private = 0;
  result->close = bdbw_db_close;
  result->cursor = bdbw_db_cursor;
  result->del = bdbw_db_del;
  result->get = bdbw_db_get;
  result->key_range = bdbw_db_key_range;
  result->open = bdbw_db_open;
  result->put = bdbw_db_put;
  result->remove = bdbw_db_remove;
  result->rename = bdbw_db_rename;
  result->set_bt_compare = bdbw_db_set_bt_compare;
  result->set_flags = bdbw_db_set_flags;
  result->stat = bdbw_db_stat;
  result->i = malloc(sizeof(*result->i));
  r=db_create(&result->i->db, env->i->env, convert_db_create_flags(flags));
  result->i->objnum = objnum++;
  result->i->db->app_private = result;
  result->i->bt_compare = 0;
  result->i->env = env;
  *db = result;
  tracef("r=db_create(new_dbobj(%lld), envobj(%lld), %d); assert(r==%d);\n",
	 result->i->objnum, env->i->objnum, flags, r);  
  return r;
}

#if 0

void bdbw_db_env_err (const DB_ENV_ydb *env, int error, const char *fmt, ...) {
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
#define doit(flag) if (flag & flags) { if (doneone) printf(" | "); printf("%s", #flag);  doneone=1; gotit|=flag; }
  printf(" flags=");
  doit(DB_INIT_LOCK_ydb);
  doit(DB_INIT_LOG_ydb);
  doit(DB_INIT_MPOOL_ydb);
  doit(DB_INIT_TXN_ydb);
  doit(DB_CREATE_ydb);
  doit(DB_THREAD_ydb);
  doit(DB_RECOVER_ydb);
  doit(DB_PRIVATE_ydb);
  if (gotit!=flags) printf("  flags 0x%x not accounted for", flags&~gotit);
  printf("\n");
}

int  yobi_db_env_open (DB_ENV_ydb *env, const char *home, u_int32_t flags, int mode) {
  notef("(%p, \"%s\", 0x%x, 0%o)\n", env, home, flags, mode);
  env->dir = strdup(home);
  env->open_flags = flags;
  env->open_mode  = mode;
  print_flags(flags);
  assert(DB_PRIVATE & flags); // This means that we don't have to do anything with shared memory.  And that's good enough for mysql. 
  return 0;
}
int  yobi_db_env_close (DB_ENV_ydb * env, u_int32_t flags) {
  barf();
  return 1;
}
int  yobi_db_env_log_archive (DB_ENV_ydb *env, char **list[], u_int32_t flags) {
  barf();
  return 1;
}
int  yobi_db_env_log_flush (DB_ENV_ydb * env, const DB_LSN_ydb * lsn) {
  barf();
  return 1;
}
int  yobi_db_env_set_cachesize (DB_ENV_ydb * env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
  barf();
  return 1;
}
int  yobi_db_env_set_data_dir (DB_ENV_ydb * env, const char *dir) {
  barf();
  return 1;
}
void yobi_db_env_set_errcall (DB_ENV_ydb *env, void (*errcall)(const char *, char *)) {
  note();
  env->errcall=errcall;
}
void yobi_db_env_set_errpfx (DB_ENV_ydb * env, const char *errpfx) {
  notef("(%p, %s)\n", env, errpfx);
  env->errpfx = errpfx;
}
int  yobi_db_env_set_flags (DB_ENV_ydb *env, u_int32_t flags, int onoff) {
  barf();
  return 1;
}
int  yobi_db_env_set_lg_bsize (DB_ENV_ydb * env, u_int32_t bsize) {
  barf();
  return 1;
}
int  yobi_db_env_set_lg_dir (DB_ENV_ydb * env, const char * dir) {
  barf();
  return 1;
}
int  yobi_db_env_set_lg_max (DB_ENV_ydb *env, u_int32_t lg_max) {
  barf();
  return 1;
}
int  yobi_db_env_set_lk_detect (DB_ENV_ydb *env, u_int32_t detect) {
  barf();
  return 1;
}
int  yobi_db_env_set_lk_max (DB_ENV_ydb *env, u_int32_t lk_max) {
  barf();
  return 1;
}
void yobi_db_env_set_noticecall (DB_ENV_ydb *env, void (*noticeall)(DB_ENV_ydb *, db_notices_ydb)) {
  barf();
}
int  yobi_db_env_set_tmp_dir (DB_ENV_ydb * env, const char *tmp_dir) {
  barf();
  return 1;
}
int  yobi_db_env_set_verbose (DB_ENV_ydb *env, u_int32_t which, int onoff) {
  barf();
  return 1;
}
int  yobi_db_env_txn_checkpoint (DB_ENV_ydb *env, u_int32_t kbyte, u_int32_t min, u_int32_t flags) {
  barf();
  return 1;
}

int  yobi_db_env_txn_stat (DB_ENV_ydb *env, DB_TXN_STAT_ydb **statp, u_int32_t flags) {
  barf();
  return 1;
}

void yobi_default_errcall(const char *errpfx, char *msg) {
  fprintf(stderr, "YDB: %s: %s", errpfx, msg);
}




int yobi_db_txn_commit (DB_TXN_ydb *txn, u_int32_t flags) {
  notef("flags=%d\n", flags);
  return 0;
}

u_int32_t yobi_db_txn_id (DB_TXN_ydb *txn) {
  barf();
  abort();
}

int log_compare_ydb (const DB_LSN_ydb *a, const DB_LSN_ydb *b) {
  fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
  abort();
}

#endif
