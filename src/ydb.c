/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
 
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

const char *toku_patent_string = "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it.";
const char *toku_copyright_string = "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved.";

#include <toku_portability.h>
#include <toku_pthread.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <db.h>
#include "toku_assert.h"
#include "ydb.h"
#include "ydb-internal.h"
#include "brt-internal.h"
#include "cachetable.h"
#include "log.h"
#include "memory.h"
#include "dlmalloc.h"
#include "checkpoint.h"
#include "key.h"
#include "loader.h"
#include "indexer.h"
#include "ydb_load.h"
#include "brtloader.h"

#ifdef TOKUTRACE
 #define DB_ENV_CREATE_FUN db_env_create_toku10
 #define DB_CREATE_FUN db_create_toku10
#else
 #define DB_ENV_CREATE_FUN db_env_create
 #define DB_CREATE_FUN db_create
 int toku_set_trace_file (char *fname __attribute__((__unused__))) { return 0; }
 int toku_close_trace_file (void) { return 0; } 
#endif

#define DB_ISOLATION_FLAGS (DB_READ_COMMITTED | DB_READ_UNCOMMITTED | DB_TXN_SNAPSHOT | DB_SERIALIZABLE | DB_INHERIT_ISOLATION)

// Accountability: operation counters available for debugging and for "show engine status"
static u_int64_t num_inserts;
static u_int64_t num_inserts_fail;
static u_int64_t num_deletes;
static u_int64_t num_deletes_fail;
static u_int64_t num_updates;
static u_int64_t num_updates_fail;
static u_int64_t num_multi_inserts;
static u_int64_t num_multi_inserts_fail;
static u_int64_t num_multi_deletes;
static u_int64_t num_multi_deletes_fail;
static u_int64_t num_multi_updates;
static u_int64_t num_multi_updates_fail;
static u_int64_t num_point_queries;
static u_int64_t num_sequential_queries;
static u_int64_t logsuppress;                // number of times logs are suppressed for empty table (2440)
static u_int64_t logsuppressfail;            // number of times unable to suppress logs for empty table (2440)
static time_t    startuptime;                // timestamp of system startup
static DB_ENV *  most_recent_env;            // most recently opened env, used for engine status on crash

static void
init_status_info(void) {
    num_inserts = 0;
    num_inserts_fail = 0;
    num_deletes = 0;
    num_deletes_fail = 0;
    num_updates = 0;
    num_updates_fail = 0;
    num_multi_inserts = 0;
    num_multi_inserts_fail = 0;
    num_multi_deletes = 0;
    num_multi_deletes_fail = 0;
    num_multi_updates = 0;
    num_multi_updates_fail = 0;
    num_point_queries = 0;
    num_sequential_queries = 0;
    logsuppress = 0;
    logsuppressfail = 0;
    startuptime = time(NULL);
}

const char * environmentdictionary = "tokudb.environment";
const char * fileopsdirectory = "tokudb.directory";

static int env_get_iname(DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt);

static const char single_process_lock_file[] = "/__tokudb_lock_dont_delete_me_";

static int
single_process_lock(const char *lock_dir, const char *which, int *lockfd) {
    if (!lock_dir)
        return ENOENT;
    int namelen=strlen(lock_dir)+strlen(which);
    char lockfname[namelen+sizeof(single_process_lock_file)];

    int l = snprintf(lockfname, sizeof(lockfname), "%s%s%s", lock_dir, single_process_lock_file, which);
    assert(l+1 == (signed)(sizeof(lockfname)));
    *lockfd = toku_os_lock_file(lockfname);
    if (*lockfd < 0) {
        int e = errno;
        fprintf(stderr, "Couldn't start tokudb because some other tokudb process is using the same directory [%s] for [%s]\n", lock_dir, which);
        return e;
    }
    return 0;
}

static int
single_process_unlock(int *lockfd) {
    int fd = *lockfd;
    *lockfd = -1;
    if (fd>=0) {
        int r = toku_os_unlock_file(fd);
        if (r != 0)
            return errno;
    }
    return 0;
}

/** The default maximum number of persistent locks in a lock tree  */
const u_int32_t __toku_env_default_max_locks = 0x7FFFFFFF;
const uint64_t __toku_env_default_max_lock_memory = 1000*1024;

static inline DBT*
init_dbt_realloc(DBT *dbt) {
    memset(dbt, 0, sizeof(*dbt));
    dbt->flags = DB_DBT_REALLOC;
    return dbt;
}

//Callback used for redirecting dictionaries.
static void
ydb_set_brt(DB *db, BRT brt) {
    db->i->brt = brt;
}

int 
toku_ydb_init(void) {
    int r = 0;
    //Lower level must be initialized first.
    if (r==0) 
        r = toku_brt_init(toku_ydb_lock, toku_ydb_unlock, ydb_set_brt);
    if (r==0) 
        r = toku_ydb_lock_init();
    return r;
}

int 
toku_ydb_destroy(void) {
    int r = 0;
    if (r==0)
        r = toku_ydb_lock_destroy();
    //Lower level must be cleaned up last.
    if (r==0)
        r = toku_brt_destroy();
    return r;
}

static int
ydb_getf_do_nothing(DBT const* UU(key), DBT const* UU(val), void* UU(extra)) {
    return 0;
}

/* env methods */
static int toku_env_close(DB_ENV *env, u_int32_t flags);
static int toku_env_set_data_dir(DB_ENV * env, const char *dir);
static int toku_env_set_lg_dir(DB_ENV * env, const char *dir);
static int toku_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir);

static inline int 
env_opened(DB_ENV *env) {
    return env->i->cachetable != 0;
}

static void 
env_init_open_txn(DB_ENV *env) {
    toku_list_init(&env->i->open_txns);
}

// add a txn to the list of open txn's
static void 
env_add_open_txn(DB_ENV *env, DB_TXN *txn) {
    toku_list_push(&env->i->open_txns, (struct toku_list *) (void *) &txn->open_txns);
}

// remove a txn from the list of open txn's
static void 
env_remove_open_txn(DB_ENV *UU(env), DB_TXN *txn) {
    toku_list_remove((struct toku_list *) (void *) &txn->open_txns);
}

static int toku_txn_abort(DB_TXN * txn, TXN_PROGRESS_POLL_FUNCTION, void*);

static void
env_fs_report_in_yellow(DB_ENV *UU(env)) {
    char tbuf[26];
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb file system space is low\n", ctime_r(&tnow, tbuf)); fflush(stderr);
}

static void
env_fs_report_in_red(DB_ENV *UU(env)) {
    char tbuf[26];
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb file system space is really low and access is restricted\n", ctime_r(&tnow, tbuf)); fflush(stderr);
}

static inline uint64_t
env_fs_redzone(DB_ENV *env, uint64_t total) {
    return total * env->i->redzone / 100;
}

#define ZONEREPORTLIMIT 12
// Check the available space in the file systems used by tokudb and erect barriers when available space gets low.
static int
env_fs_poller(void *arg) {
    if (0) printf("%s:%d %p\n", __FUNCTION__, __LINE__, arg);
 
    DB_ENV *env = (DB_ENV *) arg;
    int r;
#if 0
    // get the cachetable size limit (not yet needed)
    uint64_t cs = toku_cachetable_get_size_limit(env->i->cachetable);
#endif

    int in_yellow; // set true to issue warning to user
    int in_red;    // set true to prevent certain operations (returning ENOSPC)

    // get the fs sizes for the home dir
    uint64_t avail_size, total_size;
    r = toku_get_filesystem_sizes(env->i->dir, &avail_size, NULL, &total_size);
    assert(r == 0);
    if (0) fprintf(stderr, "%s %"PRIu64" %"PRIu64"\n", env->i->dir, avail_size, total_size);
    in_yellow = (avail_size < 2 * env_fs_redzone(env, total_size));
    in_red = (avail_size < env_fs_redzone(env, total_size));
    
    // get the fs sizes for the data dir if different than the home dir
    if (strcmp(env->i->dir, env->i->real_data_dir) != 0) {
        r = toku_get_filesystem_sizes(env->i->real_data_dir, &avail_size, NULL, &total_size);
        assert(r == 0);
        if (0) fprintf(stderr, "%s %"PRIu64" %"PRIu64"\n", env->i->real_data_dir, avail_size, total_size);
        in_yellow += (avail_size < 2 * env_fs_redzone(env, total_size));
        in_red += (avail_size < env_fs_redzone(env, total_size));
    }

    // get the fs sizes for the log dir if different than the home dir and data dir
    if (strcmp(env->i->dir, env->i->real_log_dir) != 0 && strcmp(env->i->real_data_dir, env->i->real_log_dir) != 0) {
        r = toku_get_filesystem_sizes(env->i->real_log_dir, &avail_size, NULL, &total_size);
        assert(r == 0);
        if (0) fprintf(stderr, "%s %"PRIu64" %"PRIu64"\n", env->i->real_log_dir, avail_size, total_size);
        in_yellow += (avail_size < 2 * env_fs_redzone(env, total_size));
        in_red += (avail_size < env_fs_redzone(env, total_size));
    }

    
    env->i->fs_seq++;                    // how many times through this polling loop?
    uint64_t now = env->i->fs_seq;

    // Don't issue report if we have not been out of this fs_state for a while, unless we're at system startup
    switch (env->i->fs_state) {
    case FS_RED:
	if (!in_red) {
	    if (in_yellow) {
		env->i->fs_state = FS_YELLOW;
	    } else {
		env->i->fs_state = FS_GREEN;
	    }
	}
        break;
    case FS_YELLOW:
        if (in_red) {
	    if ((now - env->i->last_seq_entered_red > ZONEREPORTLIMIT) || (now < ZONEREPORTLIMIT))
		env_fs_report_in_red(env);
            env->i->fs_state = FS_RED;
	    env->i->last_seq_entered_red = now;
        } else if (!in_yellow) {
            env->i->fs_state = FS_GREEN;
        }
        break;
    case FS_GREEN:
        if (in_red) {
	    if ((now - env->i->last_seq_entered_red > ZONEREPORTLIMIT) || (now < ZONEREPORTLIMIT))
		env_fs_report_in_red(env);
            env->i->fs_state = FS_RED;
	    env->i->last_seq_entered_red = now;
        } else if (in_yellow) {
	    if ((now - env->i->last_seq_entered_yellow > ZONEREPORTLIMIT) || (now < ZONEREPORTLIMIT))
		env_fs_report_in_yellow(env);
            env->i->fs_state = FS_YELLOW;
	    env->i->last_seq_entered_yellow = now;
        }
        break;
    }
    return 0;
}
#undef ZONEREPORTLIMIT

static void
env_fs_init(DB_ENV *env) {
    env->i->fs_state = FS_GREEN;
    env->i->fs_poll_time = 5;  // seconds
    env->i->redzone = 5;       // percent of total space
    env->i->fs_poller_is_init = FALSE;
}

// Initialize the minicron that polls file system space
static int
env_fs_init_minicron(DB_ENV *env) {
    int r = toku_minicron_setup(&env->i->fs_poller, env->i->fs_poll_time, env_fs_poller, env); 
    assert(r == 0);
    env->i->fs_poller_is_init = TRUE;
    return r;
}

// Destroy the file system space minicron
static void
env_fs_destroy(DB_ENV *env) {
    if (env->i->fs_poller_is_init) {
        int r = toku_minicron_shutdown(&env->i->fs_poller);
        assert(r == 0);
        env->i->fs_poller_is_init = FALSE;
    }
}

// Check if the available file system space is less than the reserve
// Returns ENOSPC if not enough space, othersize 0
static inline int 
env_check_avail_fs_space(DB_ENV *env) {
    int r = env->i->fs_state == FS_RED ? ENOSPC : 0; 
    if (r) env->i->enospc_redzone_ctr++;
    return r;
}

int
toku_ydb_check_avail_fs_space(DB_ENV *env) {
    int rval = env_check_avail_fs_space(env);
    return rval;
}

/* db methods */
static inline int db_opened(DB *db) {
    return db->i->opened != 0;
}


static int toku_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags);
static int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags);
static int toku_db_cursor(DB *db, DB_TXN * txn, DBC **c, u_int32_t flags, int is_temporary_cursor);

/* txn methods */

/* lightweight cursor methods. */
static int toku_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_set(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_set_range(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_set_range_reverse(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);

// Effect: Lightweight cursor get

/* cursor methods */
static int toku_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag);
static int toku_c_del(DBC *c, u_int32_t flags);
static int toku_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags);
static int toku_c_close(DBC * c);

static void
env_setup_real_dir(DB_ENV *env, char **real_dir, const char *nominal_dir) {
    toku_free(*real_dir);
    *real_dir = NULL;

    assert(env->i->dir);
    if (nominal_dir) 
	*real_dir = toku_construct_full_name(2, env->i->dir, nominal_dir);
    else
        *real_dir = toku_strdup(env->i->dir);
}

static void
env_setup_real_data_dir(DB_ENV *env) {
    env_setup_real_dir(env, &env->i->real_data_dir, env->i->data_dir);
}

static void
env_setup_real_log_dir(DB_ENV *env) {
    env_setup_real_dir(env, &env->i->real_log_dir, env->i->lg_dir);
}

static void
env_setup_real_tmp_dir(DB_ENV *env) {
    env_setup_real_dir(env, &env->i->real_tmp_dir, env->i->tmp_dir);
}

static int 
ydb_do_recovery (DB_ENV *env) {
    assert(env->i->real_log_dir);
    toku_ydb_unlock();
    int r = tokudb_recover(env->i->dir, env->i->real_log_dir, env->i->bt_compare,
                           env->i->generate_row_for_put, env->i->generate_row_for_del,
                           env->i->cachetable_size);
    toku_ydb_lock();
    return r;
}

static int 
needs_recovery (DB_ENV *env) {
    assert(env->i->real_log_dir);
    int recovery_needed = tokudb_needs_recovery(env->i->real_log_dir, TRUE);
    return recovery_needed ? DB_RUNRECOVERY : 0;
}

static int toku_db_create(DB ** db, DB_ENV * env, u_int32_t flags);
static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode);
static int toku_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte, u_int32_t min, u_int32_t flags);
static int toku_db_close(DB * db, u_int32_t flags);
static int toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags, int internal);
static int toku_txn_commit(DB_TXN * txn, u_int32_t flags, TXN_PROGRESS_POLL_FUNCTION, void*);
static int db_open_iname(DB * db, DB_TXN * txn, const char *iname, u_int32_t flags, int mode);

static void finalize_file_removal(DICTIONARY_ID dict_id, void * extra);

// Instruct db to use the default (built-in) key comparison function
// by setting the flag bits in the db and brt structs
static int
db_use_builtin_key_cmp(DB *db) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (db_opened(db))
        r = toku_ydb_do_error(db->dbenv, EINVAL, "Comparison functions cannot be set after DB open.\n");
    else if (db->i->key_compare_was_set)
        r = toku_ydb_do_error(db->dbenv, EINVAL, "Key comparison function already set.\n");
    else {
        u_int32_t tflags;
        r = toku_brt_get_flags(db->i->brt, &tflags);
        if (r!=0) return r;

        tflags |= TOKU_DB_KEYCMP_BUILTIN;
        r = toku_brt_set_flags(db->i->brt, tflags);
        if (!r)
            db->i->key_compare_was_set = TRUE;
    }
    return r;
}

// Keys used in persistent environment dictionary:
// Following keys added in version 12
static const char * orig_env_ver_key = "original_version";
static const char * curr_env_ver_key = "current_version";  
// Following keys added in version 13
static const char * creation_time_key = "creation_time";
static const char * last_lsn_of_v12_key = "last_lsn_of_v12";
static const char * upgrade_v13_time_key = "upgrade_v13_time";  // Add more keys for future upgrades

// Values read from (or written into) persistent environment,
// kept here for read-only access from engine status.
static uint32_t persistent_original_env_version;
static uint32_t persistent_stored_env_version_at_startup;    // read from curr_env_ver_key, prev version as of this startup
static time_t   persistent_creation_time;
static uint64_t persistent_last_lsn_of_v12;
static time_t   persistent_upgrade_v13_time;

// Requires: persistent environment dictionary is already open.
// Input arg is lsn of clean shutdown of previous version,
// or ZERO_LSN if no upgrade or if crash between log upgrade and here.
static int
maybe_upgrade_persistent_environment_dictionary(DB_ENV * env, DB_TXN * txn, LSN last_lsn_of_clean_shutdown_read_from_log) {
    int r;
    DBT key, val;
    DB *persistent_environment = env->i->persistent_environment;

    toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
    toku_init_dbt(&val);
    r = toku_db_get(persistent_environment, txn, &key, &val, 0);
    assert(r == 0);
    uint32_t stored_env_version = toku_dtoh32(*(uint32_t*)val.data);
    persistent_stored_env_version_at_startup = stored_env_version;
    if (stored_env_version > BRT_LAYOUT_VERSION)
	r = TOKUDB_DICTIONARY_TOO_NEW;
    else if (stored_env_version < BRT_LAYOUT_MIN_SUPPORTED_VERSION)
	r = TOKUDB_DICTIONARY_TOO_OLD;
    else if (stored_env_version < BRT_LAYOUT_VERSION) {
        const uint32_t curr_env_ver_d = toku_htod32(BRT_LAYOUT_VERSION);
        toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
        toku_fill_dbt(&val, &curr_env_ver_d, sizeof(curr_env_ver_d));
        r = toku_db_put(persistent_environment, txn, &key, &val, DB_YESOVERWRITE);
        assert(r==0);
	
	uint64_t last_lsn_of_v12_d = toku_htod64(last_lsn_of_clean_shutdown_read_from_log.lsn);
	toku_fill_dbt(&key, last_lsn_of_v12_key, strlen(last_lsn_of_v12_key));
	toku_fill_dbt(&val, &last_lsn_of_v12_d, sizeof(last_lsn_of_v12_d));
	r = toku_db_put(persistent_environment, txn, &key, &val, DB_YESOVERWRITE);
        assert(r==0);
	
	time_t upgrade_v13_time_d = toku_htod64(time(NULL));
	toku_fill_dbt(&key, upgrade_v13_time_key, strlen(upgrade_v13_time_key));
	toku_fill_dbt(&val, &upgrade_v13_time_d, sizeof(upgrade_v13_time_d));
	r = toku_db_put(persistent_environment, txn, &key, &val, DB_NOOVERWRITE);
        assert(r==0);
    }
    return r;
}


// Capture persistent env contents to be read by engine status
static void
capture_persistent_env (DB_ENV * env, DB_TXN * txn) {
    int r;
    DBT key, val;
    DB *persistent_environment = env->i->persistent_environment;

    toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
    toku_init_dbt(&val);
    r = toku_db_get(persistent_environment, txn, &key, &val, 0);
    assert(r == 0);
    uint32_t curr_env_version = toku_dtoh32(*(uint32_t*)val.data);
    assert(curr_env_version == BRT_LAYOUT_VERSION);

    toku_fill_dbt(&key, orig_env_ver_key, strlen(orig_env_ver_key));
    toku_init_dbt(&val);
    r = toku_db_get(persistent_environment, txn, &key, &val, 0);
    assert(r == 0);
    persistent_original_env_version = toku_dtoh32(*(uint32_t*)val.data);
    assert(persistent_original_env_version <= curr_env_version);

    // make no assertions about timestamps, clock may have been reset
    if (persistent_original_env_version >= BRT_LAYOUT_VERSION_13) {
	toku_fill_dbt(&key, creation_time_key, strlen(creation_time_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	persistent_creation_time = toku_dtoh64((*(time_t*)val.data));
    }

    if (persistent_original_env_version != curr_env_version) {
	// an upgrade was performed at some time, capture info about the upgrade
	
	toku_fill_dbt(&key, last_lsn_of_v12_key, strlen(last_lsn_of_v12_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	persistent_last_lsn_of_v12 = toku_dtoh64(*(uint32_t*)val.data);

	toku_fill_dbt(&key, upgrade_v13_time_key, strlen(upgrade_v13_time_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	persistent_upgrade_v13_time = toku_dtoh64((*(time_t*)val.data));
    }

}




// return 0 if log exists or ENOENT if log does not exist
static int
ydb_recover_log_exists(DB_ENV *env) {
    int r = tokudb_recover_log_exists(env->i->real_log_dir);
    return r;
}


// Validate that all required files are present, no side effects.
// Return 0 if all is well, ENOENT if some files are present but at least one is missing, 
// other non-zero value if some other error occurs.
// Set *valid_newenv if creating a new environment (all files missing).
// (Note, if special dictionaries exist, then they were created transactionally and log should exist.)
static int 
validate_env(DB_ENV * env, BOOL * valid_newenv, BOOL need_rollback_cachefile) {
    int r;
    BOOL expect_newenv = FALSE;        // set true if we expect to create a new env
    toku_struct_stat buf;
    char* path = NULL;

    // Test for persistent environment
    path = toku_construct_full_name(2, env->i->dir, environmentdictionary);
    assert(path);
    r = toku_stat(path, &buf);
    int stat_errno = errno;
    toku_free(path);
    if (r == 0) {
	expect_newenv = FALSE;  // persistent info exists
    }
    else if (stat_errno == ENOENT) {
	expect_newenv = TRUE;
	r = 0;
    }
    else {
	r = toku_ydb_do_error(env, errno, "Unable to access persistent environment\n");
	assert(r);
    }

    // Test for existence of rollback cachefile if it is expected to exist
    if (r == 0 && need_rollback_cachefile) {
	path = toku_construct_full_name(2, env->i->dir, ROLLBACK_CACHEFILE_NAME);
	assert(path);
	r = toku_stat(path, &buf);
	stat_errno = errno;
	toku_free(path);
	if (r == 0) {  
	    if (expect_newenv)  // rollback cachefile exists, but persistent env is missing
		r = toku_ydb_do_error(env, ENOENT, "Persistent environment is missing\n");
	}
	else if (stat_errno == ENOENT) {
	    if (!expect_newenv)  // rollback cachefile is missing but persistent env exists
		r = toku_ydb_do_error(env, ENOENT, "rollback cachefile directory is missing\n");
	    else 
		r = 0;           // both rollback cachefile and persistent env are missing
	}
	else {
	    r = toku_ydb_do_error(env, stat_errno, "Unable to access rollback cachefile\n");
	    assert(r);
	}
    }

    // Test for fileops directory
    if (r == 0) {
	path = toku_construct_full_name(2, env->i->dir, fileopsdirectory);
	assert(path);
	r = toku_stat(path, &buf);
	stat_errno = errno;
	toku_free(path);
	if (r == 0) {  
	    if (expect_newenv)  // fileops directory exists, but persistent env is missing
		r = toku_ydb_do_error(env, ENOENT, "Persistent environment is missing\n");
	}
	else if (stat_errno == ENOENT) {
	    if (!expect_newenv)  // fileops directory is missing but persistent env exists
		r = toku_ydb_do_error(env, ENOENT, "Fileops directory is missing\n");
	    else 
		r = 0;           // both fileops directory and persistent env are missing
	}
	else {
	    r = toku_ydb_do_error(env, stat_errno, "Unable to access fileops directory\n");
	    assert(r);
	}
    }

    // Test for recovery log
    if ((r == 0) && (env->i->open_flags & DB_INIT_LOG)) {
	// if using transactions, test for existence of log
	r = ydb_recover_log_exists(env);  // return 0 or ENOENT
	if (expect_newenv && (r != ENOENT))
	    r = toku_ydb_do_error(env, ENOENT, "Persistent environment information is missing (but log exists)\n");
	else if (!expect_newenv && r == ENOENT)
	    r = toku_ydb_do_error(env, ENOENT, "Recovery log is missing (persistent environment information is present)\n");
	else
	    r = 0;
    }

    if (r == 0)
	*valid_newenv = expect_newenv;
    else 
	*valid_newenv = FALSE;
    return r;
}

static int
ydb_maybe_upgrade_env (DB_ENV *env, LSN * last_lsn_of_clean_shutdown_read_from_log, BOOL * upgrade_in_progress) {
    int r = 0;
    if (env->i->open_flags & DB_INIT_TXN && env->i->open_flags & DB_INIT_LOG) {
        toku_ydb_unlock();
        r = toku_maybe_upgrade_log(env->i->dir, env->i->real_log_dir, last_lsn_of_clean_shutdown_read_from_log, upgrade_in_progress);
        toku_ydb_lock();
    }
    return r;
}


static void
unlock_single_process(DB_ENV *env) {
    int r;
    r = single_process_unlock(&env->i->envdir_lockfd);
    lazy_assert(r==0);
    r = single_process_unlock(&env->i->datadir_lockfd);
    lazy_assert(r==0);
    r = single_process_unlock(&env->i->logdir_lockfd);
    lazy_assert(r==0);
    r = single_process_unlock(&env->i->tmpdir_lockfd);
    lazy_assert(r==0);
}

// Open the environment.
// If this is a new environment, then create the necessary files.
// Return 0 on success, ENOENT if any of the expected necessary files are missing.
// (The set of necessary files is defined in the function validate_env() above.)
static int 
toku_env_open(DB_ENV * env, const char *home, u_int32_t flags, int mode) {
    HANDLE_PANICKED_ENV(env);
    int r;
    BOOL newenv;  // true iff creating a new environment
    u_int32_t unused_flags=flags;

    if (env_opened(env)) {
	r = toku_ydb_do_error(env, EINVAL, "The environment is already open\n");
        goto cleanup;
    }

    assert(sizeof(time_t) == sizeof(uint64_t));

    HANDLE_EXTRA_FLAGS(env, flags, 
                       DB_CREATE|DB_PRIVATE|DB_INIT_LOG|DB_INIT_TXN|DB_RECOVER|DB_INIT_MPOOL|DB_INIT_LOCK|DB_THREAD);


    // DB_CREATE means create if env does not exist, and Tokudb requires it because
    // Tokudb requries DB_PRIVATE.
    if ((flags & DB_PRIVATE) && !(flags & DB_CREATE)) {
	r = toku_ydb_do_error(env, ENOENT, "DB_PRIVATE requires DB_CREATE (seems gratuitous to us, but that's BDB's behavior\n");
        goto cleanup;
    }

    if (!(flags & DB_PRIVATE)) {
	r = toku_ydb_do_error(env, ENOENT, "TokuDB requires DB_PRIVATE\n");
        goto cleanup;
    }

    if ((flags & DB_INIT_LOG) && !(flags & DB_INIT_TXN)) {
	r = toku_ydb_do_error(env, EINVAL, "TokuDB requires transactions for logging\n");
        goto cleanup;
    }

    if (!home) home = ".";

    // Verify that the home exists.
    {
	BOOL made_new_home = FALSE;
        char* new_home = NULL;
    	toku_struct_stat buf;
        if (strlen(home) > 1 && home[strlen(home)-1] == '\\') {
            new_home = toku_malloc(strlen(home));
            memcpy(new_home, home, strlen(home));
            new_home[strlen(home) - 1] = 0;
            made_new_home = TRUE;
        }
    	r = toku_stat(made_new_home? new_home : home, &buf);
        if (made_new_home) {
            toku_free(new_home);
        }
    	if (r!=0) {
    	    r = toku_ydb_do_error(env, errno, "Error from toku_stat(\"%s\",...)\n", home);
            goto cleanup;
    	}
    }
    unused_flags &= ~DB_PRIVATE;

    if (env->i->dir)
        toku_free(env->i->dir);
    env->i->dir = toku_strdup(home);
    if (env->i->dir == 0) {
	r = toku_ydb_do_error(env, ENOMEM, "Out of memory\n");
        goto cleanup;
    }
    if (0) {
        died1:
        toku_free(env->i->dir);
        env->i->dir = NULL;
        goto cleanup;
    }
    env->i->open_flags = flags;
    env->i->open_mode = mode;

    env_setup_real_data_dir(env);
    env_setup_real_log_dir(env);
    env_setup_real_tmp_dir(env);

    r = single_process_lock(env->i->dir, "environment", &env->i->envdir_lockfd);
    if (r!=0) goto cleanup;
    r = single_process_lock(env->i->real_data_dir, "data", &env->i->datadir_lockfd);
    if (r!=0) goto cleanup;
    r = single_process_lock(env->i->real_log_dir, "logs", &env->i->logdir_lockfd);
    if (r!=0) goto cleanup;
    r = single_process_lock(env->i->real_tmp_dir, "temp", &env->i->tmpdir_lockfd);
    if (r!=0) goto cleanup;


    BOOL need_rollback_cachefile = FALSE;
    if (flags & (DB_INIT_TXN | DB_INIT_LOG)) {
        need_rollback_cachefile = TRUE;
    }

    init_status_info();  // do this before possibly upgrading, so upgrade work is counted in status counters

    LSN last_lsn_of_clean_shutdown_read_from_log = ZERO_LSN;
    BOOL upgrade_in_progress = FALSE;
    r = ydb_maybe_upgrade_env(env, &last_lsn_of_clean_shutdown_read_from_log, &upgrade_in_progress);
    if (r!=0) goto cleanup;

    if (upgrade_in_progress) {
	// Delete old rollback file.  There was a clean shutdown, so it has nothing useful,
	// and there is no value in upgrading it.  It is simpler to just create a new one.
	char* rollback_filename = toku_construct_full_name(2, env->i->dir, ROLLBACK_CACHEFILE_NAME);
	assert(rollback_filename);
	r = unlink(rollback_filename);
	toku_free(rollback_filename);
	assert(r==0 || errno==ENOENT);	
	need_rollback_cachefile = FALSE;  // we're not expecting it to exist now
    }
    
    r = validate_env(env, &newenv, need_rollback_cachefile);  // make sure that environment is either new or complete
    if (r != 0) goto cleanup;

    unused_flags &= ~DB_INIT_TXN & ~DB_INIT_LOG;

    // do recovery only if there exists a log and recovery is requested
    // otherwise, a log is created when the logger is opened later
    if (!newenv) {
        if (flags & DB_INIT_LOG) {
            // the log does exist
            if (flags & DB_RECOVER) {
                r = ydb_do_recovery(env);
                if (r != 0) goto cleanup;
            } else {
                // the log is required to have clean shutdown if recovery is not requested
                r = needs_recovery(env);
                if (r != 0) goto cleanup;
            }
        }
    }
    
    toku_loader_cleanup_temp_files(env);

    if (flags & (DB_INIT_TXN | DB_INIT_LOG)) {
	assert(env->i->logger);
        toku_logger_write_log_files(env->i->logger, (BOOL)((flags & DB_INIT_LOG) != 0));
        r = toku_logger_open(env->i->real_log_dir, env->i->logger);
	if (r!=0) {
	    toku_ydb_do_error(env, r, "Could not open logger\n");
	died2:
	    toku_logger_close(&env->i->logger);
	    goto died1;
	}
    } else {
	r = toku_logger_close(&env->i->logger); // if no logging system, then kill the logger
	assert(r==0);
    }

    unused_flags &= ~DB_INIT_MPOOL; // we always init an mpool.
    unused_flags &= ~DB_CREATE;     // we always do DB_CREATE
    unused_flags &= ~DB_INIT_LOCK;  // we check this later (e.g. in db->open)
    unused_flags &= ~DB_RECOVER;

// This is probably correct, but it will be pain...
//    if ((flags & DB_THREAD)==0) {
//	r = toku_ydb_do_error(env, EINVAL, "TokuDB requires DB_THREAD");
//	goto cleanup;
//    }
    unused_flags &= ~DB_THREAD;

    if (unused_flags!=0) {
	r = toku_ydb_do_error(env, EINVAL, "Extra flags not understood by tokudb: %u\n", unused_flags);
        goto cleanup;
    }

    r = toku_brt_create_cachetable(&env->i->cachetable, env->i->cachetable_size, ZERO_LSN, env->i->logger);
    if (r!=0) goto died2;

    toku_cachetable_set_env_dir(env->i->cachetable, env->i->dir);

    int using_txns = env->i->open_flags & DB_INIT_TXN;
    if (env->i->logger) {
	// if this is a newborn env or if this is an upgrade, then create a brand new rollback file
	BOOL create_new_rollback_file = newenv | upgrade_in_progress;
	assert (using_txns);
	toku_logger_set_cachetable(env->i->logger, env->i->cachetable);
	toku_logger_set_remove_finalize_callback(env->i->logger, finalize_file_removal, env->i->ltm);
        r = toku_logger_open_rollback(env->i->logger, env->i->cachetable, create_new_rollback_file);
        assert(r==0);
    }

    DB_TXN *txn=NULL;
    if (using_txns) {
        r = toku_txn_begin(env, 0, &txn, 0, 1);
        assert(r==0);
    }

    {
        r = toku_db_create(&env->i->persistent_environment, env, 0);
        assert(r==0);
        r = db_use_builtin_key_cmp(env->i->persistent_environment);
        assert(r==0);
	r = db_open_iname(env->i->persistent_environment, txn, environmentdictionary, DB_CREATE, mode);
	assert(r==0);
	if (newenv) {
	    // create new persistent_environment
	    DBT key, val;
	    persistent_original_env_version = BRT_LAYOUT_VERSION;
	    const uint32_t environment_version = toku_htod32(persistent_original_env_version);

	    toku_fill_dbt(&key, orig_env_ver_key, strlen(orig_env_ver_key));
	    toku_fill_dbt(&val, &environment_version, sizeof(environment_version));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0);
	    assert(r==0);

	    toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
	    toku_fill_dbt(&val, &environment_version, sizeof(environment_version));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0);
	    assert(r==0);

	    time_t creation_time_d = toku_htod64(time(NULL));
	    toku_fill_dbt(&key, creation_time_key, strlen(creation_time_key));
	    toku_fill_dbt(&val, &creation_time_d, sizeof(creation_time_d));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0);
	    assert(r==0);
	}
	else {
	    r = maybe_upgrade_persistent_environment_dictionary(env, txn, last_lsn_of_clean_shutdown_read_from_log);
	    assert(r==0);
	}
	capture_persistent_env(env, txn);
    }
    {
        r = toku_db_create(&env->i->directory, env, 0);
        assert(r==0);
        r = db_use_builtin_key_cmp(env->i->directory);
        assert(r==0);
        r = db_open_iname(env->i->directory, txn, fileopsdirectory, DB_CREATE, mode);
        assert(r==0);
    }
    if (using_txns) {
        r = toku_txn_commit(txn, 0, NULL, NULL);
        assert(r==0);
    }
    toku_ydb_unlock();
    r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL);
    assert(r==0);
    toku_ydb_lock();
    env_fs_poller(env);          // get the file system state at startup
    env_fs_init_minicron(env); 
cleanup:
    if (r!=0) {
        if (env && env->i) {
            unlock_single_process(env);
        }
    }
    if (r == 0) {
	errno = 0; // tabula rasa
	most_recent_env = env;
    }
    return r;
}

static int 
toku_env_close(DB_ENV * env, u_int32_t flags) {
    int r = 0;

    // if panicked, or if any open transactions, or any open dbs, then do nothing.

    if (toku_env_is_panicked(env)) goto panic_and_quit_early;
    if (!toku_list_empty(&env->i->open_txns)) {
        r = toku_ydb_do_error(env, EINVAL, "Cannot close environment due to open transactions\n");
        goto panic_and_quit_early;
    }
    { //Verify open dbs. Zombies are ok at this stage, fully open is not.
        uint32_t size = toku_omt_size(env->i->open_dbs);
        assert(size == env->i->num_open_dbs + env->i->num_zombie_dbs);
        if (env->i->num_open_dbs > 0) {
            r = toku_ydb_do_error(env, EINVAL, "Cannot close environment due to open DBs\n");
            goto panic_and_quit_early;
        }
    }
    {
        if (env->i->persistent_environment) {
            r = toku_db_close(env->i->persistent_environment, 0);
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close persistent environment dictionary (DB->close error)\n");
                goto panic_and_quit_early;
            }
        }
        if (env->i->directory) {
            r = toku_db_close(env->i->directory, 0);
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close Directory dictionary (DB->close error)\n");
                goto panic_and_quit_early;
            }
        }
    }
    if (env->i->cachetable) {
	toku_ydb_unlock();  // ydb lock must not be held when shutting down minicron
	toku_cachetable_minicron_shutdown(env->i->cachetable);
        if (env->i->logger) {
            if ( flags && DB_CLOSE_DONT_TRIM_LOG ) {
                toku_logger_trim_log_files(env->i->logger, FALSE);
            }
            r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL);
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close environment (error during checkpoint)\n");
                goto panic_and_quit_early;
            }
            { //Verify open dbs. Neither Zombies nor fully open are ok at this stage.
                uint32_t size = toku_omt_size(env->i->open_dbs);
                assert(size == env->i->num_open_dbs + env->i->num_zombie_dbs);
                if (size > 0) {
                    r = toku_ydb_do_error(env, EINVAL, "Cannot close environment due to zombie DBs\n");
                    goto panic_and_quit_early;
                }
            }
            r = toku_logger_close_rollback(env->i->logger, FALSE);
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close environment (error during closing rollback cachefile)\n");
                goto panic_and_quit_early;
            }
            //Do a second checkpoint now that the rollback cachefile is closed.
            r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL);
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close environment (error during checkpoint)\n");
                goto panic_and_quit_early;
            }
            r = toku_logger_shutdown(env->i->logger); 
            if (r) {
                toku_ydb_do_error(env, r, "Cannot close environment (error during logger shutdown)\n");
                goto panic_and_quit_early;
            }
        }
	toku_ydb_lock();
        r=toku_cachetable_close(&env->i->cachetable);
	if (r) {
	    toku_ydb_do_error(env, r, "Cannot close environment (cachetable close error)\n");
            goto panic_and_quit_early;
	}
    }
    if (env->i->logger) {
        r=toku_logger_close(&env->i->logger);
	if (r) {
            env->i->logger = NULL;
	    toku_ydb_do_error(env, r, "Cannot close environment (logger close error)\n");
            goto panic_and_quit_early;
	}
    }
    // Even if nothing else went wrong, but we were panicked, then raise an error.
    // But if something else went wrong then raise that error (above)
    if (toku_env_is_panicked(env))
        goto panic_and_quit_early;
    else
	assert(env->i->panic_string==0);

    env_fs_destroy(env);
    toku_ltm_close(env->i->ltm);
    if (env->i->data_dir)
        toku_free(env->i->data_dir);
    if (env->i->lg_dir)
        toku_free(env->i->lg_dir);
    if (env->i->tmp_dir)
        toku_free(env->i->tmp_dir);
    if (env->i->real_data_dir)
	toku_free(env->i->real_data_dir);
    if (env->i->real_log_dir)
	toku_free(env->i->real_log_dir);
    if (env->i->real_tmp_dir)
	toku_free(env->i->real_tmp_dir);
    if (env->i->open_dbs)
        toku_omt_destroy(&env->i->open_dbs);
    if (env->i->dir)
	toku_free(env->i->dir);
    //Immediately before freeing internal environment unlock the directories
    unlock_single_process(env);
    toku_free(env->i);
    env->i = NULL;
    toku_free(env);
    env = NULL;
    if ((flags!=0) && !(flags==DB_CLOSE_DONT_TRIM_LOG))
        r = EINVAL;
    return r;

panic_and_quit_early:
    //release lock files.
    unlock_single_process(env);
    //r is the panic error
    if (toku_env_is_panicked(env)) {
        char *panic_string = env->i->panic_string;
        r = toku_ydb_do_error(env, toku_env_is_panicked(env), "Cannot close environment due to previous error: %s\n", panic_string);
    }
    else
        env->i->is_panicked = r;
    return r;
}

static int 
toku_env_log_archive(DB_ENV * env, char **list[], u_int32_t flags) {
    return toku_logger_log_archive(env->i->logger, list, flags);
}

static int 
toku_env_log_flush(DB_ENV * env, const DB_LSN * lsn __attribute__((__unused__))) {
    HANDLE_PANICKED_ENV(env);
    // We just flush everything.  MySQL uses lsn==0 which means flush everything.  For anyone else using the log, it is correct to flush too much, so we are OK.
    return toku_logger_fsync(env->i->logger);
}

static int 
toku_env_set_cachesize(DB_ENV * env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    HANDLE_PANICKED_ENV(env);
    if (ncache != 1)
        return EINVAL;
    u_int64_t cs64 = ((u_int64_t) gbytes << 30) + bytes;
    unsigned long cs = cs64;
    if (cs64 > cs)
        return EINVAL;
    env->i->cachetable_size = cs;
    return 0;
}

static int toku_env_dbremove(DB_ENV * env, DB_TXN *txn, const char *fname, const char *dbname, u_int32_t flags);

static int
locked_env_dbremove(DB_ENV * env, DB_TXN *txn, const char *fname, const char *dbname, u_int32_t flags) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock();
    int r = toku_env_dbremove(env, txn, fname, dbname, flags);
    toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

static int toku_env_dbrename(DB_ENV *env, DB_TXN *txn, const char *fname, const char *dbname, const char *newname, u_int32_t flags);

static int
locked_env_dbrename(DB_ENV *env, DB_TXN *txn, const char *fname, const char *dbname, const char *newname, u_int32_t flags) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock();
    int r = toku_env_dbrename(env, txn, fname, dbname, newname, flags);
    toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}


#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3

static int 
toku_env_get_cachesize(DB_ENV * env, u_int32_t *gbytes, u_int32_t *bytes, int *ncache) {
    HANDLE_PANICKED_ENV(env);
    *gbytes = env->i->cachetable_size >> 30;
    *bytes = env->i->cachetable_size & ((1<<30)-1);
    *ncache = 1;
    return 0;
}

static int 
locked_env_get_cachesize(DB_ENV *env, u_int32_t *gbytes, u_int32_t *bytes, int *ncache) {
    toku_ydb_lock(); int r = toku_env_get_cachesize(env, gbytes, bytes, ncache); toku_ydb_unlock(); return r;
}
#endif

static int 
toku_env_set_data_dir(DB_ENV * env, const char *dir) {
    HANDLE_PANICKED_ENV(env);
    int r;
    
    if (env_opened(env) || !dir) {
	r = toku_ydb_do_error(env, EINVAL, "You cannot set the data dir after opening the env\n");
    }
    else if (env->i->data_dir)
	r = toku_ydb_do_error(env, EINVAL, "You cannot set the data dir more than once.\n");
    else {
        env->i->data_dir = toku_strdup(dir);
        if (env->i->data_dir==NULL) {
            assert(errno == ENOMEM);
            r = toku_ydb_do_error(env, ENOMEM, "Out of memory\n");
        }
        else r = 0;
    }
    return r;
}

static void 
toku_env_set_errcall(DB_ENV * env, toku_env_errcall_t errcall) {
    env->i->errcall = errcall;
}

static void 
toku_env_set_errfile(DB_ENV*env, FILE*errfile) {
    env->i->errfile = errfile;
}

static void 
toku_env_set_errpfx(DB_ENV * env, const char *errpfx) {
    env->i->errpfx = errpfx;
}

static int 
toku_env_set_flags(DB_ENV * env, u_int32_t flags, int onoff) {
    HANDLE_PANICKED_ENV(env);

    u_int32_t change = 0;
    if (flags & DB_AUTO_COMMIT) {
        change |=  DB_AUTO_COMMIT;
        flags  &= ~DB_AUTO_COMMIT;
    }
    if (flags != 0 && onoff) {
	return toku_ydb_do_error(env, EINVAL, "TokuDB does not (yet) support any nonzero ENV flags other than DB_AUTO_COMMIT\n");
    }
    if   (onoff) env->i->open_flags |=  change;
    else         env->i->open_flags &= ~change;
    return 0;
}

static int 
toku_env_set_lg_bsize(DB_ENV * env, u_int32_t bsize) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_set_lg_bsize(env->i->logger, bsize);
}

static int 
toku_env_set_lg_dir(DB_ENV * env, const char *dir) {
    HANDLE_PANICKED_ENV(env);
    if (env_opened(env)) {
	return toku_ydb_do_error(env, EINVAL, "Cannot set log dir after opening the env\n");
    }

    if (env->i->lg_dir) toku_free(env->i->lg_dir);
    if (dir) {
        env->i->lg_dir = toku_strdup(dir);
        if (!env->i->lg_dir) {
	    return toku_ydb_do_error(env, ENOMEM, "Out of memory\n");
	}
    }
    else env->i->lg_dir = NULL;
    return 0;
}

static int 
toku_env_set_lg_max(DB_ENV * env, u_int32_t lg_max) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_set_lg_max(env->i->logger, lg_max);
}

static int 
toku_env_get_lg_max(DB_ENV * env, u_int32_t *lg_maxp) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_get_lg_max(env->i->logger, lg_maxp);
}

static int 
toku_env_set_lk_detect(DB_ENV * env, u_int32_t detect) {
    HANDLE_PANICKED_ENV(env);
    detect=detect;
    return toku_ydb_do_error(env, EINVAL, "TokuDB does not (yet) support set_lk_detect\n");
}

static int 
toku_env_set_lk_max_locks(DB_ENV *dbenv, u_int32_t max) {
    int r = ENOSYS;
    HANDLE_PANICKED_ENV(dbenv);
    if (env_opened(dbenv))         { return EINVAL; }
    r = toku_ltm_set_max_locks(dbenv->i->ltm, max);
    return r;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
static int 
toku_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    return toku_env_set_lk_max_locks(env, lk_max);
}

static int 
locked_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    toku_ydb_lock(); int r = toku_env_set_lk_max(env, lk_max); toku_ydb_unlock(); return r;
}
#endif

static int 
toku_env_get_lk_max_locks(DB_ENV *dbenv, u_int32_t *lk_maxp) {
    HANDLE_PANICKED_ENV(dbenv);
    return toku_ltm_get_max_locks(dbenv->i->ltm, lk_maxp);
}

static int 
locked_env_set_lk_max_locks(DB_ENV *dbenv, u_int32_t max) {
    toku_ydb_lock(); int r = toku_env_set_lk_max_locks(dbenv, max); toku_ydb_unlock(); return r;
}

static int 
locked_env_get_lk_max_locks(DB_ENV *dbenv, u_int32_t *lk_maxp) {
    toku_ydb_lock(); int r = toku_env_get_lk_max_locks(dbenv, lk_maxp); toku_ydb_unlock(); return r;
}

static int 
toku_env_set_lk_max_memory(DB_ENV *dbenv, uint64_t max) {
    int r = ENOSYS;
    HANDLE_PANICKED_ENV(dbenv);
    if (env_opened(dbenv))         { return EINVAL; }
    r = toku_ltm_set_max_lock_memory(dbenv->i->ltm, max);
    return r;
}

static int 
toku_env_get_lk_max_memory(DB_ENV *dbenv, uint64_t *lk_maxp) {
    HANDLE_PANICKED_ENV(dbenv);
    return toku_ltm_get_max_lock_memory(dbenv->i->ltm, lk_maxp);
}

static int 
locked_env_set_lk_max_memory(DB_ENV *dbenv, uint64_t max) {
    toku_ydb_lock(); 
    int r = toku_env_set_lk_max_memory(dbenv, max); 
    toku_ydb_unlock(); 
    return r;
}

static int locked_env_get_lk_max_memory(DB_ENV *dbenv, uint64_t *lk_maxp) {
    toku_ydb_lock(); int r = toku_env_get_lk_max_memory(dbenv, lk_maxp); toku_ydb_unlock(); return r;
}

//void toku__env_set_noticecall (DB_ENV *env, void (*noticecall)(DB_ENV *, db_notices)) {
//    env->i->noticecall = noticecall;
//}

static int 
toku_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir) {
    HANDLE_PANICKED_ENV(env);
    if (env_opened(env)) {
	return toku_ydb_do_error(env, EINVAL, "Cannot set the tmp dir after opening an env\n");
    }
    if (!tmp_dir) {
	return toku_ydb_do_error(env, EINVAL, "Tmp dir bust be non-null\n");
    }
    if (env->i->tmp_dir)
        toku_free(env->i->tmp_dir);
    env->i->tmp_dir = toku_strdup(tmp_dir);
    return env->i->tmp_dir ? 0 : ENOMEM;
}

static int 
toku_env_set_verbose(DB_ENV * env, u_int32_t which, int onoff) {
    HANDLE_PANICKED_ENV(env);
    which=which; onoff=onoff;
    return 1;
}

// For test purposes only.
// These callbacks are never used in production code, only as a way to test the system
// (for example, by causing crashes at predictable times).
static void (*checkpoint_callback_f)(void*) = NULL;
static void * checkpoint_callback_extra     = NULL;
static void (*checkpoint_callback2_f)(void*) = NULL;
static void * checkpoint_callback2_extra     = NULL;

static int 
toku_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte __attribute__((__unused__)), u_int32_t min __attribute__((__unused__)), u_int32_t flags __attribute__((__unused__))) {
    int r = toku_checkpoint(env->i->cachetable, env->i->logger,
			    checkpoint_callback_f,  checkpoint_callback_extra,
			    checkpoint_callback2_f, checkpoint_callback2_extra);
    if (r) {
	env->i->is_panicked = r; // Panicking the whole environment may be overkill, but I'm not sure what else to do.
	env->i->panic_string = toku_strdup("checkpoint error");
        toku_ydb_do_error(env, r, "Checkpoint\n");
    }
    return r;
}

static int 
toku_env_txn_stat(DB_ENV * env, DB_TXN_STAT ** statp, u_int32_t flags) {
    HANDLE_PANICKED_ENV(env);
    statp=statp;flags=flags;
    return 1;
}

static int 
locked_env_open(DB_ENV * env, const char *home, u_int32_t flags, int mode) {
    toku_ydb_lock(); int r = toku_env_open(env, home, flags, mode); toku_ydb_unlock(); return r;
}

static int 
locked_env_close(DB_ENV * env, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_close(env, flags); toku_ydb_unlock(); return r;
}

static int 
locked_env_log_archive(DB_ENV * env, char **list[], u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_log_archive(env, list, flags); toku_ydb_unlock(); return r;
}

static int 
locked_env_log_flush(DB_ENV * env, const DB_LSN * lsn) {
    toku_ydb_lock(); int r = toku_env_log_flush(env, lsn); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_cachesize(DB_ENV *env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    toku_ydb_lock(); int r = toku_env_set_cachesize(env, gbytes, bytes, ncache); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_data_dir(DB_ENV * env, const char *dir) {
    toku_ydb_lock(); int r = toku_env_set_data_dir(env, dir); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_flags(DB_ENV * env, u_int32_t flags, int onoff) {
    toku_ydb_lock(); int r = toku_env_set_flags(env, flags, onoff); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_lg_bsize(DB_ENV * env, u_int32_t bsize) {
    toku_ydb_lock(); int r = toku_env_set_lg_bsize(env, bsize); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_lg_dir(DB_ENV * env, const char *dir) {
    toku_ydb_lock(); int r = toku_env_set_lg_dir(env, dir); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_lg_max(DB_ENV * env, u_int32_t lg_max) {
    toku_ydb_lock(); int r = toku_env_set_lg_max(env, lg_max); toku_ydb_unlock(); return r;
}

static int 
locked_env_get_lg_max(DB_ENV * env, u_int32_t *lg_maxp) {
    toku_ydb_lock(); int r = toku_env_get_lg_max(env, lg_maxp); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_lk_detect(DB_ENV * env, u_int32_t detect) {
    toku_ydb_lock(); int r = toku_env_set_lk_detect(env, detect); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir) {
    toku_ydb_lock(); int r = toku_env_set_tmp_dir(env, tmp_dir); toku_ydb_unlock(); return r;
}

static int 
locked_env_set_verbose(DB_ENV * env, u_int32_t which, int onoff) {
    toku_ydb_lock(); int r = toku_env_set_verbose(env, which, onoff); toku_ydb_unlock(); return r;
}

static int 
locked_env_txn_stat(DB_ENV * env, DB_TXN_STAT ** statp, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_txn_stat(env, statp, flags); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_set_period(DB_ENV * env, u_int32_t seconds) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (!env_opened(env)) r = EINVAL;
    else
        r = toku_set_checkpoint_period(env->i->cachetable, seconds);
    return r;
}

static int
locked_env_checkpointing_set_period(DB_ENV * env, u_int32_t seconds) {
    toku_ydb_lock(); int r = env_checkpointing_set_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_get_period(DB_ENV * env, u_int32_t *seconds) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else 
        *seconds = toku_get_checkpoint_period(env->i->cachetable);
    return r;
}

static int
locked_env_checkpointing_get_period(DB_ENV * env, u_int32_t *seconds) {
    toku_ydb_lock(); int r = env_checkpointing_get_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_postpone(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_checkpoint_safe_client_lock();
    return r;
}

static int
env_checkpointing_resume(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_checkpoint_safe_client_unlock();
    return r;
}

static int
env_checkpointing_begin_atomic_operation(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_multi_operation_client_lock();
    return r;
}

static int
env_checkpointing_end_atomic_operation(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_multi_operation_client_unlock();
    return r;
}

static int
env_set_default_bt_compare(DB_ENV * env, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (env_opened(env)) r = EINVAL;
    else {
        env->i->bt_compare = bt_compare;
    }
    return r;
}

static int
locked_env_set_default_bt_compare(DB_ENV * env, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    toku_ydb_lock();
    int r = env_set_default_bt_compare(env, bt_compare);
    toku_ydb_unlock();
    return r;
}

static int
env_set_generate_row_callback_for_put(DB_ENV *env, generate_row_for_put_func generate_row_for_put) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (env_opened(env)) r = EINVAL;
    else {
        env->i->generate_row_for_put = generate_row_for_put;
    }
    return r;
}

static int
env_set_generate_row_callback_for_del(DB_ENV *env, generate_row_for_del_func generate_row_for_del) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (env_opened(env)) r = EINVAL;
    else {
        env->i->generate_row_for_del = generate_row_for_del;
    }
    return r;
}

static int
locked_env_set_generate_row_callback_for_put(DB_ENV *env, generate_row_for_put_func generate_row_for_put) {
    toku_ydb_lock();
    int r = env_set_generate_row_callback_for_put(env, generate_row_for_put);
    toku_ydb_unlock();
    return r;
}

static int
locked_env_set_generate_row_callback_for_del(DB_ENV *env, generate_row_for_del_func generate_row_for_del) {
    toku_ydb_lock();
    int r = env_set_generate_row_callback_for_del(env, generate_row_for_del);
    toku_ydb_unlock();
    return r;
}

static int env_put_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn, 
                            const DBT *key, const DBT *val, 
                            uint32_t num_dbs, DB **db_array, DBT *keys, DBT *vals, uint32_t *flags_array);

static int env_del_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn, 
                            const DBT *key, const DBT *val, 
                            uint32_t num_dbs, DB **db_array, DBT *keys, uint32_t *flags_array);

static int env_update_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn, 
                               DBT *old_src_key, DBT *old_src_data,
                               DBT *new_src_key, DBT *new_src_data,
                               uint32_t num_dbs, DB **db_array, uint32_t* flags_array, 
                               uint32_t num_keys, DBT *keys, 
                               uint32_t num_vals, DBT *vals);

static int
locked_env_put_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn, const DBT *key, const DBT *val, uint32_t num_dbs, DB **db_array, DBT *keys, DBT *vals, uint32_t *flags_array) {
    int r = env_check_avail_fs_space(env);
    if (r == 0) {
	toku_ydb_lock();
	r = env_put_multiple(env, src_db, txn, key, val, num_dbs, db_array, keys, vals, flags_array);
	toku_ydb_unlock();
    }
    return r;
}

static int
locked_env_del_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn, const DBT *key, const DBT *val, uint32_t num_dbs, DB **db_array, DBT *keys, uint32_t *flags_array) {
    toku_ydb_lock();
    int r = env_del_multiple(env, src_db, txn, key, val, num_dbs, db_array, keys, flags_array);
    toku_ydb_unlock();
    return r;
}

static int
locked_env_update_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn,                                
                           DBT *old_src_key, DBT *old_src_data,
                           DBT *new_src_key, DBT *new_src_data,
                           uint32_t num_dbs, DB **db_array, uint32_t* flags_array, 
                           uint32_t num_keys, DBT *keys, 
                           uint32_t num_vals, DBT *vals) {
    toku_ydb_lock();
    int r = env_update_multiple(env, src_db, txn, old_src_key, old_src_data, new_src_key, new_src_data, num_dbs, db_array, flags_array, num_keys, keys, num_vals, vals);
    toku_ydb_unlock();
    return r;
}

static int
env_set_redzone(DB_ENV *env, int redzone) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (env_opened(env))
        r = EINVAL;
    else {
        env->i->redzone = redzone;
        r = 0;
    }
    return r;
}

static int 
locked_env_set_redzone(DB_ENV *env, int redzone) {
    toku_ydb_lock();
    int r= env_set_redzone(env, redzone);
    toku_ydb_unlock();
    return r;
}

static void
format_time(const time_t *timer, char *buf) {
    ctime_r(timer, buf);
    size_t len = strlen(buf);
    assert(len < 26);
    char end;

    assert(len>=1);
    end = buf[len-1];
    while (end == '\n' || end == '\r') {
        buf[len-1] = '\0';
        len--;
        assert(len>=1);
        end = buf[len-1];
    }
}

// Do not take ydb lock or any other lock around or in this function.  
// If the engine is blocked because some thread is holding a lock, this function
// can help diagnose the problem.
// This function only collects information, and it does not matter if something gets garbled
// because of a race condition.  
// Note, engine status is still collected even if the environment or logger is panicked
static int
env_get_engine_status(DB_ENV * env, ENGINE_STATUS * engstat, char * env_panic_string_buf, int env_panic_string_length) {
    int r;
    if (env_panic_string_buf) {
	if (env && env->i && env->i->is_panicked && env->i->panic_string)
	    strncpy(env_panic_string_buf, env->i->panic_string, env_panic_string_length);
	else 
	    *env_panic_string_buf = '\0';
    }

    if ( !(env)     || 
	 !(env->i)  || 
	 !(env_opened(env)) )
	r = EINVAL;
    else {
	r = 0;
	engstat->env_panic = env->i->is_panicked;
	format_time(&persistent_creation_time, engstat->creationtime);
	time_t now = time(NULL);
        format_time(&now, engstat->now);
        format_time(&startuptime, engstat->startuptime);
	{
	    SCHEDULE_STATUS_S schedstat;
	    toku_ydb_lock_get_status(&schedstat);
	    engstat->ydb_lock_ctr = schedstat.ydb_lock_ctr;                        /* how many times has ydb lock been taken/released */ 
	    engstat->max_possible_sleep = schedstat.max_possible_sleep;            /* max possible sleep time for ydb lock scheduling (constant) */ 
	    engstat->processor_freq_mhz = schedstat.processor_freq_mhz;            /* clock frequency in MHz */
	    engstat->max_requested_sleep = schedstat.max_requested_sleep;          /* max sleep time requested, can be larger than max possible */ 
	    engstat->times_max_sleep_used = schedstat.times_max_sleep_used;        /* number of times the max_possible_sleep was used to sleep */ 
	    engstat->total_sleepers = schedstat.total_sleepers;                    /* total number of times a client slept for ydb lock scheduling */ 
	    engstat->total_sleep_time = schedstat.total_sleep_time;                /* total time spent sleeping for ydb lock scheduling */ 
	    engstat->max_waiters = schedstat.max_waiters;                          /* max number of simultaneous client threads kept waiting for ydb lock  */ 
	    engstat->total_waiters = schedstat.total_waiters;                      /* total number of times a client thread waited for ydb lock  */ 
	    engstat->total_clients = schedstat.total_clients;                      /* total number of separate client threads that use ydb lock  */ 
	    engstat->time_ydb_lock_held_unavailable = schedstat.time_ydb_lock_held_unavailable;  /* number of times a thread migrated and theld is unavailable */ 
	    engstat->total_time_ydb_lock_held = schedstat.total_time_ydb_lock_held;/* total time client threads held the ydb lock  */ 
	    engstat->max_time_ydb_lock_held = schedstat.max_time_ydb_lock_held;    /* max time client threads held the ydb lock  */ 
	}

	engstat->checkpoint_period = toku_get_checkpoint_period_unlocked(env->i->cachetable);  // do not take any locks (not even minicron lock)
	{
            CHECKPOINT_STATUS_S cpstat;
            toku_checkpoint_get_status(&cpstat);
            engstat->checkpoint_footprint = cpstat.footprint;
	    format_time(&cpstat.time_last_checkpoint_begin_complete, engstat->checkpoint_time_begin_complete);
	    format_time(&cpstat.time_last_checkpoint_begin,          engstat->checkpoint_time_begin);
	    format_time(&cpstat.time_last_checkpoint_end,            engstat->checkpoint_time_end);
	    engstat->checkpoint_last_lsn   = cpstat.last_lsn;
	    engstat->checkpoint_count      = cpstat.checkpoint_count;
	    engstat->checkpoint_count_fail = cpstat.checkpoint_count_fail;
	}
	{
	    TXN_STATUS_S txnstat;
	    toku_txn_get_status(&txnstat);
	    engstat->txn_begin   = txnstat.begin;
	    engstat->txn_commit  = txnstat.commit;
	    engstat->txn_abort   = txnstat.abort;
	    engstat->txn_close   = txnstat.close;
	    {
		uint64_t oldest_xid = 0;
		uint64_t next_lsn   = 0;
		TOKULOGGER logger = env->i->logger;
		if (logger) {
		    oldest_xid = toku_logger_get_oldest_living_xid(env->i->logger);
		    next_lsn   = (toku_logger_get_next_lsn(env->i->logger)).lsn;
		}
		engstat->txn_oldest_live = oldest_xid;
		engstat->next_lsn = next_lsn;
	    }
	}
	{
	    CACHETABLE_STATUS_S ctstat;
	    toku_cachetable_get_status(env->i->cachetable, &ctstat);
	    engstat->cachetable_lock_taken    = ctstat.lock_taken;
	    engstat->cachetable_lock_released = ctstat.lock_released;
	    engstat->cachetable_hit           = ctstat.hit;
	    engstat->cachetable_miss          = ctstat.miss;
	    engstat->cachetable_misstime      = ctstat.misstime;
	    engstat->cachetable_waittime      = ctstat.waittime;
	    engstat->cachetable_wait_reading  = ctstat.wait_reading;
	    engstat->cachetable_wait_writing  = ctstat.wait_writing;
	    engstat->cachetable_wait_checkpoint = ctstat.wait_checkpoint;
	    engstat->puts                     = ctstat.puts;
	    engstat->prefetches               = ctstat.prefetches;
	    engstat->maybe_get_and_pins       = ctstat.maybe_get_and_pins;
	    engstat->maybe_get_and_pin_hits   = ctstat.maybe_get_and_pin_hits;
	    engstat->cachetable_size_current  = ctstat.size_current;
	    engstat->cachetable_size_limit    = ctstat.size_limit;
	    engstat->cachetable_size_writing  = ctstat.size_writing;
	    engstat->get_and_pin_footprint    = ctstat.get_and_pin_footprint;
	    engstat->local_checkpoint         = ctstat.local_checkpoint;
	    engstat->local_checkpoint_files   = ctstat.local_checkpoint_files;
	    engstat->local_checkpoint_during_checkpoint = ctstat.local_checkpoint_during_checkpoint;
	}
	{
	    toku_ltm* ltm = env->i->ltm;
	    LTM_STATUS_S ltmstat;
	    uint32_t max_locks, curr_locks;
	    uint64_t max_lock_memory, curr_lock_memory;
	    toku_ltm_get_status(ltm, &max_locks, &curr_locks, 
				&max_lock_memory, &curr_lock_memory,
				&ltmstat);
	    engstat->range_locks_max                 = max_locks;
	    engstat->range_locks_curr                = curr_locks;
	    engstat->range_locks_max_memory          = max_lock_memory;
	    engstat->range_locks_curr_memory         = curr_lock_memory;
	    engstat->range_lock_escalation_successes = ltmstat.lock_escalation_successes;
	    engstat->range_lock_escalation_failures  = ltmstat.lock_escalation_failures;
	    engstat->range_read_locks                = ltmstat.read_lock;
	    engstat->range_read_locks_fail           = ltmstat.read_lock_fail;
	    engstat->range_out_of_read_locks         = ltmstat.out_of_read_locks;
	    engstat->range_write_locks               = ltmstat.write_lock;
	    engstat->range_write_locks_fail          = ltmstat.write_lock_fail;
	    engstat->range_out_of_write_locks        = ltmstat.out_of_write_locks;
	}
	{
     	    engstat->inserts            = num_inserts;
	    engstat->inserts_fail       = num_inserts_fail;
	    engstat->deletes            = num_deletes;
	    engstat->deletes_fail       = num_deletes_fail;
	    engstat->updates            = num_updates;
	    engstat->updates_fail       = num_updates_fail;
     	    engstat->multi_inserts      = num_multi_inserts;
	    engstat->multi_inserts_fail = num_multi_inserts_fail;
	    engstat->multi_deletes      = num_multi_deletes;
	    engstat->multi_deletes_fail = num_multi_deletes_fail;
	    engstat->multi_updates      = num_multi_updates;
	    engstat->multi_updates_fail = num_multi_updates_fail;
	    engstat->point_queries      = num_point_queries;
	    engstat->sequential_queries = num_sequential_queries;
	}
	{
	    u_int64_t fsync_count, fsync_time;
	    toku_get_fsync_times(&fsync_count, &fsync_time);
	    engstat->fsync_count = fsync_count;
	    engstat->fsync_time  = fsync_time;
	}
	{
	    LOGGER_STATUS_S log_stat;
	    TOKULOGGER logger = env->i->logger;
	    toku_logger_get_status(logger, &log_stat);
	    engstat->logger_ilock_ctr = log_stat.ilock_ctr;
	    engstat->logger_olock_ctr = log_stat.olock_ctr;
	    engstat->logger_swap_ctr  = log_stat.swap_ctr;
	    engstat->logger_panic     = log_stat.panicked;
	    engstat->logger_panic_errno = log_stat.panic_errno;
	}
	{
	    time_t    enospc_most_recent_timestamp;
	    u_int64_t enospc_threads_blocked, enospc_ctr;
	    toku_fs_get_write_info(&enospc_most_recent_timestamp, &enospc_threads_blocked, &enospc_ctr);
	    format_time(&enospc_most_recent_timestamp, engstat->enospc_most_recent);	    
	    engstat->enospc_threads_blocked = enospc_threads_blocked;
	    engstat->enospc_ctr = enospc_ctr;
	}
	{
	    engstat->enospc_redzone_ctr   = env->i->enospc_redzone_ctr;   // number of operations rejected by enospc prevention (red zone)
	    engstat->enospc_state         = env->i->fs_state;
	}
	{
	    LOADER_STATUS_S loader_stat;
	    toku_loader_get_status(&loader_stat);
	    engstat->loader_create         = loader_stat.create;
	    engstat->loader_create_fail    = loader_stat.create_fail;
	    engstat->loader_put            = loader_stat.put;
	    engstat->loader_close          = loader_stat.close;
	    engstat->loader_close_fail     = loader_stat.close_fail;
	    engstat->loader_abort          = loader_stat.abort;
	    engstat->loader_current        = loader_stat.current;
	    engstat->loader_max            = loader_stat.max;
	    
	    engstat->logsuppress     = logsuppress;
	    engstat->logsuppressfail = logsuppressfail;
	}
	{
	    INDEXER_STATUS_S indexer_stat;
	    toku_indexer_get_status(&indexer_stat);
	    engstat->indexer_create         = indexer_stat.create;
	    engstat->indexer_create_fail    = indexer_stat.create_fail;
	    engstat->indexer_build          = indexer_stat.build;
	    engstat->indexer_build_fail     = indexer_stat.build_fail;
	    engstat->indexer_close          = indexer_stat.close;
	    engstat->indexer_close_fail     = indexer_stat.close_fail;
	    engstat->indexer_abort          = indexer_stat.abort;
	    engstat->indexer_current        = indexer_stat.current;
	    engstat->indexer_max            = indexer_stat.max;
	}
	{
	    BRT_UPGRADE_STATUS_S brt_upgrade_stat;
	    toku_brt_get_upgrade_status(&brt_upgrade_stat);

	    engstat->upgrade_env_status = toku_log_upgrade_get_footprint();
	    engstat->upgrade_header     = brt_upgrade_stat.header_12;
	    engstat->upgrade_nonleaf    = brt_upgrade_stat.nonleaf_12;
	    engstat->upgrade_leaf       = brt_upgrade_stat.leaf_12;
	    engstat->original_ver       = persistent_original_env_version;
	    engstat->ver_at_startup     = persistent_stored_env_version_at_startup;
	    engstat->last_lsn_v12       = persistent_last_lsn_of_v12;
	    format_time(&persistent_upgrade_v13_time, engstat->upgrade_v13_time);
	}
    }
    return r;
}


// Fill buff with text description of engine status up to bufsiz bytes.
// Intended for use by test programs that do not have the handlerton available,
// and for use by toku_assert logic to print diagnostic info on crash.
static int
env_get_engine_status_text(DB_ENV * env, char * buff, int bufsiz) {
    ENGINE_STATUS engstat;
    uint32_t stringsize = 80;
    char panicstring[stringsize];
    int n = 0;  // number of characters printed so far

    int r = env_get_engine_status(env, &engstat, panicstring, stringsize);    

    if (strlen(panicstring)) {
	invariant(strlen(panicstring) <= stringsize);
	n += snprintf(buff + n, bufsiz - n, "Env panic: %s\n", panicstring);
    }

    if (r) {
	n += snprintf(buff + n, bufsiz - n, "Engine status not available: ");
	if (!env) {
	    n += snprintf(buff + n, bufsiz - n, "no environment\n");
	}
	else if (!(env->i)) {
	    n += snprintf(buff + n, bufsiz - n, "environment internal struct is null\n");
	}
	else if (!env_opened(env)) {
	    n += snprintf(buff + n, bufsiz - n, "environment is not open\n");
	}
    }
    else {
	n += snprintf(buff + n, bufsiz - n, "env panic                        %"PRIu64"\n", engstat.env_panic);
	n += snprintf(buff + n, bufsiz - n, "creationtime                     %s \n", engstat.creationtime);
	n += snprintf(buff + n, bufsiz - n, "startuptime                      %s \n", engstat.startuptime);
	n += snprintf(buff + n, bufsiz - n, "now                              %s \n", engstat.now);
	n += snprintf(buff + n, bufsiz - n, "ydb_lock_ctr                     %"PRIu64"\n", engstat.ydb_lock_ctr);
	n += snprintf(buff + n, bufsiz - n, "max_possible_sleep               %"PRIu64"\n", engstat.max_possible_sleep);
	n += snprintf(buff + n, bufsiz - n, "processor_freq_mhz               %"PRIu64"\n", engstat.processor_freq_mhz);
	n += snprintf(buff + n, bufsiz - n, "max_requested_sleep              %"PRIu64"\n", engstat.max_requested_sleep);
	n += snprintf(buff + n, bufsiz - n, "times_max_sleep_used             %"PRIu64"\n", engstat.times_max_sleep_used);
	n += snprintf(buff + n, bufsiz - n, "total_sleepers                   %"PRIu64"\n", engstat.total_sleepers);
	n += snprintf(buff + n, bufsiz - n, "total_sleep_time                 %"PRIu64"\n", engstat.total_sleep_time);
	n += snprintf(buff + n, bufsiz - n, "max_waiters                      %"PRIu64"\n", engstat.max_waiters);
	n += snprintf(buff + n, bufsiz - n, "total_waiters                    %"PRIu64"\n", engstat.total_waiters);
	n += snprintf(buff + n, bufsiz - n, "total_clients                    %"PRIu64"\n", engstat.total_clients);
	n += snprintf(buff + n, bufsiz - n, "time_ydb_lock_held_unavailable   %"PRIu64"\n", engstat.time_ydb_lock_held_unavailable);
	n += snprintf(buff + n, bufsiz - n, "max_time_ydb_lock_held           %"PRIu64"\n", engstat.max_time_ydb_lock_held);
	n += snprintf(buff + n, bufsiz - n, "total_time_ydb_lock_held         %"PRIu64"\n", engstat.total_time_ydb_lock_held);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_period                %d \n", engstat.checkpoint_period);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_footprint             %d \n", engstat.checkpoint_footprint);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_time_begin            %s \n", engstat.checkpoint_time_begin);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_time_begin_complete   %s \n", engstat.checkpoint_time_begin_complete);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_time_end              %s \n", engstat.checkpoint_time_end);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_last_lsn              %"PRIu64"\n", engstat.checkpoint_last_lsn);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_count                 %"PRIu32"\n", engstat.checkpoint_count);
	n += snprintf(buff + n, bufsiz - n, "checkpoint_count_fail            %"PRIu32"\n", engstat.checkpoint_count_fail);
	n += snprintf(buff + n, bufsiz - n, "txn_begin                        %"PRIu64"\n", engstat.txn_begin);
	n += snprintf(buff + n, bufsiz - n, "txn_commit                       %"PRIu64"\n", engstat.txn_commit);
	n += snprintf(buff + n, bufsiz - n, "txn_abort                        %"PRIu64"\n", engstat.txn_abort);
	n += snprintf(buff + n, bufsiz - n, "txn_close                        %"PRIu64"\n", engstat.txn_close);
	n += snprintf(buff + n, bufsiz - n, "txn_oldest_live                  %"PRIu64"\n", engstat.txn_oldest_live);
	n += snprintf(buff + n, bufsiz - n, "next_lsn                         %"PRIu64"\n", engstat.next_lsn);
	n += snprintf(buff + n, bufsiz - n, "cachetable_lock_taken            %"PRIu64"\n", engstat.cachetable_lock_taken);
	n += snprintf(buff + n, bufsiz - n, "cachetable_lock_released         %"PRIu64"\n", engstat.cachetable_lock_released);
	n += snprintf(buff + n, bufsiz - n, "cachetable_hit                   %"PRIu64"\n", engstat.cachetable_hit);
	n += snprintf(buff + n, bufsiz - n, "cachetable_miss                  %"PRIu64"\n", engstat.cachetable_miss);
	n += snprintf(buff + n, bufsiz - n, "cachetable_misstime              %"PRIu64"\n", engstat.cachetable_misstime);
	n += snprintf(buff + n, bufsiz - n, "cachetable_waittime              %"PRIu64"\n", engstat.cachetable_waittime);
	n += snprintf(buff + n, bufsiz - n, "cachetable_wait_reading          %"PRIu64"\n", engstat.cachetable_wait_reading);
	n += snprintf(buff + n, bufsiz - n, "cachetable_wait_writing          %"PRIu64"\n", engstat.cachetable_wait_writing);
	n += snprintf(buff + n, bufsiz - n, "puts                             %"PRIu64"\n", engstat.puts);
	n += snprintf(buff + n, bufsiz - n, "prefetches                       %"PRIu64"\n", engstat.prefetches);
	n += snprintf(buff + n, bufsiz - n, "maybe_get_and_pins               %"PRIu64"\n", engstat.maybe_get_and_pins);
	n += snprintf(buff + n, bufsiz - n, "maybe_get_and_pin_hits           %"PRIu64"\n", engstat.maybe_get_and_pin_hits);
	n += snprintf(buff + n, bufsiz - n, "cachetable_size_current          %"PRId64"\n", engstat.cachetable_size_current);
	n += snprintf(buff + n, bufsiz - n, "cachetable_size_limit            %"PRId64"\n", engstat.cachetable_size_limit);
	n += snprintf(buff + n, bufsiz - n, "cachetable_size_writing          %"PRId64"\n", engstat.cachetable_size_writing);
	n += snprintf(buff + n, bufsiz - n, "get_and_pin_footprint            %"PRId64"\n", engstat.get_and_pin_footprint);
	n += snprintf(buff + n, bufsiz - n, "local_checkpoint                 %"PRId64"\n", engstat.local_checkpoint);
	n += snprintf(buff + n, bufsiz - n, "local_checkpoint_files           %"PRId64"\n", engstat.local_checkpoint_files);
	n += snprintf(buff + n, bufsiz - n, "local_checkpoint_during_checkpoint  %"PRId64"\n", engstat.local_checkpoint_during_checkpoint);
	n += snprintf(buff + n, bufsiz - n, "range_locks_max                  %"PRIu32"\n", engstat.range_locks_max);
	n += snprintf(buff + n, bufsiz - n, "range_locks_curr                 %"PRIu32"\n", engstat.range_locks_curr);
	n += snprintf(buff + n, bufsiz - n, "range_locks_max_memory           %"PRIu64"\n", engstat.range_locks_max_memory);
	n += snprintf(buff + n, bufsiz - n, "range_locks_curr_memory          %"PRIu64"\n", engstat.range_locks_curr_memory);
	n += snprintf(buff + n, bufsiz - n, "range_locks_escalation_successes %"PRIu32"\n", engstat.range_lock_escalation_successes);
	n += snprintf(buff + n, bufsiz - n, "range_locks_escalation_failures  %"PRIu32"\n", engstat.range_lock_escalation_failures);
	n += snprintf(buff + n, bufsiz - n, "range_read_locks                 %"PRIu64"\n", engstat.range_read_locks);
	n += snprintf(buff + n, bufsiz - n, "range_read_locks_fail            %"PRIu64"\n", engstat.range_read_locks_fail);
	n += snprintf(buff + n, bufsiz - n, "range_out_of_read_locks          %"PRIu64"\n", engstat.range_out_of_read_locks);
	n += snprintf(buff + n, bufsiz - n, "range_write_locks                %"PRIu64"\n", engstat.range_write_locks);
	n += snprintf(buff + n, bufsiz - n, "range_write_locks_fail           %"PRIu64"\n", engstat.range_write_locks_fail);
	n += snprintf(buff + n, bufsiz - n, "range_out_of_write_locks         %"PRIu64"\n", engstat.range_out_of_write_locks);
	n += snprintf(buff + n, bufsiz - n, "inserts                          %"PRIu64"\n", engstat.inserts);
	n += snprintf(buff + n, bufsiz - n, "inserts_fail                     %"PRIu64"\n", engstat.inserts_fail);
	n += snprintf(buff + n, bufsiz - n, "deletes                          %"PRIu64"\n", engstat.deletes);
	n += snprintf(buff + n, bufsiz - n, "deletes_fail                     %"PRIu64"\n", engstat.deletes_fail);
	n += snprintf(buff + n, bufsiz - n, "updates                          %"PRIu64"\n", engstat.updates);
	n += snprintf(buff + n, bufsiz - n, "updates_fail                     %"PRIu64"\n", engstat.updates_fail);
	n += snprintf(buff + n, bufsiz - n, "multi_inserts                    %"PRIu64"\n", engstat.multi_inserts);
	n += snprintf(buff + n, bufsiz - n, "multi_inserts_fail               %"PRIu64"\n", engstat.multi_inserts_fail);
	n += snprintf(buff + n, bufsiz - n, "multi_deletes                    %"PRIu64"\n", engstat.multi_deletes);
	n += snprintf(buff + n, bufsiz - n, "multi_deletes_fail               %"PRIu64"\n", engstat.multi_deletes_fail);
	n += snprintf(buff + n, bufsiz - n, "multi_updates                    %"PRIu64"\n", engstat.multi_updates);
	n += snprintf(buff + n, bufsiz - n, "multi_updates_fail               %"PRIu64"\n", engstat.multi_updates_fail);
	n += snprintf(buff + n, bufsiz - n, "point_queries                    %"PRIu64"\n", engstat.point_queries);
	n += snprintf(buff + n, bufsiz - n, "sequential_queries               %"PRIu64"\n", engstat.sequential_queries);
	n += snprintf(buff + n, bufsiz - n, "fsync_count                      %"PRIu64"\n", engstat.fsync_count);
	n += snprintf(buff + n, bufsiz - n, "fsync_time                       %"PRIu64"\n", engstat.fsync_time);
	n += snprintf(buff + n, bufsiz - n, "logger ilock count               %"PRIu64"\n", engstat.logger_ilock_ctr);
	n += snprintf(buff + n, bufsiz - n, "logger olock count               %"PRIu64"\n", engstat.logger_olock_ctr);
	n += snprintf(buff + n, bufsiz - n, "logger swap count                %"PRIu64"\n", engstat.logger_swap_ctr);
	n += snprintf(buff + n, bufsiz - n, "logger panic                     %"PRIu64"\n", engstat.logger_panic);
	n += snprintf(buff + n, bufsiz - n, "logger panic_errno               %"PRIu64"\n", engstat.logger_panic_errno);
	n += snprintf(buff + n, bufsiz - n, "enospc_most_recent               %s \n", engstat.enospc_most_recent);
	n += snprintf(buff + n, bufsiz - n, "enospc threads blocked           %"PRIu64"\n", engstat.enospc_threads_blocked);
	n += snprintf(buff + n, bufsiz - n, "enospc count                     %"PRIu64"\n", engstat.enospc_ctr);
	n += snprintf(buff + n, bufsiz - n, "enospc redzone ctr               %"PRIu64"\n", engstat.enospc_redzone_ctr);
	n += snprintf(buff + n, bufsiz - n, "enospc state                     %"PRIu64"\n", engstat.enospc_state);
	n += snprintf(buff + n, bufsiz - n, "loader_create                    %"PRIu64"\n", engstat.loader_create);
	n += snprintf(buff + n, bufsiz - n, "loader_create_fail               %"PRIu64"\n", engstat.loader_create_fail);
	n += snprintf(buff + n, bufsiz - n, "loader_put                       %"PRIu64"\n", engstat.loader_put);
	n += snprintf(buff + n, bufsiz - n, "loader_close                     %"PRIu64"\n", engstat.loader_close);
	n += snprintf(buff + n, bufsiz - n, "loader_close_fail                %"PRIu64"\n", engstat.loader_close_fail);
	n += snprintf(buff + n, bufsiz - n, "loader_abort                     %"PRIu64"\n", engstat.loader_abort);
	n += snprintf(buff + n, bufsiz - n, "loader_current                   %"PRIu32"\n", engstat.loader_current);
	n += snprintf(buff + n, bufsiz - n, "loader_max                       %"PRIu32"\n", engstat.loader_max);
	n += snprintf(buff + n, bufsiz - n, "logsuppress                      %"PRIu64"\n", engstat.logsuppress);
	n += snprintf(buff + n, bufsiz - n, "logsuppressfail                  %"PRIu64"\n", engstat.logsuppressfail);
	n += snprintf(buff + n, bufsiz - n, "indexer_create                   %"PRIu64"\n", engstat.indexer_create);
	n += snprintf(buff + n, bufsiz - n, "indexer_create_fail              %"PRIu64"\n", engstat.indexer_create_fail);
	n += snprintf(buff + n, bufsiz - n, "indexer_build                    %"PRIu64"\n", engstat.indexer_build);
	n += snprintf(buff + n, bufsiz - n, "indexer_build_fail               %"PRIu64"\n", engstat.indexer_build_fail);
	n += snprintf(buff + n, bufsiz - n, "indexer_close                    %"PRIu64"\n", engstat.indexer_close);
	n += snprintf(buff + n, bufsiz - n, "indexer_close_fail               %"PRIu64"\n", engstat.indexer_close_fail);
	n += snprintf(buff + n, bufsiz - n, "indexer_abort                    %"PRIu64"\n", engstat.indexer_abort);
	n += snprintf(buff + n, bufsiz - n, "indexer_current                  %"PRIu32"\n", engstat.indexer_current);
	n += snprintf(buff + n, bufsiz - n, "indexer_max                      %"PRIu32"\n", engstat.indexer_max);
	n += snprintf(buff + n, bufsiz - n, "upgrade_env_status               %"PRIu64"\n", engstat.upgrade_env_status);
	n += snprintf(buff + n, bufsiz - n, "upgrade_header                   %"PRIu64"\n", engstat.upgrade_header);
	n += snprintf(buff + n, bufsiz - n, "upgrade_nonleaf                  %"PRIu64"\n", engstat.upgrade_nonleaf);
	n += snprintf(buff + n, bufsiz - n, "upgrade_leaf                     %"PRIu64"\n", engstat.upgrade_leaf);
	n += snprintf(buff + n, bufsiz - n, "original_ver                     %"PRIu64"\n", engstat.original_ver);
	n += snprintf(buff + n, bufsiz - n, "ver_at_startup                   %"PRIu64"\n", engstat.ver_at_startup);
	n += snprintf(buff + n, bufsiz - n, "last_lsn_v12                     %"PRIu64"\n", engstat.last_lsn_v12);
	n += snprintf(buff + n, bufsiz - n, "upgrade_v13_time                 %s \n", engstat.upgrade_v13_time);
    }
    if (n > bufsiz) {
	char * errmsg = "BUFFER TOO SMALL\n";
	int len = strlen(errmsg) + 1;
	(void) snprintf(buff + (bufsiz - 1) - len, len, "%s", errmsg);
    }

    return r;
}

static int toku_maybe_get_engine_status_text (char* buff, int buffsize);

// assign value to global pointer so that other files can access via tentative definition if linked to this library (.so)
int (*toku_maybe_get_engine_status_text_p)(char* buff, int buffsize) = toku_maybe_get_engine_status_text;

// intended for use by toku_assert logic, when env is not known
static int 
toku_maybe_get_engine_status_text (char * buff, int buffsize) {
    DB_ENV * env = most_recent_env;
    int r = env_get_engine_status_text(env, buff, buffsize);
    return r;
}



static int locked_txn_begin(DB_ENV * env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags);

static int toku_db_lt_panic(DB* db, int r);

static toku_dbt_cmp toku_db_get_compare_fun(DB* db);

static int 
toku_env_create(DB_ENV ** envp, u_int32_t flags) {
    int r = ENOSYS;
    DB_ENV* result = NULL;

    if (flags!=0)    { r = EINVAL; goto cleanup; }
    MALLOC(result);
    if (result == 0) { r = ENOMEM; goto cleanup; }
    memset(result, 0, sizeof *result);
    result->err = (void (*)(const DB_ENV * env, int error, const char *fmt, ...)) toku_locked_env_err;
#define SENV(name) result->name = locked_env_ ## name
    SENV(dbremove);
    SENV(dbrename);
    SENV(set_default_bt_compare);
    SENV(set_generate_row_callback_for_put);
    SENV(set_generate_row_callback_for_del);
    SENV(put_multiple);
    SENV(del_multiple);
    SENV(update_multiple);
    SENV(checkpointing_set_period);
    SENV(checkpointing_get_period);
    result->checkpointing_postpone = env_checkpointing_postpone;
    result->checkpointing_resume = env_checkpointing_resume;
    result->checkpointing_begin_atomic_operation = env_checkpointing_begin_atomic_operation;
    result->checkpointing_end_atomic_operation = env_checkpointing_end_atomic_operation;
    result->get_engine_status = env_get_engine_status;
    result->get_engine_status_text = env_get_engine_status_text;
    result->get_iname = env_get_iname;
    SENV(open);
    SENV(close);
    result->txn_checkpoint = toku_env_txn_checkpoint;
    SENV(log_flush);
    result->set_errcall = toku_env_set_errcall;
    result->set_errfile = toku_env_set_errfile;
    result->set_errpfx = toku_env_set_errpfx;
    //SENV(set_noticecall);
    SENV(set_flags);
    SENV(set_data_dir);
    SENV(set_tmp_dir);
    SENV(set_verbose);
    SENV(set_lg_bsize);
    SENV(set_lg_dir);
    SENV(set_lg_max);
    SENV(get_lg_max);
    SENV(set_lk_max_locks);
    SENV(get_lk_max_locks);
    SENV(set_lk_max_memory);
    SENV(get_lk_max_memory);
    SENV(set_cachesize);
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    SENV(get_cachesize);
#endif
    SENV(set_lk_detect);
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    SENV(set_lk_max);
#endif
    SENV(log_archive);
    SENV(txn_stat);
    result->txn_begin = locked_txn_begin;
    SENV(set_redzone);
#undef SENV
    result->create_loader = toku_loader_create_loader;
    result->create_indexer = toku_indexer_create_indexer;

    MALLOC(result->i);
    if (result->i == 0) { r = ENOMEM; goto cleanup; }
    memset(result->i, 0, sizeof *result->i);
    result->i->envdir_lockfd  = -1;
    result->i->datadir_lockfd = -1;
    result->i->logdir_lockfd  = -1;
    result->i->tmpdir_lockfd  = -1;
    env_init_open_txn(result);
    env_fs_init(result);

    r = toku_ltm_create(&result->i->ltm,
                        __toku_env_default_max_locks, __toku_env_default_max_lock_memory,
                         toku_db_lt_panic, 
                         toku_db_get_compare_fun,
                         toku_malloc, toku_free, toku_realloc);
    if (r!=0) { goto cleanup; }

    {
	r = toku_logger_create(&result->i->logger);
	if (r!=0) { goto cleanup; }
	assert(result->i->logger);
    }
    {
        r = toku_omt_create(&result->i->open_dbs);
        if (r!=0) goto cleanup;
        assert(result->i->open_dbs);
    }

    *envp = result;
    r = 0;
cleanup:
    if (r!=0) {
        if (result) {
            if (result->i) {
                if (result->i->ltm) {
                    toku_ltm_close(result->i->ltm);
                }
                if (result->i->open_dbs)
                    toku_omt_destroy(&result->i->open_dbs);
                toku_free(result->i);
            }
            toku_free(result);
        }
    }
    return r;
}

int 
DB_ENV_CREATE_FUN (DB_ENV ** envp, u_int32_t flags) {
    toku_ydb_lock(); 
    int r = toku_env_create(envp, flags); 
    toku_ydb_unlock(); 
    return r;
}

static int 
toku_txn_release_locks(DB_TXN* txn) {
    assert(txn);
    toku_lth* lth = db_txn_struct_i(txn)->lth;

    int r = ENOSYS;
    int first_error = 0;
    if (lth) {
        toku_lth_start_scan(lth);
        toku_lock_tree* next = toku_lth_next(lth);
        while (next) {
            r = toku_lt_unlock(next, toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn));
            if (!first_error && r!=0) { first_error = r; }
            if (r == 0) {
                r = toku_lt_remove_ref(next);
                if (!first_error && r!=0) { first_error = r; }
            }
            next = toku_lth_next(lth);
        }
        toku_lth_close(lth);
        db_txn_struct_i(txn)->lth = NULL;
    }
    r = first_error;

    return r;
}

// Yield the lock so someone else can work, and then reacquire the lock.
// Useful while processing commit or rollback logs, to allow others to access the system.
static void 
ydb_yield (voidfp f, void *fv, void *UU(v)) {
    toku_ydb_unlock(); 
    if (f) f(fv);
    toku_ydb_lock();
}

static int 
toku_txn_commit(DB_TXN * txn, u_int32_t flags,
                TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    if (!txn) return EINVAL;
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, flags, NULL, NULL);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            txn->mgrp->i->is_panicked = r_child;
            txn->mgrp->i->panic_string = toku_strdup("Recursive child commit failed during parent commit.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    //Remove from parent
    if (txn->parent) {
        assert(db_txn_struct_i(txn->parent)->child == txn);
        db_txn_struct_i(txn->parent)->child=NULL;
    }
    env_remove_open_txn(txn->mgrp, txn);
    //toku_ydb_notef("flags=%d\n", flags);
    if (flags & DB_TXN_SYNC) {
        toku_txn_force_fsync_on_commit(db_txn_struct_i(txn)->tokutxn);
        flags &= ~DB_TXN_SYNC;
    }
    int nosync = (flags & DB_TXN_NOSYNC)!=0 || (db_txn_struct_i(txn)->flags&DB_TXN_NOSYNC);
    flags &= ~DB_TXN_NOSYNC;

    int r;
    if (flags!=0)
	// frees the tokutxn
	// Calls ydb_yield(NULL) occasionally
        //r = toku_logger_abort(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL);
        r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL, poll, poll_extra);
    else
	// frees the tokutxn
	// Calls ydb_yield(NULL) occasionally
        //r = toku_logger_commit(db_txn_struct_i(txn)->tokutxn, nosync, ydb_yield, NULL);
        r = toku_txn_commit_txn(db_txn_struct_i(txn)->tokutxn, nosync, ydb_yield, NULL,
				poll, poll_extra);

    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
        txn->mgrp->i->is_panicked = r;
        txn->mgrp->i->panic_string = toku_strdup("Error during commit.\n");
    }
    //If panicked, we're done.
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert(r==0);

    // Close the logger after releasing the locks
    r = toku_txn_release_locks(txn);
    //toku_logger_txn_close(db_txn_struct_i(txn)->tokutxn);
    toku_txn_close_txn(db_txn_struct_i(txn)->tokutxn);
    // the toxutxn is freed, and we must free the rest. */

    //Promote list to parent (dbs that must close before abort)
    if (txn->parent) {
        //Combine lists.
        while (!toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort)) {
            struct toku_list *list = toku_list_pop(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort);
            toku_list_push(&db_txn_struct_i(txn->parent)->dbs_that_must_close_before_abort, list);
        }
    }
    else {
        //Empty the list
        while (!toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort)) {
            toku_list_pop(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort);
        }
    }

    // The txn is no good after the commit even if the commit fails, so free it up.
#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);    txn = NULL;
    if (flags!=0) return EINVAL;
    return r;
}

static u_int32_t 
toku_txn_id(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    toku_ydb_barf();
    abort();
    return -1;
}

static int 
toku_txn_abort(DB_TXN * txn,
               TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children (abort or commit are both correct, commit is cheaper)
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, DB_TXN_NOSYNC, NULL, NULL);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            txn->mgrp->i->is_panicked = r_child;
            txn->mgrp->i->panic_string = toku_strdup("Recursive child commit failed during parent abort.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    //Remove from parent
    if (txn->parent) {
        assert(db_txn_struct_i(txn->parent)->child == txn);
        db_txn_struct_i(txn->parent)->child=NULL;
    }
    env_remove_open_txn(txn->mgrp, txn);

    //All dbs that must close before abort, must now be closed
    assert(toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort));

    //int r = toku_logger_abort(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL);
    int r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL, poll, poll_extra);
    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
        txn->mgrp->i->is_panicked = r;
        txn->mgrp->i->panic_string = toku_strdup("Error during abort.\n");
    }
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert(r==0);
    r = toku_txn_release_locks(txn);
    //toku_logger_txn_close(db_txn_struct_i(txn)->tokutxn);
    toku_txn_close_txn(db_txn_struct_i(txn)->tokutxn);

#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);
    return r;
}

static int 
locked_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_txn_begin(env, stxn, txn, flags, 0); toku_ydb_unlock(); return r;
}

static u_int32_t 
locked_txn_id(DB_TXN *txn) {
    toku_ydb_lock(); u_int32_t r = toku_txn_id(txn); toku_ydb_unlock(); return r;
}

static int 
toku_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    XMALLOC(*txn_stat);
    return toku_logger_txn_rollback_raw_count(db_txn_struct_i(txn)->tokutxn, &(*txn_stat)->rollback_raw_count);
}

static int 
locked_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    toku_ydb_lock(); u_int32_t r = toku_txn_stat(txn, txn_stat); toku_ydb_unlock(); return r;
}

static int 
locked_txn_commit_with_progress(DB_TXN *txn, u_int32_t flags,
                                TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    toku_multi_operation_client_lock(); //Cannot checkpoint during a commit.
    toku_ydb_lock(); int r = toku_txn_commit(txn, flags, poll, poll_extra); toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Cannot checkpoint during a commit.
    return r;
}

static int 
locked_txn_abort_with_progress(DB_TXN *txn,
                               TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    toku_multi_operation_client_lock(); //Cannot checkpoint during an abort.
    toku_ydb_lock(); int r = toku_txn_abort(txn, poll, poll_extra); toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Cannot checkpoint during an abort.
    return r;
}

static int 
locked_txn_commit(DB_TXN *txn, u_int32_t flags) {
    int r;
    r = locked_txn_commit_with_progress(txn, flags, NULL, NULL);
    return r;
}

static int 
locked_txn_abort(DB_TXN *txn) {
    int r;
    r = locked_txn_abort_with_progress(txn, NULL, NULL);
    return r;
}

static int 
toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags, int internal) {
    HANDLE_PANICKED_ENV(env);
    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, stxn); //Cannot create child while child already exists.
    if (!toku_logger_is_open(env->i->logger)) return toku_ydb_do_error(env, EINVAL, "Environment does not have logging enabled\n");
    if (!(env->i->open_flags & DB_INIT_TXN))  return toku_ydb_do_error(env, EINVAL, "Environment does not have transactions enabled\n");
    u_int32_t txn_flags = 0;
    txn_flags |= DB_TXN_NOWAIT; //We do not support blocking locks.
    TOKU_ISOLATION child_isolation = 0;
    u_int32_t iso_flags = flags & DB_ISOLATION_FLAGS;
    if (!(iso_flags == 0 || 
          iso_flags == DB_TXN_SNAPSHOT || 
          iso_flags == DB_READ_COMMITTED || 
          iso_flags == DB_READ_UNCOMMITTED || 
          iso_flags == DB_SERIALIZABLE || 
          iso_flags == DB_INHERIT_ISOLATION)
       ) 
    {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Invalid isolation flags set\n"
            );
    }
    flags &= ~iso_flags;

    if (internal && stxn) {
        child_isolation = db_txn_struct_i(stxn)->iso;
    }
    else {
        switch (iso_flags) {
            case (DB_INHERIT_ISOLATION):
                if (stxn) {
                    child_isolation = db_txn_struct_i(stxn)->iso;
                }
                else {
                    return toku_ydb_do_error(
                        env, 
                        EINVAL, 
                        "Cannot set DB_INHERIT_ISOLATION when no parent exists\n"
                        );                    
                }
                break;
            case (DB_READ_COMMITTED):
                child_isolation = TOKU_ISO_READ_COMMITTED;
                break;
            case (DB_READ_UNCOMMITTED):
                child_isolation = TOKU_ISO_READ_UNCOMMITTED;
                break;
            case (DB_TXN_SNAPSHOT):
                child_isolation = TOKU_ISO_SNAPSHOT;
                break;
            case (DB_SERIALIZABLE):
            case (0):
                child_isolation = TOKU_ISO_SERIALIZABLE;
                break;
            default:
                assert(FALSE); // error path is above, so this should not happen
                break;
        }
    }
    if (stxn && child_isolation != db_txn_struct_i(stxn)->iso) {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Cannot set isolation level of transaction to something different \
                isolation level\n"
            );   
    }

    if (flags&DB_TXN_NOWAIT) {
        txn_flags |=  DB_TXN_NOWAIT;
        flags     &= ~DB_TXN_NOWAIT;
    }
    if (flags&DB_TXN_NOSYNC) {
        txn_flags |=  DB_TXN_NOSYNC;
        flags     &= ~DB_TXN_NOSYNC;
    }
    if (flags!=0) return toku_ydb_do_error(env, EINVAL, "Invalid flags passed to DB_ENV->txn_begin\n");

    size_t result_size = sizeof(DB_TXN)+sizeof(struct __toku_db_txn_internal); // the internal stuff is stuck on the end.
    DB_TXN *result = toku_malloc(result_size);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, result_size);
    //toku_ydb_notef("parent=%p flags=0x%x\n", stxn, flags);
    result->mgrp = env;
#define STXN(name) result->name = locked_txn_ ## name
    STXN(abort);
    STXN(commit);
    STXN(abort_with_progress);
    STXN(commit_with_progress);
    STXN(id);
#undef STXN
    result->txn_stat = locked_txn_stat;


    result->parent = stxn;
#if !TOKUDB_NATIVE_H
    MALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    memset(db_txn_struct_i(result), 0, sizeof *db_txn_struct_i(result));
    db_txn_struct_i(result)->flags = txn_flags;
    db_txn_struct_i(result)->iso = child_isolation;
    toku_list_init(&db_txn_struct_i(result)->dbs_that_must_close_before_abort);

    int r;
    if (env->i->open_flags & DB_INIT_LOCK && !stxn) {
        r = toku_lth_create(&db_txn_struct_i(result)->lth,
                            toku_malloc, toku_free, toku_realloc);
        if (r!=0) {
#if !TOKUDB_NATIVE_H
            toku_free(db_txn_struct_i(result));
#endif
            toku_free(result);
            return r;
        }
    }
    
    //r = toku_logger_txn_begin(stxn ? db_txn_struct_i(stxn)->tokutxn : 0, &db_txn_struct_i(result)->tokutxn, env->i->logger);
    TXN_SNAPSHOT_TYPE snapshot_type;
    switch(db_txn_struct_i(result)->iso){
        case(TOKU_ISO_SNAPSHOT):
        {
            snapshot_type = TXN_SNAPSHOT_ROOT;
            break;
        }
        case(TOKU_ISO_READ_COMMITTED):
        {
            snapshot_type = TXN_SNAPSHOT_CHILD;
            break;
        }
        default:
        {
            snapshot_type = TXN_SNAPSHOT_NONE;
            break;
        }
    }
    r = toku_txn_begin_txn(result,		   
			   stxn ? db_txn_struct_i(stxn)->tokutxn : 0, 
			   &db_txn_struct_i(result)->tokutxn, 
			   env->i->logger,
			   snapshot_type
			   );
    if (r != 0)
        return r;

    //Add to the list of children for the parent.
    if (result->parent) {
        assert(!db_txn_struct_i(result->parent)->child);
        db_txn_struct_i(result->parent)->child = result;
    }
    env_add_open_txn(env, result);
    *txn = result;
    return 0;
}

int 
log_compare(const DB_LSN * a, const DB_LSN * b) {
    toku_ydb_lock();
    fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
    assert(0);
    toku_ydb_unlock();
    return 0;
}

static void env_note_zombie_db_closed(DB_ENV *env, DB *db);

static int
db_close_before_brt(DB *db, u_int32_t UU(flags)) {
    if (db_opened(db) && db->i->dname) {
        // internal (non-user) dictionary has no dname
        env_note_zombie_db_closed(db->dbenv, db);  // tell env that this db is no longer a zombie (it is completely closed)
    }
    char *error_string = 0;
    int r1 = toku_close_brt(db->i->brt, &error_string);
    if (r1) {
	db->dbenv->i->is_panicked = r1; // Panicking the whole environment may be overkill, but I'm not sure what else to do.
	db->dbenv->i->panic_string = error_string;
	if (error_string) {
	    toku_ydb_do_error(db->dbenv, r1, "%s\n", error_string);
	} else {
	    toku_ydb_do_error(db->dbenv, r1, "Closing file\n");
	}
	error_string=0;
    }
    assert(error_string==0);
    int r2 = 0;
    if (db->i->lt) {
        toku_lt_remove_db_ref(db->i->lt, db);
    }
    // printf("%s:%d %d=__toku_db_close(%p)\n", __FILE__, __LINE__, r, db);
    // Even if panicked, let's close as much as we can.
    int is_panicked = toku_env_is_panicked(db->dbenv); 
    toku_sdbt_cleanup(&db->i->skey);
    toku_sdbt_cleanup(&db->i->sval);
    if (db->i->dname) toku_free(db->i->dname);
    toku_free(db->i);
    toku_free(db);
    if (r1) return r1;
    if (r2) return r2;
    if (is_panicked) return EINVAL;
    return 0;
}

// return 0 if v and dbv refer to same db (including same dname)
// return <0 if v is earlier in omt than dbv
// return >0 if v is later in omt than dbv
static int
find_db_by_db (OMTVALUE v, void *dbv) {
    DB *db = v;            // DB* that is stored in the omt
    DB *dbfind = dbv;      // extra, to be compared to v
    int cmp;
    const char *dname     = db->i->dname;
    const char *dnamefind = dbfind->i->dname;
    cmp = strcmp(dname, dnamefind);
    if (cmp != 0) return cmp;
    int is_zombie     = db->i->is_zombie != 0;
    int is_zombiefind = dbfind->i->is_zombie != 0;
    cmp = is_zombie - is_zombiefind;
    if (cmp != 0) return cmp;
    if (db < dbfind) return -1;
    if (db > dbfind) return  1;
    return 0;
}

// Tell env that there is a new db handle (with non-unique dname in db->i-dname)
static void
env_note_db_opened(DB_ENV *env, DB *db) {
    assert(db->i->dname);  // internal (non-user) dictionary has no dname
    assert(!db->i->is_zombie);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_open_dbs++;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx, NULL);
    assert(r==DB_NOTFOUND); //Must not already be there.
    r = toku_omt_insert_at(env->i->open_dbs, db, idx);
    assert(r==0);
}

static void
env_note_db_closed(DB_ENV *env, DB *db) {
    assert(db->i->dname);
    assert(!db->i->is_zombie);
    assert(env->i->num_open_dbs);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_open_dbs--;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx, NULL);
    assert(r==0); //Must already be there.
    assert((DB*)dbv == db);
    r = toku_omt_delete_at(env->i->open_dbs, idx);
    assert(r==0);
}

// Tell env that there is a new db handle (with non-unique dname in db->i-dname)
static void
env_note_zombie_db(DB_ENV *env, DB *db) {
    assert(db->i->dname);  // internal (non-user) dictionary has no dname
    assert(db->i->is_zombie);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_zombie_dbs++;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx, NULL);
    assert(r==DB_NOTFOUND); //Must not already be there.
    r = toku_omt_insert_at(env->i->open_dbs, db, idx);
    assert(r==0);
}

static void
env_note_zombie_db_closed(DB_ENV *env, DB *db) {
    assert(db->i->dname);
    assert(db->i->is_zombie);
    assert(env->i->num_zombie_dbs);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_zombie_dbs--;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx, NULL);
    assert(r==0); //Must already be there.
    assert((DB*)dbv == db);
    r = toku_omt_delete_at(env->i->open_dbs, idx);
    assert(r==0);
}

static int
find_zombie_db_by_dname (OMTVALUE v, void *dnamev) {
    DB *db = v;            // DB* that is stored in the omt
    int cmp;
    const char *dname     = db->i->dname;
    const char *dnamefind = dnamev;
    cmp = strcmp(dname, dnamefind);
    if (cmp != 0) return cmp;
    int is_zombie     = db->i->is_zombie != 0;
    int is_zombiefind = 1;
    cmp = is_zombie - is_zombiefind;
    return cmp;
}

static int
find_open_db_by_dname (OMTVALUE v, void *dnamev) {
    DB *db = v;            // DB* that is stored in the omt
    int cmp;
    const char *dname     = db->i->dname;
    const char *dnamefind = dnamev;
    cmp = strcmp(dname, dnamefind);
    if (cmp != 0) return cmp;
    int is_zombie     = db->i->is_zombie != 0;
    int is_zombiefind = 0;
    cmp = is_zombie - is_zombiefind;
    return cmp;
}

// return true if there is any db open with the given dname
static BOOL
env_is_db_with_dname_open(DB_ENV *env, const char *dname) {
    int r;
    BOOL rval;
    OMTVALUE dbv;
    uint32_t idx;
    r = toku_omt_find_zero(env->i->open_dbs, find_open_db_by_dname, (void*)dname, &dbv, &idx, NULL);
    if (r==0) {
        DB *db = dbv;
        assert(strcmp(dname, db->i->dname) == 0);
        assert(!db->i->is_zombie);
        rval = TRUE;
    }
    else {
        assert(r==DB_NOTFOUND);
        rval = FALSE;
    }
    return rval;
}

// return true if there is any db open with the given dname
static DB*
env_get_zombie_db_with_dname(DB_ENV *env, const char *dname) {
    int r;
    DB* rval;
    OMTVALUE dbv;
    uint32_t idx;
    r = toku_omt_find_zero(env->i->open_dbs, find_zombie_db_by_dname, (void*)dname, &dbv, &idx, NULL);
    if (r==0) {
        DB *db = dbv;
        assert(db);
        assert(strcmp(dname, db->i->dname) == 0);
        assert(db->i->is_zombie);
        rval = db;
    }
    else {
        assert(r==DB_NOTFOUND);
        rval = NULL;
    }
    return rval;
}

void
toku_db_add_ref(DB *db) {
    db->i->refs++;
}

void
toku_db_release_ref(DB *db){
    db->i->refs--;
}

//DB->close()
static int 
toku_db_close(DB * db, u_int32_t flags) {
    int r = 0;
    if (db->i->refs != 1) {
        r = EBUSY;
    } else {
        db->i->refs = 0;
        if (db_opened(db) && db->i->dname) {
            // internal (non-user) dictionary has no dname
            env_note_db_closed(db->dbenv, db);  // tell env that this db is no longer in use by the user of this api (user-closed, may still be in use by fractal tree internals)
            db->i->is_zombie = TRUE;
            env_note_zombie_db(db->dbenv, db);  // tell env that this db is a zombie
        }
        //Remove from transaction's list of 'must close' if necessary.
        if (!toku_list_empty(&db->i->dbs_that_must_close_before_abort))
            toku_list_remove(&db->i->dbs_that_must_close_before_abort);

        r = toku_brt_db_delay_closed(db->i->brt, db, db_close_before_brt, flags);
    }
    return r;
}


//Get the main portion of a cursor flag (excluding the bitwise or'd components).
static int 
get_main_cursor_flag(u_int32_t flags) {
    return flags & DB_OPFLAGS_MASK;
}

static int 
get_nonmain_cursor_flags(u_int32_t flags) {
    return flags & ~(DB_OPFLAGS_MASK);
}

static inline BOOL 
toku_c_uninitialized(DBC* c) {
    return toku_brt_cursor_uninitialized(dbc_struct_i(c)->c);
}            

typedef struct query_context_wrapped_t {
    DBT               *key;
    DBT               *val;
    struct simple_dbt *skey;
    struct simple_dbt *sval;
} *QUERY_CONTEXT_WRAPPED, QUERY_CONTEXT_WRAPPED_S;

static inline void
query_context_wrapped_init(QUERY_CONTEXT_WRAPPED context, DBC *c, DBT *key, DBT *val) {
    context->key  = key;
    context->val  = val;
    context->skey = dbc_struct_i(c)->skey;
    context->sval = dbc_struct_i(c)->sval;
}

static int
c_get_wrapper_callback(DBT const *key, DBT const *val, void *extra) {
    QUERY_CONTEXT_WRAPPED context = extra;
    int r;
              r = toku_dbt_set(key->size, key->data, context->key, context->skey);
    if (r==0) r = toku_dbt_set(val->size, val->data, context->val, context->sval);
    return r;
}

static int 
toku_c_get_current_unconditional(DBC* c, u_int32_t flags, DBT* key, DBT* val) {
    int r;
    QUERY_CONTEXT_WRAPPED_S context; 
    query_context_wrapped_init(&context, c, key, val);
    r = toku_c_getf_current_binding(c, flags, c_get_wrapper_callback, &context);
    return r;
}

static inline void 
toku_swap_flag(u_int32_t* flag, u_int32_t* get_flag, u_int32_t new_flag) {
    *flag    -= *get_flag;
    *get_flag =  new_flag;
    *flag    += *get_flag;
}

/*
    Used for partial implementation of nested transactions.
    Work is done by children as normal, but all locking is done by the
    root of the nested txn tree.
    This may hold extra locks, and will not work as expected when
    a node has two non-completed txns at any time.
*/
static inline DB_TXN* 
toku_txn_ancestor(DB_TXN* txn) {
    while (txn && txn->parent) txn = txn->parent;

    return txn;
}

static int toku_txn_add_lt(DB_TXN* txn, toku_lock_tree* lt);

/* c_get has many subfunctions with lots of parameters
 * this structure exists to simplify it. */
typedef struct {
    DBC*        c;                  // The cursor
    DB*         db;                 // db the cursor is iterating over
    DB_TXN*     txn_anc;            // The (root) ancestor of the transaction
    TXNID       id_anc;
    DBT         tmp_key;            // Temporary key to protect out param
    DBT         tmp_val;            // Temporary val to protect out param
    u_int32_t   flag;               // The c_get flag
    u_int32_t   op;                 // The operation portion of the c_get flag
    u_int32_t   lock_flags;         // The prelock flags.
    BOOL        cursor_is_write;    // Whether op can change position of cursor
    BOOL        key_is_read;        
    BOOL        key_is_write;
    BOOL        val_is_read;
    BOOL        val_is_write;
    BOOL        duplicates;
    BOOL        tmp_key_malloced;
    BOOL        tmp_val_malloced;
} C_GET_VARS;


static inline u_int32_t 
get_prelocked_flags(u_int32_t flags) {
    u_int32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);
    return lock_flags;
}

static inline u_int32_t 
get_cursor_prelocked_flags(u_int32_t flags, DBC* dbc) {
    u_int32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);

    //DB_READ_UNCOMMITTED and DB_READ_COMMITTED transactions 'own' all read locks for user-data dictionaries.
    if (dbc_struct_i(dbc)->iso != TOKU_ISO_SERIALIZABLE)
    {
        lock_flags |= DB_PRELOCKED;
    }
    return lock_flags;
}

static int
toku_c_get(DBC* c, DBT* key, DBT* val, u_int32_t flag) {
    //This function exists for legacy (test compatibility) purposes/parity with bdb.
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    u_int32_t main_flag       = get_main_cursor_flag(flag);
    u_int32_t remaining_flags = get_nonmain_cursor_flags(flag);
    int r;
    QUERY_CONTEXT_WRAPPED_S context;
    //Passing in NULL for a key or val means that it is NOT an output.
    //    Both key and val are output:
    //        query_context_wrapped_init(&context, c, key,  val);
    //    Val is output, key is not:
    //            query_context_wrapped_init(&context, c, NULL, val);
    //    Neither key nor val are output:
    //	    query_context_wrapped_init(&context, c, NULL, NULL); // Used for DB_GET_BOTH
    switch (main_flag) {
        case (DB_FIRST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_first(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_LAST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_last(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT):
        case (DB_NEXT_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_next(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_PREV):
        case (DB_PREV_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#ifdef DB_PREV_DUP
        case (DB_PREV_DUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev_dup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#endif
        case (DB_CURRENT):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_CURRENT_BINDING):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current_binding(c, remaining_flags, c_get_wrapper_callback, &context);
            break;

        case (DB_SET):
            query_context_wrapped_init(&context, c, NULL, val);
            r = toku_c_getf_set(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_set_range(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE_REVERSE):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_set_range_reverse(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        default:
            r = EINVAL;
            break;
    }
    return r;
}

static int 
locked_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_first(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_last(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_next(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_prev(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_current(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_current_binding(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_set(DBC *c, u_int32_t flag, DBT * key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_set(c, flag, key, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_set_range(DBC *c, u_int32_t flag, DBT * key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_set_range(c, flag, key, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_getf_set_range_reverse(DBC *c, u_int32_t flag, DBT * key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_set_range_reverse(c, flag, key, f, extra); toku_ydb_unlock(); return r;
}

typedef struct {
    BOOL            is_read_lock;
    DB_TXN         *txn;
    DB             *db;
    toku_lock_tree *lt;
    DBT const      *left_key;
    DBT const      *right_key;
} *RANGE_LOCK_REQUEST, RANGE_LOCK_REQUEST_S;

static void
range_lock_request_init(RANGE_LOCK_REQUEST request,
                        BOOL       is_read_lock,
                        DB_TXN    *txn,
                        DB        *db,
                        DBT const *left_key,
                        DBT const *right_key) {
    request->is_read_lock = is_read_lock;
    request->txn = txn;
    request->db = db;
    request->lt = db->i->lt;
    request->left_key = left_key;
    request->right_key = right_key;
}

static void
read_lock_request_init(RANGE_LOCK_REQUEST request,
                       DB_TXN    *txn,
                       DB        *db,
                       DBT const *left_key,
                       DBT const *right_key) {
    range_lock_request_init(request, TRUE, txn, db,
                            left_key, right_key);
}

static void
write_lock_request_init(RANGE_LOCK_REQUEST request,
                        DB_TXN    *txn,
                        DB        *db,
                        DBT const *left_key,
                        DBT const *right_key) {
    range_lock_request_init(request, FALSE, txn, db,
                            left_key, right_key);
}

static int
grab_range_lock(RANGE_LOCK_REQUEST request) {
    int r;
    //TODO: (Multithreading) Grab lock protecting lock tree
    DB_TXN *txn_anc = toku_txn_ancestor(request->txn);
    r = toku_txn_add_lt(txn_anc, request->lt);
    if (r==0) {
        TXNID txn_anc_id = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);
        if (request->is_read_lock)
            r = toku_lt_acquire_range_read_lock(request->lt, request->db, txn_anc_id,
                                                request->left_key, request->right_key);
        else 
            r = toku_lt_acquire_range_write_lock(request->lt, request->db, txn_anc_id,
                                                 request->left_key, request->right_key);
    }
    //TODO: (Multithreading) Release lock protecting lock tree
    return r;
}

int
toku_grab_read_lock_on_directory (DB* db, DB_TXN * txn) {
    RANGE_LOCK_REQUEST_S request;
    char *   dname = db->i->dname;
    DBT key_in_directory;

    // bad hack because some environment dictionaries do not have a dname
    if (!dname || (db->dbenv->i->directory->i->lt == NULL))
        return 0;

    toku_fill_dbt(&key_in_directory, dname, strlen(dname)+1);
    //Left end of range == right end of range (point lock)
    read_lock_request_init(
        &request, 
        txn, 
        db->dbenv->i->directory,
        &key_in_directory,
        &key_in_directory
        );
    int r = grab_range_lock(&request);
    return r;
}

//This is the user level callback function given to ydb layer functions like
//toku_c_getf_first

typedef struct __toku_is_write_op {
    BOOL is_write_op;
} WRITE_OP;

typedef struct query_context_base_t {
    BRT_CURSOR  c;
    DB_TXN     *txn;
    DB         *db;
    void       *f_extra;
    int         r_user_callback;
    BOOL        do_locking;
    BOOL        is_write_op;
} *QUERY_CONTEXT_BASE, QUERY_CONTEXT_BASE_S;

typedef struct query_context_t {
    QUERY_CONTEXT_BASE_S  base;
    YDB_CALLBACK_FUNCTION f;
} *QUERY_CONTEXT, QUERY_CONTEXT_S;

typedef struct query_context_with_input_t {
    QUERY_CONTEXT_BASE_S  base;
    YDB_CALLBACK_FUNCTION f;
    DBT                  *input_key;
    DBT                  *input_val;
} *QUERY_CONTEXT_WITH_INPUT, QUERY_CONTEXT_WITH_INPUT_S;


static void
query_context_base_init(QUERY_CONTEXT_BASE context, DBC *c, u_int32_t flag, WRITE_OP is_write_op, void *extra) {
    context->c       = dbc_struct_i(c)->c;
    context->txn     = dbc_struct_i(c)->txn;
    context->db      = c->dbp;
    context->f_extra = extra;
    context->is_write_op = is_write_op.is_write_op;
    u_int32_t lock_flags = get_cursor_prelocked_flags(flag, c);
    flag &= ~lock_flags;
    if (context->is_write_op) lock_flags &= DB_PRELOCKED_WRITE; // Only care about whether already locked for write
    assert(flag==0);
    context->do_locking = (BOOL)(context->db->i->lt!=NULL && !lock_flags);
    context->r_user_callback = 0;
}

static void
query_context_init(QUERY_CONTEXT context, DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    WRITE_OP is_write = {FALSE};
    query_context_base_init(&context->base, c, flag, is_write, extra);
    context->f = f;
}

static void
query_context_init_write_op(QUERY_CONTEXT context, DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    WRITE_OP is_write = {TRUE};
    query_context_base_init(&context->base, c, flag, is_write, extra);
    context->f = f;
}

static void
query_context_with_input_init(QUERY_CONTEXT_WITH_INPUT context, DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    WRITE_OP is_write = {FALSE};
    query_context_base_init(&context->base, c, flag, is_write, extra);
    context->f         = f;
    context->input_key = key;
    context->input_val = val;
}

static int c_del_callback(DBT const *key, DBT const *val, void *extra);

//Delete whatever the cursor is pointing at.
static int
toku_c_del(DBC * c, u_int32_t flags) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    u_int32_t unchecked_flags = flags;
    //DB_DELETE_ANY means delete regardless of whether it exists in the db.
    u_int32_t flag_for_brt = flags&DB_DELETE_ANY;
    unchecked_flags &= ~flag_for_brt;
    u_int32_t lock_flags = get_cursor_prelocked_flags(flags, c);
    unchecked_flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(c->dbp->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));

    int r = 0;
    if (unchecked_flags!=0) r = EINVAL;
    else {
        if (do_locking) {
            QUERY_CONTEXT_S context;
            query_context_init_write_op(&context, c, lock_flags, NULL, NULL);
            //We do not need a read lock, we must already have it.
            r = toku_c_getf_current_binding(c, DB_PRELOCKED, c_del_callback, &context);
        }
        if (r==0) {
            //Do the actual delete.
            TOKUTXN txn = dbc_struct_i(c)->txn ? db_txn_struct_i(dbc_struct_i(c)->txn)->tokutxn : 0;
            r = toku_brt_cursor_delete(dbc_struct_i(c)->c, flag_for_brt, txn);
        }
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_del_callback(DBT const *key, DBT const *val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    assert(context->do_locking);
    assert(context->is_write_op);
    assert(key!=NULL);
    assert(val!=NULL);
    //Lock:
    //  left(key,val)==right(key,val) == (key, val);
    RANGE_LOCK_REQUEST_S request;
    write_lock_request_init(&request, context->txn, context->db,
                            key, key);
    r = grab_range_lock(&request);

    //Give brt-layer an error (if any) to return from toku_c_getf_current_binding
    return r;
}

static int c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    num_point_queries++;   // accountability
    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    //toku_brt_cursor_first will call c_getf_first_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_first(dbc_struct_i(c)->c, c_getf_first_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, &found_key);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_first
    return r;
}

static int c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    num_point_queries++;   // accountability
    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    //toku_brt_cursor_last will call c_getf_last_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_last(dbc_struct_i(c)->c, c_getf_last_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   &found_key,           toku_lt_infinity);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_last
    return r;
}

static int c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (toku_c_uninitialized(c)) r = toku_c_getf_first(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        num_sequential_queries++;   // accountability
        query_context_init(&context, c, flag, f, extra); 
        //toku_brt_cursor_next will call c_getf_next_callback(..., context) (if query is successful)
        r = toku_brt_cursor_next(dbc_struct_i(c)->c, c_getf_next_callback, &context);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *right_key = key==NULL ? toku_lt_infinity : &found_key;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               prevkey,
                               right_key);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_next
    return r;
}

static int c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (toku_c_uninitialized(c)) r = toku_c_getf_last(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        num_sequential_queries++;   // accountability
        query_context_init(&context, c, flag, f, extra); 
        //toku_brt_cursor_prev will call c_getf_prev_callback(..., context) (if query is successful)
        r = toku_brt_cursor_prev(dbc_struct_i(c)->c, c_getf_prev_callback, &context);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *left_key = key==NULL ? toku_lt_neg_infinity : &found_key;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               left_key,
                               prevkey);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_prev
    return r;
}

static int c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    num_sequential_queries++;   // accountability
    query_context_init(&context, c, flag, f, extra); 
    //toku_brt_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_current(dbc_struct_i(c)->c, DB_CURRENT, c_getf_current_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    int r=0;
    //Call application-layer callback if found.
    if (key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_current
    return r;
}

static int
toku_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    num_sequential_queries++;   // accountability
    query_context_init(&context, c, flag, f, extra); 
    //toku_brt_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_current(dbc_struct_i(c)->c, DB_CURRENT_BINDING, c_getf_current_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

static int c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_set(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    num_point_queries++;   // accountability
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    //toku_brt_cursor_set will call c_getf_set_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set(dbc_struct_i(c)->c, key, c_getf_set_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key,val) = (input_key, found ? found_val : infinity)
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key,
                                   super_context->input_key);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key,
                                   super_context->input_key);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_set
    return r;
}

static int c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_set_range(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    num_point_queries++;   // accountability
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    //toku_brt_cursor_set_range will call c_getf_set_range_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set_range(dbc_struct_i(c)->c, key, c_getf_set_range_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key) = found ? found_key : infinity
    //  right(val) = found ? found_val : infinity
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key,
                                   &found_key);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key,
                                   toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_set_range
    return r;
}

static int c_getf_set_range_reverse_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_set_range_reverse(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    num_point_queries++;   // accountability
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    //toku_brt_cursor_set_range_reverse will call c_getf_set_range_reverse_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set_range_reverse(dbc_struct_i(c)->c, key, c_getf_set_range_reverse_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_range_reverse_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key) = found ? found_key : -infinity
    //  left(val) = found ? found_val : -infinity
    //  right(key,val)  = (input_key, infinity)
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   &found_key,
                                   super_context->input_key);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity,
                                   super_context->input_key);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_set_range_reverse
    return r;
}

static int toku_c_close(DBC * c) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = toku_brt_cursor_close(dbc_struct_i(c)->c);
    toku_sdbt_cleanup(&dbc_struct_i(c)->skey_s);
    toku_sdbt_cleanup(&dbc_struct_i(c)->sval_s);
#if !TOKUDB_NATIVE_H
    toku_free(dbc_struct_i(c));
#endif
    toku_free(c);
    return r;
}

static inline int 
keyeq(DBC *c, DBT *a, DBT *b) {
    DB *db = c->dbp;
    return db->i->brt->compare_fun(db, a, b) == 0;
}

// Return the number of entries whose key matches the key currently 
// pointed to by the brt cursor.  
static int 
toku_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags) {
    HANDLE_PANICKED_DB(cursor->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(cursor);
    int r;
    DBC *count_cursor = 0;
    DBT currentkey;

    init_dbt_realloc(&currentkey);
    u_int32_t lock_flags = get_cursor_prelocked_flags(flags, cursor);
    flags &= ~lock_flags;
    if (flags != 0) {
        r = EINVAL; goto finish;
    }

    r = toku_c_get_current_unconditional(cursor, lock_flags, &currentkey, NULL);
    if (r != 0) goto finish;

    //TODO: Optimization
    //if (do_locking) {
    //   do a lock from currentkey,-infinity to currentkey,infinity
    //   lock_flags |= DB_PRELOCKED
    //}
    
    r = toku_db_cursor(cursor->dbp, dbc_struct_i(cursor)->txn, &count_cursor, 0, 0);
    if (r != 0) goto finish;

    r = toku_c_getf_set(count_cursor, lock_flags, &currentkey, ydb_getf_do_nothing, NULL);
    if (r==0) {
	*count = 1; // there is a key, so the count is one (since we don't have DUP dbs anymore, the only answers are 0 or 1.
    } else {
	*count = 0;
    }
    r = 0;
finish:
    if (currentkey.data) toku_free(currentkey.data);
    if (count_cursor) {
        int rr = toku_c_close(count_cursor); assert(rr == 0);
    }
    return r;
}


///////////
//db_getf_XXX is equivalent to c_getf_XXX, without a persistent cursor

static int
db_getf_set(DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    DBC *c;
    uint32_t iso_flags = flags & DB_ISOLATION_FLAGS;
    flags &= ~DB_ISOLATION_FLAGS;
    int r = toku_db_cursor(db, txn, &c, iso_flags, 1);
    if (r==0) {
        r = toku_c_getf_set(c, flags, key, f, extra);
        int r2 = toku_c_close(c);
        if (r==0) r = r2;
    }
    return r;
}

////////////

static int 
get_point_lock(DB *db, DB_TXN *txn, const DBT *key) {
    RANGE_LOCK_REQUEST_S request;
    //Left end of range == right end of range (point lock)
    write_lock_request_init(&request, txn, db, key, key);
    int r = grab_range_lock(&request);
    return r;
}

static int
toku_db_del(DB *db, DB_TXN *txn, DBT *key, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);

    u_int32_t unchecked_flags = flags;
    //DB_DELETE_ANY means delete regardless of whether it exists in the db.
    BOOL error_if_missing = (BOOL)(!(flags&DB_DELETE_ANY));
    unchecked_flags &= ~DB_DELETE_ANY;
    u_int32_t lock_flags = get_prelocked_flags(flags);
    unchecked_flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(db->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));

    int r = 0;
    if (unchecked_flags!=0) 
        r = EINVAL;

    if (r == 0) {
        r = toku_grab_read_lock_on_directory(db, txn);
    }
    if (r == 0 && error_if_missing) {
        //Check if the key exists in the db.
        r = db_getf_set(db, txn, lock_flags|DB_SERIALIZABLE, key, ydb_getf_do_nothing, NULL);
    }
    if (r == 0 && do_locking) {
        //Do locking if necessary.
        r = get_point_lock(db, txn, key);
    }
    if (r == 0) {
        //Do the actual deleting.
        r = toku_brt_delete(db->i->brt, key, txn ? db_txn_struct_i(txn)->tokutxn : 0);
    }

    if (r == 0) 
        num_deletes++;       // accountability 
    else
        num_deletes_fail++;

    return r;
}

static int
log_del_single(DB_TXN *txn, BRT brt, const DBT *key) {
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    int r = toku_brt_log_del(ttxn, brt, key);
    return r;
}

static uint32_t
sum_size(uint32_t num_keys, DBT keys[], uint32_t overhead) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < num_keys; i++) 
        sum += keys[i].size + overhead;
    return sum;
}

static int
log_del_multiple(DB_TXN *txn, DB *src_db, const DBT *key, const DBT *val, uint32_t num_dbs, BRT brts[], DBT keys[]) {
    int r = 0;
    if (num_dbs > 0) {
        TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
        BRT src_brt  = src_db ? src_db->i->brt : NULL;
        const uint32_t log_entry_overhead = 24; // rough approximation of the log entry overhead for deletes
        uint32_t del_multiple_size = key->size + val->size + log_entry_overhead;
        uint32_t del_single_sizes = sum_size(num_dbs, keys, log_entry_overhead);
        if (del_single_sizes < del_multiple_size) {
            for (uint32_t i = 0; r == 0 && i < num_dbs; i++)
                r = log_del_single(txn, brts[i], &keys[i]);
        } else {
            r = toku_brt_log_del_multiple(ttxn, src_brt, brts, num_dbs, key, val);
        }
    }
    return r;
}

static uint32_t 
lookup_src_db(uint32_t num_dbs, DB *db_array[], DB *src_db) {
    uint32_t which_db;
    for (which_db = 0; which_db < num_dbs; which_db++) 
        if (db_array[which_db] == src_db)
            break;
    return which_db;
}

static int
do_del_multiple(DB_TXN *txn, uint32_t num_dbs, DB *db_array[], DBT keys[]) {
    int r = 0;
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    for (uint32_t which_db = 0; r == 0 && which_db < num_dbs; which_db++) {
        DB *db = db_array[which_db];
	int do_delete = TRUE;
	DB_INDEXER *indexer = toku_db_get_indexer(db);
	if (indexer) { // if this db is the index under construction
            DB *src_db = toku_indexer_get_src_db(indexer);
            invariant(src_db != NULL);
            uint32_t which_src_db = lookup_src_db(num_dbs, db_array, src_db);
            if (which_src_db >= num_dbs)
                r = EINVAL;
            else
                do_delete = !toku_indexer_is_key_right_of_le_cursor(indexer, src_db, &keys[which_src_db]);
        }
	if (r == 0 && do_delete) {
            r = toku_brt_maybe_delete(db->i->brt, &keys[which_db], ttxn, FALSE, ZERO_LSN, FALSE);
        }
    }
    return r;
}

static int
env_del_multiple(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *key, 
    const DBT *val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys, 
    uint32_t *flags_array) 
{
    int r;
    DBT del_keys[num_dbs];
    BOOL multi_accounting = TRUE;  // use num_multi_delete accountability counters 

    // special case single DB
    if (num_dbs == 1 && src_db == db_array[0]) {
	multi_accounting = FALSE;
        r = toku_db_del(db_array[0], txn, (DBT *) key, flags_array[0]);
        goto cleanup;
    }

    HANDLE_PANICKED_ENV(env);

    if (!txn) {
        r = EINVAL;
        goto cleanup;
    }
    if (!env->i->generate_row_for_del) {
        r = EINVAL;
        goto cleanup;
    }

    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn);

    {
    uint32_t lock_flags[num_dbs];
    uint32_t remaining_flags[num_dbs];
    BRT brts[num_dbs];

    for (uint32_t which_db = 0; which_db < num_dbs; which_db++) {
        DB *db = db_array[which_db];

        if (db == src_db) {
            del_keys[which_db] = *key;
        }
        else {
        //Generate the key
            r = env->i->generate_row_for_del(db, src_db, &keys[which_db], key, val);
            if (r != 0) goto cleanup;
            del_keys[which_db] = keys[which_db];
        }
        lock_flags[which_db] = get_prelocked_flags(flags_array[which_db]);
        remaining_flags[which_db] = flags_array[which_db] & ~lock_flags[which_db];

        if (remaining_flags[which_db] & ~DB_DELETE_ANY) {
            r = EINVAL;
            goto cleanup;
        }
        BOOL error_if_missing = (BOOL)(!(remaining_flags[which_db]&DB_DELETE_ANY));
        if (error_if_missing) {
            //Check if the key exists in the db.
            r = db_getf_set(db, txn, lock_flags[which_db]|DB_SERIALIZABLE, &del_keys[which_db], ydb_getf_do_nothing, NULL);
            if (r != 0) goto cleanup;
        }

        //Do locking if necessary.
        r = toku_grab_read_lock_on_directory(db, txn);
        if (r != 0) goto cleanup;

        if (db->i->lt && !(lock_flags[which_db] & DB_PRELOCKED_WRITE)) {
            //Needs locking
            r = get_point_lock(db, txn, &del_keys[which_db]);
            if (r != 0) goto cleanup;
        }
        brts[which_db] = db->i->brt;
    }

    if (num_dbs == 1)
        r = log_del_single(txn, brts[0], &del_keys[0]);
    else
        r = log_del_multiple(txn, src_db, key, val, num_dbs, brts, del_keys);

    if (r == 0) 
        r = do_del_multiple(txn, num_dbs, db_array, del_keys);
    }

cleanup:
    if (multi_accounting) {
	if (r == 0)
	    num_multi_deletes += num_dbs;
	else
	    num_multi_deletes_fail += num_dbs;
    }
    return r;
}


static int 
locked_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag) {
    //{ unsigned int i; printf("cget flags=%d keylen=%d key={", flag, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    toku_ydb_lock(); int r = toku_c_get(c, key, data, flag); toku_ydb_unlock();
    //{ unsigned int i; printf("cgot r=%d keylen=%d key={", r, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    return r;
}

static int 
locked_c_close(DBC * c) {
    toku_ydb_lock(); int r = toku_c_close(c); toku_ydb_unlock(); return r;
}

static int 
locked_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_c_count(cursor, count, flags); toku_ydb_unlock(); return r;
}

static int 
locked_c_del(DBC * c, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_c_del(c, flags); toku_ydb_unlock(); return r;
}

static int locked_c_pre_acquire_read_lock(DBC *dbc, const DBT *key_left, const DBT *key_right);

static int 
toku_db_cursor(DB * db, DB_TXN * txn, DBC ** c, u_int32_t flags, int is_temporary_cursor) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    DB_ENV* env = db->dbenv;
    int r;
    size_t result_size = sizeof(DBC)+sizeof(struct __toku_dbc_internal); // internal stuff stuck on the end
    if (!(flags == 0 || 
          flags == DB_SERIALIZABLE || 
          flags == DB_INHERIT_ISOLATION)
       ) 
    {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Invalid isolation flags set for toku_db_cursor\n"
            );
    }
    r = toku_grab_read_lock_on_directory(db, txn);
    if (r != 0) {
        return r;
    }
    
    DBC *result = toku_malloc(result_size);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, result_size);
#define SCRS(name) result->name = locked_ ## name
    SCRS(c_get);
    SCRS(c_close);
    SCRS(c_del);
    SCRS(c_count);
    SCRS(c_getf_first);
    SCRS(c_getf_last);
    SCRS(c_getf_next);
    SCRS(c_getf_prev);
    SCRS(c_getf_current);
    SCRS(c_getf_current_binding);
    SCRS(c_getf_set);
    SCRS(c_getf_set_range);
    SCRS(c_getf_set_range_reverse);
    SCRS(c_pre_acquire_read_lock);
#undef SCRS

#if !TOKUDB_NATIVE_H
    MALLOC(result->i); // otherwise it is allocated as part of result->ii
    assert(result->i);
#endif
    result->dbp = db;
    dbc_struct_i(result)->txn = txn;
    dbc_struct_i(result)->skey_s = (struct simple_dbt){0,0};
    dbc_struct_i(result)->sval_s = (struct simple_dbt){0,0};
    if (is_temporary_cursor) {
	dbc_struct_i(result)->skey = &db->i->skey;
	dbc_struct_i(result)->sval = &db->i->sval;
    } else {
	dbc_struct_i(result)->skey = &dbc_struct_i(result)->skey_s;
	dbc_struct_i(result)->sval = &dbc_struct_i(result)->sval_s;
    }
    switch(flags) {
        case (DB_SERIALIZABLE):
            dbc_struct_i(result)->iso = TOKU_ISO_SERIALIZABLE;
            break;
        default:
            dbc_struct_i(result)->iso = txn ? db_txn_struct_i(txn)->iso : TOKU_ISO_SERIALIZABLE;
            break;
    }
    BOOL is_snapshot_read = FALSE;
    if (txn) {
        is_snapshot_read = (dbc_struct_i(result)->iso == TOKU_ISO_READ_COMMITTED || 
                            dbc_struct_i(result)->iso == TOKU_ISO_SNAPSHOT);
    }
    r = toku_brt_cursor(
        db->i->brt, 
        &dbc_struct_i(result)->c,
        txn ? db_txn_struct_i(txn)->tokutxn : NULL,
        is_snapshot_read
        );
    assert(r == 0 || r == TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    if (r == 0) {
        *c = result;
    }
    else {
#if !TOKUDB_NATIVE_H
        toku_free(result->i); // otherwise it is allocated as part of result->ii
#endif
        toku_free(result);
    }
    return r;
}

static inline int 
db_thread_need_flags(DBT *dbt) {
    return (dbt->flags & (DB_DBT_MALLOC+DB_DBT_REALLOC+DB_DBT_USERMEM)) == 0;
}

static int 
toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;
    u_int32_t iso_flags = flags & DB_ISOLATION_FLAGS;

    if ((db->i->open_flags & DB_THREAD) && db_thread_need_flags(data))
        return EINVAL;

    u_int32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);
    flags &= ~lock_flags;
    flags &= ~DB_ISOLATION_FLAGS;
    // And DB_GET_BOTH is no longer supported. #2862.
    if (flags != 0) return EINVAL;


    DBC *dbc;
    r = toku_db_cursor(db, txn, &dbc, iso_flags, 1);
    if (r!=0) return r;
    u_int32_t c_get_flags = DB_SET;
    r = toku_c_get(dbc, key, data, c_get_flags | lock_flags);
    int r2 = toku_c_close(dbc);
    return r ? r : r2;
}

#if 0
static int 
toku_db_key_range(DB * db, DB_TXN * txn, DBT * dbt, DB_KEY_RANGE * kr, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    txn=txn; dbt=dbt; kr=kr; flags=flags;
    toku_ydb_barf();
    abort();
}
#endif

static int 
toku_db_lt_panic(DB* db, int r) {
    assert(r!=0);
    assert(db && db->i && db->dbenv && db->dbenv->i);
    DB_ENV* env = db->dbenv;
    env->i->is_panicked = r;

    if (r < 0) env->i->panic_string = toku_strdup(toku_lt_strerror((TOKU_LT_ERROR)r));
    else       env->i->panic_string = toku_strdup("Error in locktree.\n");

    return toku_ydb_do_error(env, r, "%s", env->i->panic_string);
}

static int 
toku_txn_add_lt(DB_TXN* txn, toku_lock_tree* lt) {
    int r = ENOSYS;
    assert(txn && lt);
    toku_lth* lth = db_txn_struct_i(txn)->lth;
    assert(lth);

    toku_lock_tree* find = toku_lth_find(lth, lt);
    if (find) {
        assert(find == lt);
        r = 0;
        goto cleanup;
    }
    r = toku_lth_insert(lth, lt);
    if (r != 0) { goto cleanup; }
    
    toku_lt_add_ref(lt);
    r = 0;
cleanup:
    return r;
}

static toku_dbt_cmp 
toku_db_get_compare_fun(DB* db) {
    return db->i->brt->compare_fun;
}

/***** TODO 2216 delete this 
static int 
toku_db_fd(DB *db, int *fdp) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    return toku_brt_get_fd(db->i->brt, fdp);
}
*******/

static int
db_open_subdb(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    int r;
    if (!fname || !dbname) r = EINVAL;
    else {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
        assert(bytes==(int)sizeof(subdb_full_name)-1);
        const char *null_subdbname = NULL;
        r = toku_db_open(db, txn, subdb_full_name, null_subdbname, dbtype, flags, mode);
    }
    return r;
}

static void
create_iname_hint(const char *dname, char *hint) {
    //Requires: size of hint array must be > strlen(dname)
    //Copy alphanumeric characters only.
    //Replace strings of non-alphanumeric characters with a single underscore.
    BOOL underscored = FALSE;
    while (*dname) {
        if (isalnum(*dname)) {
            char c = *dname++;
            *hint++ = c;
            underscored = FALSE;
        }
        else {
            if (!underscored)
                *hint++ = '_';
            dname++;
            underscored = TRUE;
        }
    }
    *hint = '\0';
}


// n >= 0 means to include "_L_" with hex value of n in iname
// (intended for use by loader, which will create many inames using one txnid).
static char *
create_iname(DB_ENV *env, u_int64_t id, char *hint, int n) {
    int bytes;
    char inamebase[strlen(hint) +
		   8 +  // hex file format version
		   16 + // hex id (normally the txnid)
		   8  + // hex value of n if non-neg
		   sizeof("_L___.tokudb")]; // extra pieces
    if (n < 0)
	bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%"PRIx64"_%"PRIx32            ".tokudb",
                         hint, id, BRT_LAYOUT_VERSION);
    else
	bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%"PRIx64"_%"PRIx32"_L_%"PRIx32".tokudb",
                         hint, id, BRT_LAYOUT_VERSION, n);
    assert(bytes>0);
    assert(bytes<=(int)sizeof(inamebase)-1);
    char *rval;
    if (env->i->data_dir)
        rval = toku_construct_full_name(2, env->i->data_dir, inamebase);
    else
        rval = toku_construct_full_name(1, inamebase);
    assert(rval);
    return rval;
}


static int db_open_iname(DB * db, DB_TXN * txn, const char *iname, u_int32_t flags, int mode);


// inames are created here.
// algorithm:
//  begin txn
//  convert dname to iname (possibly creating new iname)
//  open file (toku_brt_open() will handle logging)
//  close txn
//  if created a new iname, take full range lock
static int 
toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    if (dbname!=NULL) 
        return db_open_subdb(db, txn, fname, dbname, dbtype, flags, mode);

    // at this point fname is the dname
    //This code ONLY supports single-db files.
    assert(dbname==NULL);
    const char * dname = fname;  // db_open_subdb() converts (fname, dbname) to dname

    ////////////////////////////// do some level of parameter checking.
    u_int32_t unused_flags = flags;
    int using_txns = db->dbenv->i->open_flags & DB_INIT_TXN;
    int r;
    if (dbtype!=DB_BTREE && dbtype!=DB_UNKNOWN) return EINVAL;
    int is_db_excl    = flags & DB_EXCL;    unused_flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  unused_flags&=~DB_CREATE;

    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
                                            unused_flags&=~DB_READ_UNCOMMITTED;
                                            unused_flags&=~DB_READ_COMMITTED;
                                            unused_flags&=~DB_SERIALIZABLE;
    if (unused_flags & ~DB_THREAD) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;
    if (dbtype==DB_UNKNOWN && is_db_excl) return EINVAL;

    /* tokudb supports no duplicates and sorted duplicates only */
    unsigned int tflags;
    r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r != 0) 
        return r;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    //////////////////////////////

    DB_TXN *child = NULL;
    // begin child (unless transactionless)
    if (using_txns) {
        r = toku_txn_begin(db->dbenv, txn, &child, DB_TXN_NOSYNC, 1);
        assert(r==0);
    }

    // convert dname to iname
    //  - look up dname, get iname
    //  - if dname does not exist, create iname and make entry in directory
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds iname_in_env
    toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
    init_dbt_realloc(&iname_dbt);  // sets iname_dbt.data = NULL
    r = toku_db_get(db->dbenv->i->directory, child, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = iname_dbt.data;
    if (r==DB_NOTFOUND && !is_db_create)
        r = ENOENT;
    else if (r==0 && is_db_excl) {
        r = EEXIST;
    }
    else if (r==DB_NOTFOUND) {
        char hint[strlen(dname) + 1];

        // create iname and make entry in directory
        u_int64_t id = 0;

        if (using_txns) {
            id = toku_txn_get_txnid(db_txn_struct_i(child)->tokutxn);
        }
        create_iname_hint(dname, hint);
        iname = create_iname(db->dbenv, id, hint, -1);  // allocated memory for iname
        toku_fill_dbt(&iname_dbt, iname, strlen(iname) + 1);
        r = toku_db_put(db->dbenv->i->directory, child, &dname_dbt, &iname_dbt, DB_YESOVERWRITE);  // DB_YESOVERWRITE for performance only, avoid unnecessary query
    }

    // we now have an iname
    if (r == 0) {
        r = db_open_iname(db, child, iname, flags, mode);
        if (r==0) {
            db->i->dname = toku_xstrdup(dname);
            env_note_db_opened(db->dbenv, db);  // tell env that a new db handle is open (using dname)
        }
    }

    // free string holding iname
    if (iname) toku_free(iname);

    if (using_txns) {
        // close txn
        if (r == 0) {  // commit
            r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL);
            assert(r==0);  // TODO panic
        }
        else {         // abort
            int r2 = toku_txn_abort(child, NULL, NULL);
            assert(r2==0);  // TODO panic
        }
    }

    return r;
}

static int 
db_open_iname(DB * db, DB_TXN * txn, const char *iname_in_env, u_int32_t flags, int mode) {
    int r;

    //Set comparison functions if not yet set.
    if (!db->i->key_compare_was_set && db->dbenv->i->bt_compare) {
        r = toku_brt_set_bt_compare(db->i->brt, db->dbenv->i->bt_compare);
        assert(r==0);
        db->i->key_compare_was_set = TRUE;
    }
    BOOL need_locktree = (BOOL)((db->dbenv->i->open_flags & DB_INIT_LOCK) &&
                                (db->dbenv->i->open_flags & DB_INIT_TXN));

    int is_db_excl    = flags & DB_EXCL;    flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  flags&=~DB_CREATE;
    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
                                            flags&=~DB_READ_UNCOMMITTED;
                                            flags&=~DB_READ_COMMITTED;
                                            flags&=~DB_SERIALIZABLE;
    if (flags & ~DB_THREAD) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;

    /* tokudb supports no duplicates and sorted duplicates only */
    unsigned int tflags;
    r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r != 0) 
        return r;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    
    db->i->open_flags = flags;
    db->i->open_mode = mode;

    r = toku_brt_open(db->i->brt, iname_in_env,
		      is_db_create, is_db_excl,
		      db->dbenv->i->cachetable,
		      txn ? db_txn_struct_i(txn)->tokutxn : NULL_TXN,
		      db);
    if (r != 0)
        goto error_cleanup;

    db->i->opened = 1;
    if (need_locktree) {
	db->i->dict_id = toku_brt_get_dictionary_id(db->i->brt);
        r = toku_ltm_get_lt(db->dbenv->i->ltm, &db->i->lt, db->i->dict_id, db);
        if (r!=0) { goto error_cleanup; }
    }
    //Add to transaction's list of 'must close' if necessary.
    if (txn) {
        //Do last so we don't have to undo.
        toku_list_push(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort,
                  &db->i->dbs_that_must_close_before_abort);
    }

    return 0;
 
error_cleanup:
    db->i->dict_id = DICTIONARY_ID_NONE;
    db->i->opened = 0;
    if (db->i->lt) {
        toku_lt_remove_db_ref(db->i->lt, db);
        db->i->lt = NULL;
    }
    return r;
}

//Return 0 if proposed pair do not violate size constraints of DB
//(insertion is legal)
//Return non zero otherwise.
static int
db_put_check_size_constraints(DB *db, DBT *key, DBT *val) {
    int r;

    //Check limits on size of key and val.
    unsigned int nodesize;
    r = toku_brt_get_nodesize(db->i->brt, &nodesize); assert(r == 0);

    u_int32_t limit = nodesize / BRT_FANOUT;
    if (key->size > limit)
	r = toku_ydb_do_error(db->dbenv, EINVAL, "The largest key allowed is %u bytes", limit);
    else if (val->size > nodesize)
	r = toku_ydb_do_error(db->dbenv, EINVAL, "The largest value allowed is %u bytes", nodesize);
    
    return r;
}

//Return 0 if supported.
//Return ERANGE if out of range.
static int
db_row_size_supported(DB *db, u_int32_t size) {
    DBT key, val;

    toku_fill_dbt(&key, NULL, size);
    toku_fill_dbt(&val, NULL, 0);
    int r = db_put_check_size_constraints(db, &key, &val);
    if (r!=0) r = ERANGE;
    return r;
}

static int
locked_db_row_size_supported(DB *db, u_int32_t size) {
    toku_ydb_lock();
    int r = db_row_size_supported(db, size);
    toku_ydb_unlock();
    return r;
}

//Return 0 if insert is legal
static int
db_put_check_overwrite_constraint(DB *db, DB_TXN *txn, DBT *key,
                                  u_int32_t lock_flags, u_int32_t overwrite_flag) {
    int r;

    //DB_YESOVERWRITE does not impose constraints.
    if (overwrite_flag==DB_YESOVERWRITE) r = 0;
    else if (overwrite_flag==DB_NOOVERWRITE) {
        //Check if (key,anything) exists in dictionary.
        //If exists, fail.  Otherwise, do insert.
        r = db_getf_set(db, txn, lock_flags|DB_SERIALIZABLE, key, ydb_getf_do_nothing, NULL);
        if (r==DB_NOTFOUND) r = 0;
        else if (r==0)      r = DB_KEYEXIST;
        //Any other error is passed through.
    }
    else if (overwrite_flag==0) {
        //in a nodup db:   overwrite_flag==0 is an alias for DB_YESOVERWRITE
        //in a dupsort db: overwrite_flag==0 is an error
	r = 0;
    }
    else if (overwrite_flag==DB_NOOVERWRITE_NO_ERROR) {
        r = 0;
    }
    else {
        //Other flags are not (yet) supported.
        r = EINVAL;
    }
    return r;
}

static int
toku_db_put(DB *db, DB_TXN *txn, DBT *key, DBT *val, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;

    u_int32_t lock_flags = get_prelocked_flags(flags);
    flags &= ~lock_flags;

    r = toku_grab_read_lock_on_directory(db, txn);

    if (r == 0)
        r = db_put_check_size_constraints(db, key, val);
    if (r == 0) {
        //Do any checking required by the flags.
        r = db_put_check_overwrite_constraint(db, txn, key, lock_flags, flags);
    }
    BOOL do_locking = (BOOL)(db->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));
    if (r == 0 && do_locking) {
        //Do locking if necessary.
        r = get_point_lock(db, txn, key);
    }
    if (r == 0) {
        //Insert into the brt.
        TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
        enum brt_msg_type type = BRT_INSERT;
        if (flags==DB_NOOVERWRITE_NO_ERROR)
            type = BRT_INSERT_NO_OVERWRITE;
        r = toku_brt_maybe_insert(db->i->brt, key, val, ttxn, FALSE, ZERO_LSN, TRUE, type);
    }

    if (r == 0)
	num_inserts++;
    else
        num_inserts_fail++;

    return r;
}

static int
log_put_single(DB_TXN *txn, BRT brt, const DBT *key, const DBT *val) {
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    int r = toku_brt_log_put(ttxn, brt, key, val);
    return r;
}

static int
log_put_multiple(DB_TXN *txn, DB *src_db, const DBT *src_key, const DBT *src_val, uint32_t num_dbs, BRT brts[]) {
    int r = 0;
    if (num_dbs > 0) {
        TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
        BRT src_brt  = src_db ? src_db->i->brt : NULL;
        r = toku_brt_log_put_multiple(ttxn, src_brt, brts, num_dbs, src_key, src_val);
    }
    return r;
}

static int
do_put_multiple(DB_TXN *txn, uint32_t num_dbs, DB *db_array[], DBT keys[], DBT vals[]) {
    int r = 0;
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    for (uint32_t which_db = 0; r == 0 && which_db < num_dbs; which_db++) {
        DB *db = db_array[which_db];
	int do_put = TRUE;
	DB_INDEXER *indexer = toku_db_get_indexer(db);
	if (indexer) { // if this db is the index under construction
            DB *src_db = toku_indexer_get_src_db(indexer);
            invariant(src_db != NULL);
            uint32_t which_src_db = lookup_src_db(num_dbs, db_array, src_db);
            if (which_src_db >= num_dbs)
                r = EINVAL;
            else
                do_put = !toku_indexer_is_key_right_of_le_cursor(indexer, src_db, &keys[which_src_db]);
        }
        if (r == 0 && do_put) {
            r = toku_brt_maybe_insert(db->i->brt, &keys[which_db], &vals[which_db], ttxn, FALSE, ZERO_LSN, FALSE, BRT_INSERT);
        }
    }
    return r;
}

static int
env_put_multiple(
    DB_ENV *env, 
    DB *src_db, 
    DB_TXN *txn, 
    const DBT *key, 
    const DBT *val, 
    uint32_t num_dbs, 
    DB **db_array, 
    DBT *keys, 
    DBT *vals, 
    uint32_t *flags_array) 
{
    int r;
    DBT put_keys[num_dbs];
    DBT put_vals[num_dbs];
    BOOL multi_accounting = TRUE;  // use num_multi_insert accountability counters 

    // special case for a single DB
    if (num_dbs == 1 && src_db == db_array[0]) {
	multi_accounting = FALSE;
        r = toku_db_put(src_db, txn, (DBT *) key, (DBT *) val, flags_array[0]);
        goto cleanup;
    }

    HANDLE_PANICKED_ENV(env);

    {
    uint32_t lock_flags[num_dbs];
    uint32_t remaining_flags[num_dbs];
    BRT brts[num_dbs];

    if (!txn || !num_dbs) {
        r = EINVAL;
        goto cleanup;
    }
    if (!env->i->generate_row_for_put) {
        r = EINVAL;
        goto cleanup;
    }

    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn);

    for (uint32_t which_db = 0; which_db < num_dbs; which_db++) {
        DB *db = db_array[which_db];

        //Generate the row
        if (db == src_db) {
            put_keys[which_db] = *key;
            put_vals[which_db] = *val;
        }
        else {
            r = env->i->generate_row_for_put(db, src_db, &keys[which_db], &vals[which_db], key, val);
            if (r != 0) goto cleanup;
            put_keys[which_db] = keys[which_db];
            put_vals[which_db] = vals[which_db];            
        }

        lock_flags[which_db] = get_prelocked_flags(flags_array[which_db]);
        remaining_flags[which_db] = flags_array[which_db] & ~lock_flags[which_db];

        // check size constraints
        r = db_put_check_size_constraints(db, &put_keys[which_db], &put_vals[which_db]);
        if (r != 0) goto cleanup;

        //Check overwrite constraints
        r = db_put_check_overwrite_constraint(db, txn,
                                              &put_keys[which_db],
                                              lock_flags[which_db], remaining_flags[which_db]);
        if (r != 0) goto cleanup;
        if (remaining_flags[which_db] == DB_NOOVERWRITE_NO_ERROR) {
            //put_multiple does not support delaying the no error, since we would
            //have to log the flag in the put_multiple.
            r = EINVAL; goto cleanup;
        }

        //Do locking if necessary.
        r = toku_grab_read_lock_on_directory(db, txn);
        if (r != 0) goto cleanup;

        if (db->i->lt && !(lock_flags[which_db] & DB_PRELOCKED_WRITE)) {
            //Needs locking
            r = get_point_lock(db, txn, &put_keys[which_db]);
            if (r != 0) goto cleanup;
        }
        brts[which_db] = db->i->brt;
    }

    if (num_dbs == 1)
        r = log_put_single(txn, brts[0], &put_keys[0], &put_vals[0]);
    else
        r = log_put_multiple(txn, src_db, key, val, num_dbs, brts);
    
    if (r == 0)
        r = do_put_multiple(txn, num_dbs, db_array, put_keys, put_vals);

    }

cleanup:
    if (multi_accounting) {
	if (r == 0)
	    num_multi_inserts += num_dbs;
	else
	    num_multi_inserts_fail += num_dbs;
    }
    return r;
}

static int
dbt_cmp(const DBT *a, const DBT *b) {
    if (a->size < b->size)
        return -1;
    if (a->size > b->size)
        return +1;
    return memcmp(a->data, b->data, a->size);
}

static int
update_single(
    DB_ENV *env,
    DB *db, 
    uint32_t flags, 
    DB_TXN *txn, 
    DBT *old_key, 
    DBT *old_data, 
    DBT *new_key, 
    DBT *new_data) 
{
    int r = 0;
    uint32_t lock_flags;
    uint32_t remaining_flags;
    lock_flags = get_prelocked_flags(flags);
    remaining_flags = flags & ~lock_flags;

    r = toku_grab_read_lock_on_directory(db, txn);
    if (r != 0) goto cleanup;

    int (*cmpfun)(DB *db, const DBT *a, const DBT *b) = toku_builtin_compare_fun;
    if (env->i->bt_compare)
        cmpfun = env->i->bt_compare;

    BOOL key_eq = cmpfun(db, old_key, new_key) == 0;
    if (!key_eq) {
        //Check overwrite constraints only in the case where 
        // the keys are not equal.
        // If the keys are equal, then we do not care of the flag is DB_NOOVERWRITE or DB_YESOVERWRITE
        r = db_put_check_overwrite_constraint(db, txn,
                                              new_key,
                                              lock_flags, remaining_flags);
        if (r != 0) goto cleanup;

        r = toku_db_del(db, txn, (DBT *) old_key, DB_DELETE_ANY);
    }
    
    if (r == 0 && (!key_eq || !(dbt_cmp(old_data, new_data) == 0))) {
        r = toku_db_put(db, txn, (DBT *) new_key, (DBT *) new_data, DB_YESOVERWRITE);
    }
cleanup:
    if (r == 0)
	num_updates++;
    else
	num_updates_fail++;
    return r;
}

static int
env_update_multiple(DB_ENV *env, DB *src_db, DB_TXN *txn,                                
                    DBT *old_src_key, DBT *old_src_data,
                    DBT *new_src_key, DBT *new_src_data,
                    uint32_t num_dbs, DB **db_array, uint32_t* flags_array, 
                    uint32_t num_keys, DBT keys[], 
                    uint32_t num_vals, DBT vals[]) {
    int r = 0;
    BOOL multi_accounting = TRUE;  // use num_multi_update accountability counters 

    // special case for a single DB 
    if (num_dbs == 1 && src_db == db_array[0]) {
	multi_accounting = FALSE;	
        r = update_single(env,
            db_array[0], 
            flags_array[0], 
            txn, 
            old_src_key, 
            old_src_data, 
            new_src_key, 
            new_src_data
            );
        goto cleanup;
    }

    HANDLE_PANICKED_ENV(env);

    if (!txn) {
        r = EINVAL;
        goto cleanup;
    }
    if (!env->i->generate_row_for_put) {
        r = EINVAL;
        goto cleanup;
    }

    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn);

    {
        uint32_t n_del_dbs = 0;
        DB *del_dbs[num_dbs];
        BRT del_brts[num_dbs];
        DBT del_keys[num_dbs];
        
        uint32_t n_put_dbs = 0;
        DB *put_dbs[num_dbs];
        BRT put_brts[num_dbs];
        DBT put_keys[num_dbs];
        DBT put_vals[num_dbs];

        uint32_t lock_flags[num_dbs];
        uint32_t remaining_flags[num_dbs];

        int (*cmpfun)(DB *db, const DBT *a, const DBT *b) = toku_builtin_compare_fun;
        if (env->i->bt_compare)
            cmpfun = env->i->bt_compare;

        for (uint32_t which_db = 0; which_db < num_dbs; which_db++) {
            DB *db = db_array[which_db];
            DBT curr_old_key, curr_new_key, curr_new_val;
            

            lock_flags[which_db] = get_prelocked_flags(flags_array[which_db]);
            remaining_flags[which_db] = flags_array[which_db] & ~lock_flags[which_db];

            // keys[0..num_dbs-1] are the new keys
            // keys[num_dbs..2*num_dbs-1] are the old keys
            // vals[0..num_dbs-1] are the new vals

            // Generate the old key and val
            if (which_db + num_dbs >= num_keys) {
                r = ENOMEM; goto cleanup;
            }
            if (db == src_db) {
                curr_old_key = *old_src_key;
            }
            else {
                r = env->i->generate_row_for_put(db, src_db, &keys[which_db + num_dbs], NULL, old_src_key, old_src_data);
                if (r != 0) goto cleanup;
                curr_old_key = keys[which_db + num_dbs];
            }
            // Generate the new key and val
            if (which_db >= num_keys || which_db >= num_vals) {
                r = ENOMEM; goto cleanup;
            }
            if (db == src_db) {
                curr_new_key = *new_src_key;
                curr_new_val = *new_src_data;
            }
            else {
                r = env->i->generate_row_for_put(db, src_db, &keys[which_db], &vals[which_db], new_src_key, new_src_data);
                if (r != 0) goto cleanup;
                curr_new_key = keys[which_db];
                curr_new_val = vals[which_db];
            }
            BOOL key_eq = cmpfun(db, &curr_old_key, &curr_new_key) == 0;
            if (!key_eq) {
                r = toku_grab_read_lock_on_directory(db, txn);
                if (r != 0) goto cleanup;

                //Check overwrite constraints only in the case where 
                // the keys are not equal.
                // If the keys are equal, then we do not care of the flag is DB_NOOVERWRITE or DB_YESOVERWRITE
                r = db_put_check_overwrite_constraint(db, txn,
                                                      &curr_new_key,
                                                      lock_flags[which_db], remaining_flags[which_db]);
                if (r != 0) goto cleanup;
                if (remaining_flags[which_db] == DB_NOOVERWRITE_NO_ERROR) {
                    //update_multiple does not support delaying the no error, since we would
                    //have to log the flag in the put_multiple.
                    r = EINVAL; goto cleanup;
                }

                // lock old key
                if (db->i->lt && !(lock_flags[which_db] & DB_PRELOCKED_WRITE)) {
                    r = get_point_lock(db, txn, &curr_old_key);
                    if (r != 0) goto cleanup;
                }
                del_dbs[n_del_dbs] = db;
                del_brts[n_del_dbs] = db->i->brt;
                del_keys[n_del_dbs] = curr_old_key;
                n_del_dbs++;
                
            }

            // we take a shortcut and avoid generating the old val
            // we assume that any new vals with size > 0 are different than the old val
            // if (!key_eq || !(dbt_cmp(&vals[which_db], &vals[which_db + num_dbs]) == 0)) {
            if (!key_eq || curr_new_val.size > 0) {
                r = db_put_check_size_constraints(db, &curr_new_key, &curr_new_val);
                if (r != 0) goto cleanup;

                r = toku_grab_read_lock_on_directory(db, txn);
                if (r != 0) goto cleanup;

                // lock new key
                if (db->i->lt) {
                    r = get_point_lock(db, txn, &curr_new_key);
                    if (r != 0) goto cleanup;
                }
                put_dbs[n_put_dbs] = db;
                put_brts[n_put_dbs] = db->i->brt;
                put_keys[n_put_dbs] = curr_new_key;
                put_vals[n_put_dbs] = curr_new_val;
                n_put_dbs++;
            }
        }

        if (r == 0 && n_del_dbs > 0) {
            if (n_del_dbs == 1)
                r = log_del_single(txn, del_brts[0], &del_keys[0]);
            else
                r = log_del_multiple(txn, src_db, old_src_key, old_src_data, n_del_dbs, del_brts, del_keys);
            if (r == 0) 
                r = do_del_multiple(txn, n_del_dbs, del_dbs, del_keys);
        }

        if (r == 0 && n_put_dbs > 0) {
            if (n_put_dbs == 1)
                r = log_put_single(txn, put_brts[0], &put_keys[0], &put_vals[0]);
            else
                r = log_put_multiple(txn, src_db, new_src_key, new_src_data, n_put_dbs, put_brts);
            if (r == 0)
                r = do_put_multiple(txn, n_put_dbs, put_dbs, put_keys, put_vals);
        }
    }

cleanup:
    if (multi_accounting) {
	if (r == 0)
	    num_multi_updates += num_dbs;
	else
	    num_multi_updates_fail += num_dbs;
    }
    return r;
}

static int toku_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags);

//We do not (yet?) support deleting subdbs by deleting the enclosing 'fname'
static int
env_dbremove_subdb(DB_ENV * env, DB_TXN * txn, const char *fname, const char *dbname, int32_t flags) {
    int r;
    if (!fname || !dbname) r = EINVAL;
    else {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
        assert(bytes==(int)sizeof(subdb_full_name)-1);
        const char *null_subdbname = NULL;
        r = toku_env_dbremove(env, txn, subdb_full_name, null_subdbname, flags);
    }
    return r;
}


//Called during committing an fdelete ONLY IF you still have an fd AND it is not connected to /dev/null
//Called during aborting an fcreate (harmless to do, and definitely correct)
static void
finalize_file_removal(DICTIONARY_ID dict_id, void * extra) {
    toku_ltm *ltm = (toku_ltm*) extra;
    if (ltm) {
        //Poison the lock tree to prevent a future file from re-using it.
        toku_ltm_invalidate_lt(ltm, dict_id);
    }
}

//static int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn);

static int
toku_env_dbremove(DB_ENV * env, DB_TXN *txn, const char *fname, const char *dbname, u_int32_t flags) {
    int r;
    HANDLE_PANICKED_ENV(env);
    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn);
    if (!env_opened(env)) return EINVAL;
    if (dbname!=NULL) 
        return env_dbremove_subdb(env, txn, fname, dbname, flags);
    // env_dbremove_subdb() converts (fname, dbname) to dname

    const char * dname = fname;
    assert(dbname == NULL);

    if (flags!=0) return EINVAL;
    if (env_is_db_with_dname_open(env, dname))
        return toku_ydb_do_error(env, EINVAL, "Cannot remove dictionary with an open handle.\n");
    
    DBT dname_dbt;  
    DBT iname_dbt;  
    toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
    init_dbt_realloc(&iname_dbt);  // sets iname_dbt.data = NULL

    int using_txns = env->i->open_flags & DB_INIT_TXN;
    DB_TXN *child = NULL;
    // begin child (unless transactionless)
    if (using_txns) {
	r = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1);
	assert(r==0);
    }

    // get iname
    r = toku_db_get(env->i->directory, child, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = iname_dbt.data;
    if (r==DB_NOTFOUND)
        r = ENOENT;
    else if (r==0) {
	// remove (dname,iname) from directory
	r = toku_db_del(env->i->directory, child, &dname_dbt, DB_DELETE_ANY);
	if (r == 0) {
            if (using_txns) {
                r = toku_brt_remove_on_commit(db_txn_struct_i(child)->tokutxn, &iname_dbt);
		assert(r==0);
                //Now that we have a writelock on dname, verify that there are still no handles open. (to prevent race conditions)
                if (r==0 && env_is_db_with_dname_open(env, dname))
                    r = toku_ydb_do_error(env, EINVAL, "Cannot remove dictionary with an open handle.\n");
                if (r==0) {
                    DB* zombie = env_get_zombie_db_with_dname(env, dname);
                    if (zombie)
                        r = toku_db_pre_acquire_table_lock(zombie, child, TRUE);
                    if (r!=0 && r!=DB_LOCK_NOTGRANTED)
                        toku_ydb_do_error(env, r, "Cannot remove dictionary.\n");
                }
            }
            else {
                r = toku_brt_remove_now(env->i->cachetable, &iname_dbt);
		assert(r==0);
            }
	}
    }

    if (using_txns) {
	// close txn
	if (r == 0) {  // commit
	    r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL);
	    assert(r==0);  // TODO panic
	}
	else {         // abort
	    int r2 = toku_txn_abort(child, NULL, NULL);
	    assert(r2==0);  // TODO panic
	}
    }

    if (iname) toku_free(iname);
    return r;

}


static int
toku_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    DB_TXN *null_txn = NULL;
    int r  = toku_env_dbremove(db->dbenv, null_txn, fname, dbname, flags);
    int r2 = toku_db_close(db, 0);
    if (r==0) r = r2;
    return r;
}

static int
env_dbrename_subdb(DB_ENV *env, DB_TXN *txn, const char *fname, const char *dbname, const char *newname, u_int32_t flags) {
    int r;
    if (!fname || !dbname || !newname) r = EINVAL;
    else {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        {
            int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
            assert(bytes==(int)sizeof(subdb_full_name)-1);
        }
        char new_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        {
            int bytes = snprintf(new_full_name, sizeof(new_full_name), "%s/%s", fname, dbname);
            assert(bytes==(int)sizeof(new_full_name)-1);
        }
        const char *null_subdbname = NULL;
        r = toku_env_dbrename(env, txn, subdb_full_name, null_subdbname, new_full_name, flags);
    }
    return r;
}


static int
toku_env_dbrename(DB_ENV *env, DB_TXN *txn, const char *fname, const char *dbname, const char *newname, u_int32_t flags) {
    int r;
    HANDLE_PANICKED_ENV(env);
    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, txn);
    if (!env_opened(env)) return EINVAL;
    if (dbname!=NULL) 
        return env_dbrename_subdb(env, txn, fname, dbname, newname, flags);
    // env_dbrename_subdb() converts (fname, dbname) to dname and (fname, newname) to newdname

    const char * dname = fname;
    assert(dbname == NULL);

    if (flags!=0) return EINVAL;
    if (env_is_db_with_dname_open(env, dname))
        return toku_ydb_do_error(env, EINVAL, "Cannot rename dictionary with an open handle.\n");
    if (env_is_db_with_dname_open(env, newname))
        return toku_ydb_do_error(env, EINVAL, "Cannot rename dictionary; Dictionary with target name has an open handle.\n");
    
    DBT old_dname_dbt;  
    DBT new_dname_dbt;  
    DBT iname_dbt;  
    toku_fill_dbt(&old_dname_dbt, dname, strlen(dname)+1);
    toku_fill_dbt(&new_dname_dbt, newname, strlen(newname)+1);
    init_dbt_realloc(&iname_dbt);  // sets iname_dbt.data = NULL

    int using_txns = env->i->open_flags & DB_INIT_TXN;
    DB_TXN *child = NULL;
    // begin child (unless transactionless)
    if (using_txns) {
	r = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1);
	assert(r==0);
    }

    r = toku_db_get(env->i->directory, child, &old_dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = iname_dbt.data;
    if (r==DB_NOTFOUND)
        r = ENOENT;
    else if (r==0) {
	// verify that newname does not already exist
	r = db_getf_set(env->i->directory, child, DB_SERIALIZABLE, &new_dname_dbt, ydb_getf_do_nothing, NULL);
	if (r == 0) 
	    r = EEXIST;
	else if (r == DB_NOTFOUND) {
	    // remove old (dname,iname) and insert (newname,iname) in directory
	    r = toku_db_del(env->i->directory, child, &old_dname_dbt, DB_DELETE_ANY);
	    if (r == 0)
		r = toku_db_put(env->i->directory, child, &new_dname_dbt, &iname_dbt, DB_YESOVERWRITE);
            //Now that we have writelocks on both dnames, verify that there are still no handles open. (to prevent race conditions)
            if (r==0 && env_is_db_with_dname_open(env, dname))
                r = toku_ydb_do_error(env, EINVAL, "Cannot rename dictionary with an open handle.\n");
            DB* zombie = NULL;
            if (r==0) {
                zombie = env_get_zombie_db_with_dname(env, dname);
                if (zombie)
                    r = toku_db_pre_acquire_table_lock(zombie, child, TRUE);
                if (r!=0 && r!=DB_LOCK_NOTGRANTED)
                    toku_ydb_do_error(env, r, "Cannot rename dictionary.\n");
            }
            if (r==0 && env_is_db_with_dname_open(env, newname))
                r = toku_ydb_do_error(env, EINVAL, "Cannot rename dictionary; Dictionary with target name has an open handle.\n");
            if (r==0 && zombie) {
                //Update zombie in list if exists.
                env_note_zombie_db_closed(env, zombie);  // tell env that this db is no longer a zombie (it is completely closed)
                toku_free(zombie->i->dname);
                zombie->i->dname = toku_xstrdup(newname);
                env_note_zombie_db(env, zombie);  // tell env that this db is a zombie
            }
	}
    }

    if (using_txns) {
	// close txn
	if (r == 0) {  // commit
	    r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL);
	    assert(r==0);  // TODO panic
	}
	else {         // abort
	    int r2 = toku_txn_abort(child, NULL, NULL);
	    assert(r2==0);  // TODO panic
	}
    }

    if (iname) toku_free(iname);
    return r;

}

static int
toku_db_rename(DB * db, const char *fname, const char *dbname, const char *newname, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    DB_TXN *null_txn = NULL;
    int r  = toku_env_dbrename(db->dbenv, null_txn, fname, dbname, newname, flags);
    int r2 = toku_db_close(db, 0);
    if (r==0) r = r2;
    return r;
}

static int 
toku_db_set_descriptor(DB *db, u_int32_t version, const DBT* descriptor) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (db_opened(db)) return EINVAL;
    else if (!descriptor) r = EINVAL;
    else if (descriptor->size>0 && !descriptor->data) r = EINVAL;
    else r = toku_brt_set_descriptor(db->i->brt, version, descriptor);
    return r;
}

static int 
toku_db_set_flags(DB *db, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);

    /* the following matches BDB */
    if (db_opened(db) && flags != 0) return EINVAL;

    return 0;
}

static int 
toku_db_get_flags(DB *db, u_int32_t *pflags) {
    HANDLE_PANICKED_DB(db);
    if (!pflags) return EINVAL;
    *pflags = 0;
    return 0;
}

static int 
toku_db_set_pagesize(DB *db, u_int32_t pagesize) {
    HANDLE_PANICKED_DB(db);
    int r = toku_brt_set_nodesize(db->i->brt, pagesize);
    return r;
}

static int 
toku_db_stat64(DB * db, DB_TXN *txn, DB_BTREE_STAT64 *s) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    struct brtstat64_s brtstat;
    int r = toku_brt_stat64(db->i->brt, db_txn_struct_i(txn)->tokutxn, &brtstat);
    if (r==0) {
	s->bt_nkeys = brtstat.nkeys;
	s->bt_ndata = brtstat.ndata;
	s->bt_dsize = brtstat.dsize;
	s->bt_fsize = brtstat.fsize;
    }
    return r;
}

static int 
locked_db_stat64 (DB *db, DB_TXN *txn, DB_BTREE_STAT64 *s) {
    toku_ydb_lock();
    int r = toku_db_stat64(db, txn, s);
    toku_ydb_unlock();
    return r;
}

static int 
toku_db_key_range64(DB* db, DB_TXN* txn __attribute__((__unused__)), DBT* key, u_int64_t* less, u_int64_t* equal, u_int64_t* greater, int* is_exact) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);

    // note that toku_brt_keyrange does not have a txn param
    // this will be fixed later
    // temporarily, because the caller, locked_db_keyrange, 
    // has the ydb lock, we are ok
    int r = toku_brt_keyrange(db->i->brt, key, less, equal, greater);
    if (r != 0) { goto cleanup; }
    // temporarily set is_exact to 0 because brt_keyrange does not have this parameter
    *is_exact = 0;
cleanup:
    return r;
}

static int 
toku_c_pre_acquire_read_lock(DBC *dbc, const DBT *key_left, const DBT *key_right) {
    DB* db = dbc->dbp;
    DB_TXN* txn = dbc_struct_i(dbc)->txn;
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return EINVAL;
    //READ_UNCOMMITTED and READ_COMMITTED transactions do not need read locks.
    if (dbc_struct_i(dbc)->iso != TOKU_ISO_SERIALIZABLE) {
         return 0;
    }

    int r;
    {
	RANGE_LOCK_REQUEST_S request;
	read_lock_request_init(&request, txn, db,
			       key_left,
			       key_right);
        r = grab_range_lock(&request);
    }
    return r;
}

static int toku_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn) {
    RANGE_LOCK_REQUEST_S request;
    char *   dname = db->i->dname;
    DBT key_in_directory;
    //
    // bad hack because some environment dictionaries do not have a dname
    //
    if (!dname) {
        return 0;
    }
    toku_fill_dbt(&key_in_directory, dname, strlen(dname)+1);
    //Left end of range == right end of range (point lock)
    write_lock_request_init(
        &request, 
        txn, 
        db->dbenv->i->directory,
        &key_in_directory,
        &key_in_directory
        );
    int r = grab_range_lock(&request);
    return r;
}

//static int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
// needed by loader.c
int 
toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn, BOOL just_lock) {
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return EINVAL;

    int r;

    {
        RANGE_LOCK_REQUEST_S request;
        write_lock_request_init(
            &request, 
            txn, 
            db,
            toku_lt_neg_infinity,
            toku_lt_infinity
            );
        r = grab_range_lock(&request);
    }

    if (r==0 && !just_lock &&
        !toku_brt_is_recovery_logging_suppressed(db->i->brt) &&
        toku_brt_is_empty_fast(db->i->brt)
    ) {
        //Try to suppress both rollback and recovery logs
        DB_LOADER *loader;
        DB *dbs[1] = {db};
        uint32_t db_flags[1]  = {DB_NOOVERWRITE};
        uint32_t dbt_flags[1] = {0};
        uint32_t loader_flags = DB_PRELOCKED_WRITE; //Don't recursively prelock
        DB_ENV *env = db->dbenv;
	DB_TXN *child = NULL;
	
	{
	    // begin child
	    int rt = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1);
	    assert(rt==0);
	}

        toku_ydb_unlock(); //Cannot hold ydb lock when creating loader
	
        int r_loader = env->create_loader(env, child, &loader, NULL, 1, dbs, db_flags, dbt_flags, loader_flags);
        if (r_loader==0) {
            r_loader = loader->set_error_callback(loader, NULL, NULL);
            assert(r_loader==0);
            r_loader = loader->set_poll_function(loader, NULL, NULL);
            assert(r_loader==0);
            // close the loader
            r_loader = loader->close(loader);
	    if (r_loader==0) {
		toku_brt_suppress_recovery_logs(db->i->brt, db_txn_struct_i(child)->tokutxn);
	    }
        }
        else if (r_loader != DB_LOCK_NOTGRANTED) {
            //Lock not granted is not an error.
            //It just means we cannot use the loader optimization.
            assert(r==0);
            r = r_loader;
        }
	if (r_loader == 0) { // commit
	    r = locked_txn_commit(child, 0);
	    assert(r==0);
	    logsuppress++;
	}
	else {  // abort
	    r = locked_txn_abort(child);
	    assert(r==0);
	    logsuppressfail++;
	}
        toku_ydb_lock(); //Reaquire ydb lock.
    }

    return r;
}

//TODO: DB_AUTO_COMMIT.
//TODO: Nowait only conditionally?
//TODO: NOSYNC change to SYNC if DB_ENV has something in set_flags
static inline int 
toku_db_construct_autotxn(DB* db, DB_TXN **txn, BOOL* changed, BOOL force_auto_commit) {
    assert(db && txn && changed);
    DB_ENV* env = db->dbenv;
    if (*txn || !(env->i->open_flags & DB_INIT_TXN)) {
        *changed = FALSE;
        return 0;
    }
    BOOL nosync = (BOOL)(!force_auto_commit && !(env->i->open_flags & DB_AUTO_COMMIT));
    u_int32_t txn_flags = DB_TXN_NOWAIT | (nosync ? DB_TXN_NOSYNC : 0);
    int r = toku_txn_begin(env, NULL, txn, txn_flags, 1);
    if (r!=0) return r;
    *changed = TRUE;
    return 0;
}

static inline int 
toku_db_destruct_autotxn(DB_TXN *txn, int r, BOOL changed) {
    if (!changed) return r;
    if (r==0) return toku_txn_commit(txn, 0, NULL, NULL);
    toku_txn_abort(txn, NULL, NULL);
    return r; 
}

static int 
locked_db_close(DB * db, u_int32_t flags) {
    toku_ydb_lock(); 
    int r = toku_db_close(db, flags); 
    toku_ydb_unlock(); 
    return r;
}

static inline int 
autotxn_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    if (!txn && (db->dbenv->i->open_flags & DB_INIT_TXN)) {
        return toku_ydb_do_error(db->dbenv, EINVAL,
              "Cursors in a transaction environment must have transactions.\n");
    }
    return toku_db_cursor(db, txn, c, flags, 0);
}

static int 
locked_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_cursor(db, txn, c, flags); toku_ydb_unlock(); return r;
}

static inline int 
autotxn_db_del(DB* db, DB_TXN* txn, DBT* key, u_int32_t flags) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_del(db, txn, key, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_del(DB * db, DB_TXN * txn, DBT * key, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_del(db, txn, key, flags); toku_ydb_unlock(); return r;
}

static inline int 
autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, u_int32_t flags) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_get(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_get(db, txn, key, data, flags); toku_ydb_unlock(); return r;
}

static inline int 
autotxn_db_getf_set (DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = db_getf_set(db, txn, flags, key, f, extra);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_getf_set (DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock(); int r = autotxn_db_getf_set(db, txn, flags, key, f, extra); toku_ydb_unlock(); return r;
}

static int 
locked_c_pre_acquire_read_lock(DBC *dbc, const DBT *key_left, const DBT *key_right) {
    toku_ydb_lock();
    int r = toku_c_pre_acquire_read_lock(dbc, key_left, key_right);
    toku_ydb_unlock();
    return r;
}

static int 
locked_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
    toku_ydb_lock();
    int r = toku_db_pre_acquire_table_lock(db, txn, FALSE);
    toku_ydb_unlock();
    return r;
}

static int locked_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn) {
    toku_ydb_lock();
    int r = toku_db_pre_acquire_fileops_lock(db, txn);
    toku_ydb_unlock();
    return r;
}

// truncate a database
// effect: remove all of the rows from a database
static int 
toku_db_truncate(DB *db, DB_TXN *txn, u_int32_t *row_count, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;

    u_int32_t unhandled_flags = flags;
    int ignore_cursors = 0;
    if (flags & DB_TRUNCATE_WITHCURSORS) {
        ignore_cursors = 1;
        unhandled_flags &= ~DB_TRUNCATE_WITHCURSORS;
    }

    // dont support flags (yet)
    if (unhandled_flags)
        return EINVAL;
    // dont support cursors unless explicitly told to
    if (!ignore_cursors && toku_brt_get_cursor_count(db->i->brt) > 0)
        return EINVAL;

    // acquire a table lock
    if (txn) {
        r = toku_db_pre_acquire_fileops_lock(db, txn);
        if (r != 0) {
            return r;
        }
        r = toku_db_pre_acquire_table_lock(db, txn, TRUE);
        if (r != 0) {
            return r;
        }
    }

    *row_count = 0;

    r = toku_brt_truncate(db->i->brt);

    return r;
}

static inline int 
autotxn_db_open(DB* db, DB_TXN* txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, (BOOL)((flags & DB_AUTO_COMMIT) != 0));
    if (r!=0) return r;
    r = toku_db_open(db, txn, fname, dbname, dbtype, flags & ~DB_AUTO_COMMIT, mode);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_open(DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock(); int r = autotxn_db_open(db, txn, fname, dbname, dbtype, flags, mode); toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

static inline int 
autotxn_db_put(DB* db, DB_TXN* txn, DBT* key, DBT* data, u_int32_t flags) {
    //{ unsigned i; printf("put %p keylen=%d key={", db, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_put(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    int r = env_check_avail_fs_space(db->dbenv);
    if (r == 0) {
	toku_ydb_lock(); 
	r = autotxn_db_put(db, txn, key, data, flags); 
	toku_ydb_unlock(); 
    }
    return r;
}

static int 
locked_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock();
    int r = toku_db_remove(db, fname, dbname, flags);
    toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

static int 
locked_db_rename(DB * db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock();
    int r = toku_db_rename(db, namea, nameb, namec, flags);
    toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

static int 
locked_db_set_descriptor(DB *db, u_int32_t version, const DBT* descriptor) {
    toku_ydb_lock();
    int r = toku_db_set_descriptor(db, version, descriptor);
    toku_ydb_unlock();
    return r;
}

static void 
locked_db_set_errfile (DB *db, FILE *errfile) {
    db->dbenv->set_errfile(db->dbenv, errfile);
}

static int 
locked_db_set_flags(DB *db, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_db_set_flags(db, flags); toku_ydb_unlock(); return r;
}

static int 
locked_db_get_flags(DB *db, u_int32_t *flags) {
    toku_ydb_lock(); int r = toku_db_get_flags(db, flags); toku_ydb_unlock(); return r;
}

static int 
locked_db_set_pagesize(DB *db, u_int32_t pagesize) {
    toku_ydb_lock(); int r = toku_db_set_pagesize(db, pagesize); toku_ydb_unlock(); return r;
}

// TODO 2216 delete this
static int 
locked_db_fd(DB * UU(db), int * UU(fdp)) {
    //    toku_ydb_lock(); 
    // int r = toku_db_fd(db, fdp); 
    //    toku_ydb_unlock(); 
    //    return r;
    return 0;
}


static int locked_db_key_range64(DB* db, DB_TXN* txn, DBT* dbt, u_int64_t* less, u_int64_t* equal, u_int64_t* greater, int* is_exact) {
    toku_ydb_lock(); int r = toku_db_key_range64(db, txn, dbt, less, equal, greater, is_exact); toku_ydb_unlock(); return r;
}

static const DBT* toku_db_dbt_pos_infty(void) __attribute__((pure));
static const DBT*
toku_db_dbt_pos_infty(void) {
    return toku_lt_infinity;
}

static const DBT* toku_db_dbt_neg_infty(void) __attribute__((pure));
static const DBT* 
toku_db_dbt_neg_infty(void) {
    return toku_lt_neg_infinity;
}

static int 
locked_db_truncate(DB *db, DB_TXN *txn, u_int32_t *row_count, u_int32_t flags) {
    toku_checkpoint_safe_client_lock();
    toku_ydb_lock();
    int r = toku_db_truncate(db, txn, row_count, flags);
    toku_ydb_unlock();
    toku_checkpoint_safe_client_unlock();
    return r;
}

static int
toku_db_optimize(DB *db) {
    HANDLE_PANICKED_DB(db);
    int r = toku_brt_optimize(db->i->brt);
    return r;
}

static int
toku_db_flatten(DB *db, DB_TXN *txn) {
    HANDLE_PANICKED_DB(db);
    TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
    int r = toku_brt_flatten(db->i->brt, ttxn);
    return r;
}

static inline int 
autotxn_db_flatten(DB* db, DB_TXN* txn) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_flatten(db, txn);
    return toku_db_destruct_autotxn(txn, r, changed);
}


static int 
locked_db_flatten(DB *db, DB_TXN *txn) {
    toku_ydb_lock(); int r = autotxn_db_flatten(db, txn); toku_ydb_unlock(); return r;
}

static int 
locked_db_optimize(DB *db) {
    toku_ydb_lock();
    int r = toku_db_optimize(db);
    toku_ydb_unlock();
    return r;
}

static int
db_get_fragmentation(DB * db, TOKU_DB_FRAGMENTATION report) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (!db_opened(db))
        r = toku_ydb_do_error(db->dbenv, EINVAL, "Fragmentation report available only on open DBs.\n");
    else
        r = toku_brt_get_fragmentation(db->i->brt, report);
    return r;
}

static int
locked_db_get_fragmentation(DB * db, TOKU_DB_FRAGMENTATION report) {
    toku_ydb_lock();
    int r = db_get_fragmentation(db, report);
    toku_ydb_unlock();
    return r;
}

int 
toku_db_set_indexer(DB *db, DB_INDEXER * indexer) {
    db->i->indexer = indexer;
    return 0;
}

static int 
locked_db_set_indexer(DB *db, DB_INDEXER *indexer) {
    toku_ydb_lock(); int r = toku_db_set_indexer(db, indexer); toku_ydb_unlock(); return r;
}

DB_INDEXER *
toku_db_get_indexer(DB *db) {
    return db->i->indexer;
}

static void 
locked_db_get_indexer(DB *db, DB_INDEXER **indexer_ptr) {
    toku_ydb_lock(); *indexer_ptr = toku_db_get_indexer(db); toku_ydb_unlock();
}

static int 
toku_db_create(DB ** db, DB_ENV * env, u_int32_t flags) {
    int r;

    if (flags || env == NULL) 
        return EINVAL;

    if (!env_opened(env))
        return EINVAL;
    
    DB *MALLOC(result);
    if (result == 0) {
        return ENOMEM;
    }
    memset(result, 0, sizeof *result);
    result->dbenv = env;
#define SDB(name) result->name = locked_db_ ## name
    SDB(key_range64);
    SDB(close);
    SDB(cursor);
    SDB(del);
    SDB(get);
    //    SDB(key_range);
    SDB(open);
    SDB(put);
    SDB(remove);
    SDB(rename);
    SDB(set_descriptor);
    SDB(set_errfile);
    SDB(set_pagesize);
    SDB(set_flags);
    SDB(get_flags);
    SDB(stat64);
    SDB(fd);
    SDB(pre_acquire_table_lock);
    SDB(pre_acquire_fileops_lock);
    SDB(truncate);
    SDB(row_size_supported);
    SDB(getf_set);
    SDB(flatten);
    SDB(optimize);
    SDB(get_fragmentation);
    SDB(set_indexer);
    SDB(get_indexer);
#undef SDB
    result->dbt_pos_infty = toku_db_dbt_pos_infty;
    result->dbt_neg_infty = toku_db_dbt_neg_infty;
    MALLOC(result->i);
    if (result->i == 0) {
        toku_free(result);
        return ENOMEM;
    }
    memset(result->i, 0, sizeof *result->i);
    result->i->dict_id = DICTIONARY_ID_NONE;
    result->i->opened = 0;
    result->i->open_flags = 0;
    result->i->open_mode = 0;
    result->i->brt = 0;
    result->i->indexer = NULL;
    result->i->refs = 1;
    toku_list_init(&result->i->dbs_that_must_close_before_abort);
    r = toku_brt_create(&result->i->brt);
    if (r != 0) {
        toku_free(result->i);
        toku_free(result);
        return r;
    }
    *db = result;
    return 0;
}

int 
DB_CREATE_FUN (DB ** db, DB_ENV * env, u_int32_t flags) {
    toku_ydb_lock(); 
    int r = toku_db_create(db, env, flags); 
    toku_ydb_unlock(); 
    return r;
}

/* need db_strerror_r for multiple threads */

char *
db_strerror(int error) {
    char *errorstr;
    if (error >= 0) {
        errorstr = strerror(error);
        if (errorstr)
            return errorstr;
    }
    
    switch (error) {
        case DB_BADFORMAT:
            return "Database Bad Format (probably a corrupted database)";
        case DB_NOTFOUND:
            return "Not found";
        case TOKUDB_OUT_OF_LOCKS:
            return "Out of locks";
        case TOKUDB_DICTIONARY_TOO_OLD:
            return "Dictionary too old for this version of TokuDB";
        case TOKUDB_DICTIONARY_TOO_NEW:
            return "Dictionary too new for this version of TokuDB";
        case TOKUDB_CANCELED:
            return "User cancelled operation";
        case TOKUDB_NO_DATA:
            return "Ran out of data (not EOF)";
    }

    static char unknown_result[100];    // Race condition if two threads call this at the same time. However even in a bad case, it should be some sort of null-terminated string.
    errorstr = unknown_result;
    snprintf(errorstr, sizeof unknown_result, "Unknown error code: %d", error);
    return errorstr;
}

const char *
db_version(int *major, int *minor, int *patch) {
    if (major)
        *major = DB_VERSION_MAJOR;
    if (minor)
        *minor = DB_VERSION_MINOR;
    if (patch)
        *patch = DB_VERSION_PATCH;
#if defined(TOKUDB_REVISION)
#define xstr(X) str(X)
#define str(X) #X
    return "tokudb " xstr(DB_VERSION_MAJOR) "." xstr(DB_VERSION_MINOR) "." xstr(DB_VERSION_PATCH) " build " xstr(TOKUDB_REVISION);
#else
    return DB_VERSION_STRING;
#endif
}
 
int 
db_env_set_func_fsync (int (*fsync_function)(int)) {
    return toku_set_func_fsync(fsync_function);
}

int 
db_env_set_func_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    return toku_set_func_pwrite(pwrite_function);
}

int 
db_env_set_func_full_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    return toku_set_func_full_pwrite(pwrite_function);
}

int 
db_env_set_func_write (ssize_t (*write_function)(int, const void *, size_t)) {
    return toku_set_func_write(write_function);
}

int 
db_env_set_func_full_write (ssize_t (*write_function)(int, const void *, size_t)) {
    return toku_set_func_full_write(write_function);
}

int 
db_env_set_func_fdopen (FILE * (*fdopen_function)(int, const char *)) {
    return toku_set_func_fdopen(fdopen_function);
}

int 
db_env_set_func_fopen (FILE * (*fopen_function)(const char *, const char *)) {
    return toku_set_func_fopen(fopen_function);
}

int 
db_env_set_func_open (int (*open_function)(const char *, int, int)) {
    return toku_set_func_open(open_function);
}

int 
db_env_set_func_fclose (int (*fclose_function)(FILE*)) {
    return toku_set_func_fclose(fclose_function);
}

void 
db_env_set_func_loader_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*)) {
    brtloader_set_os_fwrite(fwrite_fun);
}

int 
db_env_set_func_malloc (void *(*f)(size_t)) {
    return toku_set_func_malloc(f);
}

int 
db_env_set_func_realloc (void *(*f)(void*, size_t)) {
    return toku_set_func_realloc(f);
}

int 
db_env_set_func_free (void (*f)(void*)) {
    return toku_set_func_free(f);
}

// Got to call dlmalloc, or else it won't get included.
void 
setup_dlmalloc (void) {
    db_env_set_func_malloc(dlmalloc);
    db_env_set_func_realloc(dlrealloc);
    db_env_set_func_free(dlfree);
}

// For test purposes only.
// With this interface, all checkpoint users get the same callbacks and the same extras.
void 
db_env_set_checkpoint_callback (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback_f = callback_f;
    checkpoint_callback_extra = extra;
    toku_checkpoint_safe_client_unlock();
    //printf("set callback = %p, extra = %p\n", callback_f, extra);
}

void 
db_env_set_checkpoint_callback2 (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback2_f = callback_f;
    checkpoint_callback2_extra = extra;
    toku_checkpoint_safe_client_unlock();
    //printf("set callback2 = %p, extra2 = %p\n", callback2_f, extra2);
}

void 
db_env_set_recover_callback (void (*callback_f)(void*), void* extra) {
    toku_recover_set_callback(callback_f, extra);
}

void 
db_env_set_recover_callback2 (void (*callback_f)(void*), void* extra) {
    toku_recover_set_callback2(callback_f, extra);
}

void 
db_env_set_loader_size_factor (uint32_t factor) {
    toku_brtloader_set_size_factor(factor);
}

void 
db_env_set_mvcc_garbage_collection_verification(u_int32_t verification_mode) {
    garbage_collection_debug = (verification_mode != 0);
}



// HACK: To ensure toku_pthread_yield gets included in the .so
// non-static would require a prototype in a header
// static (since unused) would give a warning
// static + unused would not actually help toku_pthread_yield get in the .so
// static + used avoids all the warnings and makes sure toku_pthread_yield is in the .so
static void __attribute__((__used__))
include_toku_pthread_yield (void) {
    toku_pthread_yield();
}


// For test purposes only, translate dname to iname
static int 
env_get_iname(DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt) {
    toku_ydb_lock();
    DB *directory = env->i->directory;
    int r = autotxn_db_get(directory, NULL, dname_dbt, iname_dbt, DB_SERIALIZABLE|DB_PRELOCKED); // allocates memory for iname
    toku_ydb_unlock();
    return r;
}

/* Following functions (ydb_load_xxx()) are used by loader:
 */


// When the loader is created, it makes this call.
// For each dictionary to be loaded, replace old iname in directory
// with a newly generated iname.  This will also take a write lock
// on the directory entries.  The write lock will be released when
// the transaction of the loader is completed.
// If the transaction commits, the new inames are in place.
// If the transaction aborts, the old inames will be restored.
// The new inames are returned to the caller.  
// It is the caller's responsibility to free them.
// Return 0 on success (could fail if write lock not available).
int
ydb_load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[N], char * new_inames_in_env[N], LSN *load_lsn) {
    int rval;
    int i;
    
    int using_txns = env->i->open_flags & DB_INIT_TXN;
    DB_TXN * child = NULL;
    TXNID xid = 0;
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds new iname
    
    for (i=0; i<N; i++) {
	new_inames_in_env[i] = NULL;
    }

    // begin child (unless transactionless)
    if (using_txns) {
	rval = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1);
	assert(rval == 0);
	xid = toku_txn_get_txnid(db_txn_struct_i(child)->tokutxn);
    }
    for (i = 0; i < N; i++) {
	char * dname = dbs[i]->i->dname;
	toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
	// now create new iname
	char hint[strlen(dname) + 1];
	create_iname_hint(dname, hint);
	char * new_iname = create_iname(env, xid, hint, i);               // allocates memory for iname_in_env
	new_inames_in_env[i] = new_iname;
        toku_fill_dbt(&iname_dbt, new_iname, strlen(new_iname) + 1);      // iname_in_env goes in directory
        rval = toku_db_put(env->i->directory, child, &dname_dbt, &iname_dbt, DB_YESOVERWRITE);  // DB_YESOVERWRITE necessary
	if (rval) break;
    }

    // Generate load log entries.
    if (!rval && using_txns) {
        TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
        int do_fsync = 0;
        LSN *get_lsn = NULL;
        for (i = 0; i < N; i++) {
            BRT brt  = dbs[i]->i->brt;
            //Fsync is necessary for the last one only.
            if (i==N-1) {
                do_fsync = 1; //We only need a single fsync of logs.
                get_lsn  = load_lsn; //Set pointer to capture the last lsn.
            }
            rval = toku_brt_load(brt, ttxn, new_inames_in_env[i], do_fsync, get_lsn);
            if (rval) break;
        }
    }
	
    if (using_txns) {
	// close txn
	if (rval == 0) {  // all well so far, commit child
	    rval = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL);
	    assert(rval==0);
	}
	else {         // abort child
	    int r2 = toku_txn_abort(child, NULL, NULL);
	    assert(r2==0);
	    for (i=0; i<N; i++) {
		if (new_inames_in_env[i]) {
		    toku_free(new_inames_in_env[i]);
		    new_inames_in_env[i] = NULL;
		}
	    }
	}
    }

    return rval;
}

int
locked_ydb_load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[N], char * new_inames_in_env[N], LSN *load_lsn) {
    toku_ydb_lock();
    int r = ydb_load_inames(env, txn, N, dbs, new_inames_in_env, load_lsn);
    toku_ydb_unlock();
    return r;
}

// TODO 2216:  Patch out this (dangerous) function when loader is working and 
//             we don't need to test the low-level redirect anymore.
// for use by test programs only, just a wrapper around brt call:
int
toku_test_db_redirect_dictionary(DB * db, char * dname_of_new_file, DB_TXN *dbtxn) {
    int r;
    DBT dname_dbt;
    DBT iname_dbt;
    char * new_iname_in_env;

    BRT brt = db->i->brt;
    TOKUTXN tokutxn = db_txn_struct_i(dbtxn)->tokutxn;

    toku_fill_dbt(&dname_dbt, dname_of_new_file, strlen(dname_of_new_file)+1);
    init_dbt_realloc(&iname_dbt);  // sets iname_dbt.data = NULL
    r = toku_db_get(db->dbenv->i->directory, dbtxn, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    assert(r==0);
    new_iname_in_env = iname_dbt.data;

    r = toku_dictionary_redirect(new_iname_in_env, brt, tokutxn);

    toku_free(new_iname_in_env);
    return r;
}

//Tets only function
uint64_t
toku_test_get_latest_lsn(DB_ENV *env) {
    LSN rval = ZERO_LSN;
    if (env && env->i->logger) {
        rval = toku_logger_last_lsn(env->i->logger);
    }
    return rval.lsn;
}

int 
toku_test_get_checkpointing_user_data_status (void) {
    return toku_cachetable_get_checkpointing_user_data_status();
}

int
toku_grab_write_lock (DB* db, DBT* key, TOKUTXN tokutxn) {
    RANGE_LOCK_REQUEST_S request;
    DB_TXN * txn = toku_txn_get_container_db_txn(tokutxn);
    //Left end of range == right end of range (point lock)
    write_lock_request_init(&request, txn, db,
			    key,
			    key);
    int r = grab_range_lock(&request);
    return r;
}

