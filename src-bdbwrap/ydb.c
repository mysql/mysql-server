/* This version is what Mysql calls.
 * It invokes the version in bdbw.
 * The version in bdbw then converts to Berkeley DB Calls. */
#include <sys/types.h>
/* This include is to the ydb include, which is what mysql sees. */
#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <toku_assert.h>
#include "bdbw.h"

#define barf() ({ fprintf(stderr, "YDB: BARF %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define barff(fmt,...) ({ fprintf(stderr, "YDB: BARF %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })
#define note() ({ fprintf(stderr, "YDB: Note %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define notef(fmt,...) ({ fprintf(stderr, "YDB: Note %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })

int db_env_create (DB_ENV **envp, u_int32_t flags) {
    return db_env_create_bdbw(envp, flags);
}

int txn_abort (DB_TXN *txn) {
    return txn_abort_bdbw(txn);
}

int txn_begin (DB_ENV *env, DB_TXN *stxn, DB_TXN **txn, u_int32_t flags) {
    return txn_begin_bdbw(env, stxn, txn, flags);
}


int txn_commit (DB_TXN *txn, u_int32_t flags) {
    return txn_commit_bdbw(txn, flags);
}



struct ydb_db_internal {
    int foo;
};

void print_flags (u_int32_t flags) {
  u_int32_t gotit=0;
  int doneone=0;
#define doit(flag) if (flag & flags) { if (doneone) printf(" | "); printf("%s", #flag);  doneone=1; gotit|=flag; }
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
  printf("\n");
}

int log_compare (const DB_LSN *a, const DB_LSN *b) {
  fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
  abort();
}

static int yobi_db_close (DB *db, u_int32_t flags) {
  barf();
  abort();
}

int yobi_db_cursor (DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
  barf();
  abort();
}

int  yobi_db_del (DB *db, DB_TXN *txn, DBT *dbt, u_int32_t flags) {
  barf();
  abort();
}
  
int  yobi_db_get (DB *db, DB_TXN *txn, DBT *dbta, DBT *dbtb, u_int32_t flags) {
  barf();
  abort();
}

int  yobi_db_key_range (DB *db, DB_TXN *txn, DBT *dbt, DB_KEY_RANGE *kr, u_int32_t flags) {
  barf();
  abort();
}
int  yobi_db_open (DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
  notef("txn=%p fname=%s dbname=%s dbtype=%d flags=0x%x mode=0%o\n", txn, fname, dbname, dbtype, flags, mode);
  print_flags(flags);
  return 0;
}
int  yobi_db_put (DB *db, DB_TXN *txn, DBT *dbta, DBT *dbtb, u_int32_t flags) {
  barf();
  abort();
}
int  yobi_db_remove (DB *db, const char *fname, const char *dbname, u_int32_t flags) {
  barf();
  abort();
}
int  yobi_db_rename (DB *db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
  barf();
  abort();
}
int  yobi_db_set_flags (DB *db, u_int32_t flags) {
  barf();
  abort();
}
int yobi_db_stat (DB *db, void *v, u_int32_t flags) {
  barf();
  abort();
}

int db_create (DB **db, DB_ENV *env, u_int32_t flags) {
    return db_create_bdbw(db, env, flags);
}

