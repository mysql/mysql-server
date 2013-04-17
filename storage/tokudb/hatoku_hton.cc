/* -*- mode: C; c-basic-offset: 4 -*- */
#define MYSQL_SERVER 1
#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
#if defined(_WIN32)
#include "misc.h"
#endif
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
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

#define TOKU_METADB_NAME "tokudb_meta"

static inline void *thd_data_get(THD *thd, int slot) {
    return thd->ha_data[slot].ha_ptr;
}

static inline void thd_data_set(THD *thd, int slot, void *data) {
    thd->ha_data[slot].ha_ptr = data;
}



static uchar *tokudb_get_key(TOKUDB_SHARE * share, size_t * length, my_bool not_used __attribute__ ((unused))) {
    *length = share->table_name_length;
    return (uchar *) share->table_name;
}

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root);
static MYSQL_THDVAR_BOOL(commit_sync, PLUGIN_VAR_THDLOCAL, "sync on txn commit", 
                         /* check */ NULL, /* update */ NULL, /* default*/ TRUE);
static MYSQL_THDVAR_ULONGLONG(write_lock_wait,
  0,
  "time waiting for write lock",
  NULL, 
  NULL, 
  5000, // default
  0, // min?
  ULONGLONG_MAX, // max
  1 // blocksize
  );

static MYSQL_THDVAR_ULONGLONG(read_lock_wait,
  0,
  "time waiting for read lock",
  NULL, 
  NULL, 
  4000, // default
  0, // min?
  ULONGLONG_MAX, // max
  1 // blocksize
  );
static MYSQL_THDVAR_UINT(pk_insert_mode,
  0,
  "set the primary key insert mode",
  NULL, 
  NULL, 
  1, // default
  0, // min?
  2, // max
  1 // blocksize
  );


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
handlerton *tokudb_hton;

const char *ha_tokudb_ext = ".tokudb";
char *tokudb_data_dir;
ulong tokudb_debug;
DB_ENV *db_env;
DB* metadata_db;
HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;
pthread_mutex_t tokudb_meta_mutex;


//my_bool tokudb_shared_data = FALSE;
static u_int32_t tokudb_init_flags = 
    DB_CREATE | DB_THREAD | DB_PRIVATE | 
    DB_INIT_LOCK | 
    DB_INIT_MPOOL |
    DB_INIT_TXN | 
    DB_INIT_LOG |
    DB_RECOVER;
static u_int32_t tokudb_env_flags = 0;
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
u_int32_t tokudb_write_status_frequency;
u_int32_t tokudb_read_status_frequency;
my_bool tokudb_prelock_empty;
#ifdef TOKUDB_VERSION
char *tokudb_version = (char*) TOKUDB_VERSION;
#else
 char *tokudb_version;
#endif
static int tokudb_fs_reserve_percent;  // file system reserve as a percentage of total disk space
struct st_mysql_storage_engine storage_engine_structure = { MYSQL_HANDLERTON_INTERFACE_VERSION };

#if defined(_WIN32)
extern "C" {
#include "ydb.h"
}
#endif

static int tokudb_init_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_init_func");
    int r;
#if defined(_WIN64)
        r = toku_ydb_init();
        if (r) {
            printf("got error %d\n", r);
            goto error;
        }
#endif
    db_env = NULL;
    metadata_db = NULL;

    tokudb_hton = (handlerton *) p;

    VOID(pthread_mutex_init(&tokudb_mutex, MY_MUTEX_INIT_FAST));
    VOID(pthread_mutex_init(&tokudb_meta_mutex, MY_MUTEX_INIT_FAST));
    (void) my_hash_init(&tokudb_open_tables, table_alias_charset, 32, 0, 0, (my_hash_get_key) tokudb_get_key, 0, 0);

    tokudb_hton->state = SHOW_OPTION_YES;
    // tokudb_hton->flags= HTON_CAN_RECREATE;  // QQQ this came from skeleton
    tokudb_hton->flags = HTON_CLOSE_CURSORS_AT_COMMIT;
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

    //
    // set default comparison functions
    //
    r = db_env->set_default_bt_compare(db_env, tokudb_cmp_dbt_key);
    if (r) {
        DBUG_PRINT("info", ("set_default_bt_compare%d\n", r));
        goto error; 
    }

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

    if (db_env->set_redzone) {
        r = db_env->set_redzone(db_env, tokudb_fs_reserve_percent);
        if (r && (tokudb_debug & TOKUDB_DEBUG_INIT))
            TOKUDB_TRACE("%s:%d r=%d\n", __FUNCTION__, __LINE__, r);
    }

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env open:flags=%x\n", __FUNCTION__, tokudb_init_flags);

    r = db_env->set_generate_row_callback_for_put(db_env,generate_row_for_put);
    assert(!r);

    r = db_env->open(db_env, tokudb_home, tokudb_init_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env opened:return=%d\n", __FUNCTION__, r);

    if (r) {
        DBUG_PRINT("info", ("env->open %d\n", r));
        goto error;
    }

    r = db_env->checkpointing_set_period(db_env, tokudb_checkpointing_period);
    assert(!r);

    r = db_create(&metadata_db, db_env, 0);
    if (r) {
        DBUG_PRINT("info", ("failed to create metadata db %d\n", r));
        goto error;
    }
    

    r= metadata_db->open(metadata_db, NULL, TOKU_METADB_NAME, NULL, DB_BTREE, DB_THREAD, 0);
    if (r) {
        if (r != ENOENT) {
            sql_print_error("Got error %d when trying to open metadata_db", r);
            goto error;
        }
        sql_print_warning("No metadata table exists, so creating it");
        r = metadata_db->close(metadata_db,0);
        assert(r == 0);
        r = db_create(&metadata_db, db_env, 0);
        if (r) {
            DBUG_PRINT("info", ("failed to create metadata db %d\n", r));
            goto error;
        }

        r= metadata_db->open(metadata_db, NULL, TOKU_METADB_NAME, NULL, DB_BTREE, DB_THREAD | DB_CREATE | DB_EXCL, my_umask);
        if (r) {
            goto error;
        }
    }


    DBUG_RETURN(FALSE);

error:
    if (metadata_db) {
        int rr = metadata_db->close(metadata_db, 0);
        assert(rr==0);
    }
    if (db_env) {
        int rr= db_env->close(db_env, 0);
        assert(rr==0);
        db_env = 0;
    }
    DBUG_RETURN(TRUE);
}

static int tokudb_done_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_done_func");
    int error = 0;

    if (tokudb_open_tables.records)
        error = 1;
    my_hash_free(&tokudb_open_tables);
    pthread_mutex_destroy(&tokudb_mutex);
    pthread_mutex_destroy(&tokudb_meta_mutex);
#if defined(_WIN64)
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
    if (metadata_db) {
        int r = metadata_db->close(metadata_db, 0);
        assert(r==0);
    }
    if (db_env) {
        if (tokudb_init_flags & DB_INIT_LOG)
            tokudb_cleanup_log_files();
        error = db_env->close(db_env, 0);       // Error is logged
        assert(error==0);
        db_env = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_close_connection(handlerton * hton, THD * thd) {
    int error = 0;
    tokudb_trx_data* trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (trx && trx->checkpoint_lock_taken) {
        error = db_env->checkpointing_resume(db_env);
    }
    my_free(trx, MYF(0));
    return error;
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

ulonglong get_write_lock_wait_time (THD* thd) {
    ulonglong ret_val = THDVAR(thd, write_lock_wait);
    return (ret_val == 0) ? ULONGLONG_MAX : ret_val;
}


ulonglong get_read_lock_wait_time (THD* thd) {
    ulonglong ret_val = THDVAR(thd, read_lock_wait);
    return (ret_val == 0) ? ULONGLONG_MAX : ret_val;
}

uint get_pk_insert_mode(THD* thd) {
    return THDVAR(thd, pk_insert_mode);
}


typedef struct txn_progress_info {
    char status[200];
    THD* thd;
} *TXN_PROGRESS_INFO;


void txn_progress_func(TOKU_TXN_PROGRESS progress, void* extra) {
    TXN_PROGRESS_INFO progress_info = (TXN_PROGRESS_INFO)extra;
    int r;
    if (progress->stalled_on_checkpoint) {
        r = sprintf(
            progress_info->status, 
            "Writing committed changes to disk, processed %"PRId64" out of %"PRId64, 
            progress->entries_processed, 
            progress->entries_total
            ); 
        assert(r >= 0);
    }
    else {
        r = sprintf(
            progress_info->status, 
            "processed %"PRId64" out of %"PRId64, 
            progress->entries_processed, 
            progress->entries_total
            ); 
        assert(r >= 0);
    }
    thd_proc_info(progress_info->thd, progress_info->status);
}


static void commit_txn_with_progress(DB_TXN* txn, u_int32_t flags, THD* thd) {
    int r;
    struct txn_progress_info info;
    info.thd = thd;
    r = txn->commit_with_progress(txn, flags, txn_progress_func, &info);
    if (r != 0) {
        sql_print_error("tried committing transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

static void abort_txn_with_progress(DB_TXN* txn, THD* thd) {
    int r;
    struct txn_progress_info info;
    info.thd = thd;
    r = txn->abort_with_progress(txn, txn_progress_func, &info);
    if (r != 0) {
        sql_print_error("tried aborting transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

static int tokudb_commit(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("tokudb_commit");
    DBUG_PRINT("trans", ("ending transaction %s", all ? "all" : "stmt"));
    u_int32_t syncflag = THDVAR(thd, commit_sync) ? 0 : DB_TXN_NOSYNC;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    if (*txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("commit:%d:%p\n", all, *txn);
        }
        commit_txn_with_progress(*txn, syncflag, thd);
        if (*txn == trx->sp_level) {
            trx->sp_level = 0;
        }
        *txn = 0;
    } 
    else if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("commit0\n");
    }
    reset_stmt_progress(&trx->stmt_progress);
    TOKUDB_DBUG_RETURN(0);
}

static int tokudb_rollback(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("tokudb_rollback");
    DBUG_PRINT("trans", ("aborting transaction %s", all ? "all" : "stmt"));
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    if (*txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("rollback:%p\n", *txn);
        }
        abort_txn_with_progress(*txn, thd);
        if (*txn == trx->sp_level) {
            trx->sp_level = 0;
        }
        *txn = 0;
    } 
    else {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("abort0\n");
        }
    }
    reset_stmt_progress(&trx->stmt_progress);
    TOKUDB_DBUG_RETURN(0);
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

static int smart_dbt_do_nothing (DBT const *key, DBT  const *row, void *context) {
  return 0;
}


static bool tokudb_show_data_size(THD * thd, stat_print_fn * stat_print, bool exact) {
    TOKUDB_DBUG_ENTER("tokudb_show_data_size");
    int error;
    u_int64_t num_bytes_in_db = 0;
    DB* curr_db = NULL;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBC* tmp_table_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    char data_amount_msg[50] = {0};
    memset(&curr_key, 0, sizeof curr_key); 
    memset(&curr_val, 0, sizeof curr_val);
    pthread_mutex_lock(&tokudb_meta_mutex);

    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error) {
        goto cleanup;
    }
    error = metadata_db->cursor(metadata_db, txn, &tmp_cursor, 0);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        //
        // here, and in other places, check if process has been killed
        // if so, get out of function so user is not stalled
        //
        if (thd->killed) {
            break;
        }
        
        //
        // do not need this to be super fast, so use old simple API
        //
        error = tmp_cursor->c_get(
            tmp_cursor, 
            &curr_key, 
            &curr_val, 
            DB_NEXT
            );
        if (!error) {
            char* name = (char *)curr_key.data;
            char* newname = NULL;
            u_int64_t curr_num_bytes = 0;
            DB_BTREE_STAT64 dict_stats;

            newname = (char *)my_malloc(
                get_max_dict_name_path_length(name),
                MYF(MY_WME|MY_ZEROFILL)
                );
            if (newname == NULL) { 
                error = ENOMEM;
                goto cleanup;
            }

            make_name(newname, name, "main");

            error = db_create(&curr_db, db_env, 0);
            if (error) { goto cleanup; }
            
            error = curr_db->open(curr_db, txn, newname, NULL, DB_BTREE, DB_THREAD, 0);
            if (error == ENOENT) { error = 0; continue; }
            if (error) { goto cleanup; }

            if (exact) {
                //
                // flatten if exact is required
                //
                uint curr_num_items = 0;                
                error = curr_db->cursor(curr_db, txn, &tmp_table_cursor, 0);
                if (error) {
                    tmp_table_cursor = NULL;
                    goto cleanup;
                }
                while (error != DB_NOTFOUND) {
                    error = tmp_table_cursor->c_getf_next(tmp_table_cursor, 0, smart_dbt_do_nothing, NULL);
                    if (error && error != DB_NOTFOUND) {
                        goto cleanup;
                    }
                    curr_num_items++;
                    //
                    // allow early exit if command has been killed
                    //
                    if ( (curr_num_items % 1000) == 0 && thd->killed) {
                        goto cleanup;
                    }
                }
                error = tmp_table_cursor->c_close(tmp_table_cursor);
                assert(error==0);
                tmp_table_cursor = NULL;
            }

            error = curr_db->stat64(
                curr_db, 
                txn, 
                &dict_stats
                );
            if (error) { goto cleanup; }

            curr_num_bytes = dict_stats.bt_dsize;
            if (*(uchar *)curr_val.data) {
                //
                // in this case, we have a hidden primary key, do not
                // want to report space taken up by the hidden primary key to the user
                //
                u_int64_t hpk_space = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH*dict_stats.bt_ndata;
                curr_num_bytes = (hpk_space > curr_num_bytes) ? 0 : curr_num_bytes - hpk_space;
            }
            else {
                //
                // one infinity byte per key needs to be subtracted
                //
                u_int64_t inf_byte_space = dict_stats.bt_ndata;
                curr_num_bytes = (inf_byte_space > curr_num_bytes) ? 0 : curr_num_bytes - inf_byte_space;
            }

            num_bytes_in_db += curr_num_bytes;

            {
                int r = curr_db->close(curr_db, 0);
                assert(r==0);
                curr_db = NULL;
            }
            my_free(newname,MYF(MY_ALLOW_ZERO_PTR));
        }
    }

    sprintf(data_amount_msg, "Number of bytes in database: %" PRIu64, num_bytes_in_db);
    stat_print(
        thd, 
        tokudb_hton_name, 
        tokudb_hton_name_length, 
        "Data in tables", 
        strlen("Data in tables"), 
        data_amount_msg,
        strlen(data_amount_msg)
        );

    error = 0;

cleanup:
    if (curr_db) {
        int r = curr_db->close(curr_db, 0);
        assert(r==0);
    }
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
    }
    if (tmp_table_cursor) {
        int r = tmp_table_cursor->c_close(tmp_table_cursor);
        assert(r==0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    if (error) {
        sql_print_error("got an error %d in show_data_size\n", error);
    }
    pthread_mutex_unlock(&tokudb_meta_mutex);
    if (error) { my_errno = error; }
    TOKUDB_DBUG_RETURN(error);
}


#define STATPRINT(legend, val) stat_print(thd, \
                                          tokudb_hton_name, \
                                          tokudb_hton_name_length, \
                                          legend, \
                                          strlen(legend), \
                                          val, \
                                          strlen(val))

static bool tokudb_show_engine_status(THD * thd, stat_print_fn * stat_print) {
    TOKUDB_DBUG_ENTER("tokudb_show_engine_status");
    int error;
    const int bufsiz = 1024;
    char buf[bufsiz] = {'\0'};

    ENGINE_STATUS engstat;

    error = db_env->get_engine_status(db_env, &engstat);
    if (error == 0) {
      if(engstat.enospc_threads_blocked) {
	  STATPRINT("*** URGENT WARNING ***", "FILE SYSTEM IS COMPLETELY FULL");
	  snprintf(buf, bufsiz, "FILE SYSTEM IS COMPLETELY FULL");
      }
      else if (engstat.enospc_seal_state == 0) {
	  snprintf(buf, bufsiz, "more than %d percent of total file system space", 2*tokudb_fs_reserve_percent);
      }
      else if (engstat.enospc_seal_state == 1) {
	  snprintf(buf, bufsiz, "*** WARNING *** FILE SYSTEM IS GETTING FULL (less than %d percent free)", 2*tokudb_fs_reserve_percent);
      } 
      else if (engstat.enospc_seal_state == 2){
	  snprintf(buf, bufsiz, "*** WARNING *** FILE SYSTEM IS GETTING VERY FULL (less than %d percent free): INSERTS ARE PROHIBITED", tokudb_fs_reserve_percent);
      }
      else {
	  snprintf(buf, bufsiz, "information unavailable %" PRIu64, engstat.enospc_seal_state);
      }
      STATPRINT ("disk free space", buf);

      STATPRINT("time now", engstat.now);

      const char * lockstat = (engstat.ydb_lock_ctr & 0x01) ? "Locked" : "Unlocked";
      u_int64_t lockctr     =  engstat.ydb_lock_ctr >> 1;   // lsb indicates if locked
      snprintf(buf, bufsiz, "%" PRIu64, lockctr);  
      STATPRINT("ydb lock", lockstat);
      STATPRINT("ydb lock counter", buf);

      snprintf(buf, bufsiz, "%" PRIu64, engstat.max_possible_sleep);  
      STATPRINT("max_possible_sleep (microseconds)", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.processor_freq_mhz);  
      STATPRINT("processor_freq_mhz", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.max_requested_sleep);  
      STATPRINT("max_requested_sleep", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.times_max_sleep_used);  
      STATPRINT("times_max_sleep_used", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.total_sleepers);  
      STATPRINT("total_sleepers", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.total_sleep_time);  
      STATPRINT("total_sleep_time", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.max_waiters);  
      STATPRINT("max_waiters", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.total_waiters);  
      STATPRINT("total_waiters", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.total_clients);  
      STATPRINT("total_clients", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.time_ydb_lock_held_unavailable);  
      STATPRINT("time_ydb_lock_held_unavailable", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.max_time_ydb_lock_held);  
      STATPRINT("max_time_ydb_lock_held", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.total_time_ydb_lock_held);  
      STATPRINT("total_time_ydb_lock_held", buf);

      snprintf(buf, bufsiz, "%" PRIu32, engstat.checkpoint_period);
      STATPRINT("checkpoint period", buf);
      snprintf(buf, bufsiz, "%" PRIu32, engstat.checkpoint_footprint);
      STATPRINT("checkpoint status code (0 = idle)", buf);
      STATPRINT("last checkpoint began ", engstat.checkpoint_time_begin);
      STATPRINT("last complete checkpoint began ", engstat.checkpoint_time_begin_complete);
      STATPRINT("last complete checkpoint ended ", engstat.checkpoint_time_end);

      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_lock_taken);  
      STATPRINT("cachetable lock taken", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_lock_released);  
      STATPRINT("cachetable lock released", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_hit);  
      STATPRINT("cachetable hit", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_miss);  
      STATPRINT("cachetable miss", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_misstime);  
      STATPRINT("cachetable misstime", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_waittime);  
      STATPRINT("cachetable waittime", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_wait_reading);  
      STATPRINT("cachetable wait reading", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_wait_writing);  
      STATPRINT("cachetable wait writing", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.puts);  
      STATPRINT("cachetable puts (new node)", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.prefetches);  
      STATPRINT("cachetable prefetches", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.maybe_get_and_pins);  
      STATPRINT("cachetable maybe_get_and_pins", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.maybe_get_and_pin_hits);  
      STATPRINT("cachetable maybe_get_and_pin_hits", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_size_current);  
      STATPRINT("cachetable size_current", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_size_limit);  
      STATPRINT("cachetable size_limit", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.cachetable_size_writing);  
      STATPRINT("cachetable size_writing", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.get_and_pin_footprint);  
      STATPRINT("cachetable get_and_pin_footprint", buf);

      snprintf(buf, bufsiz, "%" PRIu32, engstat.range_locks_max);
      STATPRINT("max range locks", buf);
      snprintf(buf, bufsiz, "%" PRIu32, engstat.range_locks_max_per_index);
      STATPRINT("max range locks per index", buf);
      snprintf(buf, bufsiz, "%" PRIu32, engstat.range_locks_curr);
      STATPRINT("range locks in use", buf);
      snprintf(buf, bufsiz, "%" PRIu32, engstat.range_lock_escalation_successes);
      STATPRINT("range lock escalation successes", buf);
      snprintf(buf, bufsiz, "%" PRIu32, engstat.range_lock_escalation_failures);
      STATPRINT("range lock escalation failures", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.inserts);
      STATPRINT("dictionary inserts", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.deletes);
      STATPRINT("dictionary deletes", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.point_queries);
      STATPRINT("dictionary point queries", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.sequential_queries);
      STATPRINT("dictionary sequential queries", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.commits);
      STATPRINT("txn commits", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.aborts);
      STATPRINT("txn aborts", buf);

      snprintf(buf, bufsiz, "%" PRIu64, engstat.fsync_count);
      STATPRINT("fsync count", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.fsync_time);
      STATPRINT("fsync time", buf);

      snprintf(buf, bufsiz, "%" PRIu64, engstat.logger_ilock_ctr);
      STATPRINT("logger ilock count", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.logger_olock_ctr);
      STATPRINT("logger olock count", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.logger_swap_ctr);
      STATPRINT("logger swap count", buf);

      STATPRINT("most recent disk full", engstat.enospc_most_recent);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.enospc_threads_blocked);
      STATPRINT("threads currently blocked by full disk", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.enospc_total);
      STATPRINT("ENOSPC blocked count", buf);
      snprintf(buf, bufsiz, "%" PRIu64, engstat.enospc_seal_ctr);
      STATPRINT("ENOSPC reserve count", buf);
    }
    if (error) { my_errno = error; }
    TOKUDB_DBUG_RETURN(error);
}


int tokudb_checkpoint_lock(THD * thd, stat_print_fn * stat_print) {
    int error;
    tokudb_trx_data* trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        error = create_tokudb_trx_data_instance(&trx);
        if (error) { goto cleanup; }
        thd_data_set(thd, tokudb_hton->slot, trx);
    }
    
    if (trx->checkpoint_lock_taken) {
        STATPRINT("checkpoint lock", "Lock already taken");
        error = 0;
        goto cleanup;
    }
    error = db_env->checkpointing_postpone(db_env);
    if (error) { goto cleanup; }

    trx->checkpoint_lock_taken = true;
    STATPRINT("checkpoint lock", "Lock successfully taken");
    error = 0;
    
cleanup:
    if (error) { my_errno = error; }
    return error;
}

int tokudb_checkpoint_unlock(THD * thd, stat_print_fn * stat_print) {
    int error;
    tokudb_trx_data* trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        error = 0;
        STATPRINT("checkpoint unlock", "Lock never taken");
        goto  cleanup;
    }
    if (!trx->checkpoint_lock_taken) {
        error = 0;
        STATPRINT("checkpoint unlock", "Lock never taken");
        goto  cleanup;
    }
    //
    // at this point, we know the checkpoint lock has been taken
    //
    error = db_env->checkpointing_resume(db_env);
    if (error) {goto cleanup;}

    trx->checkpoint_lock_taken = false;
    STATPRINT("checkpoint unlock", "Successfully unlocked");
    
cleanup:
    if (error) { my_errno = error; }
    return error;
}




bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * stat_print, enum ha_stat_type stat_type) {
    switch (stat_type) {
    case HA_ENGINE_DATA_AMOUNT:
        return tokudb_show_data_size(thd, stat_print, false);
        break;
    case HA_ENGINE_DATA_EXACT_AMOUNT:
        return tokudb_show_data_size(thd, stat_print, true);
        break;
    case HA_ENGINE_STATUS:
        return tokudb_show_engine_status(thd, stat_print);
        break;
    case HA_ENGINE_CHECKPOINT_LOCK:
        return tokudb_checkpoint_lock(thd, stat_print);
        break;
    case HA_ENGINE_CHECKPOINT_UNLOCK:
        return tokudb_checkpoint_unlock(thd, stat_print);
        break;
    default:
        break;
    }
    return FALSE;
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
static MYSQL_SYSVAR_BOOL(prelock_empty, tokudb_prelock_empty, 0, "Tokudb Prelock Empty Table", NULL, NULL, TRUE);
static MYSQL_SYSVAR_UINT(write_status_frequency, tokudb_write_status_frequency, 0, "TokuDB frequency that show processlist updates status of writes", NULL, NULL, 1000, 0, ~0L, 0);
static MYSQL_SYSVAR_UINT(read_status_frequency, tokudb_read_status_frequency, 0, "TokuDB frequency that show processlist updates status of reads", NULL, NULL, 10000, 0, ~0L, 0);
static MYSQL_SYSVAR_INT(fs_reserve_percent, tokudb_fs_reserve_percent, PLUGIN_VAR_READONLY, "TokuDB file system space reserve (percent free required)", NULL, NULL, 5, 0, 100, 0);
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
    MYSQL_SYSVAR(write_lock_wait),
    MYSQL_SYSVAR(read_lock_wait),
    MYSQL_SYSVAR(pk_insert_mode),
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(init_flags),
    MYSQL_SYSVAR(checkpointing_period),
    MYSQL_SYSVAR(prelock_empty),
    MYSQL_SYSVAR(write_status_frequency),
    MYSQL_SYSVAR(read_status_frequency),
    MYSQL_SYSVAR(fs_reserve_percent),
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
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    0x0210,                    /* 2.1.0 */
    NULL,                      /* status variables */
    tokudb_system_variables,   /* system variables */
    NULL                       /* config options */
}
mysql_declare_plugin_end;

