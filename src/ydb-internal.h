/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <db.h>
#include <limits.h>

#include <ft/fttypes.h>
#include <ft/ft-ops.h>
#include <ft/minicron.h>
// TODO: remove vanilla omt in favor of templated one
#include <ft/omt.h>

#include <util/growable_array.h>
#include <util/omt.h>

#include <locktree/locktree.h>
#include <locktree/range_buffer.h>

#include <toku_list.h>

struct __toku_db_internal {
    int opened;
    uint32_t open_flags;
    int open_mode;
    FT_HANDLE ft_handle;
    DICTIONARY_ID dict_id;        // unique identifier used by locktree logic
    toku::locktree *lt;
    struct simple_dbt skey, sval; // static key and value
    bool key_compare_was_set;     // true if a comparison function was provided before call to db->open()  (if false, use environment's comparison function).  
    char *dname;                  // dname is constant for this handle (handle must be closed before file is renamed)
    DB_INDEXER *indexer;
};

int toku_db_set_indexer(DB *db, DB_INDEXER *indexer);
DB_INDEXER *toku_db_get_indexer(DB *db);

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
    uint32_t open_flags;
    int open_mode;
    toku_env_errcall_t errcall;
    void *errfile;
    const char *errpfx;
    char *dir;                  /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *lg_dir;
    char *data_dir;
    int (*bt_compare)  (DB *, const DBT *, const DBT *);
    int (*update_function)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);
    generate_row_for_put_func generate_row_for_put;
    generate_row_for_del_func generate_row_for_del;

    unsigned long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
    toku::locktree::manager ltm;

    DB *directory;                                      // Maps dnames to inames
    DB *persistent_environment;                         // Stores environment settings, can be used for upgrade
    // TODO: toku::omt<DB *>
    OMT open_dbs;                                       // Stores open db handles, sorted first by dname and then by numerical value of pointer to the db (arbitrarily assigned memory location)
    toku_mutex_t open_dbs_lock;                         // lock that protects the OMT of open dbs.

    char *real_data_dir;                                // data dir used when the env is opened (relative to cwd, or absolute with leading /)
    char *real_log_dir;                                 // log dir used when the env is opened  (relative to cwd, or absolute with leading /)
    char *real_tmp_dir;                                 // tmp dir used for temporary files (relative to cwd, or absoulte with leading /)

    fs_redzone_state fs_state;
    uint64_t fs_seq;                                    // how many times has fs_poller run?
    uint64_t last_seq_entered_red;
    uint64_t last_seq_entered_yellow;
    int redzone;                                        // percent of total fs space that marks boundary between yellow and red zones
    int enospc_redzone_ctr;                             // number of operations rejected by enospc prevention  (red zone)
    int fs_poll_time;                                   // Time in seconds between statfs calls
    struct minicron fs_poller;                          // Poll the file systems
    bool fs_poller_is_init;
    uint32_t fsync_log_period_ms;
    bool fsync_log_cron_is_init;
    struct minicron fsync_log_cron;                     // fsync recovery log
    int envdir_lockfd;
    int datadir_lockfd;
    int logdir_lockfd;
    int tmpdir_lockfd;
};

// test-only environment function for running lock escalation
static inline void toku_env_run_lock_escalation_for_test(DB_ENV *env) {
    toku::locktree::manager *mgr = &env->i->ltm;
    mgr->run_escalation_for_test();
}

// Common error handling macros and panic detection
#define MAYBE_RETURN_ERROR(cond, status) if (cond) return status;
#define HANDLE_PANICKED_ENV(env) if (toku_env_is_panicked(env)) { sleep(1); return EINVAL; }
#define HANDLE_PANICKED_DB(db) HANDLE_PANICKED_ENV(db->dbenv)

// Only commit/abort/prelock (which are used by handlerton) are allowed when a child exists.
#define HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn) \
        MAYBE_RETURN_ERROR(((txn) && db_txn_struct_i(txn)->child), \
                             toku_ydb_do_error((env),                \
                                               EINVAL,               \
                                               "%s: Transaction cannot do work when child exists\n", __FUNCTION__))

#define HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn) \
        HANDLE_ILLEGAL_WORKING_PARENT_TXN((db)->dbenv, txn)

#define HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c)   \
        HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN((c)->dbp, dbc_struct_i(c)->txn)

// Bail out if we get unknown flags
#define HANDLE_EXTRA_FLAGS(env, flags_to_function, allowed_flags) \
        MAYBE_RETURN_ERROR((env) && ((flags_to_function) & ~(allowed_flags)), \
			 toku_ydb_do_error((env),			\
					   EINVAL,			\
					   "Unknown flags (%" PRIu32 ") in " __FILE__ ":%s(): %d\n", (flags_to_function) & ~(allowed_flags), __FUNCTION__, __LINE__))

int toku_ydb_check_avail_fs_space(DB_ENV *env);

void toku_ydb_error_all_cases(const DB_ENV * env, 
                              int error, 
                              bool include_stderrstring, 
                              bool use_stderr_if_nothing_else, 
                              const char *fmt, va_list ap)
    __attribute__((format (printf, 5, 0)))
    __attribute__((__visibility__("default"))); // this is needed by the C++ interface. 

int toku_ydb_do_error (const DB_ENV *dbenv, int error, const char *string, ...)
                       __attribute__((__format__(__printf__, 3, 4)));

/* Environment related errors */
int toku_env_is_panicked(DB_ENV *dbenv);
void toku_env_err(const DB_ENV * env, int error, const char *fmt, ...) 
                         __attribute__((__format__(__printf__, 3, 4)));

typedef enum __toku_isolation_level { 
    TOKU_ISO_SERIALIZABLE=0,
    TOKU_ISO_SNAPSHOT=1,
    TOKU_ISO_READ_COMMITTED=2, 
    TOKU_ISO_READ_UNCOMMITTED=3
} TOKU_ISOLATION;

// needed in ydb_db.c
#define DB_ISOLATION_FLAGS (DB_READ_COMMITTED | DB_READ_UNCOMMITTED | DB_TXN_SNAPSHOT | DB_SERIALIZABLE | DB_INHERIT_ISOLATION)

struct txn_lock_range {
    DBT left;
    DBT right;
};

struct txn_lt_key_ranges {
    toku::locktree *lt;
    toku::range_buffer *buffer;
};

struct __toku_db_txn_internal {
    struct tokutxn *tokutxn;
    uint32_t flags;
    TOKU_ISOLATION iso;
    DB_TXN *child;
    toku_mutex_t txn_mutex;

    // maps a locktree to a buffer of key ranges that are locked.
    // it is protected by the txn_mutex, so hot indexing and a client
    // thread can concurrently operate on this txn.
    toku::omt<txn_lt_key_ranges> lt_map;
};

struct __toku_db_txn_external {
    struct __toku_db_txn           external_part;
    struct __toku_db_txn_internal  internal_part;
};
#define db_txn_struct_i(x) (&((struct __toku_db_txn_external *)x)->internal_part)

struct __toku_dbc_internal {
    struct ft_cursor *c;
    DB_TXN *txn;
    TOKU_ISOLATION iso;
    struct simple_dbt skey_s,sval_s;
    struct simple_dbt *skey,*sval;

    // if the rmw flag is asserted, cursor operations (like set) grab write locks instead of read locks
    // the rmw flag is set when the cursor is created with the DB_RMW flag set
    bool rmw;
};

struct __toku_dbc_external {
    struct __toku_dbc          external_part;
    struct __toku_dbc_internal internal_part;
};
	
#define dbc_struct_i(x) (&((struct __toku_dbc_external *)x)->internal_part)

static inline int 
env_opened(DB_ENV *env) {
    return env->i->cachetable != 0;
}

static inline bool
txn_is_read_only(DB_TXN* txn) {
    if (txn && (db_txn_struct_i(txn)->flags & DB_TXN_READ_ONLY)) {
        return true;
    }
    return false;
}

#define HANDLE_READ_ONLY_TXN(txn) if(txn_is_read_only(txn)) return EINVAL;

void env_panic(DB_ENV * env, int cause, const char * msg);
void env_note_db_opened(DB_ENV *env, DB *db);
void env_note_db_closed(DB_ENV *env, DB *db);

#endif
