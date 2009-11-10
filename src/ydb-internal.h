/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <db.h>
#include "../newbrt/brttypes.h"
#include "../newbrt/brt.h"
#include "toku_list.h"
#include "./lock_tree/locktree.h"
#include "./lock_tree/db_id.h"
#include "./lock_tree/idlth.h"
#include <limits.h>

struct __toku_lock_tree;

struct __toku_db_internal {
    DB *db; // A pointer back to the DB.
    int freed;
    int opened;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    FILENUM fileid;
    struct __toku_lock_tree* lt;
    toku_db_id* db_id;
    struct simple_dbt skey, sval; // static key and value
    BOOL key_compare_was_set;     // true if a comparison function was provided before call to db->open()  (if false, use environment's comparison function)
    BOOL val_compare_was_set;
    char *dname;    // dname is constant for this handle (handle must be closed before file is renamed)
    BOOL is_zombie; // True if DB->close has been called on this DB
    struct toku_list dbs_that_must_close_before_abort;
};

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
    int (*dup_compare) (DB *, const DBT *, const DBT *);
    //void (*noticecall)(DB_ENV *, db_notices);
    unsigned long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
    toku_ltm* ltm;
    struct toku_list open_txns;
    DB *directory; //Maps dnames to inames
    DB *persistent_environment; //Stores environment settings, can be used for upgrade
    OMT open_dbs; //Stores open db handles, sorted first by dname and then by numerical value of pointer to the db (arbitrarily assigned memory location)
};

/* *********************************************************

   Ephemeral locking

   ********************************************************* */

typedef struct {
    u_int64_t        ydb_lock_ctr;            /* how many times has ydb lock been taken/released */ 
    u_int64_t        max_possible_sleep;      /* max possible sleep time for ydb lock scheduling (constant) */ 
    u_int64_t        processor_freq_mhz;      /* clock frequency in MHz */ 
    u_int64_t        max_requested_sleep;     /* max sleep time requested, can be larger than max possible */ 
    u_int64_t        times_max_sleep_used;    /* number of times the max_possible_sleep was used to sleep */ 
    u_int64_t        total_sleepers;          /* total number of times a client slept for ydb lock scheduling */ 
    u_int64_t        total_sleep_time;        /* total time spent sleeping for ydb lock scheduling */ 
    u_int64_t        max_waiters;             /* max number of simultaneous client threads kept waiting for ydb lock  */ 
    u_int64_t        total_waiters;           /* total number of times a client thread waited for ydb lock  */ 
    u_int64_t        total_clients;           /* total number of separate client threads that use ydb lock  */ 
    u_int64_t        time_ydb_lock_held_unavailable;  /* number of times a thread migrated and theld is unavailable */
    u_int64_t        max_time_ydb_lock_held;  /* max time a client thread held the ydb lock  */ 
    u_int64_t        total_time_ydb_lock_held;/* total time client threads held the ydb lock  */ 
} SCHEDULE_STATUS_S, *SCHEDULE_STATUS;



int toku_ydb_lock_init(void);
int toku_ydb_lock_destroy(void);
void toku_ydb_lock(void);
void toku_ydb_unlock(void);

void toku_ydb_lock_get_status(SCHEDULE_STATUS statp);


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

#endif
