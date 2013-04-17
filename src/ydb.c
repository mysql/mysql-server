/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

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
#include "brt-flusher.h"
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
#include "log_header.h"
#include "ydb_cursor.h"
#include "ydb_row_lock.h"
#include "ydb_env_func.h"
#include "ydb_db.h"
#include "ydb_write.h"
#include "ydb_txn.h"

#ifdef TOKUTRACE
 #define DB_ENV_CREATE_FUN db_env_create_toku10
 #define DB_CREATE_FUN db_create_toku10
#else
 #define DB_ENV_CREATE_FUN db_env_create
 #define DB_CREATE_FUN db_create
 int toku_set_trace_file (char *fname __attribute__((__unused__))) { return 0; }
 int toku_close_trace_file (void) { return 0; } 
#endif

// Set when env is panicked, never cleared.
static int env_is_panicked = 0;


void
env_panic(DB_ENV * env, int cause, char * msg) {
    if (cause == 0)
	cause = -1;  // if unknown cause, at least guarantee panic
    if (msg == NULL)
	msg = "Unknown cause in env_panic\n";
    env_is_panicked = cause;
    env->i->is_panicked = cause;
    env->i->panic_string = toku_strdup(msg);
}

static int env_get_engine_status_num_rows (DB_ENV * UU(env), uint64_t * num_rowsp);


/********************************************************************************
 * Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */

typedef enum {
    YDB_LAYER_TIME_CREATION = 0,            /* timestamp of environment creation, read from persistent environment */
    YDB_LAYER_TIME_STARTUP,                 /* timestamp of system startup */
    YDB_LAYER_TIME_NOW,                     /* timestamp of engine status query */
    YDB_LAYER_NUM_DB_OPEN,
    YDB_LAYER_NUM_DB_CLOSE,
    YDB_LAYER_NUM_OPEN_DBS,
    YDB_LAYER_MAX_OPEN_DBS,
#if 0
    YDB_LAYER_ORIGINAL_ENV_VERSION,         /* version of original environment, read from persistent environment */
    YDB_LAYER_STARTUP_ENV_VERSION,          /* version of environment at this startup, read from persistent environment (curr_env_ver_key) */
    YDB_LAYER_LAST_LSN_OF_V13,              /* read from persistent environment */
    YDB_LAYER_UPGRADE_V14_TIME,             /* timestamp of upgrade to version 14, read from persistent environment */
    YDB_LAYER_UPGRADE_V14_FOOTPRINT,        /* footprint of upgrade to version 14, read from persistent environment */
#endif
    YDB_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_layer_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_LAYER_STATUS_NUM_ROWS];
} YDB_LAYER_STATUS_S, *YDB_LAYER_STATUS;

static YDB_LAYER_STATUS_S ydb_layer_status;
#define STATUS_VALUE(x) ydb_layer_status.status[x].value.num

#define STATUS_INIT(k,t,l) { \
	ydb_layer_status.status[k].keyname = #k; \
	ydb_layer_status.status[k].type    = t;  \
	ydb_layer_status.status[k].legend  = l; \
    }
static void
ydb_layer_status_init (void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(YDB_LAYER_TIME_CREATION,              UNIXTIME, "time of environment creation");
    STATUS_INIT(YDB_LAYER_TIME_STARTUP,               UNIXTIME, "time of engine startup");
    STATUS_INIT(YDB_LAYER_TIME_NOW,                   UNIXTIME, "time now");
    STATUS_INIT(YDB_LAYER_NUM_DB_OPEN,                UINT64,   "db opens");
    STATUS_INIT(YDB_LAYER_NUM_DB_CLOSE,               UINT64,   "db closes");
    STATUS_INIT(YDB_LAYER_NUM_OPEN_DBS,               UINT64,   "num open dbs now");
    STATUS_INIT(YDB_LAYER_MAX_OPEN_DBS,               UINT64,   "max open dbs");

    STATUS_VALUE(YDB_LAYER_TIME_STARTUP) = time(NULL);
    ydb_layer_status.initialized = true;
}
#undef STATUS_INIT

static void
ydb_layer_get_status(YDB_LAYER_STATUS statp) {
    STATUS_VALUE(YDB_LAYER_TIME_NOW) = time(NULL);
    *statp = ydb_layer_status;
}


/********************************************************************************
 * End of ydb_layer local status section.
 */


static DB_ENV * volatile most_recent_env;   // most recently opened env, used for engine status on crash.  Note there are likely to be races on this if you have multiple threads creating and closing environments in parallel.  We'll declare it volatile since at least that helps make sure the compiler doesn't optimize away certain code (e.g., if while debugging, you write a code that spins on most_recent_env, you'd like to compiler not to optimize your code away.)

const char * environmentdictionary = "tokudb.environment";
const char * fileopsdirectory = "tokudb.directory";

static int env_get_iname(DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt);
static int toku_maybe_get_engine_status_text (char* buff, int buffsize);  // for use by toku_assert
static void toku_maybe_set_env_panic(int code, char * msg);               // for use by toku_assert


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
static const u_int32_t __toku_env_default_locks_limit = 0x7FFFFFFF;
static const uint64_t __toku_env_default_lock_memory_limit = 1000*1024;

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

// Do not clean up resources if env is panicked, just exit ugly
int 
toku_ydb_destroy(void) {
    int r = 0;
    if (env_is_panicked == 0) {
        r = toku_ydb_lock_destroy();
	//Lower level must be cleaned up last.
	if (r==0)
	    r = toku_brt_destroy();
    }
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

static void 
env_init_open_txn(DB_ENV *env) {
    env->i->open_txns = 0;
}

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
    default:
        assert(0);
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
                           env->i->update_function,
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

static int toku_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte, u_int32_t min, u_int32_t flags);

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
// Following keys added in version 14, add more keys for future versions
static const char * creation_time_key         = "creation_time";
static const char * last_lsn_of_v13_key       = "last_lsn_of_v13";
static const char * upgrade_v14_time_key      = "upgrade_v14_time";      
static const char * upgrade_v14_footprint_key = "upgrade_v14_footprint";


// Values read from (or written into) persistent environment,
// kept here for read-only access from engine status.
// Note, persistent_upgrade_status info is separate in part to simplify its exclusion from engine status until relevant.
typedef enum {
    PERSISTENT_UPGRADE_ORIGINAL_ENV_VERSION = 0,
    PERSISTENT_UPGRADE_STORED_ENV_VERSION_AT_STARTUP,    // read from curr_env_ver_key, prev version as of this startup
    PERSISTENT_UPGRADE_LAST_LSN_OF_V13,
    PERSISTENT_UPGRADE_V14_TIME,
    PERSISTENT_UPGRADE_V14_FOOTPRINT,
    PERSISTENT_UPGRADE_STATUS_NUM_ROWS
} persistent_upgrade_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[PERSISTENT_UPGRADE_STATUS_NUM_ROWS];
} PERSISTENT_UPGRADE_STATUS_S, *PERSISTENT_UPGRADE_STATUS;

static PERSISTENT_UPGRADE_STATUS_S persistent_upgrade_status;

#define PERSISTENT_UPGRADE_STATUS_INIT(k,t,l) { \
	persistent_upgrade_status.status[k].keyname = #k; \
	persistent_upgrade_status.status[k].type    = t;  \
	persistent_upgrade_status.status[k].legend  = "upgrade: " l; \
    }

static void
persistent_upgrade_status_init (void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    PERSISTENT_UPGRADE_STATUS_INIT(PERSISTENT_UPGRADE_ORIGINAL_ENV_VERSION,           UINT64,   "original version (at time of environment creation)");
    PERSISTENT_UPGRADE_STATUS_INIT(PERSISTENT_UPGRADE_STORED_ENV_VERSION_AT_STARTUP,  UINT64,   "version at time of startup");
    PERSISTENT_UPGRADE_STATUS_INIT(PERSISTENT_UPGRADE_LAST_LSN_OF_V13,                UINT64,   "last LSN of version 13");
    PERSISTENT_UPGRADE_STATUS_INIT(PERSISTENT_UPGRADE_V14_TIME,                       UNIXTIME, "time of upgrade to version 14");
    PERSISTENT_UPGRADE_STATUS_INIT(PERSISTENT_UPGRADE_V14_FOOTPRINT,                  UINT64,   "footprint from version 13 to 14");
    persistent_upgrade_status.initialized = true;
}

#define PERSISTENT_UPGRADE_STATUS_VALUE(x) persistent_upgrade_status.status[x].value.num

// Requires: persistent environment dictionary is already open.
// Input arg is lsn of clean shutdown of previous version,
// or ZERO_LSN if no upgrade or if crash between log upgrade and here.
// NOTE: To maintain compatibility with previous versions, do not change the 
//       format of any information stored in the persistent environment dictionary.
//       For example, some values are stored as 32 bits, even though they are immediately
//       converted to 64 bits when read.  Do not change them to be stored as 64 bits.
//
static int
maybe_upgrade_persistent_environment_dictionary(DB_ENV * env, DB_TXN * txn, LSN last_lsn_of_clean_shutdown_read_from_log) {
    int r;
    DBT key, val;
    DB *persistent_environment = env->i->persistent_environment;

    if (!persistent_upgrade_status.initialized)
        persistent_upgrade_status_init();

    toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
    toku_init_dbt(&val);
    r = toku_db_get(persistent_environment, txn, &key, &val, 0);
    assert(r == 0);
    uint32_t stored_env_version = toku_dtoh32(*(uint32_t*)val.data);
    PERSISTENT_UPGRADE_STATUS_VALUE(PERSISTENT_UPGRADE_STORED_ENV_VERSION_AT_STARTUP) = stored_env_version;
    if (stored_env_version > BRT_LAYOUT_VERSION)
	r = TOKUDB_DICTIONARY_TOO_NEW;
    else if (stored_env_version < BRT_LAYOUT_MIN_SUPPORTED_VERSION)
	r = TOKUDB_DICTIONARY_TOO_OLD;
    else if (stored_env_version < BRT_LAYOUT_VERSION) {
        const uint32_t curr_env_ver_d = toku_htod32(BRT_LAYOUT_VERSION);
        toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
        toku_fill_dbt(&val, &curr_env_ver_d, sizeof(curr_env_ver_d));
        r = toku_db_put(persistent_environment, txn, &key, &val, 0, TRUE);
        assert_zero(r);
	
	uint64_t last_lsn_of_v13_d = toku_htod64(last_lsn_of_clean_shutdown_read_from_log.lsn);
	toku_fill_dbt(&key, last_lsn_of_v13_key, strlen(last_lsn_of_v13_key));
	toku_fill_dbt(&val, &last_lsn_of_v13_d, sizeof(last_lsn_of_v13_d));
	r = toku_db_put(persistent_environment, txn, &key, &val, 0, TRUE);
        assert_zero(r);
	
	time_t upgrade_v14_time_d = toku_htod64(time(NULL));
	toku_fill_dbt(&key, upgrade_v14_time_key, strlen(upgrade_v14_time_key));
	toku_fill_dbt(&val, &upgrade_v14_time_d, sizeof(upgrade_v14_time_d));
	r = toku_db_put(persistent_environment, txn, &key, &val, DB_NOOVERWRITE, TRUE);
        assert_zero(r);

	uint64_t upgrade_v14_footprint_d = toku_htod64(toku_log_upgrade_get_footprint());
	toku_fill_dbt(&key, upgrade_v14_footprint_key, strlen(upgrade_v14_footprint_key));
	toku_fill_dbt(&val, &upgrade_v14_footprint_d, sizeof(upgrade_v14_footprint_d));
	r = toku_db_put(persistent_environment, txn, &key, &val, DB_NOOVERWRITE, TRUE);
        assert_zero(r);
    }
    return r;
}


// Capture contents of persistent_environment dictionary so that it can be read by engine status
static void
capture_persistent_env_contents (DB_ENV * env, DB_TXN * txn) {
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
    uint64_t persistent_original_env_version = toku_dtoh32(*(uint32_t*)val.data);
    PERSISTENT_UPGRADE_STATUS_VALUE(PERSISTENT_UPGRADE_ORIGINAL_ENV_VERSION) = persistent_original_env_version;
    assert(persistent_original_env_version <= curr_env_version);

    // make no assertions about timestamps, clock may have been reset
    if (persistent_original_env_version >= BRT_LAYOUT_VERSION_14) {
	toku_fill_dbt(&key, creation_time_key, strlen(creation_time_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	STATUS_VALUE(YDB_LAYER_TIME_CREATION) = toku_dtoh64((*(time_t*)val.data));
    }

    if (persistent_original_env_version != curr_env_version) {
	// an upgrade was performed at some time, capture info about the upgrade
	
	toku_fill_dbt(&key, last_lsn_of_v13_key, strlen(last_lsn_of_v13_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	PERSISTENT_UPGRADE_STATUS_VALUE(PERSISTENT_UPGRADE_LAST_LSN_OF_V13) = toku_dtoh64(*(uint32_t*)val.data);

	toku_fill_dbt(&key, upgrade_v14_time_key, strlen(upgrade_v14_time_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	PERSISTENT_UPGRADE_STATUS_VALUE(PERSISTENT_UPGRADE_V14_TIME) = toku_dtoh64((*(time_t*)val.data));

	toku_fill_dbt(&key, upgrade_v14_footprint_key, strlen(upgrade_v14_footprint_key));
	toku_init_dbt(&val);
	r = toku_db_get(persistent_environment, txn, &key, &val, 0);
	assert(r == 0);
	PERSISTENT_UPGRADE_STATUS_VALUE(PERSISTENT_UPGRADE_V14_FOOTPRINT) = toku_dtoh64((*(uint64_t*)val.data));
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


// The version of the environment (on disk) is the version of the recovery log.  
// If the recovery log is of the current version, then there is no upgrade to be done.  
// If the recovery log is of an old version, then replacing it with a new recovery log
// of the current version is how the upgrade is done.  
// Note, the upgrade procedure takes a checkpoint, so we must release the ydb lock.
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
    lazy_assert_zero(r);
    r = single_process_unlock(&env->i->datadir_lockfd);
    lazy_assert_zero(r);
    r = single_process_unlock(&env->i->logdir_lockfd);
    lazy_assert_zero(r);
    r = single_process_unlock(&env->i->tmpdir_lockfd);
    lazy_assert_zero(r);
}

static int toku_db_lt_panic(DB* db, int r);

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

    most_recent_env = NULL;

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

    ydb_layer_status_init();  // do this before possibly upgrading, so upgrade work is counted in status counters

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
	assert_zero(r);
    }

    r = toku_ltm_open(env->i->ltm);
    assert_zero(r);

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
    toku_cachetable_set_lock_unlock_for_io(env->i->cachetable, toku_ydb_lock, toku_ydb_unlock);

    toku_cachetable_set_env_dir(env->i->cachetable, env->i->dir);

    int using_txns = env->i->open_flags & DB_INIT_TXN;
    if (env->i->logger) {
	// if this is a newborn env or if this is an upgrade, then create a brand new rollback file
	BOOL create_new_rollback_file = newenv | upgrade_in_progress;
	assert (using_txns);
	toku_logger_set_cachetable(env->i->logger, env->i->cachetable);
	toku_logger_set_remove_finalize_callback(env->i->logger, finalize_file_removal, env->i->ltm);
        r = toku_logger_open_rollback(env->i->logger, env->i->cachetable, create_new_rollback_file);
        assert_zero(r);
    }

    DB_TXN *txn=NULL;
    if (using_txns) {
        r = toku_txn_begin(env, 0, &txn, 0, 1, true);
        assert_zero(r);
    }

    {
        r = toku_db_create(&env->i->persistent_environment, env, 0);
        assert_zero(r);
        r = db_use_builtin_key_cmp(env->i->persistent_environment);
        assert_zero(r);
	r = db_open_iname(env->i->persistent_environment, txn, environmentdictionary, DB_CREATE, mode);
	assert_zero(r);
	if (newenv) {
	    // create new persistent_environment
	    DBT key, val;
	    uint32_t persistent_original_env_version = BRT_LAYOUT_VERSION;
	    const uint32_t environment_version = toku_htod32(persistent_original_env_version);

	    toku_fill_dbt(&key, orig_env_ver_key, strlen(orig_env_ver_key));
	    toku_fill_dbt(&val, &environment_version, sizeof(environment_version));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0, TRUE);
	    assert_zero(r);

	    toku_fill_dbt(&key, curr_env_ver_key, strlen(curr_env_ver_key));
	    toku_fill_dbt(&val, &environment_version, sizeof(environment_version));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0, TRUE);
	    assert_zero(r);

	    time_t creation_time_d = toku_htod64(time(NULL));
	    toku_fill_dbt(&key, creation_time_key, strlen(creation_time_key));
	    toku_fill_dbt(&val, &creation_time_d, sizeof(creation_time_d));
	    r = toku_db_put(env->i->persistent_environment, txn, &key, &val, 0, TRUE);
	    assert_zero(r);
	}
	else {
	    r = maybe_upgrade_persistent_environment_dictionary(env, txn, last_lsn_of_clean_shutdown_read_from_log);
	    assert_zero(r);
	}
	capture_persistent_env_contents(env, txn);
    }
    {
        r = toku_db_create(&env->i->directory, env, 0);
        assert_zero(r);
        r = db_use_builtin_key_cmp(env->i->directory);
        assert_zero(r);
        r = db_open_iname(env->i->directory, txn, fileopsdirectory, DB_CREATE, mode);
        assert_zero(r);
    }
    if (using_txns) {
        r = toku_txn_commit(txn, 0, NULL, NULL, false);
        assert_zero(r);
    }
    toku_ydb_unlock();
    r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL, STARTUP_CHECKPOINT);
    assert_zero(r);
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
	errno = 0; // tabula rasa.   If there's a crash after env was successfully opened, no misleading errno will have been left around by this code.
	most_recent_env = env;
        uint64_t num_rows;
        env_get_engine_status_num_rows(env, &num_rows);
	toku_assert_set_fpointers(toku_maybe_get_engine_status_text, toku_maybe_set_env_panic, num_rows);
    }
    return r;
}


static int 
toku_env_close(DB_ENV * env, u_int32_t flags) {
    int r = 0;
    char * err_msg = NULL;

    most_recent_env = NULL; // Set most_recent_env to NULL so that we don't have a dangling pointer (and if there's an error, the toku assert code would try to look at the env.)

    // if panicked, or if any open transactions, or any open dbs, then do nothing.

    if (toku_env_is_panicked(env)) goto panic_and_quit_early;
    if (env->i->open_txns != 0) {
	err_msg = "Cannot close environment due to open transactions\n";
        r = toku_ydb_do_error(env, EINVAL, "%s", err_msg);
        goto panic_and_quit_early;
    }
    if (env->i->open_dbs) { //Verify open dbs. Zombies are ok at this stage, fully open is not.
        uint32_t size = toku_omt_size(env->i->open_dbs);
        assert(size == env->i->num_open_dbs + env->i->num_zombie_dbs);
        if (env->i->num_open_dbs > 0) {
	    err_msg = "Cannot close environment due to open DBs\n";
            r = toku_ydb_do_error(env, EINVAL, "%s", err_msg);
            goto panic_and_quit_early;
        }
    }
    {
        if (env->i->persistent_environment) {
            r = toku_db_close(env->i->persistent_environment, 0);
            if (r) {
		err_msg = "Cannot close persistent environment dictionary (DB->close error)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
        }
        if (env->i->directory) {
            r = toku_db_close(env->i->directory, 0);
            if (r) {
		err_msg = "Cannot close Directory dictionary (DB->close error)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
        }
    }
    if (env->i->cachetable) {
	toku_ydb_unlock();  // ydb lock must not be held when shutting down minicron
	toku_cachetable_minicron_shutdown(env->i->cachetable);
        if (env->i->logger) {
            r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
            if (r) {
		err_msg = "Cannot close environment (error during checkpoint)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
            { //Verify open dbs. Neither Zombies nor fully open are ok at this stage.
                uint32_t size = toku_omt_size(env->i->open_dbs);
                assert(size == env->i->num_open_dbs + env->i->num_zombie_dbs);
                if (size > 0) {
		    err_msg = "Cannot close environment due to zombie DBs\n";
                    r = toku_ydb_do_error(env, EINVAL, "%s", err_msg);
                    goto panic_and_quit_early;
                }
            }
            r = toku_logger_close_rollback(env->i->logger, FALSE);
            if (r) {
		err_msg = "Cannot close environment (error during closing rollback cachefile)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
            //Do a second checkpoint now that the rollback cachefile is closed.
            r = toku_checkpoint(env->i->cachetable, env->i->logger, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
            if (r) {
		err_msg = "Cannot close environment (error during checkpoint)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
            r = toku_logger_shutdown(env->i->logger); 
            if (r) {
		err_msg = "Cannot close environment (error during logger shutdown)\n";
                toku_ydb_do_error(env, r, "%s", err_msg);
                goto panic_and_quit_early;
            }
        }
	toku_ydb_lock();
        r=toku_cachetable_close(&env->i->cachetable);
	if (r) {
	    err_msg = "Cannot close environment (cachetable close error)\n";
	    toku_ydb_do_error(env, r, "%s", err_msg);
            goto panic_and_quit_early;
	}
    }
    if (env->i->logger) {
        r=toku_logger_close(&env->i->logger);
	if (r) {
	    err_msg = "Cannot close environment (logger close error)\n";
            env->i->logger = NULL;
	    toku_ydb_do_error(env, r, "%s", err_msg);
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
    if (env->i->ltm) {
        toku_ltm_close(env->i->ltm);
        env->i->ltm = NULL;
    }
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
    if (flags!=0)
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
    else {
	env_panic(env, r, err_msg);
    }
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

static int
locked_env_dbremove(DB_ENV * env, DB_TXN *txn, const char *fname, const char *dbname, u_int32_t flags) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock();
    int r = toku_env_dbremove(env, txn, fname, dbname, flags);
    toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

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
toku_env_set_lk_max_locks(DB_ENV *env, u_int32_t locks_limit) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (env_opened(env)) {
        r = EINVAL;
    } else {
        r = toku_ltm_set_max_locks(env->i->ltm, locks_limit);
    }
    return r;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
static int 
toku_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    return toku_env_set_lk_max_locks(env, lk_max);
}

static int 
locked_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    toku_ydb_lock(); 
    int r = toku_env_set_lk_max(env, lk_max); 
    toku_ydb_unlock(); 
    return r;
}
#endif

static int 
toku_env_get_lk_max_locks(DB_ENV *env, u_int32_t *lk_maxp) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (lk_maxp == NULL) 
        r = EINVAL;
    else {
        r = toku_ltm_get_max_locks(env->i->ltm, lk_maxp);
    }
    return r;
}

static int 
locked_env_set_lk_max_locks(DB_ENV *env, u_int32_t max) {
    toku_ydb_lock(); 
    int r = toku_env_set_lk_max_locks(env, max); 
    toku_ydb_unlock(); 
    return r;
}

static int 
locked_env_get_lk_max_locks(DB_ENV *env, u_int32_t *lk_maxp) {
    toku_ydb_lock(); 
    int r = toku_env_get_lk_max_locks(env, lk_maxp); 
    toku_ydb_unlock(); 
    return r;
}

static int 
toku_env_set_lk_max_memory(DB_ENV *env, uint64_t lock_memory_limit) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (env_opened(env)) {
        r = EINVAL;
    } else {
        r = toku_ltm_set_max_lock_memory(env->i->ltm, lock_memory_limit);
    }
    return r;
}

static int 
toku_env_get_lk_max_memory(DB_ENV *env, uint64_t *lk_maxp) {
    HANDLE_PANICKED_ENV(env);
    int r = toku_ltm_get_max_lock_memory(env->i->ltm, lk_maxp);
    return r;
}

static int 
locked_env_set_lk_max_memory(DB_ENV *env, uint64_t max) {
    toku_ydb_lock(); 
    int r = toku_env_set_lk_max_memory(env, max); 
    toku_ydb_unlock(); 
    return r;
}

static int locked_env_get_lk_max_memory(DB_ENV *env, uint64_t *lk_maxp) {
    toku_ydb_lock(); 
    int r = toku_env_get_lk_max_memory(env, lk_maxp); 
    toku_ydb_unlock(); 
    return r;
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

static int 
toku_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte __attribute__((__unused__)), u_int32_t min __attribute__((__unused__)), u_int32_t flags __attribute__((__unused__))) {
    int r = toku_checkpoint(env->i->cachetable, env->i->logger,
			    checkpoint_callback_f,  checkpoint_callback_extra,
			    checkpoint_callback2_f, checkpoint_callback2_extra,
			    CLIENT_CHECKPOINT);
    if (r) {
	// Panicking the whole environment may be overkill, but I'm not sure what else to do.
	env_panic(env, r, "checkpoint error\n");
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
env_cleaner_set_period(DB_ENV * env, u_int32_t seconds) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (!env_opened(env)) r = EINVAL;
    else
        r = toku_set_cleaner_period(env->i->cachetable, seconds);
    return r;
}

static int
locked_env_cleaner_set_period(DB_ENV * env, u_int32_t seconds) {
    toku_ydb_lock(); int r = env_cleaner_set_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_cleaner_set_iterations(DB_ENV * env, u_int32_t iterations) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (!env_opened(env)) r = EINVAL;
    else
        r = toku_set_cleaner_iterations(env->i->cachetable, iterations);
    return r;
}

static int
locked_env_cleaner_set_iterations(DB_ENV * env, u_int32_t iterations) {
    toku_ydb_lock(); int r = env_cleaner_set_iterations(env, iterations); toku_ydb_unlock(); return r;
}

static int
locked_env_create_indexer(DB_ENV *env,
                          DB_TXN *txn,
                          DB_INDEXER **indexerp,
                          DB *src_db,
                          int N,
                          DB *dest_dbs[N],
                          uint32_t db_flags[N],
                          uint32_t indexer_flags) {
    toku_ydb_lock();
    int r = toku_indexer_create_indexer(env, txn, indexerp, src_db, N, dest_dbs, db_flags, indexer_flags);
    toku_ydb_unlock();
    return r;
}

static int
locked_env_create_loader(DB_ENV *env,
                         DB_TXN *txn, 
                         DB_LOADER **blp, 
                         DB *src_db, 
                         int N, 
                         DB *dbs[], 
                         uint32_t db_flags[N], 
                         uint32_t dbt_flags[N], 
                         uint32_t loader_flags) {
    toku_ydb_lock();
    int r = toku_loader_create_loader(env, txn, blp, src_db, N, dbs, db_flags, dbt_flags, loader_flags);
    toku_ydb_unlock();
    return r;
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
env_cleaner_get_period(DB_ENV * env, u_int32_t *seconds) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else 
        *seconds = toku_get_cleaner_period(env->i->cachetable);
    return r;
}

static int
locked_env_cleaner_get_period(DB_ENV * env, u_int32_t *seconds) {
    toku_ydb_lock(); int r = env_cleaner_get_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_cleaner_get_iterations(DB_ENV * env, u_int32_t *iterations) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else 
        *iterations = toku_get_cleaner_iterations(env->i->cachetable);
    return r;
}

static int
locked_env_cleaner_get_iterations(DB_ENV * env, u_int32_t *iterations) {
    toku_ydb_lock(); int r = env_cleaner_get_iterations(env, iterations); toku_ydb_unlock(); return r;
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

static void
env_set_update (DB_ENV *env, int (*update_function)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra)) {
    env->i->update_function = update_function;
}

static void
locked_env_set_update (DB_ENV *env, int (*update_function)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra)) {
    toku_ydb_lock();
    env_set_update (env, update_function);
    toku_ydb_unlock();
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

static int
env_get_lock_timeout(DB_ENV *env, uint64_t *lock_timeout_msec) {
    toku_ltm_get_lock_wait_time(env->i->ltm, lock_timeout_msec);
    return 0;
}

static int
locked_env_get_lock_timeout(DB_ENV *env, uint64_t *lock_timeout_msec) {
    toku_ydb_lock();
    int r = env_get_lock_timeout(env, lock_timeout_msec);
    toku_ydb_unlock();
    return r;
}

static int
env_set_lock_timeout(DB_ENV *env, uint64_t lock_timeout_msec) {
    toku_ltm_set_lock_wait_time(env->i->ltm, lock_timeout_msec);
    return 0;
}

static int
locked_env_set_lock_timeout(DB_ENV *env, uint64_t lock_timeout_msec) {
    toku_ydb_lock();
    int r = env_set_lock_timeout(env, lock_timeout_msec);
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

////////////////////////////////////////////////////////////////////////////////////////////////
// Local definition of status information from portability layer, which should not include db.h.
// Local status structs are used to concentrate file system information collected from various places
// and memory information collected from memory.c.
//
typedef enum {
    FS_ENOSPC_REDZONE_STATE = 0,  // possible values are enumerated by fs_redzone_state
    FS_ENOSPC_THREADS_BLOCKED,    // how many threads currently blocked on ENOSPC
    FS_ENOSPC_REDZONE_CTR,        // number of operations rejected by enospc prevention (red zone)
    FS_ENOSPC_MOST_RECENT,        // most recent time that file system was completely full
    FS_ENOSPC_COUNT,              // total number of times ENOSPC was returned from an attempt to write
    FS_FSYNC_TIME ,
    FS_FSYNC_COUNT,
    FS_STATUS_NUM_ROWS
} fs_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FS_STATUS_NUM_ROWS];
} FS_STATUS_S, *FS_STATUS;

static FS_STATUS_S fsstat;

#define FS_STATUS_INIT(k,t,l) {           \
	fsstat.status[k].keyname = #k; \
	fsstat.status[k].type    = t;  \
	fsstat.status[k].legend  = "filesystem: " l; \
    }

static void
fs_status_init(void) {
    FS_STATUS_INIT(FS_ENOSPC_REDZONE_STATE,   FS_STATE, "ENOSPC redzone state");
    FS_STATUS_INIT(FS_ENOSPC_THREADS_BLOCKED, UINT64,   "threads currently blocked by full disk");
    FS_STATUS_INIT(FS_ENOSPC_REDZONE_CTR,     UINT64,   "number of operations rejected by enospc prevention (red zone)");
    FS_STATUS_INIT(FS_ENOSPC_MOST_RECENT,     UNIXTIME, "most recent disk full");
    FS_STATUS_INIT(FS_ENOSPC_COUNT,           UINT64,   "number of write operations that returned ENOSPC");
    FS_STATUS_INIT(FS_FSYNC_TIME,             UINT64,   "fsync time");
    FS_STATUS_INIT(FS_FSYNC_COUNT,            UINT64,   "fsync count");
    fsstat.initialized = true;
}
#undef FS_STATUS_INIT

#define FS_STATUS_VALUE(x) fsstat.status[x].value.num

static void
fs_get_status(DB_ENV * env, fs_redzone_state * redzone_state) {
    if (!fsstat.initialized)
        fs_status_init();
    
    time_t   enospc_most_recent_timestamp;
    uint64_t enospc_threads_blocked, enospc_total;
    toku_fs_get_write_info(&enospc_most_recent_timestamp, &enospc_threads_blocked, &enospc_total);
    if (enospc_threads_blocked)
        FS_STATUS_VALUE(FS_ENOSPC_REDZONE_STATE) = FS_BLOCKED;
    else
        FS_STATUS_VALUE(FS_ENOSPC_REDZONE_STATE) = env->i->fs_state;
    *redzone_state = (fs_redzone_state) FS_STATUS_VALUE(FS_ENOSPC_REDZONE_STATE);
    FS_STATUS_VALUE(FS_ENOSPC_THREADS_BLOCKED) = enospc_threads_blocked;
    FS_STATUS_VALUE(FS_ENOSPC_REDZONE_CTR) = env->i->enospc_redzone_ctr;
    FS_STATUS_VALUE(FS_ENOSPC_MOST_RECENT) = enospc_most_recent_timestamp;
    FS_STATUS_VALUE(FS_ENOSPC_COUNT) = enospc_total;
    
    u_int64_t fsync_count, fsync_time;
    toku_get_fsync_times(&fsync_count, &fsync_time);
    FS_STATUS_VALUE(FS_FSYNC_COUNT) = fsync_count;
    FS_STATUS_VALUE(FS_FSYNC_TIME) = fsync_time;
}
#undef FS_STATUS_VALUE

// Local status struct used to get information from memory.c
typedef enum {
    MEMORY_MALLOC_COUNT = 0,
    MEMORY_FREE_COUNT,  
    MEMORY_REALLOC_COUNT,
    MEMORY_MALLOC_FAIL,  
    MEMORY_REALLOC_FAIL, 
    MEMORY_REQUESTED,    
    MEMORY_USED,         
    MEMORY_FREED,        
    MEMORY_MAX_IN_USE,
    MEMORY_MALLOCATOR_VERSION,
    MEMORY_MMAP_THRESHOLD,
    MEMORY_STATUS_NUM_ROWS
} memory_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[MEMORY_STATUS_NUM_ROWS];
} MEMORY_STATUS_S, *MEMORY_STATUS;

static MEMORY_STATUS_S memory_status;

#define STATUS_INIT(k,t,l) { \
	memory_status.status[k].keyname = #k; \
	memory_status.status[k].type    = t;  \
	memory_status.status[k].legend  = "memory: " l; \
    }

static void
memory_status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(MEMORY_MALLOC_COUNT,       UINT64,  "number of malloc operations");
    STATUS_INIT(MEMORY_FREE_COUNT,         UINT64,  "number of free operations");
    STATUS_INIT(MEMORY_REALLOC_COUNT,      UINT64,  "number of realloc operations");
    STATUS_INIT(MEMORY_MALLOC_FAIL,        UINT64,  "number of malloc operations that failed");
    STATUS_INIT(MEMORY_REALLOC_FAIL,       UINT64,  "number of realloc operations that failed" );
    STATUS_INIT(MEMORY_REQUESTED,          UINT64,  "number of bytes requested");
    STATUS_INIT(MEMORY_USED,               UINT64,  "number of bytes used (requested + overhead)");
    STATUS_INIT(MEMORY_FREED,              UINT64,  "number of bytes freed");
    STATUS_INIT(MEMORY_MAX_IN_USE,         UINT64,  "estimated maximum memory footprint");
    STATUS_INIT(MEMORY_MALLOCATOR_VERSION, CHARSTR, "mallocator version");
    STATUS_INIT(MEMORY_MMAP_THRESHOLD,     UINT64,  "mmap threshold");
    memory_status.initialized = true;  
}
#undef STATUS_INIT

#define MEMORY_STATUS_VALUE(x) memory_status.status[x].value.num

static void
memory_get_status(void) {
    if (!memory_status.initialized)
	memory_status_init();
    LOCAL_MEMORY_STATUS_S local_memstat;
    toku_memory_get_status(&local_memstat);
    MEMORY_STATUS_VALUE(MEMORY_MALLOC_COUNT) = local_memstat.malloc_count;
    MEMORY_STATUS_VALUE(MEMORY_FREE_COUNT) = local_memstat.free_count;  
    MEMORY_STATUS_VALUE(MEMORY_REALLOC_COUNT) = local_memstat.realloc_count;
    MEMORY_STATUS_VALUE(MEMORY_MALLOC_FAIL) = local_memstat.malloc_fail;
    MEMORY_STATUS_VALUE(MEMORY_REALLOC_FAIL) = local_memstat.realloc_fail;
    MEMORY_STATUS_VALUE(MEMORY_REQUESTED) = local_memstat.requested; 
    MEMORY_STATUS_VALUE(MEMORY_USED) = local_memstat.used;
    MEMORY_STATUS_VALUE(MEMORY_FREED) = local_memstat.freed;
    MEMORY_STATUS_VALUE(MEMORY_MAX_IN_USE) = local_memstat.max_in_use;
    MEMORY_STATUS_VALUE(MEMORY_MMAP_THRESHOLD) = local_memstat.mmap_threshold;
    memory_status.status[MEMORY_MALLOCATOR_VERSION].value.str = local_memstat.mallocator_version;
}
#undef MEMORY_STATUS_VALUE


// how many rows are in engine status?
static int
env_get_engine_status_num_rows (DB_ENV * UU(env), uint64_t * num_rowsp) {
    uint64_t num_rows = 0;
    num_rows += YDB_LAYER_STATUS_NUM_ROWS;
    num_rows += YDB_C_LAYER_STATUS_NUM_ROWS;
    num_rows += YDB_WRITE_LAYER_STATUS_NUM_ROWS;
    num_rows += YDB_LOCK_STATUS_NUM_ROWS;
    num_rows += LE_STATUS_NUM_ROWS;
    num_rows += CP_STATUS_NUM_ROWS;
    num_rows += CT_STATUS_NUM_ROWS;
    num_rows += LTM_STATUS_NUM_ROWS;
    num_rows += BRT_STATUS_NUM_ROWS;
    num_rows += BRT_FLUSHER_STATUS_NUM_ROWS;
    num_rows += BRT_HOT_STATUS_NUM_ROWS;
    num_rows += TXN_STATUS_NUM_ROWS;
    num_rows += LOGGER_STATUS_NUM_ROWS;
    num_rows += MEMORY_STATUS_NUM_ROWS;
    num_rows += FS_STATUS_NUM_ROWS;
    num_rows += INDEXER_STATUS_NUM_ROWS;
    num_rows += LOADER_STATUS_NUM_ROWS;
#if 0
    // enable when upgrade is supported
    num_rows += BRT_UPGRADE_STATUS_NUM_ROWS;
    num_rows += PERSISTENT_UPGRADE_STATUS_NUM_ROWS;
#endif
    *num_rowsp = num_rows;
    return 0;
}

// Do not take ydb lock or any other lock around or in this function.  
// If the engine is blocked because some thread is holding a lock, this function
// can help diagnose the problem.
// This function only collects information, and it does not matter if something gets garbled
// because of a race condition.  
// Note, engine status is still collected even if the environment or logger is panicked
static int
env_get_engine_status (DB_ENV * env, TOKU_ENGINE_STATUS_ROW engstat, uint64_t maxrows,  fs_redzone_state* redzone_state, uint64_t * env_panicp, char * env_panic_string_buf, int env_panic_string_length) {
    int r;

    if (env_panic_string_buf) {
	if (env && env->i && env->i->is_panicked && env->i->panic_string) {
	    strncpy(env_panic_string_buf, env->i->panic_string, env_panic_string_length);
	    env_panic_string_buf[env_panic_string_length - 1] = '\0';  // just in case
	}
	else 
	    *env_panic_string_buf = '\0';
    }

    if ( !(env)     || 
	 !(env->i)  || 
	 !(env_opened(env)) )
	r = EINVAL;
    else {
	r = 0;
	uint64_t row = 0;  // which row to fill next
        *env_panicp = env->i->is_panicked;

	{
	    YDB_LAYER_STATUS_S ydb_stat;
	    ydb_layer_get_status(&ydb_stat);
	    for (int i = 0; i < YDB_LAYER_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = ydb_stat.status[i];
	    }
	}
        {
            YDB_C_LAYER_STATUS_S ydb_c_stat;
            ydb_c_layer_get_status(&ydb_c_stat);
            for (int i = 0; i < YDB_C_LAYER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = ydb_c_stat.status[i];
            }
	}
        {
            YDB_WRITE_LAYER_STATUS_S ydb_write_stat;
            ydb_write_layer_get_status(&ydb_write_stat);
            for (int i = 0; i < YDB_WRITE_LAYER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = ydb_write_stat.status[i];
            }
	}
	{
	    YDB_LOCK_STATUS_S ydb_lock_status;
	    toku_ydb_lock_get_status(&ydb_lock_status);
	    for (int i = 0; i < YDB_LOCK_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = ydb_lock_status.status[i];
	    }
	}
        {
	    LE_STATUS_S lestat;                    // Rice's vampire
	    toku_le_get_status(&lestat);
	    for (int i = 0; i < LE_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = lestat.status[i];
	    }
        }
	{
            CHECKPOINT_STATUS_S cpstat;
            toku_checkpoint_get_status(env->i->cachetable, &cpstat);
	    for (int i = 0; i < CP_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = cpstat.status[i];
	    }
	}
	{
	    CACHETABLE_STATUS_S ctstat;
	    toku_cachetable_get_status(env->i->cachetable, &ctstat);
	    for (int i = 0; i < CT_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = ctstat.status[i];
	    }
	}
	{
	    LTM_STATUS_S ltmstat;
	    toku_ltm_get_status(env->i->ltm, &ltmstat);
	    for (int i = 0; i < LTM_STATUS_NUM_ROWS && row < maxrows; i++) {
		engstat[row++] = ltmstat.status[i];
	    }
	}
        {
            BRT_STATUS_S brtstat;
            toku_brt_get_status(&brtstat);
            for (int i = 0; i < BRT_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = brtstat.status[i];
            }
        }
        {
            BRT_FLUSHER_STATUS_S flusherstat;
            toku_brt_flusher_get_status(&flusherstat);
            for (int i = 0; i < BRT_FLUSHER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = flusherstat.status[i];
            }
        }
        {
            BRT_HOT_STATUS_S hotstat;
            toku_brt_hot_get_status(&hotstat);
            for (int i = 0; i < BRT_HOT_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = hotstat.status[i];
            }
        }
        {
            TXN_STATUS_S txnstat;
            toku_txn_get_status(env->i->logger, &txnstat);
            for (int i = 0; i < TXN_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = txnstat.status[i];
            }
        }
        {
            LOGGER_STATUS_S loggerstat;
            toku_logger_get_status(env->i->logger, &loggerstat);
            for (int i = 0; i < LOGGER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = loggerstat.status[i];
            }
        }

        {
            INDEXER_STATUS_S indexerstat;
            toku_indexer_get_status(&indexerstat);
            for (int i = 0; i < INDEXER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = indexerstat.status[i];
            }
        }
        {
            LOADER_STATUS_S loaderstat;
            toku_loader_get_status(&loaderstat);
            for (int i = 0; i < LOADER_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = loaderstat.status[i];
            }
        }

	{
            // memory_status is local to this file
	    memory_get_status();
            for (int i = 0; i < MEMORY_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = memory_status.status[i];
            }
	}
	{
            // Note, fs_get_status() and the fsstat structure are local to this file because they
            // are used to concentrate file system information collected from various places.
            fs_get_status(env, redzone_state);
            for (int i = 0; i < FS_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = fsstat.status[i];
            }
	}
#if 0
        // enable when upgrade is supported
	{
            for (int i = 0; i < PERSISTENT_UPGRADE_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = persistent_upgrade_status.status[i];
            }
            BRT_UPGRADE_STATUS_S brt_upgradestat;
	    toku_brt_upgrade_get_status(&brt_upgradestat);
            for (int i = 0; i < BRT_UPGRADE_STATUS_NUM_ROWS && row < maxrows; i++) {
                engstat[row++] = brt_upgradestat.status[i];
            }

	}
#endif
    }
    return r;
}


// Fill buff with text description of engine status up to bufsiz bytes.
// Intended for use by test programs that do not have the handlerton available,
// and for use by toku_assert logic to print diagnostic info on crash.
static int
env_get_engine_status_text(DB_ENV * env, char * buff, int bufsiz) {
    uint32_t stringsize = 1024;
    uint64_t panic;
    char panicstring[stringsize];
    int n = 0;  // number of characters printed so far
    uint64_t num_rows;
    fs_redzone_state redzone_state;

    n = snprintf(buff, bufsiz - n, "BUILD_ID = %d\n", BUILD_ID);

    (void) env_get_engine_status_num_rows (env, &num_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[num_rows];
    int r = env->get_engine_status (env, mystat, num_rows, &redzone_state, &panic, panicstring, stringsize);
    
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
        if (panic) {
            n += snprintf(buff + n, bufsiz - n, "Env panic code: %"PRIu64"\n", panic);
            if (strlen(panicstring)) {
                invariant(strlen(panicstring) <= stringsize);
                n += snprintf(buff + n, bufsiz - n, "Env panic string: %s\n", panicstring);
            }
        }

        for (uint64_t row = 0; row < num_rows; row++) {
            n += snprintf(buff + n, bufsiz - n, "%s: ", mystat[row].legend);
            switch (mystat[row].type) {
            case FS_STATE:
                n += snprintf(buff + n, bufsiz - n, "%"PRIu64"\n", mystat[row].value.num);
                break;
            case UINT64:
                n += snprintf(buff + n, bufsiz - n, "%"PRIu64"\n", mystat[row].value.num);
                break;
            case CHARSTR:
                n += snprintf(buff + n, bufsiz - n, "%s\n", mystat[row].value.str);
                break;
            case UNIXTIME:
                {
                    char tbuf[26];
                    format_time((time_t*)&mystat[row].value.num, tbuf);
                    n += snprintf(buff + n, bufsiz - n, "%s\n", tbuf);
                }
                break;
            case TOKUTIME:
                {
                    double t = tokutime_to_seconds(mystat[row].value.num);
                    n += snprintf(buff + n, bufsiz - n, "%.6f\n", t);
                }
                break;
            default:
                n += snprintf(buff + n, bufsiz - n, "UNKNOWN STATUS TYPE: %d\n", mystat[row].type);
                break;                
            }
        }
    }
        
    if (n > bufsiz) {
	char * errmsg = "BUFFER TOO SMALL\n";
	int len = strlen(errmsg) + 1;
	(void) snprintf(buff + (bufsiz - 1) - len, len, "%s", errmsg);
    }

    return r;
}

// intended for use by toku_assert logic, when env is not known
static int 
toku_maybe_get_engine_status_text (char * buff, int buffsize) {
    DB_ENV * env = most_recent_env;
    int r;
    if (engine_status_enable) {
	r = env_get_engine_status_text(env, buff, buffsize);
    }
    else {
	r = ENODATA;
	snprintf(buff, buffsize, "Engine status not available: disabled by user.  This should only happen in test programs.\n");
    }
    return r;
}

// Set panic code and panic string if not already panicked,
// intended for use by toku_assert when about to abort().
static void 
toku_maybe_set_env_panic(int code, char * msg) {
    if (code == 0) 
	code = -1;
    if (msg == NULL)
	msg = "Unknown cause from abort (failed assert)\n";
    env_is_panicked = code;  // disable library destructor no matter what
    DB_ENV * env = most_recent_env;
    if (env && 
	env->i &&
	(env->i->is_panicked == 0)) {
	env_panic(env, code, msg);
    }
}

// handlerton's call to fractal tree layer on failed assert in handlerton
static int 
env_crash(DB_ENV * UU(db_env), const char* msg, const char * fun, const char* file, int line, int caller_errno) {
    toku_do_assert_fail(msg, fun, file, line, caller_errno);
    return -1;  // placate compiler
}

static int 
toku_db_lt_panic(DB* db, int r) {
    assert(r!=0);
    assert(db && db->i && db->dbenv && db->dbenv->i);
    DB_ENV* env = db->dbenv;
    char * panic_string;

    if (r < 0) panic_string = toku_lt_strerror((TOKU_LT_ERROR)r);
    else       panic_string = "Error in locktree.\n";

    env_panic(env, r, panic_string);

    return toku_ydb_do_error(env, r, "%s", panic_string);
}

static int 
toku_env_create(DB_ENV ** envp, u_int32_t flags) {
    int r = ENOSYS;
    DB_ENV* result = NULL;

    engine_status_enable = 1;

    if (flags!=0)    { r = EINVAL; goto cleanup; }
    MALLOC(result);
    if (result == 0) { r = ENOMEM; goto cleanup; }
    memset(result, 0, sizeof *result);

    // locked methods
    result->err = (void (*)(const DB_ENV * env, int error, const char *fmt, ...)) toku_locked_env_err;
#define SENV(name) result->name = locked_env_ ## name
    SENV(dbremove);
    SENV(dbrename);
    SENV(set_default_bt_compare);
    SENV(set_update);
    SENV(set_generate_row_callback_for_put);
    SENV(set_generate_row_callback_for_del);
    SENV(checkpointing_set_period);
    SENV(checkpointing_get_period);
    SENV(cleaner_set_period);
    SENV(cleaner_get_period);
    SENV(cleaner_set_iterations);
    SENV(cleaner_get_iterations);
    SENV(open);
    SENV(close);
    SENV(log_flush);
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
    SENV(set_redzone);
    SENV(create_indexer);
    SENV(create_loader);
    SENV(get_lock_timeout);
    SENV(set_lock_timeout);
#undef SENV
    // methods with locking done internally
    result->put_multiple = env_put_multiple;
    result->del_multiple = env_del_multiple;
    result->update_multiple = env_update_multiple;
    
    // unlocked methods
    result->txn_checkpoint = toku_env_txn_checkpoint;
    result->checkpointing_postpone = env_checkpointing_postpone;
    result->checkpointing_resume = env_checkpointing_resume;
    result->checkpointing_begin_atomic_operation = env_checkpointing_begin_atomic_operation;
    result->checkpointing_end_atomic_operation = env_checkpointing_end_atomic_operation;
    result->get_engine_status_num_rows = env_get_engine_status_num_rows;
    result->get_engine_status = env_get_engine_status;
    result->get_engine_status_text = env_get_engine_status_text;
    result->crash = env_crash;  // handlerton's call to fractal tree layer on failed assert
    result->get_iname = env_get_iname;
    result->set_errcall = toku_env_set_errcall;
    result->set_errfile = toku_env_set_errfile;
    result->set_errpfx = toku_env_set_errpfx;
    result->txn_begin = locked_txn_begin;

    MALLOC(result->i);
    if (result->i == 0) { r = ENOMEM; goto cleanup; }
    memset(result->i, 0, sizeof *result->i);
    result->i->envdir_lockfd  = -1;
    result->i->datadir_lockfd = -1;
    result->i->logdir_lockfd  = -1;
    result->i->tmpdir_lockfd  = -1;
    env_init_open_txn(result);
    env_fs_init(result);

    result->i->bt_compare = toku_builtin_compare_fun;

    r = toku_logger_create(&result->i->logger);
    assert_zero(r);
    assert(result->i->logger);

    r = toku_ltm_create(&result->i->ltm,
                        __toku_env_default_locks_limit,
                        __toku_env_default_lock_memory_limit,
                        toku_db_lt_panic);
    assert_zero(r);
    assert(result->i->ltm);

    r = toku_omt_create(&result->i->open_dbs);
    assert_zero(r);
    assert(result->i->open_dbs);

    *envp = result;
    r = 0;
cleanup:
    if (r!=0) {
        if (result) {
            toku_free(result->i);
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

int 
log_compare(const DB_LSN * a, const DB_LSN * b) {
    toku_ydb_lock();
    fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
    assert(0);
    toku_ydb_unlock();
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
void
env_note_db_opened(DB_ENV *env, DB *db) {
    assert(db->i->dname);  // internal (non-user) dictionary has no dname
    assert(!db->i->is_zombie);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_open_dbs++;
    STATUS_VALUE(YDB_LAYER_NUM_OPEN_DBS) = env->i->num_open_dbs;
    STATUS_VALUE(YDB_LAYER_NUM_DB_OPEN)++;
    if (STATUS_VALUE(YDB_LAYER_NUM_OPEN_DBS) > STATUS_VALUE(YDB_LAYER_MAX_OPEN_DBS))
	STATUS_VALUE(YDB_LAYER_MAX_OPEN_DBS) = STATUS_VALUE(YDB_LAYER_NUM_OPEN_DBS);
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx);
    assert(r==DB_NOTFOUND); //Must not already be there.
    r = toku_omt_insert_at(env->i->open_dbs, db, idx);
    assert_zero(r);
}

void
env_note_db_closed(DB_ENV *env, DB *db) {
    assert(db->i->dname);
    assert(!db->i->is_zombie);
    assert(env->i->num_open_dbs);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_open_dbs--;
    STATUS_VALUE(YDB_LAYER_NUM_OPEN_DBS) = env->i->num_open_dbs;
    STATUS_VALUE(YDB_LAYER_NUM_DB_CLOSE)++;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx);
    assert_zero(r); //Must already be there.
    assert((DB*)dbv == db);
    r = toku_omt_delete_at(env->i->open_dbs, idx);
    assert_zero(r);
}

// Tell env that there is a new db handle (with non-unique dname in db->i-dname)
void
env_note_zombie_db(DB_ENV *env, DB *db) {
    assert(db->i->dname);  // internal (non-user) dictionary has no dname
    assert(db->i->is_zombie);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_zombie_dbs++;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx);
    assert(r==DB_NOTFOUND); //Must not already be there.
    r = toku_omt_insert_at(env->i->open_dbs, db, idx);
    assert_zero(r);
}

void
env_note_zombie_db_closed(DB_ENV *env, DB *db) {
    assert(db->i->dname);
    assert(db->i->is_zombie);
    assert(env->i->num_zombie_dbs);
    int r;
    OMTVALUE dbv;
    uint32_t idx;
    env->i->num_zombie_dbs--;
    r = toku_omt_find_zero(env->i->open_dbs, find_db_by_db, db, &dbv, &idx);
    assert_zero(r); //Must already be there.
    assert((DB*)dbv == db);
    r = toku_omt_delete_at(env->i->open_dbs, idx);
    assert_zero(r);
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
    r = toku_omt_find_zero(env->i->open_dbs, find_open_db_by_dname, (void*)dname, &dbv, &idx);
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
    r = toku_omt_find_zero(env->i->open_dbs, find_zombie_db_by_dname, (void*)dname, &dbv, &idx);
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

int
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
	r = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1, true);
	assert_zero(r);
    }

    // get iname
    r = toku_db_get(env->i->directory, child, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = iname_dbt.data;
    if (r==DB_NOTFOUND)
        r = ENOENT;
    else if (r==0) {
	// remove (dname,iname) from directory
	r = toku_db_del(env->i->directory, child, &dname_dbt, DB_DELETE_ANY, TRUE);
	if (r == 0) {
            if (using_txns) {
                r = toku_brt_remove_on_commit(db_txn_struct_i(child)->tokutxn, &iname_dbt);
		assert_zero(r);
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
		assert_zero(r);
            }
	}
    }

    if (using_txns) {
	// close txn
	if (r == 0) {  // commit
	    r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL, false);
	    invariant(r==0);  // TODO panic
	}
	else {         // abort
	    int r2 = toku_txn_abort(child, NULL, NULL, false);
	    invariant(r2==0);  // TODO panic
	}
    }

    if (iname) toku_free(iname);
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


int
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
	r = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1, true);
	assert_zero(r);
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
	    r = toku_db_del(env->i->directory, child, &old_dname_dbt, DB_DELETE_ANY, TRUE);
	    if (r == 0)
		r = toku_db_put(env->i->directory, child, &new_dname_dbt, &iname_dbt, 0, TRUE);
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
	    r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL, false);
	    invariant(r==0);  // TODO panic
	}
	else {         // abort
	    int r2 = toku_txn_abort(child, NULL, NULL, false);
	    invariant(r2==0);  // TODO panic
	}
    }

    if (iname) toku_free(iname);
    return r;

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
#error
#endif
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
// YDB lock is NOT held when this function is called,
// as it is called by user
static int 
env_get_iname(DB_ENV* env, DBT* dname_dbt, DBT* iname_dbt) {
    DB *directory = env->i->directory;
    int r = autotxn_db_get(directory, NULL, dname_dbt, iname_dbt, DB_SERIALIZABLE|DB_PRELOCKED); // allocates memory for iname
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
    assert_zero(r);
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

#undef STATUS_VALUE
#undef PERSISTENT_UPGRADE_STATUS_VALUE

#include <valgrind/helgrind.h>
void __attribute__((constructor)) toku_ydb_helgrind_ignore(void);
void
toku_ydb_helgrind_ignore(void) {
    VALGRIND_HG_DISABLE_CHECKING(&ydb_layer_status, sizeof ydb_layer_status);
}
