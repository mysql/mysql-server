#define MYSQL_SERVER 1
#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
#if defined(_WIN32)
#include "misc.h"
#endif
#include "toku_os.h"
}


/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif


#include <mysql/plugin.h>
#include "hatoku_hton.h"
#include "hatoku_defines.h"
#include "ha_tokudb.h"


#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION


static inline void *thd_data_get(THD *thd, int slot) {
#if MYSQL_VERSION_ID <= 50123
    return thd->ha_data[slot];
#else
    return thd->ha_data[slot].ha_ptr;
#endif
}

static inline void thd_data_set(THD *thd, int slot, void *data) {
#if MYSQL_VERSION_ID <= 50123
    thd->ha_data[slot] = data;
#else
    thd->ha_data[slot].ha_ptr = data;
#endif
}




static uchar *tokudb_get_key(TOKUDB_SHARE * share, size_t * length, my_bool not_used __attribute__ ((unused))) {
    *length = share->table_name_length;
    return (uchar *) share->table_name;
}

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root);
static MYSQL_THDVAR_BOOL(commit_sync, PLUGIN_VAR_THDLOCAL, "sync on txn commit", 
                         /* check */ NULL, /* update */ NULL, /* default*/ TRUE);

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer);
static void tokudb_cleanup_log_files(void);
static int tokudb_end(handlerton * hton, ha_panic_function type);
static bool tokudb_flush_logs(handlerton * hton);
static bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * print, enum ha_stat_type);
static int tokudb_close_connection(handlerton * hton, THD * thd);
static int tokudb_commit(handlerton * hton, THD * thd, bool all);
static int tokudb_rollback(handlerton * hton, THD * thd, bool all);
static uint tokudb_alter_table_flags(uint flags);
#if 0
static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_release_savepoint(handlerton * hton, THD * thd, void *savepoint);
#endif
static bool tokudb_show_logs(THD * thd, stat_print_fn * stat_print);
handlerton *tokudb_hton;

const char *ha_tokudb_ext = ".tokudb";
char *tokudb_data_dir;
ulong tokudb_debug;
DB_ENV *db_env;
HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;


//my_bool tokudb_shared_data = FALSE;
static u_int32_t tokudb_init_flags = 
    DB_CREATE | DB_THREAD | DB_PRIVATE | 
    DB_INIT_LOCK | 
    DB_INIT_MPOOL |
    DB_INIT_TXN | 
    0 | // disabled for 1.0.2 DB_INIT_LOG |
    DB_RECOVER;
static u_int32_t tokudb_env_flags = DB_LOG_AUTOREMOVE;
// static u_int32_t tokudb_lock_type = DB_LOCK_DEFAULT;
// static ulong tokudb_log_buffer_size = 0;
// static ulong tokudb_log_file_size = 0;
static ulonglong tokudb_cache_size = 0;
static char *tokudb_home;
// static char *tokudb_tmpdir;
static char *tokudb_log_dir;
// static long tokudb_lock_scan_time = 0;
// static ulong tokudb_region_size = 0;
// static ulong tokudb_cache_parts = 1;
static ulong tokudb_max_lock;
static const char tokudb_hton_name[] = "TokuDB";
static const int tokudb_hton_name_length = sizeof(tokudb_hton_name) - 1;
static u_int32_t tokudb_checkpointing_period;
#ifdef TOKUDB_VERSION
 char *tokudb_version = TOKUDB_VERSION;
#else
 char *tokudb_version;
#endif
struct st_mysql_storage_engine storage_engine_structure = { MYSQL_HANDLERTON_INTERFACE_VERSION };

extern "C" {
#include "ydb.h"
}

static int tokudb_init_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_init_func");
    int r;
#if defined(_WIN32)
    r = toku_ydb_init();
    if (r) {
        goto error;
    }
#endif

    tokudb_hton = (handlerton *) p;

    VOID(pthread_mutex_init(&tokudb_mutex, MY_MUTEX_INIT_FAST));
    (void) hash_init(&tokudb_open_tables, system_charset_info, 32, 0, 0, (hash_get_key) tokudb_get_key, 0, 0);

    tokudb_hton->state = SHOW_OPTION_YES;
    // tokudb_hton->flags= HTON_CAN_RECREATE;  // QQQ this came from skeleton
    tokudb_hton->flags = HTON_CLOSE_CURSORS_AT_COMMIT | HTON_FLUSH_AFTER_RENAME;
#ifdef DB_TYPE_TOKUDB
    tokudb_hton->db_type = DB_TYPE_TOKUDB;
#else
    tokudb_hton->db_type = DB_TYPE_UNKNOWN;
#endif

    tokudb_hton->create = tokudb_create_handler;
    tokudb_hton->close_connection = tokudb_close_connection;
#if 0
    tokudb_hton->savepoint_offset = sizeof(DB_TXN *);
    tokudb_hton->savepoint_set = tokudb_savepoint;
    tokudb_hton->savepoint_rollback = tokudb_rollback_to_savepoint;
    tokudb_hton->savepoint_release = tokudb_release_savepoint;
#endif
    tokudb_hton->commit = tokudb_commit;
    tokudb_hton->rollback = tokudb_rollback;
    tokudb_hton->panic = tokudb_end;
    tokudb_hton->flush_logs = tokudb_flush_logs;
    tokudb_hton->show_status = tokudb_show_status;
    tokudb_hton->alter_table_flags = tokudb_alter_table_flags;
#if 0
    if (!tokudb_tmpdir)
        tokudb_tmpdir = mysql_tmpdir;
    DBUG_PRINT("info", ("tokudb_tmpdir: %s", tokudb_tmpdir));
#endif
    if (!tokudb_home)
        tokudb_home = mysql_real_data_home;
    DBUG_PRINT("info", ("tokudb_home: %s", tokudb_home));
#if 0
    if (!tokudb_log_buffer_size) { // QQQ
        tokudb_log_buffer_size = max(table_cache_size * 512, 32 * 1024);
        DBUG_PRINT("info", ("computing tokudb_log_buffer_size %ld\n", tokudb_log_buffer_size));
    }
    tokudb_log_file_size = tokudb_log_buffer_size * 4;
    tokudb_log_file_size = MY_ALIGN(tokudb_log_file_size, 1024 * 1024L);
    tokudb_log_file_size = max(tokudb_log_file_size, 10 * 1024 * 1024L);
    DBUG_PRINT("info", ("computing tokudb_log_file_size: %ld\n", tokudb_log_file_size));
#endif
    if ((r = db_env_create(&db_env, 0))) {
        DBUG_PRINT("info", ("db_env_create %d\n", r));
        goto error;
    }

    DBUG_PRINT("info", ("tokudb_env_flags: 0x%x\n", tokudb_env_flags));
    r = db_env->set_flags(db_env, tokudb_env_flags, 1);
    if (r) { // QQQ
        if (tokudb_debug & TOKUDB_DEBUG_INIT) 
            TOKUDB_TRACE("%s:WARNING: flags=%x r=%d\n", __FUNCTION__, tokudb_env_flags, r); 
        // goto error;
    }

    // config error handling
    db_env->set_errcall(db_env, tokudb_print_error);
    db_env->set_errpfx(db_env, "TokuDB");

    // config directories
#if 0
    DBUG_PRINT("info", ("tokudb_tmpdir: %s\n", tokudb_tmpdir));
    db_env->set_tmp_dir(db_env, tokudb_tmpdir);
#endif

    {
    char *data_dir = tokudb_data_dir;
    if (data_dir == 0) 
        data_dir = mysql_data_home;
    DBUG_PRINT("info", ("tokudb_data_dir: %s\n", data_dir));
    db_env->set_data_dir(db_env, data_dir);
    }

    if (tokudb_log_dir) {
        DBUG_PRINT("info", ("tokudb_log_dir: %s\n", tokudb_log_dir));
        db_env->set_lg_dir(db_env, tokudb_log_dir);
    }

    // config the cache table size to min(1/2 of physical memory, 1/8 of the process address space)
    if (tokudb_cache_size == 0) {
        uint64_t physmem, maxdata;
        physmem = toku_os_get_phys_memory_size();
        tokudb_cache_size = physmem / 2;
        r = toku_os_get_max_process_data_size(&maxdata);
        if (r == 0) {
            if (tokudb_cache_size > maxdata / 8)
                tokudb_cache_size = maxdata / 8;
        }
    }
    if (tokudb_cache_size) {
        DBUG_PRINT("info", ("tokudb_cache_size: %lld\n", tokudb_cache_size));
        r = db_env->set_cachesize(db_env, (u_int32_t)(tokudb_cache_size >> 30), (u_int32_t)(tokudb_cache_size % (1024L * 1024L * 1024L)), 1);
        if (r) {
            DBUG_PRINT("info", ("set_cachesize %d\n", r));
            goto error; 
        }
    }
    u_int32_t gbytes, bytes; int parts;
    r = db_env->get_cachesize(db_env, &gbytes, &bytes, &parts);
    if (r == 0) 
        if (tokudb_debug & TOKUDB_DEBUG_INIT) 
            TOKUDB_TRACE("%s:tokudb_cache_size=%lld\n", __FUNCTION__, ((unsigned long long) gbytes << 30) + bytes);

#if 0
    // QQQ config the logs
    DBUG_PRINT("info", ("tokudb_log_file_size: %ld\n", tokudb_log_file_size));
    db_env->set_lg_max(db_env, tokudb_log_file_size);
    DBUG_PRINT("info", ("tokudb_log_buffer_size: %ld\n", tokudb_log_buffer_size));
    db_env->set_lg_bsize(db_env, tokudb_log_buffer_size);
    // DBUG_PRINT("info",("tokudb_region_size: %ld\n", tokudb_region_size));
    // db_env->set_lg_regionmax(db_env, tokudb_region_size);
#endif

    // config the locks
#if 0 // QQQ no lock types yet
    DBUG_PRINT("info", ("tokudb_lock_type: 0x%lx\n", tokudb_lock_type));
    db_env->set_lk_detect(db_env, tokudb_lock_type);
#endif
    if (tokudb_max_lock) {
        DBUG_PRINT("info",("tokudb_max_lock: %ld\n", tokudb_max_lock));
        r = db_env->set_lk_max_locks(db_env, tokudb_max_lock);
        if (r) {
            DBUG_PRINT("info", ("tokudb_set_max_locks %d\n", r));
            goto error;
        }
    }

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env open:flags=%x\n", __FUNCTION__, tokudb_init_flags);

    r = db_env->open(db_env, tokudb_home, tokudb_init_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env opened:return=%d\n", __FUNCTION__, r);

    if (r) {
        DBUG_PRINT("info", ("env->open %d\n", r));
        goto error;
    }

    r = db_env->checkpointing_set_period(db_env, tokudb_checkpointing_period);
    assert(!r);


    DBUG_RETURN(FALSE);

error:
    if (db_env) {
        db_env->close(db_env, 0);
        db_env = 0;
    }
    DBUG_RETURN(TRUE);
}

static int tokudb_done_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_done_func");
    int error = 0;

    if (tokudb_open_tables.records)
        error = 1;
    hash_free(&tokudb_open_tables);
    pthread_mutex_destroy(&tokudb_mutex);
#if defined(_WIN32)
    toku_ydb_destroy();
#endif
    TOKUDB_DBUG_RETURN(0);
}



static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root) {
    return new(mem_root) ha_tokudb(hton, table);
}

int tokudb_end(handlerton * hton, ha_panic_function type) {
    TOKUDB_DBUG_ENTER("tokudb_end");
    int error = 0;
    if (db_env) {
        if (tokudb_init_flags & DB_INIT_LOG)
            tokudb_cleanup_log_files();
        error = db_env->close(db_env, 0);       // Error is logged
        db_env = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_close_connection(handlerton * hton, THD * thd) {
    my_free(thd_data_get(thd, hton->slot), MYF(0));
    return 0;
}

bool tokudb_flush_logs(handlerton * hton) {
    TOKUDB_DBUG_ENTER("tokudb_flush_logs");
    int error;
    bool result = 0;
    u_int32_t curr_tokudb_checkpointing_period = 0;

    //
    // get the current checkpointing period
    //
    error = db_env->checkpointing_get_period(
        db_env, 
        &curr_tokudb_checkpointing_period
        );
    if (error) {
        my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
        result = 1;
        goto exit;
    }

    //
    // if the current period is not the same as the variable, that means
    // the user has changed the period and now we need to update it
    //
    if (tokudb_checkpointing_period != curr_tokudb_checkpointing_period) {
        error = db_env->checkpointing_set_period(
            db_env, 
            tokudb_checkpointing_period
            );
        if (error) {
            my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
            result = 1;
            goto exit;
        }
    }
    
    //
    // take the checkpoint
    //
    error = db_env->txn_checkpoint(db_env, 0, 0, 0);
    if (error) {
        my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
        result = 1;
        goto exit;
    }

    result = 0;
exit:
    TOKUDB_DBUG_RETURN(result);
}

static int tokudb_commit(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("tokudb_commit");
    DBUG_PRINT("trans", ("ending transaction %s", all ? "all" : "stmt"));
    u_int32_t syncflag = THDVAR(thd, commit_sync) ? 0 : DB_TXN_NOSYNC;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    int error = 0;
    if (*txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) 
            TOKUDB_TRACE("commit:%d:%p\n", all, *txn);
        error = (*txn)->commit(*txn, syncflag);
        if (*txn == trx->sp_level)
            trx->sp_level = 0;
        *txn = 0;
    } 
    else if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("commit0\n");
    }
    if (all) {
        trx->iso_level = hatoku_iso_not_set;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_rollback(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("tokudb_rollback");
    DBUG_PRINT("trans", ("aborting transaction %s", all ? "all" : "stmt"));
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    int error = 0;
    if (*txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN)
            TOKUDB_TRACE("rollback:%p\n", *txn);
        error = (*txn)->abort(*txn);
    if (*txn == trx->sp_level)
        trx->sp_level = 0;
    *txn = 0;
    } else
        if (tokudb_debug & TOKUDB_DEBUG_TXN) 
            TOKUDB_TRACE("abort0\n");
    TOKUDB_DBUG_RETURN(error);
}

#if 0

static int tokudb_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_savepoint");
    int error;
    DB_TXN **save_txn = (DB_TXN **) savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    if (!(error = db_env->txn_begin(db_env, trx->sp_level, save_txn, 0))) {
        trx->sp_level = *save_txn;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_rollback_to_savepoint");
    int error;
    DB_TXN *parent, **save_txn = (DB_TXN **) savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    parent = (*save_txn)->parent;
    if (!(error = (*save_txn)->abort(*save_txn))) {
        trx->sp_level = parent;
        error = tokudb_savepoint(hton, thd, savepoint);
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_release_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_release_savepoint");
    int error;
    DB_TXN *parent, **save_txn = (DB_TXN **) savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    parent = (*save_txn)->parent;
    if (!(error = (*save_txn)->commit(*save_txn, 0))) {
        trx->sp_level = parent;
        *save_txn = 0;
    }
    TOKUDB_DBUG_RETURN(error);
}

#endif

static bool tokudb_show_logs(THD * thd, stat_print_fn * stat_print) {
    TOKUDB_DBUG_ENTER("tokudb_show_logs");
    char **all_logs, **free_logs, **a, **f;
    int error = 1;
    MEM_ROOT **root_ptr = my_pthread_getspecific_ptr(MEM_ROOT **, THR_MALLOC);
    MEM_ROOT show_logs_root, *old_mem_root = *root_ptr;

    init_sql_alloc(&show_logs_root, BDB_LOG_ALLOC_BLOCK_SIZE, BDB_LOG_ALLOC_BLOCK_SIZE);
    *root_ptr = &show_logs_root;
    all_logs = free_logs = 0;

    error = db_env->log_archive(db_env, &all_logs, 0);
    if (error) {
        DBUG_PRINT("error", ("log_archive failed (error %d)", error));
        db_env->err(db_env, error, "log_archive");
        if (error == DB_NOTFOUND)
            error = 0;          // No log files
        goto err;
    }
    /* Error is 0 here */
    if (all_logs) {
        for (a = all_logs, f = free_logs; *a; ++a) {
            if (f && *f && strcmp(*a, *f) == 0) {
                f++;
                if ((error = stat_print(thd, tokudb_hton_name, tokudb_hton_name_length, *a, strlen(*a), STRING_WITH_LEN(SHOW_LOG_STATUS_FREE))))
                    break;
            } else {
                if ((error = stat_print(thd, tokudb_hton_name, tokudb_hton_name_length, *a, strlen(*a), STRING_WITH_LEN(SHOW_LOG_STATUS_INUSE))))
                    break;
            }
        }
    }
  err:
    if (all_logs)
        free(all_logs);
    if (free_logs)
        free(free_logs);
    free_root(&show_logs_root, MYF(0));
    *root_ptr = old_mem_root;
    TOKUDB_DBUG_RETURN(error);
}

bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * stat_print, enum ha_stat_type stat_type) {
    switch (stat_type) {
    case HA_ENGINE_LOGS:
        return tokudb_show_logs(thd, stat_print);
    default:
        return FALSE;
    }
}

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer) {
    sql_print_error("%s:  %s", db_errpfx, buffer);
}

void tokudb_cleanup_log_files(void) {
    TOKUDB_DBUG_ENTER("tokudb_cleanup_log_files");
    char **names;
    int error;

    if ((error = db_env->txn_checkpoint(db_env, 0, 0, 0)))
        my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);

    if ((error = db_env->log_archive(db_env, &names, 0)) != 0) {
        DBUG_PRINT("error", ("log_archive failed (error %d)", error));
        db_env->err(db_env, error, "log_archive");
        DBUG_VOID_RETURN;
    }

    if (names) {
        char **np;
        for (np = names; *np; ++np) {
#if 1
            if (tokudb_debug)
                TOKUDB_TRACE("%s:cleanup:%s\n", __FUNCTION__, *np);
#else
            my_delete(*np, MYF(MY_WME));
#endif
        }

        free(names);
    }

    DBUG_VOID_RETURN;
}

//
// *******NOTE*****
// If the flags HA_ONLINE_DROP_INDEX and HA_ONLINE_DROP_UNIQUE_INDEX
// are ever added, prepare_drop_index and final_drop_index will need to be modified
// so that the actual deletion of DB's is done in final_drop_index and not prepare_drop_index
//
static uint tokudb_alter_table_flags(uint flags)
{
    return (HA_ONLINE_ADD_INDEX_NO_WRITES| HA_ONLINE_DROP_INDEX_NO_WRITES |
            HA_ONLINE_ADD_UNIQUE_INDEX_NO_WRITES| HA_ONLINE_DROP_UNIQUE_INDEX_NO_WRITES);

}



// options flags
//   PLUGIN_VAR_THDLOCAL  Variable is per-connection
//   PLUGIN_VAR_READONLY  Server variable is read only
//   PLUGIN_VAR_NOSYSVAR  Not a server variable
//   PLUGIN_VAR_NOCMDOPT  Not a command line option
//   PLUGIN_VAR_NOCMDARG  No argument for cmd line
//   PLUGIN_VAR_RQCMDARG  Argument required for cmd line
//   PLUGIN_VAR_OPCMDARG  Argument optional for cmd line
//   PLUGIN_VAR_MEMALLOC  String needs memory allocated


// system variables

static MYSQL_SYSVAR_ULONGLONG(cache_size, tokudb_cache_size, PLUGIN_VAR_READONLY, "TokuDB cache table size", NULL, NULL, 0, 0, ~0LL, 0);

static MYSQL_SYSVAR_ULONG(max_lock, tokudb_max_lock, PLUGIN_VAR_READONLY, "TokuDB Max Locks", NULL, NULL, 8 * 1024, 0, ~0L, 0);

static MYSQL_SYSVAR_ULONG(debug, tokudb_debug, PLUGIN_VAR_READONLY, "TokuDB Debug", NULL, NULL, 0, 0, ~0L, 0);

static MYSQL_SYSVAR_STR(log_dir, tokudb_log_dir, PLUGIN_VAR_READONLY, "TokuDB Log Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(data_dir, tokudb_data_dir, PLUGIN_VAR_READONLY, "TokuDB Data Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(version, tokudb_version, PLUGIN_VAR_READONLY, "TokuDB Version", NULL, NULL, NULL);

static MYSQL_SYSVAR_UINT(init_flags, tokudb_init_flags, PLUGIN_VAR_READONLY, "Sets TokuDB DB_ENV->open flags", NULL, NULL, tokudb_init_flags, 0, ~0, 0);

static MYSQL_SYSVAR_UINT(checkpointing_period, tokudb_checkpointing_period, 0, "TokuDB Checkpointing period", NULL, NULL, 60, 0, ~0L, 0);
#if 0

static MYSQL_SYSVAR_ULONG(cache_parts, tokudb_cache_parts, PLUGIN_VAR_READONLY, "Sets TokuDB set_cache_parts", NULL, NULL, 0, 0, ~0L, 0);

// this is really a u_int32_t
// ? use MYSQL_SYSVAR_SET
static MYSQL_SYSVAR_UINT(env_flags, tokudb_env_flags, PLUGIN_VAR_READONLY, "Sets TokuDB env_flags", NULL, NULL, DB_LOG_AUTOREMOVE, 0, ~0, 0);

static MYSQL_SYSVAR_STR(home, tokudb_home, PLUGIN_VAR_READONLY, "Sets TokuDB env->open home", NULL, NULL, NULL);

// this is really a u_int32_t
//? use MYSQL_SYSVAR_SET

// this looks to be unused
static MYSQL_SYSVAR_LONG(lock_scan_time, tokudb_lock_scan_time, PLUGIN_VAR_READONLY, "Tokudb Lock Scan Time (UNUSED)", NULL, NULL, 0, 0, ~0L, 0);

// this is really a u_int32_t
//? use MYSQL_SYSVAR_ENUM
static MYSQL_SYSVAR_UINT(lock_type, tokudb_lock_type, PLUGIN_VAR_READONLY, "Sets set_lk_detect", NULL, NULL, DB_LOCK_DEFAULT, 0, ~0, 0);

static MYSQL_SYSVAR_ULONG(log_buffer_size, tokudb_log_buffer_size, PLUGIN_VAR_READONLY, "Tokudb Log Buffer Size", NULL, NULL, 0, 0, ~0L, 0);

static MYSQL_SYSVAR_ULONG(region_size, tokudb_region_size, PLUGIN_VAR_READONLY, "Tokudb Region Size", NULL, NULL, 128 * 1024, 0, ~0L, 0);

static MYSQL_SYSVAR_BOOL(shared_data, tokudb_shared_data, PLUGIN_VAR_READONLY, "Tokudb Shared Data", NULL, NULL, FALSE);

static MYSQL_SYSVAR_STR(tmpdir, tokudb_tmpdir, PLUGIN_VAR_READONLY, "Tokudb Tmp Dir", NULL, NULL, NULL);
#endif

static struct st_mysql_sys_var *tokudb_system_variables[] = {
    MYSQL_SYSVAR(cache_size),
    MYSQL_SYSVAR(max_lock),
    MYSQL_SYSVAR(data_dir),
    MYSQL_SYSVAR(log_dir),
    MYSQL_SYSVAR(debug),
    MYSQL_SYSVAR(commit_sync),
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(init_flags),
    MYSQL_SYSVAR(checkpointing_period),
#if 0
    MYSQL_SYSVAR(cache_parts),
    MYSQL_SYSVAR(env_flags),
    MYSQL_SYSVAR(home),
    MYSQL_SYSVAR(lock_scan_time),
    MYSQL_SYSVAR(lock_type),
    MYSQL_SYSVAR(log_buffer_size),
    MYSQL_SYSVAR(region_size),
    MYSQL_SYSVAR(shared_data),
    MYSQL_SYSVAR(tmpdir),
#endif
    NULL
};

mysql_declare_plugin(tokudb) {
    MYSQL_STORAGE_ENGINE_PLUGIN, 
    &storage_engine_structure, 
    "TokuDB", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine",
    PLUGIN_LICENSE_GPL,
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    0x0210,                    /* 2.1.0 */
    NULL,                      /* status variables */
    tokudb_system_variables,   /* system variables */
    NULL                       /* config options */
}
mysql_declare_plugin_end;

