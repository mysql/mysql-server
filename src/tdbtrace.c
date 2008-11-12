/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Provide an TokuDB API that performs tracing.
 * This is a thin layer on top of ydb.c.
 * Simply trace it and then call the ydb code.
 * The trace file ends up in the cwd.
 */

#include "portability.h"
#include "../include/db.h"
#include "toku_assert.h"
#include "memory.h"
#include "tdbtrace.h"
#include "ydb-internal.h"

#include <errno.h>
#include <toku_pthread.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

static toku_pthread_mutex_t tdb_big_lock = TOKU_PTHREAD_MUTEX_INITIALIZER;

static void tlock(void) {
    int r = toku_pthread_mutex_lock(&tdb_big_lock);   assert(r == 0);
}

static void tunlock(void) {
    int r = toku_pthread_mutex_unlock(&tdb_big_lock); assert(r == 0);
}

static FILE *tracefile = 0;

enum tracetype { TT_ENV, TT_DB, TT_TXN, TT_DBC};
static struct tracepair {
    int itemnum;
    enum tracetype tt;
    void *item;
    struct tracepair *next;
} *tracepairs=0;
static int itemnum_counter=1;

static void find_ttpairp (void *item, struct tracepair ***ptr) {
    *ptr = &tracepairs;
    while (**ptr) {
	if ((**ptr)->item==item) return;
	*ptr = &(**ptr)->next;
    }
}
static struct tracepair *create_tracepair (void *item, enum tracetype tt) {
    struct tracepair *MALLOC(pair);
    assert(pair);
    pair->itemnum = itemnum_counter++;
    pair->tt   = tt;
    pair->item = item;
    pair->next = tracepairs;
    tracepairs = pair;
    return pair;
}
static struct tracepair * find_envpair (DB_ENV *e, int *itemnum) {
    if (e==0) {
	*itemnum=0;
	return 0;
    } else {
	struct tracepair **pp;
	find_ttpairp (e, &pp);
	if (*pp==0) {
	    *itemnum = 0;
	    return 0;
	} else {
	    struct tracepair *p=*pp;
	    assert(p->tt==TT_ENV);
	    *itemnum = p->itemnum;
	    return p;
	}
    }
}

static int pairnum (void *item, enum tracetype tt) {
    struct tracepair **pp;
    find_ttpairp(item, &pp);
    if (*pp==0){
	return 0;
    } else {
	assert ((*pp)->tt==tt);
	return (*pp)->itemnum;
    }
}    


static int dbpairnum (DB *db) {
    return pairnum(db, TT_DB);
}

static int txnpairnum (DB_TXN *txn) {
    return pairnum(txn, TT_TXN);
}
static int dbcpairnum (DBC *dbc) {
    return pairnum(dbc, TT_DBC);
}

int toku_set_trace_file (char *fname) {
    tracefile = fopen(fname, "w");
    assert(tracefile);
    printf("Tracing to %s (file=%p)\n", fname, tracefile);
    return 0;
}
int toku_close_trace_file (void) {
    int r=0;
    if (fclose(tracefile)!=0) { r=errno; }
    tracefile=0;
    return r;
}

static int tokutrace_env_close (DB_ENV *env, u_int32_t flags) {
    tlock();
    DB_ENV *e = (DB_ENV*)env->i;
    int r = e->close(e, flags);
    memset(env, 0, sizeof(*e));

    struct tracepair **ptr;
    find_ttpairp(env, &ptr);
    if (*ptr) {
	assert((*ptr)->tt==TT_ENV);
	if (tracefile) {
	    fprintf(tracefile, "env_close %d %d %u\n", r, (*ptr)->itemnum, flags);
	}
	struct tracepair *hold=*ptr;
	*ptr=(*ptr)->next;
	toku_free(hold);
    }

    toku_free(env);
    tunlock();
    return r;
}
static void tokutrace_env_err(const DB_ENV * env, int error, const char *fmt, ...) __attribute__((format (printf, 3, 0)));
static void tokutrace_env_err(const DB_ENV * env, int error, const char *fmt, ...) {
    // Don't need to trace the err call.
    tlock();
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(env, error, FALSE, TRUE, fmt, ap); // call it directly
    va_end(ap);
    tunlock();
}

static int tokutrace_env_get_cachesize (DB_ENV *env, u_int32_t *gbytes, u_int32_t *bytes, int *ncache) {
    tlock();
    DB_ENV *e = (DB_ENV*)env->i;
    int r = e->get_cachesize(e, gbytes, bytes, ncache);
    if (tracefile) {
	int itemnum;
	struct tracepair *ep = find_envpair(env, &itemnum);
	if (env==0 || ep) {
	    if (r==0) {
		fprintf(tracefile, "env_get_cachesize %d %d %u %u %d\n", r, itemnum, *gbytes, *bytes, *ncache);
	    } else {
		fprintf(tracefile, "env_get_cachesize %d %d %d %d %d\n", r, itemnum, -1, -1, -1);
	    }
	}
    }
    tunlock();
    return r;
}

static void tokutrace_env_set_errfile(DB_ENV*env, FILE*errfile) {
    // Don't need to trace the set_errfile
    tlock();
    DB_ENV *e = (DB_ENV*)env->i;
    e->set_errfile(e, errfile);
    tunlock();
}

static int tokutrace_env_open (DB_ENV *env, const char *home, u_int32_t flags, int mode) {
    tlock();
    DB_ENV *e = (DB_ENV*)env->i;
    int r = e->open(e, home, flags, mode);
    if  (tracefile) {
	int itemnum;
	find_envpair(env, &itemnum);
	fprintf(tracefile, "env_open %d %d %s %u %d\n", r, itemnum, home, flags, mode);
    }
    tunlock();
    return r;
}

int db_env_create(DB_ENV ** envp, u_int32_t flags) {
    tlock();
    DB_ENV *MALLOC(result);
    int r;
    if (result==0) {	r = errno; tunlock(); return r; }
    memset(result, 0, sizeof(*result));

    DB_ENV *native_env;
    r = db_env_create_toku10(&native_env, flags);
    if (r != 0) { toku_free(result); tunlock(); return r; }

    result->i = (void*)native_env;

#define SE(name) result->name = tokutrace_env_ ## name;
    SE(close);
    result->err = (void (*)(const DB_ENV *, int, const char *, ...)) tokutrace_env_err;
    SE(get_cachesize);
    //SE(get_flags);
    //SE(get_lk_max_locks);
    //SE(log_archive);
    //SE(log_flush);
    SE(open);
    //SE(set_cachesize);
    //SE(set_data_dir);
    //SE(set_errcall);
    SE(set_errfile);
    //SE(set_errpfx);
    //SE(set_flags);
    //SE(set_lg_bsize);
    //SE(set_lg_dir);
    //SE(set_lg_max);
    //SE(set_lk_detect);
    //SE(set_lk_max);
    //SE(set_lk_max_locks);
    //SE(set_tmp_dir);
    //SE(set_verbose);
    //SE(txn_begin);
    //SE(txn_checkpoint);
    //SE(txn_stat);
#undef SE
    *envp = result;

    if (tracefile) {
	struct tracepair *pair = create_tracepair(result, TT_ENV);
	fprintf(tracefile, "db_env_create %d %d %u\n", 0, pair->itemnum, flags);
    }
    

    tunlock();
    return 0;
}

static int tokutrace_db_set_flags(DB *db, u_int32_t flags) {
    tlock();
    DB *d = (DB*)db->i;
    int r = d->set_flags(d, flags);
    if (tracefile) {
	fprintf(tracefile, "db_set_flags %d %d %u\n", r, dbpairnum(db), flags);
    }
    tunlock();
    return r;
}

static int tokutrace_db_set_pagesize(DB *db, u_int32_t pagesize) {
    tlock();
    DB *d = (DB*)db->i;
    int r = d->set_pagesize(d, pagesize);
    if (tracefile) {
	fprintf(tracefile, "db_set_pagesize %d %d %u\n", r, dbpairnum(db), pagesize);
    }
    tunlock();
    return r;
}

static int tokutrace_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    tlock();
    DB *d = (DB*)db->i;
    int r = d->open(d, txn ? (DB_TXN*)txn->i : 0, fname, dbname, dbtype, flags, mode);
    if (tracefile) {
	fprintf(tracefile, "db_open %d %d %d %s %s %d %u %d\n",
		r,
		dbpairnum(db), txnpairnum(txn), fname, dbname, (int)dbtype, flags, mode);
    }
    tunlock();
    return r;
}

static int tokutrace_db_close (DB *db, u_int32_t flags) {
    tlock();
    DB *d = (DB*)db->i;
    int r = d->close(d, flags);
    if (tracefile) fprintf(tracefile, "db_close %d %d %u\n", r, dbpairnum(db), flags);
    tunlock();
    return r;
}
static void trace_char (unsigned char ch) {
    if (isprint(ch) && ch!=' ' && !isxdigit(ch)) {
	fprintf(tracefile, "%c", ch);
    } else {
	fprintf(tracefile, "%02x", ch);
    }
}

static void trace_dbt (DBT *v) {
    fprintf(tracefile,"{%u ", v->size);
    unsigned int i;
    for (i=0; i<v->size; i++) trace_char(((unsigned char*)v->data)[i]);
    fprintf(tracefile," }");
}

static int tokutrace_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    tlock();
    int remainingflags = flags;
    int UU(yes_overwrite) = remainingflags & DB_YESOVERWRITE;
    remainingflags &= ~DB_YESOVERWRITE;
    assert(remainingflags==0); // if flags are nonzero we'll need to do something more carefully.
    DB *d = (DB*)db->i;
    int r = d->put(d, txn ? (DB_TXN*)txn->i : 0, key, data, flags);
    if (tracefile) {
	fprintf(tracefile, "db_put %d %d %d ",
		r,
		dbpairnum(db), txnpairnum(txn));
	trace_dbt(key);
	fprintf(tracefile, " ");
	trace_dbt(data);
	fprintf(tracefile, " %u\n", flags);
    }
    tunlock();
    return r;
}

static int tokutrace_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    tlock();
    assert(flags==0); // if flags are nonzero we'll need to do something more carefully.
    DB *d = (DB*)db->i;
    int r = d->get(d, txn ? (DB_TXN*)txn->i : 0, key, data, flags);
    if (tracefile) {
	fprintf(tracefile, "db_get %d %d %d ",
		r,
		dbpairnum(db), txnpairnum(txn));
	trace_dbt(key);
	fprintf(tracefile, " ");
	trace_dbt(data);
	fprintf(tracefile, " %u\n", flags);
    }
    tunlock();
    return r;

}

static int tokutrace_dbc_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag) {
    tlock();
    DBC *native_cursor = (DBC*)c->i;
    int r;
    switch (flag) {
    case DB_NEXT:
	r = native_cursor->c_get(native_cursor, key,data, flag);
	if (tracefile) {
	    fprintf(tracefile, "dbc_cget %d %d ", r, dbcpairnum(c));
	    trace_dbt(key);
	    fprintf(tracefile, " ");
	    trace_dbt(data);
	    fprintf(tracefile, " %u\n", flag);
	}
	break;
    default:
	abort();
    }
    tunlock();
    return r;
}
    
static int tokutrace_dbc_c_close (DBC *dbc) {
    tlock();
    DBC *native_cursor = (DBC*)dbc->i;
    int r = native_cursor->c_close(native_cursor);
    if (tracefile) fprintf(tracefile, "dbc_close %d %d\n", r, dbcpairnum(dbc));
    tunlock();
    return r;
}

static int tokutrace_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    tlock();
    DBC *MALLOC(result);
    assert(result);
    memset(result, 0, sizeof(*result));    
    DBC *native_cursor;
    DB  *native_db = (DB*)db->i;
    DB_TXN *native_txn = txn ? (DB_TXN*)txn->i : 0;
    int r = native_db->cursor(native_db, native_txn, &native_cursor, flags);
    if (tracefile) {
	struct tracepair *pair = create_tracepair(result, TT_DBC);
	fprintf(tracefile, "db_cursor %d %d %d %d %u\n",
		r, dbpairnum(db), txnpairnum(txn), pair->itemnum, flags);
    }
    tunlock();
#define SC(name) result->name = tokutrace_dbc_ ## name;
    SC(c_get);
    SC(c_close);
#undef SC
    result->i = (void*)native_cursor;
    *c = result;
    return r;
}

int db_create(DB **dbp, DB_ENV *env, u_int32_t flags) {
    tlock();
    DB *MALLOC(result);
    assert(result);
    memset(result, 0, sizeof(*result));
    
    DB *native_db;
    int r = db_create_toku10(&native_db, env ? (DB_ENV*)env->i : 0, flags);
    result->i = (void*)native_db;
#define SDB(name) result->name = tokutrace_db_ ## name;
    SDB(close);
    SDB(cursor);
    SDB(open);
    SDB(get);
    SDB(put);
    SDB(set_flags);
    SDB(set_pagesize);
#undef SDB
    *dbp = result;
    if (tracefile) {
	struct tracepair *pair = create_tracepair(result, TT_DB);
	int envitemnum;
	find_envpair(env, &envitemnum);
	fprintf(tracefile, "db_create %d %d %d %u\n", 0, pair->itemnum, envitemnum, flags);
    }
    tunlock();
    return r;
}
