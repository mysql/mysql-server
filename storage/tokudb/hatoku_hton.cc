/* -*- mode: C; c-basic-offset: 4 -*- */
#define MYSQL_SERVER 1
#include "hatoku_defines.h"
#include <db.h>

#include "stdint.h"
#if defined(_WIN32)
#include "misc.h"
#endif
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "toku_os.h"
#include "toku_time.h"
#include "partitioned_counter.h"

/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif

#include <mysql/plugin.h>
#include "hatoku_hton.h"
#include "ha_tokudb.h"

#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION

#define TOKU_METADB_NAME "tokudb_meta"

typedef struct savepoint_info {
    DB_TXN* txn;
    tokudb_trx_data* trx;
    bool in_sub_stmt;
} *SP_INFO, SP_INFO_T;

static uchar *tokudb_get_key(TOKUDB_SHARE * share, size_t * length, my_bool not_used __attribute__ ((unused))) {
    *length = share->table_name_length;
    return (uchar *) share->table_name;
}

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root);

static MYSQL_THDVAR_BOOL(commit_sync, 
    PLUGIN_VAR_THDLOCAL, 
    "sync on txn commit",
    /* check */ NULL, 
    /* update */ NULL,
    /* default*/ true
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
static MYSQL_THDVAR_BOOL(load_save_space,
    0,
    "if on, intial loads are slower but take less space",
    NULL, 
    NULL, 
    false
);
static MYSQL_THDVAR_BOOL(disable_slow_alter,
    0,
    "if on, alter tables that require copy are disabled",
    NULL, 
    NULL, 
    false
);
static MYSQL_THDVAR_BOOL(disable_hot_alter,
    0,
    "if on, hot alter table is disabled",
    NULL, 
    NULL, 
    false
);
static MYSQL_THDVAR_BOOL(create_index_online,
    0,
    "if on, create index done online",
    NULL, 
    NULL, 
    true
);
static MYSQL_THDVAR_BOOL(disable_prefetching,
    0,
    "if on, prefetching disabled",
    NULL, 
    NULL, 
   false
);
static MYSQL_THDVAR_BOOL(prelock_empty,
    0,
    "Tokudb Prelock Empty Table",
    NULL, 
    NULL, 
    true
);
static MYSQL_THDVAR_BOOL(log_client_errors,
    0,
    "Tokudb Log Client Errors",
    NULL, 
    NULL, 
    false
);
static MYSQL_THDVAR_UINT(block_size,
    0,
    "fractal tree block size",
    NULL, 
    NULL, 
    4<<20, // default
    4096,  // min
    ~0U,   // max
    1      // blocksize???
);
static MYSQL_THDVAR_UINT(read_block_size,
    0,
    "fractal tree read block size",
    NULL, 
    NULL, 
    128*1024, // default
    4096,  // min
    ~0U,   // max
    1      // blocksize???
);
static MYSQL_THDVAR_UINT(read_buf_size,
    0,
    "fractal tree read block size", //TODO: Is this a typo?
    NULL, 
    NULL, 
    128*1024, // default
    0,  // min
    1*1024*1024,   // max
    1      // blocksize???
);
#if TOKU_INCLUDE_UPSERT
static MYSQL_THDVAR_BOOL(disable_slow_update, 
    PLUGIN_VAR_THDLOCAL, 
    "disable slow update",
    NULL, // check
    NULL, // update
    false // default
);
static MYSQL_THDVAR_BOOL(disable_slow_upsert, 
    PLUGIN_VAR_THDLOCAL, 
    "disable slow upsert",
    NULL, // check
    NULL, // update
    false // default
);
#endif

static void tokudb_checkpoint_lock(THD * thd);
static void tokudb_checkpoint_unlock(THD * thd);

static void
tokudb_checkpoint_lock_update(
    THD* thd,
    struct st_mysql_sys_var* var,
    void* var_ptr,
    const void* save) 
{
    my_bool* val = (my_bool *) var_ptr;
    *val= *(my_bool *) save ? true : false;
    if (*val) {
        tokudb_checkpoint_lock(thd);
    }
    else {
        tokudb_checkpoint_unlock(thd);
    }
}
  
static MYSQL_THDVAR_BOOL(checkpoint_lock,
    0,
    "Tokudb Checkpoint Lock",
    NULL, 
    tokudb_checkpoint_lock_update, 
    false
);

static const char *tokudb_row_format_names[] = {
    "tokudb_uncompressed",
    "tokudb_zlib",
    "tokudb_quicklz",
    "tokudb_lzma",
    "tokudb_fast",
    "tokudb_small",
    "tokudb_default",
    NullS
};

static TYPELIB tokudb_row_format_typelib = {
    array_elements(tokudb_row_format_names) - 1,
    "tokudb_row_format_typelib",
    tokudb_row_format_names,
    NULL
};

static MYSQL_THDVAR_ENUM(row_format, PLUGIN_VAR_OPCMDARG,
                         "Specifies the compression method for a table during this session. "
                         "Possible values are TOKUDB_UNCOMPRESSED, TOKUDB_ZLIB, TOKUDB_QUICKLZ, "
                         "TOKUDB_LZMA, TOKUDB_FAST, TOKUDB_SMALL and TOKUDB_DEFAULT",
                         NULL, NULL, SRV_ROW_FORMAT_DEFAULT, &tokudb_row_format_typelib);

srv_row_format_t get_row_format(THD *thd)
{
    return (srv_row_format_t) THDVAR(thd, row_format);
}

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer);
static void tokudb_cleanup_log_files(void);
static int tokudb_end(handlerton * hton, ha_panic_function type);
static bool tokudb_flush_logs(handlerton * hton);
static bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * print, enum ha_stat_type);
static int tokudb_close_connection(handlerton * hton, THD * thd);
static int tokudb_commit(handlerton * hton, THD * thd, bool all);
static int tokudb_rollback(handlerton * hton, THD * thd, bool all);
#if TOKU_INCLUDE_XA
static int tokudb_xa_prepare(handlerton* hton, THD* thd, bool all);
static int tokudb_xa_recover(handlerton* hton, XID*  xid_list, uint  len);
static int tokudb_commit_by_xid(handlerton* hton, XID* xid);
static int tokudb_rollback_by_xid(handlerton* hton, XID*  xid);
#endif

#if defined(HA_INPLACE_ADD_INDEX_NO_READ_WRITE)
static uint tokudb_alter_table_flags(uint flags);
#endif
static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_release_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_discover(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen);
static int tokudb_discover2(handlerton *hton, THD* thd, const char *db, const char *name, bool translate_name,uchar **frmblob, size_t *frmlen);
handlerton *tokudb_hton;

const char *ha_tokudb_ext = ".tokudb";
char *tokudb_data_dir;
ulong tokudb_debug;
DB_ENV *db_env;
DB* metadata_db;
HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;
pthread_mutex_t tokudb_meta_mutex;

static PARTITIONED_COUNTER tokudb_primary_key_bytes_inserted;
void toku_hton_update_primary_key_bytes_inserted(uint64_t row_size) {
    increment_partitioned_counter(tokudb_primary_key_bytes_inserted, row_size);
}

static ulonglong tokudb_lock_timeout;
static ulong tokudb_cleaner_period;
static ulong tokudb_cleaner_iterations;

#define ASSERT_MSGLEN 1024

void toku_hton_assert_fail(const char* expr_as_string, const char * fun, const char * file, int line, int caller_errno) {
    char msg[ASSERT_MSGLEN];
    if (db_env) {
        snprintf(msg, ASSERT_MSGLEN, "Handlerton: %s ", expr_as_string);
        db_env->crash(db_env, msg, fun, file, line,caller_errno);
    }
    else {
        snprintf(msg, ASSERT_MSGLEN, "Handlerton assertion failed, no env, %s, %d, %s, %s (errno=%d)\n", file, line, fun, expr_as_string, caller_errno);
        perror(msg);
        fflush(stderr);
    }
    abort();
}

//my_bool tokudb_shared_data = false;
static uint32_t tokudb_init_flags = 
    DB_CREATE | DB_THREAD | DB_PRIVATE | 
    DB_INIT_LOCK | 
    DB_INIT_MPOOL |
    DB_INIT_TXN | 
    DB_INIT_LOG |
    DB_RECOVER;
static uint32_t tokudb_env_flags = 0;
// static uint32_t tokudb_lock_type = DB_LOCK_DEFAULT;
// static ulong tokudb_log_buffer_size = 0;
// static ulong tokudb_log_file_size = 0;
static my_bool tokudb_directio = FALSE;
static ulonglong tokudb_cache_size = 0;
static ulonglong tokudb_max_lock_memory = 0;
static char *tokudb_home;
static char *tokudb_tmp_dir;
static char *tokudb_log_dir;
// static long tokudb_lock_scan_time = 0;
// static ulong tokudb_region_size = 0;
// static ulong tokudb_cache_parts = 1;
const char *tokudb_hton_name = "TokuDB";
static uint32_t tokudb_checkpointing_period;
uint32_t tokudb_write_status_frequency;
uint32_t tokudb_read_status_frequency;
#ifdef TOKUDB_VERSION
char *tokudb_version = (char*) TOKUDB_VERSION;
#else
char *tokudb_version;
#endif
static int tokudb_fs_reserve_percent;  // file system reserve as a percentage of total disk space

#if defined(_WIN32)
extern "C" {
#include "ydb.h"
}
#endif

// A flag set if the handlerton is in an initialized, usable state,
// plus a reader-write lock to protect it without serializing reads.
// Since we don't have static initializers for the opaque rwlock type,
// use constructor and destructor functions to create and destroy
// the lock before and after main(), respectively.
static int tokudb_hton_initialized;
static rw_lock_t tokudb_hton_initialized_lock;

static void create_tokudb_hton_intialized_lock(void)  __attribute__((constructor));
static void destroy_tokudb_hton_initialized_lock(void) __attribute__((destructor));

static void create_tokudb_hton_intialized_lock(void)
{
    my_rwlock_init(&tokudb_hton_initialized_lock, 0);
}

static void destroy_tokudb_hton_initialized_lock(void)
{
    rwlock_destroy(&tokudb_hton_initialized_lock);
}

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

    // 3938: lock the handlerton's initialized status flag for writing
    r = rw_wrlock(&tokudb_hton_initialized_lock);
    assert(r == 0);

    db_env = NULL;
    metadata_db = NULL;

    tokudb_hton = (handlerton *) p;

    pthread_mutex_init(&tokudb_mutex, MY_MUTEX_INIT_FAST);
    pthread_mutex_init(&tokudb_meta_mutex, MY_MUTEX_INIT_FAST);
    (void) my_hash_init(&tokudb_open_tables, table_alias_charset, 32, 0, 0, (my_hash_get_key) tokudb_get_key, 0, 0);

    tokudb_hton->state = SHOW_OPTION_YES;
    // tokudb_hton->flags= HTON_CAN_RECREATE;  // QQQ this came from skeleton
    tokudb_hton->flags = HTON_CLOSE_CURSORS_AT_COMMIT;
#if DB_TYPE_TOKUDB_DEFINED
    tokudb_hton->db_type = DB_TYPE_TOKUDB;
#else
    tokudb_hton->db_type = DB_TYPE_UNKNOWN;
#endif

    tokudb_hton->create = tokudb_create_handler;
    tokudb_hton->close_connection = tokudb_close_connection;

    tokudb_hton->savepoint_offset = sizeof(SP_INFO_T);
    tokudb_hton->savepoint_set = tokudb_savepoint;
    tokudb_hton->savepoint_rollback = tokudb_rollback_to_savepoint;
    tokudb_hton->savepoint_release = tokudb_release_savepoint;

    tokudb_hton->discover = tokudb_discover;
#if defined(MYSQL_HANDLERTON_INCLUDE_DISCOVER2)
    tokudb_hton->discover2 = tokudb_discover2;
#endif
    tokudb_hton->commit = tokudb_commit;
    tokudb_hton->rollback = tokudb_rollback;
#if TOKU_INCLUDE_XA
    tokudb_hton->prepare=tokudb_xa_prepare;
    tokudb_hton->recover=tokudb_xa_recover;
    tokudb_hton->commit_by_xid=tokudb_commit_by_xid;
    tokudb_hton->rollback_by_xid=tokudb_rollback_by_xid;
#endif

    tokudb_hton->panic = tokudb_end;
    tokudb_hton->flush_logs = tokudb_flush_logs;
    tokudb_hton->show_status = tokudb_show_status;
#if defined(HA_INPLACE_ADD_INDEX_NO_READ_WRITE)
    tokudb_hton->alter_table_flags = tokudb_alter_table_flags;
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

    {
    char *tmp_dir = tokudb_tmp_dir;
    char *data_dir = tokudb_data_dir;
    if (data_dir == 0) {
        data_dir = mysql_data_home;
    }
    if (tmp_dir == 0) {
        tmp_dir = data_dir;
    }
    DBUG_PRINT("info", ("tokudb_data_dir: %s\n", data_dir));
    db_env->set_data_dir(db_env, data_dir);

    DBUG_PRINT("info", ("tokudb_tmp_dir: %s\n", tmp_dir));
    db_env->set_tmp_dir(db_env, tmp_dir);
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
        r = db_env->set_cachesize(db_env, (uint32_t)(tokudb_cache_size >> 30), (uint32_t)(tokudb_cache_size % (1024L * 1024L * 1024L)), 1);
        if (r) {
            DBUG_PRINT("info", ("set_cachesize %d\n", r));
            goto error; 
        }
    }
    if (tokudb_max_lock_memory == 0) {
        tokudb_max_lock_memory = tokudb_cache_size/8;
    }
    if (tokudb_max_lock_memory) {
        DBUG_PRINT("info", ("tokudb_max_lock_memory: %lld\n", tokudb_max_lock_memory));
        r = db_env->set_lk_max_memory(db_env, tokudb_max_lock_memory);
        if (r) {
            DBUG_PRINT("info", ("set_lk_max_memory %d\n", r));
            goto error; 
        }
    }
    
    uint32_t gbytes, bytes; int parts;
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

    if (db_env->set_redzone) {
        r = db_env->set_redzone(db_env, tokudb_fs_reserve_percent);
        if (r && (tokudb_debug & TOKUDB_DEBUG_INIT))
            TOKUDB_TRACE("%s:%d r=%d\n", __FUNCTION__, __LINE__, r);
    }

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env open:flags=%x\n", __FUNCTION__, tokudb_init_flags);

    r = db_env->set_generate_row_callback_for_put(db_env,generate_row_for_put);
    assert(!r);
    r = db_env->set_generate_row_callback_for_del(db_env,generate_row_for_del);
    assert(!r);
    db_env->set_update(db_env, tokudb_update_fun);
    db_env_set_direct_io(tokudb_directio == TRUE);
    r = db_env->open(db_env, tokudb_home, tokudb_init_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env opened:return=%d\n", __FUNCTION__, r);

    if (r) {
        DBUG_PRINT("info", ("env->open %d\n", r));
        goto error;
    }

    r = db_env->checkpointing_set_period(db_env, tokudb_checkpointing_period);
    assert(!r);
    r = db_env->cleaner_set_period(db_env, tokudb_cleaner_period);
    assert(r == 0);
    r = db_env->cleaner_set_iterations(db_env, tokudb_cleaner_iterations);
    assert(r == 0);

    r = db_env->set_lock_timeout(db_env, tokudb_lock_timeout);
    assert(r == 0);

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

    tokudb_primary_key_bytes_inserted = create_partitioned_counter();

    //3938: succeeded, set the init status flag and unlock
    tokudb_hton_initialized = 1;
    rw_unlock(&tokudb_hton_initialized_lock);
    DBUG_RETURN(false);

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

    // 3938: failed to initialized, drop the flag and lock
    tokudb_hton_initialized = 0;
    rw_unlock(&tokudb_hton_initialized_lock);
    DBUG_RETURN(true);
}

static int tokudb_done_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_done_func");
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
    
    // 3938: if we finalize the storage engine plugin, it is no longer
    // initialized. grab a writer lock for the duration of the
    // call, so we can drop the flag and destroy the mutexes
    // in isolation.
    rw_wrlock(&tokudb_hton_initialized_lock);
    assert(tokudb_hton_initialized);

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

    // 3938: drop the initialized flag and unlock
    tokudb_hton_initialized = 0;
    rw_unlock(&tokudb_hton_initialized_lock);

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

uint get_pk_insert_mode(THD* thd) {
    return THDVAR(thd, pk_insert_mode);
}

bool get_load_save_space(THD* thd) {
    return (THDVAR(thd, load_save_space) != 0);
}

bool get_disable_slow_alter(THD* thd) {
    return (THDVAR(thd, disable_slow_alter) != 0);
}

bool get_disable_hot_alter(THD* thd) {
    return THDVAR(thd, disable_hot_alter) != 0;
}

bool get_create_index_online(THD* thd) {
    return (THDVAR(thd, create_index_online) != 0);
}

bool get_disable_prefetching(THD* thd) {
    return (THDVAR(thd, disable_prefetching) != 0);
}

bool get_prelock_empty(THD* thd) {
    return (THDVAR(thd, prelock_empty) != 0);
}

bool get_log_client_errors(THD* thd) {
    return (THDVAR(thd, log_client_errors) != 0);
}

uint get_tokudb_block_size(THD* thd) {
    return THDVAR(thd, block_size);
}

uint get_tokudb_read_block_size(THD* thd) {
    return THDVAR(thd, read_block_size);
}

uint get_tokudb_read_buf_size(THD* thd) {
    return THDVAR(thd, read_buf_size);
}

#if TOKU_INCLUDE_UPSERT
bool get_disable_slow_update(THD* thd) {
    return (THDVAR(thd, disable_slow_update) != 0);
}

bool get_disable_slow_upsert(THD* thd) {
    return (THDVAR(thd, disable_slow_upsert) != 0);
}
#endif

typedef struct txn_progress_info {
    char status[200];
    THD* thd;
} *TXN_PROGRESS_INFO;


void txn_progress_func(TOKU_TXN_PROGRESS progress, void* extra) {
    TXN_PROGRESS_INFO progress_info = (TXN_PROGRESS_INFO)extra;
    int r;
    if (progress->stalled_on_checkpoint) {
        if (progress->is_commit) {
            r = sprintf(
                progress_info->status, 
                "Writing committed changes to disk, processing commit of transaction, %"PRId64" out of %"PRId64, 
                progress->entries_processed, 
                progress->entries_total
                ); 
            assert(r >= 0);
        }
        else {
            r = sprintf(
                progress_info->status, 
                "Writing committed changes to disk, processing abort of transaction, %"PRId64" out of %"PRId64, 
                progress->entries_processed, 
                progress->entries_total
                ); 
            assert(r >= 0);
        }
    }
    else {
        if (progress->is_commit) {
            r = sprintf(
                progress_info->status, 
                "processing commit of transaction, %"PRId64" out of %"PRId64, 
                progress->entries_processed, 
                progress->entries_total
                ); 
            assert(r >= 0);
        }
        else {
            r = sprintf(
                progress_info->status, 
                "processing abort of transaction, %"PRId64" out of %"PRId64, 
                progress->entries_processed, 
                progress->entries_total
                ); 
            assert(r >= 0);
        }
    }
    thd_proc_info(progress_info->thd, progress_info->status);
}


static void commit_txn_with_progress(DB_TXN* txn, uint32_t flags, THD* thd) {
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
    uint32_t syncflag = THDVAR(thd, commit_sync) ? 0 : DB_TXN_NOSYNC;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    if (*txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("doing txn commit:%d:%p\n", all, *txn);
        }
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_commit_before", DBUG_SUICIDE(););
        commit_txn_with_progress(*txn, syncflag, thd);
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_commit_after", DBUG_SUICIDE(););
        if (*txn == trx->sp_level) {
            trx->sp_level = 0;
        }
        *txn = 0;
        trx->sub_sp_level = NULL;
    } 
    else if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("nothing to commit %d\n", all);
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
        trx->sub_sp_level = NULL;
    } 
    else {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("abort0\n");
        }
    }
    reset_stmt_progress(&trx->stmt_progress);
    TOKUDB_DBUG_RETURN(0);
}

#if TOKU_INCLUDE_XA

static int tokudb_xa_prepare(handlerton* hton, THD* thd, bool all) {
    TOKUDB_DBUG_ENTER("tokudb_xa_prepare");
    int r = 0;
    DBUG_PRINT("trans", ("preparing transaction %s", all ? "all" : "stmt"));
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    DB_TXN* txn = all ? trx->all : trx->stmt;
    if (txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("doing txn prepare:%d:%p\n", all, txn);
        }
        // a TOKU_XA_XID is identical to a MYSQL_XID
        TOKU_XA_XID thd_xid;
        thd_get_xid(thd, (MYSQL_XID*) &thd_xid);
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_prepare_before", DBUG_SUICIDE(););
        r = txn->xa_prepare(txn, &thd_xid);
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_prepare_after", DBUG_SUICIDE(););
    } 
    else if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("nothing to prepare %d\n", all);
    }
    TOKUDB_DBUG_RETURN(r);
}

static int tokudb_xa_recover(handlerton* hton, XID*  xid_list, uint  len) {
    TOKUDB_DBUG_ENTER("tokudb_xa_recover");
    int r = 0;
    if (len == 0 || xid_list == NULL) {
        TOKUDB_DBUG_RETURN(0);
    }
    long num_returned = 0;
    r = db_env->txn_xa_recover(
        db_env,
        (TOKU_XA_XID*)xid_list,
        len,
        &num_returned,
        DB_NEXT
        );
    assert(r == 0);
    TOKUDB_DBUG_RETURN((int)num_returned);
}

static int tokudb_commit_by_xid(handlerton* hton, XID* xid) {
    TOKUDB_DBUG_ENTER("tokudb_commit_by_xid");
    int r = 0;
    DB_TXN* txn = NULL;
    TOKU_XA_XID* toku_xid = (TOKU_XA_XID*)xid;

    r = db_env->get_txn_from_xid(db_env, toku_xid, &txn);
    if (r) { goto cleanup; }

    r = txn->commit(txn, 0);
    if (r) { goto cleanup; }

    r = 0;
cleanup:
    TOKUDB_DBUG_RETURN(r);
}

static int tokudb_rollback_by_xid(handlerton* hton, XID*  xid) {
    TOKUDB_DBUG_ENTER("tokudb_rollback_by_xid");
    int r = 0;
    DB_TXN* txn = NULL;
    TOKU_XA_XID* toku_xid = (TOKU_XA_XID*)xid;

    r = db_env->get_txn_from_xid(db_env, toku_xid, &txn);
    if (r) { goto cleanup; }

    r = txn->abort(txn);
    if (r) { goto cleanup; }

    r = 0;
cleanup:
    TOKUDB_DBUG_RETURN(r);
}

#endif

static int tokudb_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_savepoint");
    int error;
    SP_INFO save_info = (SP_INFO)savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    if (thd->in_sub_stmt) {
        assert(trx->stmt);
        error = db_env->txn_begin(db_env, trx->sub_sp_level, &(save_info->txn), DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
        trx->sub_sp_level = save_info->txn;
        save_info->in_sub_stmt = true;
    }
    else {
        error = db_env->txn_begin(db_env, trx->sp_level, &(save_info->txn), DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
        trx->sp_level = save_info->txn;
        save_info->in_sub_stmt = false;
    }
    save_info->trx = trx;
    error = 0;
cleanup:
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_rollback_to_savepoint");
    int error;
    SP_INFO save_info = (SP_INFO)savepoint;
    DB_TXN* parent = NULL;
    DB_TXN* txn_to_rollback = save_info->txn;

    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    parent = txn_to_rollback->parent;
    if (!(error = txn_to_rollback->abort(txn_to_rollback))) {
        if (save_info->in_sub_stmt) {
            trx->sub_sp_level = parent;
        }
        else {
            trx->sp_level = parent;
        }
        error = tokudb_savepoint(hton, thd, savepoint);
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_release_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_release_savepoint");
    int error;

    SP_INFO save_info = (SP_INFO)savepoint;
    DB_TXN* parent = NULL;
    DB_TXN* txn_to_commit = save_info->txn;

    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, hton->slot);
    parent = txn_to_commit->parent;
    if (!(error = txn_to_commit->commit(txn_to_commit, 0))) {
        if (save_info->in_sub_stmt) {
            trx->sub_sp_level = parent;
        }
        else {
            trx->sp_level = parent;
        }
        save_info->txn = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_discover(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen) {
    return tokudb_discover2(hton, thd, db, name, true, frmblob, frmlen);
}

static int tokudb_discover2(handlerton *hton, THD* thd, const char *db, const char *name, bool translate_name,
                            uchar **frmblob, size_t *frmlen) {
    TOKUDB_DBUG_ENTER("tokudb_discover");
    int error;
    DB* status_db = NULL;
    DB_TXN* txn = NULL;
    char path[FN_REFLEN + 1];
    HA_METADATA_KEY curr_key = hatoku_frm_data;
    DBT key, value;    
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(&value));
    
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    build_table_filename(path, sizeof(path) - 1, db, name, "", translate_name ? 0 : FN_IS_TMP);
    error = open_status_dictionary(&status_db, path, txn);
    if (error) { goto cleanup; }

    key.data = &curr_key;
    key.size = sizeof(curr_key);

    error = status_db->getf_set(
        status_db, 
        txn,
        0,
        &key, 
        smart_dbt_callback_verify_frm,
        &value
        );
    if (error) {
        goto cleanup;
    }

    *frmblob = (uchar *)value.data;
    *frmlen = value.size;

    error = 0;
cleanup:
    if (status_db) {
        status_db->close(status_db,0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    TOKUDB_DBUG_RETURN(error);    
}

static int store_dbname_tablename_size(TABLE *table, char *name, uint64_t size, THD *thd) {
    char *tp = strrchr(name, '/');
    assert(tp);
    char *tablename = tp + 1;
    size_t tablename_length = strlen(tablename);

    char *dp = strchr(name, '/');
    char *dbname;
    size_t dbname_length;
    if (dp == tp) {
        dbname = name;
        dbname_length = tp - dbname;
    } else {
        dbname = dp + 1;
        dbname_length = tp - dbname;
    }

    table->field[0]->store(dbname, dbname_length, system_charset_info);
    table->field[1]->store(tablename, tablename_length, system_charset_info);
    table->field[2]->store(size, false);
    int error = schema_table_store_record(thd, table);
    return error;
}

static int tokudb_dictionary_info(TABLE *table, THD *thd) {
    int error;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    memset(&curr_key, 0, sizeof curr_key); 
    memset(&curr_val, 0, sizeof curr_val);
    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(
            tmp_cursor, 
            &curr_key, 
            &curr_val, 
            DB_NEXT
            );
        if (!error) {
            // We store the NULL terminator in the directory so it's included in the size.
            // See #5789
            // Recalculate and check just to be safe.
            size_t dname_len = strlen((const char *)curr_key.data);
            size_t iname_len = strlen((const char *)curr_val.data);
            assert(dname_len == curr_key.size - 1);
            assert(iname_len == curr_val.size - 1);
            table->field[0]->store(
                (char *)curr_key.data,
                dname_len,
                system_charset_info
                );
            table->field[1]->store(
                (char *)curr_val.data,
                iname_len,
                system_charset_info
                );
            error = schema_table_store_record(thd, table);
        }
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

static int tokudb_report_fractal_tree_info_for_db(const DBT *dname, const DBT *iname, TABLE *table, THD *thd) {
    int error;
    DB *db;
    uint64_t bt_num_blocks_allocated;
    uint64_t bt_num_blocks_in_use;
    uint64_t bt_size_allocated;
    uint64_t bt_size_in_use;

    error = db_create(&db, db_env, 0);
    if (error) {
        goto exit;
    }
    error = db->open(db, NULL, (char *)dname->data, NULL, DB_BTREE, 0, 0666);
    if (error) {
        goto exit;
    }
    error = db->get_fractal_tree_info64(db,
                                        &bt_num_blocks_allocated, &bt_num_blocks_in_use,
                                        &bt_size_allocated, &bt_size_in_use);
    {
        int close_error = db->close(db, 0);
        if (!error) {
            error = close_error;
        }
    }
    if (error) {
        goto exit;
    }

    // We store the NULL terminator in the directory so it's included in the size.
    // See #5789
    // Recalculate and check just to be safe.
    {
        size_t dname_len = strlen((const char *)dname->data);
        size_t iname_len = strlen((const char *)iname->data);
        assert(dname_len == dname->size - 1);
        assert(iname_len == iname->size - 1);
        table->field[0]->store(
            (char *)dname->data,
            dname_len,
            system_charset_info
            );
        table->field[1]->store(
            (char *)iname->data,
            iname_len,
            system_charset_info
            );
    }
    table->field[2]->store(bt_num_blocks_allocated, false);
    table->field[3]->store(bt_num_blocks_in_use, false);
    table->field[4]->store(bt_size_allocated, false);
    table->field[5]->store(bt_size_in_use, false);

    error = schema_table_store_record(thd, table);

exit:
    return error;
}

static int tokudb_fractal_tree_info(TABLE *table, THD *thd) {
    int error;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    memset(&curr_key, 0, sizeof curr_key);
    memset(&curr_val, 0, sizeof curr_val);
    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(
            tmp_cursor,
            &curr_key,
            &curr_val,
            DB_NEXT
            );
        if (!error) {
            error = tokudb_report_fractal_tree_info_for_db(&curr_key, &curr_val, table, thd);
        }
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

struct tokudb_report_fractal_tree_block_map_iterator_extra {
    int64_t num_rows;
    int64_t i;
    uint64_t *checkpoint_counts;
    int64_t *blocknums;
    int64_t *diskoffs;
    int64_t *sizes;
};

// This iterator is called while holding the blocktable lock.  We should be as quick as possible.
// We don't want to do one call to get the number of rows, release the blocktable lock, and then do another call to get all the rows because the number of rows may change if we don't hold the lock.
// As a compromise, we'll do some mallocs inside the lock on the first call, but everything else should be fast.
static int tokudb_report_fractal_tree_block_map_iterator(uint64_t checkpoint_count,
                                                          int64_t num_rows,
                                                          int64_t blocknum,
                                                          int64_t diskoff,
                                                          int64_t size,
                                                          void *iter_extra) {
    struct tokudb_report_fractal_tree_block_map_iterator_extra *e = static_cast<struct tokudb_report_fractal_tree_block_map_iterator_extra *>(iter_extra);

    assert(num_rows > 0);
    if (e->num_rows == 0) {
        e->checkpoint_counts = (uint64_t *) my_malloc(num_rows * (sizeof *e->checkpoint_counts), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->blocknums = (int64_t *) my_malloc(num_rows * (sizeof *e->blocknums), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->diskoffs = (int64_t *) my_malloc(num_rows * (sizeof *e->diskoffs), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->sizes = (int64_t *) my_malloc(num_rows * (sizeof *e->sizes), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->num_rows = num_rows;
    }

    e->checkpoint_counts[e->i] = checkpoint_count;
    e->blocknums[e->i] = blocknum;
    e->diskoffs[e->i] = diskoff;
    e->sizes[e->i] = size;
    ++(e->i);

    return 0;
}

static int tokudb_report_fractal_tree_block_map_for_db(const DBT *dname, const DBT *iname, TABLE *table, THD *thd) {
    int error;
    DB *db;
    struct tokudb_report_fractal_tree_block_map_iterator_extra e = {
        .num_rows = 0,
        .i = 0,
        .checkpoint_counts = NULL,
        .blocknums = NULL,
        .diskoffs = NULL,
        .sizes = NULL,
    };

    error = db_create(&db, db_env, 0);
    if (error) {
        goto exit;
    }
    error = db->open(db, NULL, (char *)dname->data, NULL, DB_BTREE, 0, 0666);
    if (error) {
        goto exit;
    }
    error = db->iterate_fractal_tree_block_map(db, tokudb_report_fractal_tree_block_map_iterator, &e);
    {
        int close_error = db->close(db, 0);
        if (!error) {
            error = close_error;
        }
    }
    if (error) {
        goto exit;
    }

    // If not, we should have gotten an error and skipped this section of code
    assert(e.i == e.num_rows);
    for (int64_t i = 0; error == 0 && i < e.num_rows; ++i) {
        // We store the NULL terminator in the directory so it's included in the size.
        // See #5789
        // Recalculate and check just to be safe.
        size_t dname_len = strlen((const char *)dname->data);
        size_t iname_len = strlen((const char *)iname->data);
        assert(dname_len == dname->size - 1);
        assert(iname_len == iname->size - 1);
        table->field[0]->store(
            (char *)dname->data,
            dname_len,
            system_charset_info
            );
        table->field[1]->store(
            (char *)iname->data,
            iname_len,
            system_charset_info
            );
        table->field[2]->store(e.checkpoint_counts[i], false);
        table->field[3]->store(e.blocknums[i], false);
        static const int64_t freelist_null = -1;
        static const int64_t diskoff_unused = -2;
        if (e.diskoffs[i] == diskoff_unused || e.diskoffs[i] == freelist_null) {
            table->field[4]->set_null();
        } else {
            table->field[4]->set_notnull();
            table->field[4]->store(e.diskoffs[i], false);
        }
        static const int64_t size_is_free = -1;
        if (e.sizes[i] == size_is_free) {
            table->field[5]->set_null();
        } else {
            table->field[5]->set_notnull();
            table->field[5]->store(e.sizes[i], false);
        }

        error = schema_table_store_record(thd, table);
    }

exit:
    if (e.checkpoint_counts != NULL) {
        my_free(e.checkpoint_counts, MYF(0));
        e.checkpoint_counts = NULL;
    }
    if (e.blocknums != NULL) {
        my_free(e.blocknums, MYF(0));
        e.blocknums = NULL;
    }
    if (e.diskoffs != NULL) {
        my_free(e.diskoffs, MYF(0));
        e.diskoffs = NULL;
    }
    if (e.sizes != NULL) {
        my_free(e.sizes, MYF(0));
        e.sizes = NULL;
    }
    return error;
}

static int tokudb_fractal_tree_block_map(TABLE *table, THD *thd) {
    int error;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    memset(&curr_key, 0, sizeof curr_key);
    memset(&curr_val, 0, sizeof curr_val);
    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(
            tmp_cursor,
            &curr_key,
            &curr_val,
            DB_NEXT
            );
        if (!error) {
            error = tokudb_report_fractal_tree_block_map_for_db(&curr_key, &curr_val, table, thd);
        }
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

static int tokudb_get_user_data_size(TABLE *table, THD *thd, bool exact) {
    int error;
    DB* curr_db = NULL;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBC* tmp_table_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    DB_TXN* tmp_txn = NULL;
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
        tmp_txn = NULL;
        //
        // here, and in other places, check if process has been killed
        // if so, get out of function so user is not stalled
        //
        if (thd->killed) {
            break;
        }
        error = db_env->txn_begin(db_env, 0, &tmp_txn, DB_READ_UNCOMMITTED);
        if (error) {
            goto cleanup;
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
            char* newname;
            uint64_t curr_num_bytes = 0;
            DB_BTREE_STAT64 dict_stats;

            error = db_create(&curr_db, db_env, 0);
            if (error) { goto cleanup; }

            newname = (char *)my_malloc(
                get_max_dict_name_path_length(name),
                MYF(MY_WME|MY_ZEROFILL|MY_FAE));

            make_name(newname, name, "main");
            
            error = curr_db->open(curr_db, tmp_txn, newname, NULL, DB_BTREE, DB_THREAD, 0);

            my_free(newname, MYF(0));

            if (error == ENOENT) { error = 0; continue; }
            if (error) { goto cleanup; }

            if (exact) {
                //
                // flatten if exact is required
                //
                uint curr_num_items = 0;                
                error = curr_db->cursor(curr_db, tmp_txn, &tmp_table_cursor, 0);
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
                tmp_txn, 
                &dict_stats
                );
            if (error) { goto cleanup; }

            curr_num_bytes = dict_stats.bt_dsize;
            if (*(uchar *)curr_val.data) {
                //
                // in this case, we have a hidden primary key, do not
                // want to report space taken up by the hidden primary key to the user
                //
                uint64_t hpk_space = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH*dict_stats.bt_ndata;
                curr_num_bytes = (hpk_space > curr_num_bytes) ? 0 : curr_num_bytes - hpk_space;
            }
            else {
                //
                // one infinity byte per key needs to be subtracted
                //
                uint64_t inf_byte_space = dict_stats.bt_ndata;
                curr_num_bytes = (inf_byte_space > curr_num_bytes) ? 0 : curr_num_bytes - inf_byte_space;
            }

            error = store_dbname_tablename_size(table, name, curr_num_bytes, thd);
            if (error) goto cleanup;

            {
                int r = curr_db->close(curr_db, 0);
                assert(r==0);
                curr_db = NULL;
            }
        }

        if (tmp_txn) {
            commit_txn(tmp_txn, 0);
            tmp_txn = NULL;
        }
    }

    error = 0;

cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
    }
    if (tmp_table_cursor) {
        int r = tmp_table_cursor->c_close(tmp_table_cursor);
        assert(r==0);
    }
    if (curr_db) {
        int r = curr_db->close(curr_db, 0);
        assert(r==0);
    }
    if (tmp_txn) {
        commit_txn(tmp_txn, 0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    if (error) {
        sql_print_error("got an error %d in show_data_size\n", error);
    }
    pthread_mutex_unlock(&tokudb_meta_mutex);
    return error;
}

#define STATPRINT(legend, val) if (legend != NULL && val != NULL) stat_print(thd,   \
                                          tokudb_hton_name, \
                                          strlen(tokudb_hton_name), \
                                          legend, \
                                          strlen(legend), \
                                          val, \
                                          strlen(val))

extern sys_var *intern_find_sys_var(const char *str, uint length, bool no_error);

static bool tokudb_show_engine_status(THD * thd, stat_print_fn * stat_print) {
    TOKUDB_DBUG_ENTER("tokudb_show_engine_status");
    int error;
    uint64_t panic;
    const int panic_string_len = 1024;
    char panic_string[panic_string_len] = {'\0'};
    uint64_t num_rows;
    fs_redzone_state redzone_state;
    const int bufsiz = 1024;
    char buf[bufsiz];

#if MYSQL_VERSION_ID < 50500
    {
        sys_var * version = intern_find_sys_var("version", 0, false);
        snprintf(buf, bufsiz, "%s", version->value_ptr(thd, (enum_var_type)0, (LEX_STRING*)NULL));
        STATPRINT("Version", buf);
    }
#endif
    error = db_env->get_engine_status_num_rows (db_env, &num_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[num_rows];
    error = db_env->get_engine_status (db_env, mystat, num_rows, &redzone_state, &panic, panic_string, panic_string_len);

    if (strlen(panic_string)) {
        STATPRINT("Environment panic string", panic_string);
    }
    if (error == 0) {
        if (panic) {
            snprintf(buf, bufsiz, "%" PRIu64, panic);
            STATPRINT("Environment panic", buf);
        }
        
        if(redzone_state == FS_BLOCKED) {
            STATPRINT("*** URGENT WARNING ***", "FILE SYSTEM IS COMPLETELY FULL");
            snprintf(buf, bufsiz, "FILE SYSTEM IS COMPLETELY FULL");
        }
        else if (redzone_state == FS_GREEN) {
            snprintf(buf, bufsiz, "more than %d percent of total file system space", 2*tokudb_fs_reserve_percent);
        }
        else if (redzone_state == FS_YELLOW) {
            snprintf(buf, bufsiz, "*** WARNING *** FILE SYSTEM IS GETTING FULL (less than %d percent free)", 2*tokudb_fs_reserve_percent);
        } 
        else if (redzone_state == FS_RED){
            snprintf(buf, bufsiz, "*** WARNING *** FILE SYSTEM IS GETTING VERY FULL (less than %d percent free): INSERTS ARE PROHIBITED", tokudb_fs_reserve_percent);
        }
        else {
            snprintf(buf, bufsiz, "information unavailable, unknown redzone state %d", redzone_state);
        }
        STATPRINT ("disk free space", buf);

        for (uint64_t row = 0; row < num_rows; row++) {
            switch (mystat[row].type) {
            case FS_STATE:
                snprintf(buf, bufsiz, "%"PRIu64"", mystat[row].value.num);
                break;
            case UINT64:
                snprintf(buf, bufsiz, "%"PRIu64"", mystat[row].value.num);
                break;
            case CHARSTR:
                snprintf(buf, bufsiz, "%s", mystat[row].value.str);
                break;
            case UNIXTIME:
                {
                    time_t t = mystat[row].value.num;
                    char tbuf[26];
                    snprintf(buf, bufsiz, "%.24s", ctime_r(&t, tbuf));
                }
                break;
            case TOKUTIME:
                {
                    double t = tokutime_to_seconds(mystat[row].value.num);
                    snprintf(buf, bufsiz, "%.6f", t);
                }
                break;
            case PARCOUNT:
                {
                    uint64_t v = read_partitioned_counter(mystat[row].value.parcount);
                    snprintf(buf, bufsiz, "%" PRIu64, v);
                }
                break;
            default:
                snprintf(buf, bufsiz, "UNKNOWN STATUS TYPE: %d", mystat[row].type);
                break;                
            }
            STATPRINT(mystat[row].legend, buf);
        }
        uint64_t bytes_inserted = read_partitioned_counter(tokudb_primary_key_bytes_inserted);
        snprintf(buf, bufsiz, "%" PRIu64, bytes_inserted);
        STATPRINT("handlerton: primary key bytes inserted", buf);
    }  
    if (error) { my_errno = error; }
    TOKUDB_DBUG_RETURN(error);
}

static void tokudb_checkpoint_lock(THD * thd) {
    int error;
    tokudb_trx_data* trx = NULL;
    char status_msg[200]; //buffer of 200 should be a good upper bound.
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        error = create_tokudb_trx_data_instance(&trx);
        //
        // can only fail due to memory allocation, so ok to assert
        //
        assert(!error);
        thd_data_set(thd, tokudb_hton->slot, trx);
    }
    
    if (trx->checkpoint_lock_taken) {
        goto cleanup;
    }
    //
    // This can only fail if environment is not created, which is not possible
    // in handlerton
    //
    sprintf(status_msg, "Trying to grab checkpointing lock.");
    thd_proc_info(thd, status_msg);
    error = db_env->checkpointing_postpone(db_env);
    assert(!error);

    trx->checkpoint_lock_taken = true;
cleanup:
    return;
}

static void tokudb_checkpoint_unlock(THD * thd) {
    int error;
    char status_msg[200]; //buffer of 200 should be a good upper bound.
    tokudb_trx_data* trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        error = 0;
        goto  cleanup;
    }
    if (!trx->checkpoint_lock_taken) {
        error = 0;
        goto  cleanup;
    }
    //
    // at this point, we know the checkpoint lock has been taken
    //
    sprintf(status_msg, "Trying to release checkpointing lock.");
    thd_proc_info(thd, status_msg);
    error = db_env->checkpointing_resume(db_env);
    assert(!error);

    trx->checkpoint_lock_taken = false;
    
cleanup:
    return;
}

static bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * stat_print, enum ha_stat_type stat_type) {
    switch (stat_type) {
    case HA_ENGINE_STATUS:
        return tokudb_show_engine_status(thd, stat_print);
        break;
    default:
        break;
    }
    return false;
}

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer) {
    sql_print_error("%s:  %s", db_errpfx, buffer);
}

static void tokudb_cleanup_log_files(void) {
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

#if defined(HA_INPLACE_ADD_INDEX_NO_READ_WRITE)
static uint tokudb_alter_table_flags(uint flags) {
    return HA_INPLACE_ADD_INDEX_NO_READ_WRITE
        |  HA_INPLACE_ADD_INDEX_NO_WRITE
        |  HA_INPLACE_DROP_INDEX_NO_READ_WRITE
        |  HA_INPLACE_ADD_UNIQUE_INDEX_NO_READ_WRITE
        |  HA_INPLACE_ADD_UNIQUE_INDEX_NO_WRITE
        |  HA_INPLACE_DROP_UNIQUE_INDEX_NO_READ_WRITE;
}
#endif


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

static void tokudb_lock_timeout_update(THD * thd,
        struct st_mysql_sys_var * sys_var, 
        void * var, const void * save)
{
    ulonglong * timeout = (ulonglong *) var;

    *timeout = *(const ulonglong *) save;
    db_env->set_lock_timeout(db_env, *timeout);
}

#define DEFAULT_LOCK_TIMEOUT_MSEC 4000

static MYSQL_SYSVAR_ULONGLONG(lock_timeout, tokudb_lock_timeout,
        0, "TokuDB lock timeout", 
        NULL, tokudb_lock_timeout_update, DEFAULT_LOCK_TIMEOUT_MSEC,
        0, ~0ULL, 0);

static void tokudb_cleaner_period_update(THD * thd,
        struct st_mysql_sys_var * sys_var, 
        void * var, const void * save)
{
    ulong * cleaner_period = (ulong *) var;

    *cleaner_period = *(const ulong *) save;
    int r = db_env->cleaner_set_period(db_env, *cleaner_period);
    assert(r==0);
}

#define DEFAULT_CLEANER_PERIOD 1

static MYSQL_SYSVAR_ULONG(cleaner_period, tokudb_cleaner_period,
        0, "TokuDB cleaner_period", 
        NULL, tokudb_cleaner_period_update, DEFAULT_CLEANER_PERIOD,
        0, ~0UL, 0);

static void tokudb_cleaner_iterations_update(THD * thd,
        struct st_mysql_sys_var * sys_var, 
        void * var, const void * save)
{
    ulong * cleaner_iterations = (ulong *) var;

    *cleaner_iterations = *(const ulong *) save;
    int r = db_env->cleaner_set_iterations(db_env, *cleaner_iterations);
    assert(r==0);
}

#define DEFAULT_CLEANER_ITERATIONS 5

static void tokudb_checkpointing_period_update(THD * thd,
        struct st_mysql_sys_var * sys_var, 
        void * var, const void * save)
{
    uint * checkpointing_period = (uint *) var;

    *checkpointing_period= *(const ulong *) save;
    int r = db_env->checkpointing_set_period(db_env, *checkpointing_period);
    assert(r==0);
}


static MYSQL_SYSVAR_BOOL(directio, tokudb_directio,
  PLUGIN_VAR_READONLY,
  "TokuDB Enable Direct I/O ",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(cleaner_iterations, tokudb_cleaner_iterations,
        0, "TokuDB cleaner_iterations", 
        NULL, tokudb_cleaner_iterations_update, DEFAULT_CLEANER_ITERATIONS,
        0, ~0UL, 0);

static MYSQL_SYSVAR_ULONGLONG(cache_size, tokudb_cache_size,
        PLUGIN_VAR_READONLY, "TokuDB cache table size", NULL, NULL, 0,
        0, ~0ULL, 0);
static MYSQL_SYSVAR_UINT(checkpointing_period, tokudb_checkpointing_period, 
    0, "TokuDB Checkpointing period", 
    NULL, tokudb_checkpointing_period_update, 60, 
    0, ~0U, 0);
static MYSQL_SYSVAR_ULONGLONG(max_lock_memory, tokudb_max_lock_memory, PLUGIN_VAR_READONLY, "TokuDB max memory for locks", NULL, NULL, 0, 0, ~0ULL, 0);
static MYSQL_SYSVAR_ULONG(debug, tokudb_debug, 0, "TokuDB Debug", NULL, NULL, 0, 0, ~0UL, 0);

static MYSQL_SYSVAR_STR(log_dir, tokudb_log_dir, PLUGIN_VAR_READONLY, "TokuDB Log Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(data_dir, tokudb_data_dir, PLUGIN_VAR_READONLY, "TokuDB Data Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(version, tokudb_version, PLUGIN_VAR_READONLY, "TokuDB Version", NULL, NULL, NULL);

static MYSQL_SYSVAR_UINT(init_flags, tokudb_init_flags, PLUGIN_VAR_READONLY, "Sets TokuDB DB_ENV->open flags", NULL, NULL, tokudb_init_flags, 0, ~0U, 0);

static MYSQL_SYSVAR_UINT(write_status_frequency, tokudb_write_status_frequency, 0, "TokuDB frequency that show processlist updates status of writes", NULL, NULL, 1000, 0, ~0U, 0);
static MYSQL_SYSVAR_UINT(read_status_frequency, tokudb_read_status_frequency, 0, "TokuDB frequency that show processlist updates status of reads", NULL, NULL, 10000, 0, ~0U, 0);
static MYSQL_SYSVAR_INT(fs_reserve_percent, tokudb_fs_reserve_percent, PLUGIN_VAR_READONLY, "TokuDB file system space reserve (percent free required)", NULL, NULL, 5, 0, 100, 0);
static MYSQL_SYSVAR_STR(tmp_dir, tokudb_tmp_dir, PLUGIN_VAR_READONLY, "Tokudb Tmp Dir", NULL, NULL, NULL);

static struct st_mysql_sys_var *tokudb_system_variables[] = {
    MYSQL_SYSVAR(cache_size),
    MYSQL_SYSVAR(max_lock_memory),
    MYSQL_SYSVAR(data_dir),
    MYSQL_SYSVAR(log_dir),
    MYSQL_SYSVAR(debug),
    MYSQL_SYSVAR(commit_sync),
    MYSQL_SYSVAR(lock_timeout),
    MYSQL_SYSVAR(cleaner_period),
    MYSQL_SYSVAR(cleaner_iterations),
    MYSQL_SYSVAR(pk_insert_mode),
    MYSQL_SYSVAR(load_save_space),
    MYSQL_SYSVAR(disable_slow_alter),
    MYSQL_SYSVAR(disable_hot_alter),
    MYSQL_SYSVAR(create_index_online),
    MYSQL_SYSVAR(disable_prefetching),
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(init_flags),
    MYSQL_SYSVAR(checkpointing_period),
    MYSQL_SYSVAR(prelock_empty),
    MYSQL_SYSVAR(log_client_errors),
    MYSQL_SYSVAR(checkpoint_lock),
    MYSQL_SYSVAR(write_status_frequency),
    MYSQL_SYSVAR(read_status_frequency),
    MYSQL_SYSVAR(fs_reserve_percent),
    MYSQL_SYSVAR(tmp_dir),
    MYSQL_SYSVAR(block_size),
    MYSQL_SYSVAR(read_block_size),
    MYSQL_SYSVAR(read_buf_size),
    MYSQL_SYSVAR(row_format),
    MYSQL_SYSVAR(directio),
#if TOKU_INCLUDE_UPSERT
    MYSQL_SYSVAR(disable_slow_update),
    MYSQL_SYSVAR(disable_slow_upsert),
#endif
    NULL
};

struct st_mysql_storage_engine tokudb_storage_engine = { MYSQL_HANDLERTON_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_user_data_field_info[] = {
    {"database_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"data_size", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

#if MYSQL_VERSION_ID >= 50600
static int tokudb_user_data_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_user_data_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    int error;
    TABLE *table = tables->table;
    
    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), "TokuDB");
        error = -1;
    } else {
        error = tokudb_get_user_data_size(table, thd, false);
    }

    // 3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    return error;
}

static int tokudb_user_data_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_user_data_field_info;
    schema->fill_table = tokudb_user_data_fill_table;
    return 0;
}

static int tokudb_user_data_done(void *p) {
    return 0;
}

static struct st_mysql_information_schema tokudb_user_data_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static struct st_mysql_information_schema tokudb_fractal_tree_info_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static struct st_mysql_information_schema tokudb_fractal_tree_block_map_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_user_data_exact_field_info[] = {
    {"database_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"data_size", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

static ST_FIELD_INFO tokudb_dictionary_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

static ST_FIELD_INFO tokudb_fractal_tree_info_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_num_blocks_allocated", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_num_blocks_in_use", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_size_allocated", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_size_in_use", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

static ST_FIELD_INFO tokudb_fractal_tree_block_map_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"checkpoint_count", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"blocknum", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"offset", 0, MYSQL_TYPE_LONGLONG, 0, MY_I_S_MAYBE_NULL, NULL, SKIP_OPEN_TABLE },
    {"size", 0, MYSQL_TYPE_LONGLONG, 0, MY_I_S_MAYBE_NULL, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};


#if MYSQL_VERSION_ID >= 50600
static int tokudb_dictionary_info_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_dictionary_info_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), "TokuDB");
        error = -1;
    } else {
        error = tokudb_dictionary_info(table, thd);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_user_data_exact_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_user_data_exact_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), "TokuDB");
        error = -1;
    } else {
        error = tokudb_get_user_data_size(table, thd, true);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_fractal_tree_info_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_fractal_tree_info_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), "TokuDB");
        error = -1;
    } else {
        error = tokudb_fractal_tree_info(table, thd);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_fractal_tree_block_map_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_fractal_tree_block_map_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), "TokuDB");
        error = -1;
    } else {
        error = tokudb_fractal_tree_block_map(table, thd);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    return error;
}

static int tokudb_user_data_exact_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_user_data_exact_field_info;
    schema->fill_table = tokudb_user_data_exact_fill_table;
    return 0;
}

static int tokudb_user_data_exact_done(void *p) {
    return 0;
}

static int tokudb_dictionary_info_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_dictionary_field_info;
    schema->fill_table = tokudb_dictionary_info_fill_table;
    return 0;
}

static int tokudb_dictionary_info_done(void *p) {
    return 0;
}

static int tokudb_fractal_tree_info_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_fractal_tree_info_field_info;
    schema->fill_table = tokudb_fractal_tree_info_fill_table;
    return 0;
}

static int tokudb_fractal_tree_info_done(void *p) {
    return 0;
}

static int tokudb_fractal_tree_block_map_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_fractal_tree_block_map_field_info;
    schema->fill_table = tokudb_fractal_tree_block_map_fill_table;
    return 0;
}

static int tokudb_fractal_tree_block_map_done(void *p) {
    return 0;
}

static struct st_mysql_information_schema tokudb_user_data_exact_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

enum { TOKUDB_PLUGIN_VERSION = 0x0400 };
#define TOKUDB_PLUGIN_VERSION_STR "1024"

mysql_declare_plugin(tokudb) 
{
    MYSQL_STORAGE_ENGINE_PLUGIN, 
    &tokudb_storage_engine, 
    tokudb_hton_name, 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    tokudb_system_variables,   /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_information_schema, 
    "TokuDB_user_data", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_user_data_init,     /* plugin init */
    tokudb_user_data_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_exact_information_schema, 
    "TokuDB_user_data_exact", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_user_data_exact_init,     /* plugin init */
    tokudb_user_data_exact_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_exact_information_schema, 
    "TokuDB_file_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_dictionary_info_init,     /* plugin init */
    tokudb_dictionary_info_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_info_information_schema, 
    "TokuDB_fractal_tree_info", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_fractal_tree_info_init,     /* plugin init */
    tokudb_fractal_tree_info_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_block_map_information_schema, 
    "TokuDB_fractal_tree_block_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_fractal_tree_block_map_init,     /* plugin init */
    tokudb_fractal_tree_block_map_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    NULL,                      /* config options */
#if MYSQL_VERSION_ID >= 50521
    0,                         /* flags */
#endif
}
mysql_declare_plugin_end;

#ifdef MARIA_PLUGIN_INTERFACE_VERSION

maria_declare_plugin(tokudb) 
{
    MYSQL_STORAGE_ENGINE_PLUGIN, 
    &tokudb_storage_engine, 
    tokudb_hton_name, 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    tokudb_system_variables,   /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_information_schema, 
    "TokuDB_user_data", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_user_data_init,     /* plugin init */
    tokudb_user_data_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_exact_information_schema, 
    "TokuDB_user_data_exact", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_user_data_exact_init,     /* plugin init */
    tokudb_user_data_exact_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_user_data_exact_information_schema, 
    "TokuDB_file_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_dictionary_info_init,     /* plugin init */
    tokudb_dictionary_info_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_info_information_schema, 
    "TokuDB_fractal_tree_info", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_fractal_tree_info_init,     /* plugin init */
    tokudb_fractal_tree_info_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_block_map_information_schema, 
    "TokuDB_fractal_tree_block_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_PROPRIETARY,
    tokudb_fractal_tree_block_map_init,     /* plugin init */
    tokudb_fractal_tree_block_map_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,     /* 4.0.0 */
    NULL,                      /* status variables */
    NULL,                      /* system variables */
    TOKUDB_PLUGIN_VERSION_STR, /* string version */
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
}
maria_declare_plugin_end;

#endif
