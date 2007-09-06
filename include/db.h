#ifndef _YOBI_DB_H
#define _YOBI_DB_H

#include "ydb-constants.h"

#if defined(__cplusplus)
extern "C" {
#if 0
}
#endif
#endif

#include <sys/types.h>
#include <stdio.h>

typedef enum {
	DB_BTREE=1,
	//	DB_HASH=2,
	//	DB_RECNO=3,
	//	DB_QUEUE=4,
	//	DB_UNKNOWN=5			/* Figure it out on open. */
} DBTYPE;

typedef enum {
	DB_NOTICE_LOGFILE_CHANGED
} db_notices;

enum {
  DB_VERB_CHKPOINT = 0x0001,
  DB_VERB_DEADLOCK = 0x0002,
  DB_VERB_RECOVERY = 0x0004
  
};

typedef struct yobi_db DB;
typedef struct yobi_db_btree_stat DB_BTREE_STAT;
typedef struct yobi_db_env DB_ENV;
typedef struct yobi_db_key_range DB_KEY_RANGE;
typedef struct yobi_db_lsn DB_LSN;
typedef struct yobi_db_txn DB_TXN;
typedef struct yobi_db_txn_active DB_TXN_ACTIVE;
typedef struct yobi_db_txn_stat DB_TXN_STAT;
typedef struct yobi_dbc DBC;
typedef struct yobi_dbt DBT;

struct yobi_db {
  void *app_private;
  int  (*close) (DB *, u_int32_t);
  int  (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t);
  int  (*del) (DB *, DB_TXN *, DBT *, u_int32_t);
  int  (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
  int  (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t);
  int  (*open) (DB *, DB_TXN *,
		const char *, const char *, DBTYPE, u_int32_t, int);
  int  (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
  int  (*remove) (DB *, const char *, const char *, u_int32_t);
  int  (*rename) (DB *, const char *, const char *, const char *, u_int32_t);
  int  (*set_bt_compare) (DB *,
			  int (*)(DB *, const DBT *, const DBT *));
  int  (*set_flags)    (DB *, u_int32_t);
  int  (*stat) (DB *, void *, u_int32_t);

  struct ydb_db_internal *i;
};
enum {
  DB_DBT_MALLOC = 0x002,
  DB_DBT_REALLOC = 0x010,
  DB_DBT_USERMEM = 0x020,
  DB_DBT_DUPOK  = 0x040
};
struct yobi_dbt {
  void	   *app_private;
  void     *data;
  u_int32_t flags;
  u_int32_t size;
  u_int32_t ulen;
};
struct yobi_db_txn {
  int (*commit) (DB_TXN*, u_int32_t);
  u_int32_t (*id) (DB_TXN *);
  // internal stuff
  struct yobi_db_txn_internal *i;
};
struct yobi_dbc {
  int (*c_get) (DBC *, DBT *, DBT *, u_int32_t);
  int (*c_close) (DBC *);
  int (*c_del) (DBC *, u_int32_t);
  struct yobi_dbc_internal *i;
};
struct yobi_db_env {
  // Methods used by MYSQL
  void (*err) (const DB_ENV *, int, const char *, ...);
  int  (*open) (DB_ENV *, const char *, u_int32_t, int);
  int  (*close) (DB_ENV *, u_int32_t);
  int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t);
  int  (*log_flush) (DB_ENV *, const DB_LSN *);
  void (*set_errcall) (DB_ENV *, void (*)(const char *, char *));
  void (*set_errpfx) (DB_ENV *, const char *);
  void (*set_noticecall) (DB_ENV *, void (*)(DB_ENV *, db_notices));
  int  (*set_flags) (DB_ENV *, u_int32_t, int);
  int  (*set_data_dir) (DB_ENV *, const char *);
  int  (*set_tmp_dir) (DB_ENV *, const char *);
  int  (*set_verbose) (DB_ENV *, u_int32_t, int);
  int  (*set_lg_bsize) (DB_ENV *, u_int32_t);
  int  (*set_lg_dir) (DB_ENV *, const char *);
  int  (*set_lg_max) (DB_ENV *, u_int32_t);
  int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int);
  int  (*set_lk_detect) (DB_ENV *, u_int32_t);
  int  (*set_lk_max) (DB_ENV *, u_int32_t);
  int  (*log_archive) (DB_ENV *, char **[], u_int32_t);
  int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t);
#ifdef _YDB_WRAP_H
#undef txn_begin
#endif
  int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t);
#ifdef _YDB_WRAP_H
#define txn_begin txn_begin_ydb
#endif
  // Internal state
  struct db_env_ydb_internal *i;
};
struct yobi_db_key_range {
  double less,equal,greater;
};
struct yobi_db_btree_stat {
  u_int32_t bt_ndata;
  u_int32_t bt_nkeys;
};
struct yobi_db_txn_stat {
  u_int32_t st_nactive;
  DB_TXN_ACTIVE *st_txnarray;
};
struct yobi_db_lsn {
  int hello;
};
struct yobi_db_txn_active {
  DB_LSN	lsn;
  u_int32_t	txnid;
};

#ifndef _YDB_WRAP_H
#define DB_VERSION_STRING "Yobiduck: Fractal DB (November 19, 2006)"
#else
#define DB_VERSION_STRING_ydb "Yobiduck: Fractal DB (November 19, 2006) (wrapped bdb)"
#endif

enum {
  DB_ARCH_ABS = 0x001,
  DB_ARCH_LOG = 0x004
};

enum {
  DB_CREATE     = 0x0000001,
  DB_RDONLY     = 0x0000010,
  DB_RECOVER    = 0x0000020,
  DB_THREAD     = 0x0000040, 
  DB_TXN_NOSYNC = 0x0000100,
  
  DB_PRIVATE    = 0x0100000
};

enum {
  DB_LOCK_DEFAULT = 1,
  DB_LOCK_OLDEST  = 7,
  DB_LOCK_RANDOM  = 8 
};

enum {
  DB_DUP = 0x000002
};

enum {
  DB_NOOVERWRITE = 23
};

enum {
  DB_INIT_LOCK  = 0x001000,
  DB_INIT_LOG   = 0x002000,
  DB_INIT_MPOOL = 0x004000,
  DB_INIT_TXN   = 0x008000
};

int db_create (DB **, DB_ENV *, u_int32_t);
int db_env_create (DB_ENV **, u_int32_t);

int txn_begin (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t);
int txn_commit (DB_TXN *, u_int32_t);
int txn_abort (DB_TXN *);

int log_compare (const DB_LSN *, const DB_LSN *);

#if defined(__cplusplus)
}
#endif
#endif
