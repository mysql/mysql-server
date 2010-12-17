/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H

#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <db.h>
#include "../newbrt/brttypes.h"
#include "../newbrt/brt.h"
#include "toku_list.h"
#include "./lock_tree/locktree.h"
#include "./lock_tree/idlth.h"
#include "../newbrt/minicron.h"
#include <limits.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct __toku_lock_tree;

struct __toku_db_internal {
    int opened;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    DICTIONARY_ID dict_id;        // unique identifier used by locktree logic
    struct __toku_lock_tree* lt;
    struct simple_dbt skey, sval; // static key and value
    BOOL key_compare_was_set;     // true if a comparison function was provided before call to db->open()  (if false, use environment's comparison function).  
    char *dname;                  // dname is constant for this handle (handle must be closed before file is renamed)
    BOOL is_zombie;               // True if DB->close has been called on this DB
    struct toku_list dbs_that_must_close_before_abort;
    DB_INDEXER *indexer;
    int refs;                     // reference count including indexers and loaders
};

int toku_db_set_indexer(DB *db, DB_INDEXER *indexer);
DB_INDEXER *toku_db_get_indexer(DB *db);

void toku_db_add_ref(DB *db);
void toku_db_release_ref(DB *db);

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
typedef void (*toku_env_errcall_t)(const char *, char *);
#elif DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
typedef void (*toku_env_errcall_t)(const DB_ENV *, const char *, const char *);
#else
#error
#endif

struct __toku_db_env_internal {
    int is_panicked; // if nonzero, then its an error number
    char *panic_string;
    u_int32_t open_flags;
    int open_mode;
    toku_env_errcall_t errcall;
    void *errfile;
    const char *errpfx;
    char *dir;                  /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *lg_dir;
    char *data_dir;
    int (*bt_compare)  (DB *, const DBT *, const DBT *);
    generate_row_for_put_func generate_row_for_put;
    generate_row_for_del_func generate_row_for_del;
    //void (*noticecall)(DB_ENV *, db_notices);
    unsigned long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
    toku_ltm* ltm;
    struct toku_list open_txns;
    DB *directory;                                      // Maps dnames to inames
    DB *persistent_environment;                         // Stores environment settings, can be used for upgrade
    OMT open_dbs;                                       // Stores open db handles, sorted first by dname and then by numerical value of pointer to the db (arbitrarily assigned memory location)

    char *real_data_dir;                                // data dir used when the env is opened (relative to cwd, or absolute with leading /)
    char *real_log_dir;                                 // log dir used when the env is opened  (relative to cwd, or absolute with leading /)
    char *real_tmp_dir;                                 // tmp dir used for temporary files (relative to cwd, or absoulte with leading /)

    enum { 
        FS_GREEN = 0,                                   // green zone  (we have lots of space)
        FS_YELLOW = 1,                                  // yellow zone (issue warning but allow operations)
        FS_RED = 2,                                     // red zone    (prevent operations)
    } fs_state;
    uint64_t fs_seq;                                    // how many times has fs_poller run?
    uint64_t last_seq_entered_red;
    uint64_t last_seq_entered_yellow;
    int redzone;                                        // percent of total fs space that marks boundary between yellow and red zones
    int enospc_redzone_ctr;                             // number of operations rejected by enospc prevention  (red zone)
    int fs_poll_time;                                   // Time in seconds between statfs calls
    struct minicron fs_poller;                          // Poll the file systems
    BOOL fs_poller_is_init;    
    uint32_t num_open_dbs;
    uint32_t num_zombie_dbs;
    int envdir_lockfd;
    int datadir_lockfd;
    int logdir_lockfd;
    int tmpdir_lockfd;
};

/* *********************************************************

   Ephemeral locking

   ********************************************************* */

typedef struct {
    volatile u_int64_t        ydb_lock_ctr;            /* how many times has ydb lock been taken/released.  This is precise since it is updated only when the lock is held.                              */ 
    volatile u_int32_t        num_waiters_now;         /* How many are waiting on the ydb lock right now (including the current lock holder).  This is precise since it is updated with a fetch-and-add. */
    volatile u_int32_t        max_waiters;             /* max number of simultaneous client threads kept waiting for ydb lock.  This is precise (updated only when the lock is held) but may be running a little behind (while waiting for the lock it hasn't been updated).  */ 
    volatile u_int64_t        total_sleep_time;        /* total time spent sleeping for ydb lock scheduling (useconds).   This adds up over many clients. This is precise since it is updated with an atomic fetch-and-add. */ 
    volatile u_int64_t        max_time_ydb_lock_held;  /* max time the ydb lock was held (in microseconds).  This is precise since it is updated only when the lock is held.  */ 
    volatile u_int64_t        total_time_ydb_lock_held;/* total time the ydb lock has been held (in microseconds).  */
    volatile u_int64_t        total_time_since_start;  /* total time since the ydb lock was initialized (in microseconds) This is only updated when the lock is accessed (so if you don't acquire the lock this doesn't increase), and it is updated precisely (even though it isn't updated continuously). */
} SCHEDULE_STATUS_S, *SCHEDULE_STATUS;



int toku_ydb_lock_init(void);
int toku_ydb_lock_destroy(void);
void toku_ydb_lock(void);
void toku_ydb_unlock(void);
void toku_ydb_unlock_and_yield(unsigned long useconds);

void toku_ydb_lock_get_status(SCHEDULE_STATUS statp);

int toku_ydb_check_avail_fs_space(DB_ENV *env);


/* *********************************************************

   Error handling

   ********************************************************* */

/* Exception handling */
/** Raise a C-like exception: currently returns an status code */
#define RAISE_EXCEPTION(status) {return status;}
/** Raise a C-like conditional exception: currently returns an status code 
    if condition is true */
#define RAISE_COND_EXCEPTION(cond, status) {if (cond) return status;}
/** Propagate the exception to the caller: if the status is non-zero,
    returns it to the caller */
#define PROPAGATE_EXCEPTION(status) ({if (status != 0) return status;})

/** Handle a panicked environment: return EINVAL if the env is panicked */
#define HANDLE_PANICKED_ENV(env) \
        RAISE_COND_EXCEPTION(toku_env_is_panicked(env), EINVAL)
/** Handle a panicked database: return EINVAL if the database env is panicked */
#define HANDLE_PANICKED_DB(db) HANDLE_PANICKED_ENV(db->dbenv)


/** Handle a transaction that has a child: return EINVAL if the transaction tries to do any work.
    Only commit/abort/prelock (which are used by handlerton) are allowed when a child exists.  */
#define HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn) \
        RAISE_COND_EXCEPTION(((txn) && db_txn_struct_i(txn)->child), \
                             toku_ydb_do_error((env),                \
                                               EINVAL,               \
                                               "%s: Transaction cannot do work when child exists\n", __FUNCTION__))

#define HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn) \
        HANDLE_ILLEGAL_WORKING_PARENT_TXN((db)->dbenv, txn)

#define HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c)   \
        HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN((c)->dbp, dbc_struct_i(c)->txn)

#define HANDLE_EXTRA_FLAGS(env, flags_to_function, allowed_flags) \
    RAISE_COND_EXCEPTION((env) && ((flags_to_function) & ~(allowed_flags)), \
			 toku_ydb_do_error((env),			\
					   EINVAL,			\
					   "Unknown flags (%"PRIu32") in "__FILE__ ":%s(): %d\n", (flags_to_function) & ~(allowed_flags), __FUNCTION__, __LINE__))


/* */
void toku_ydb_error_all_cases(const DB_ENV * env, 
                              int error, 
                              BOOL include_stderrstring, 
                              BOOL use_stderr_if_nothing_else, 
                              const char *fmt, va_list ap)
    __attribute__((format (printf, 5, 0)))
    __attribute__((__visibility__("default"))); // this is needed by the C++ interface. 

int toku_ydb_do_error (const DB_ENV *dbenv, int error, const char *string, ...)
                       __attribute__((__format__(__printf__, 3, 4)));

/* Location specific debug print-outs */
void toku_ydb_barf(void);
void toku_ydb_notef(const char *, ...);

/* Environment related errors */
int toku_env_is_panicked(DB_ENV *dbenv);
void toku_locked_env_err(const DB_ENV * env, int error, const char *fmt, ...) 
                         __attribute__((__format__(__printf__, 3, 4)));

typedef enum __toku_isolation_level { 
    TOKU_ISO_SERIALIZABLE=0,
    TOKU_ISO_SNAPSHOT=1,
    TOKU_ISO_READ_COMMITTED=2, 
    TOKU_ISO_READ_UNCOMMITTED=3
} TOKU_ISOLATION;

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    struct tokutxn *tokutxn;
    struct __toku_lth *lth;  //Hash table holding list of dictionaries this txn has touched
    u_int32_t flags;
    TOKU_ISOLATION iso;
    DB_TXN *child;
    struct toku_list dbs_that_must_close_before_abort;
};

struct __toku_dbc_internal {
    struct brt_cursor *c;
    DB_TXN *txn;
    TOKU_ISOLATION iso;
    struct simple_dbt skey_s,sval_s;
    struct simple_dbt *skey,*sval;
};

int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn, BOOL just_lock);

int toku_grab_write_lock (DB* db, DBT* key, TOKUTXN tokutxn);

int toku_grab_read_lock_on_directory (DB* db, DB_TXN * txn);

#if defined(__cplusplus)
}
#endif

#endif
