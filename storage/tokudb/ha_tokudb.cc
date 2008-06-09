#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation          // gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "mysql_priv.h"

#if !defined(HA_END_SPACE_KEY) || HA_END_SPACE_KEY != 0
#error
#endif

unsigned long my_getphyspages() {
    return sysconf(_SC_PHYS_PAGES);
}

#include <syscall.h>

unsigned int my_tid() {
    return syscall(__NR_gettid);
}

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

#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION

//#include "tokudb_config.h"

/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif

#include "tokudb_probes.h"

#include "ha_tokudb.h"
#include <mysql/plugin.h>

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root);

handlerton *tokudb_hton;

typedef struct st_tokudb_trx_data {
    DB_TXN *all;
    DB_TXN *stmt;
    DB_TXN *sp_level;
    uint tokudb_lock_count;
} tokudb_trx_data;

// QQQ how to tune these?
#define HA_TOKUDB_ROWS_IN_TABLE 10000   /* to get optimization right */
#define HA_TOKUDB_RANGE_COUNT   100
#define HA_TOKUDB_MAX_ROWS      10000000        /* Max rows in table */
/* extra rows for estimate_rows_upper_bound() */
#define HA_TOKUDB_EXTRA_ROWS    100

/* Bits for share->status */
#define STATUS_PRIMARY_KEY_INIT 1
#define STATUS_ROW_COUNT_INIT   2
#define STATUS_TOKUDB_ANALYZE      4
#define STATUS_AUTO_INCREMENT_INIT 8

// tokudb debug tracing
#define TOKUDB_DEBUG_INIT 1
#define TOKUDB_DEBUG_OPEN 2
#define TOKUDB_DEBUG_ENTER 4
#define TOKUDB_DEBUG_RETURN 8
#define TOKUDB_DEBUG_ERROR 16
#define TOKUDB_DEBUG_TXN 32
#define TOKUDB_DEBUG_AUTO_INCREMENT 64
#define TOKUDB_DEBUG_SAVE_TRACE 128

#define TOKUDB_TRACE(f, ...) \
    printf("%d:%s:%d:" f, my_tid(), __FILE__, __LINE__, ##__VA_ARGS__);

#define TOKUDB_DBUG_ENTER(f, ...)      \
{ \
    if (tokudb_debug & TOKUDB_DEBUG_ENTER) { \
        TOKUDB_TRACE(f "\n", ##__VA_ARGS__); \
    } \
} \
    DBUG_ENTER(__FUNCTION__);


#define TOKUDB_DBUG_RETURN(r) \
{ \
    int rr = (r); \
    if ((tokudb_debug & TOKUDB_DEBUG_RETURN) || (rr != 0 && (tokudb_debug & TOKUDB_DEBUG_ERROR))) { \
        TOKUDB_TRACE("%s:return %d\n", __FUNCTION__, rr); \
    } \
    DBUG_RETURN(rr); \
}

#define TOKUDB_DBUG_DUMP(s, p, len) \
{ \
    TOKUDB_TRACE("%s:%s", __FUNCTION__, s); \
    uint i;                                                             \
    for (i=0; i<len; i++) {                                             \
        printf("%2.2x", ((uchar*)p)[i]);                                \
    }                                                                   \
    printf("\n");                                                       \
}

const char *ha_tokudb_ext = ".tokudb";

//static my_bool tokudb_shared_data = FALSE;
static u_int32_t tokudb_init_flags = 
    DB_CREATE | DB_THREAD | DB_PRIVATE | 
    DB_INIT_LOCK | 
    DB_INIT_MPOOL |
    DB_INIT_TXN | 
    0 | // disabled for 1.0.2 DB_INIT_LOG |
    0;  // disabled for 1.0.1 DB_RECOVER;
static u_int32_t tokudb_env_flags = DB_LOG_AUTOREMOVE;
//static u_int32_t tokudb_lock_type = DB_LOCK_DEFAULT;
//static ulong tokudb_log_buffer_size = 0;
//static ulong tokudb_log_file_size = 0;
static ulonglong tokudb_cache_size = 0;
static uint tokudb_cache_memory_percent = 50;
static char *tokudb_home;
//static char *tokudb_tmpdir;
static char *tokudb_data_dir;
static char *tokudb_log_dir;
//static long tokudb_lock_scan_time = 0;
//static ulong tokudb_region_size = 0;
//static ulong tokudb_cache_parts = 1;
static ulong tokudb_trans_retry = 1;
static ulong tokudb_max_lock;
static ulong tokudb_debug;
#ifdef TOKUDB_VERSION
static char *tokudb_version = TOKUDB_VERSION;
#else
static char *tokudb_version;
#endif

static DB_ENV *db_env;

static const char tokudb_hton_name[] = "TokuDB";
static const int tokudb_hton_name_length = sizeof(tokudb_hton_name) - 1;

// thread variables

static MYSQL_THDVAR_BOOL(commit_sync, PLUGIN_VAR_THDLOCAL, "sync on txn commit", 
                         /* check */ NULL, /* update */ NULL, /* default*/ TRUE);

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer);
static void tokudb_cleanup_log_files(void);
static TOKUDB_SHARE *get_share(const char *table_name, TABLE * table);
static int free_share(TOKUDB_SHARE * share, TABLE * table, uint hidden_primary_key, bool mutex_is_locked);
static int write_status(DB * status_block, char *buff, uint length);
static void update_status(TOKUDB_SHARE * share, TABLE * table);
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

static HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;

static uchar *tokudb_get_key(TOKUDB_SHARE * share, size_t * length, my_bool not_used __attribute__ ((unused))) {
    *length = share->table_name_length;
    return (uchar *) share->table_name;
}

static int tokudb_init_func(void *p) {
    TOKUDB_DBUG_ENTER("tokudb_init_func");

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
    if (tokudb_init_flags & DB_INIT_TXN) {
        tokudb_hton->commit = tokudb_commit;
        tokudb_hton->rollback = tokudb_rollback;
    }
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
    int r;
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

    // config the cache table
    if (tokudb_cache_size == 0) {
        unsigned long pagesize = my_getpagesize();
        unsigned long long npages = my_getphyspages();
        unsigned long long physmem = npages * pagesize;
        tokudb_cache_size = (ulonglong) (physmem * (tokudb_cache_memory_percent / 100.0));
    }
    if (tokudb_cache_size) {
        DBUG_PRINT("info", ("tokudb_cache_size: %lld\n", tokudb_cache_size));
        r = db_env->set_cachesize(db_env, tokudb_cache_size / (1024 * 1024L * 1024L), tokudb_cache_size % (1024L * 1024L * 1024L), 1);
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

    r = db_env->open(db_env, tokudb_home, tokudb_init_flags, 0666);

    if (tokudb_debug & TOKUDB_DEBUG_INIT) TOKUDB_TRACE("%s:env opened:return=%d\n", __FUNCTION__, r);

    if (r) {
        DBUG_PRINT("info", ("env->open %d\n", r));
        goto error;
    }

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
    TOKUDB_DBUG_RETURN(0);
}

/** @brief
    Simple lock controls. The "share" it creates is a structure we will
    pass to each tokudb handler. Do you have to have one of these? Well, you have
    pieces that are used for locking, and they are needed to function.
*/
static TOKUDB_SHARE *get_share(const char *table_name, TABLE * table) {
    TOKUDB_SHARE *share;
    uint length;

    pthread_mutex_lock(&tokudb_mutex);
    length = (uint) strlen(table_name);

    if (!(share = (TOKUDB_SHARE *) hash_search(&tokudb_open_tables, (uchar *) table_name, length))) {
        ulong *rec_per_key;
        char *tmp_name;
        u_int32_t *key_type;
        uint num_keys = table->s->keys;

        if (!(share = (TOKUDB_SHARE *) 
            my_multi_malloc(MYF(MY_WME | MY_ZEROFILL), 
                            &share, sizeof(*share),
                            &tmp_name, length + 1, 
                            &rec_per_key, num_keys * sizeof(ha_rows), 
                            &key_type, (num_keys + 1) * sizeof(u_int32_t), 
                            NullS))) {
            pthread_mutex_unlock(&tokudb_mutex);
            return NULL;
        }
        share->use_count = 0;
        share->table_name_length = length;
        share->table_name = tmp_name;
        strmov(share->table_name, table_name);

        share->rec_per_key = rec_per_key;
        share->key_type = key_type;
        bzero((void *) share->key_file, sizeof(share->key_file));

        if (my_hash_insert(&tokudb_open_tables, (uchar *) share))
            goto error;
        thr_lock_init(&share->lock);
        pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
    }
    pthread_mutex_unlock(&tokudb_mutex);

    return share;

  error:
    pthread_mutex_destroy(&share->mutex);
    my_free((uchar *) share, MYF(0));

    return NULL;
}

static int free_share(TOKUDB_SHARE * share, TABLE * table, uint hidden_primary_key, bool mutex_is_locked) {
    int error, result = 0;
    uint num_keys = table->s->keys + test(hidden_primary_key);

    pthread_mutex_lock(&tokudb_mutex);

    if (mutex_is_locked)
        pthread_mutex_unlock(&share->mutex);
    if (!--share->use_count) {
        DBUG_PRINT("info", ("share->use_count %u", share->use_count));
        DB **key_file = share->key_file;

        /* this does share->file->close() implicitly */
        update_status(share, table);

        for (uint i = 0; i < num_keys; i++) {
            if (tokudb_debug & TOKUDB_DEBUG_OPEN)
                TOKUDB_TRACE("dbclose:%p\n", key_file[i]);
            if (key_file[i] && (error = key_file[i]->close(key_file[i], 0)))
                result = error;
        }

        if (share->status_block && (error = share->status_block->close(share->status_block, 0)))
            result = error;

        hash_delete(&tokudb_open_tables, (uchar *) share);
        thr_lock_delete(&share->lock);
        pthread_mutex_destroy(&share->mutex);
        my_free((uchar *) share, MYF(0));
    }
    pthread_mutex_unlock(&tokudb_mutex);

    return result;
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
        db_env = 0;
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
    if (tokudb_init_flags & DB_INIT_LOG) {
        if ((error = db_env->log_flush(db_env, 0))) {
            my_error(ER_ERROR_DURING_FLUSH_LOGS, MYF(0), error);
            result = 1;
        }
        if ((error = db_env->txn_checkpoint(db_env, 0, 0, 0))) {
            my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
            result = 1;
        }
    }
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
    } else
        if (tokudb_debug & TOKUDB_DEBUG_TXN) 
            TOKUDB_TRACE("commit0\n");
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
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[hton->slot];
    if (!(error = db_env->txn_begin(db_env, trx->sp_level, save_txn, 0))) {
        trx->sp_level = *save_txn;
    }
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint) {
    TOKUDB_DBUG_ENTER("tokudb_rollback_to_savepoint");
    int error;
    DB_TXN *parent, **save_txn = (DB_TXN **) savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[hton->slot];
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
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[hton->slot];
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


static int get_name_length(const char *name) {
    int n = 0;
    const char *newname = name;
    if (tokudb_data_dir) {
        n += strlen(tokudb_data_dir) + 1;
        if (strncmp("./", name, 2) == 0) 
            newname = name + 2;
    }
    n += strlen(newname);
    n += strlen(ha_tokudb_ext);
    return n;
}

static void make_name(char *newname, const char *tablename, const char *dictname) {
    const char *newtablename = tablename;
    char *nn = newname;
    if (tokudb_data_dir) {
        nn += sprintf(nn, "%s/", tokudb_data_dir);
        if (strncmp("./", tablename, 2) == 0)
            newtablename = tablename + 2;
    }
    nn += sprintf(nn, "%s%s", newtablename, ha_tokudb_ext);
    if (dictname)
        nn += sprintf(nn, "/%s%s", dictname, ha_tokudb_ext);
}


ha_tokudb::ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg)
:  
    handler(hton, table_arg), alloc_ptr(0), rec_buff(0),
    // flags defined in sql\handler.h
    int_table_flags(HA_REC_NOT_IN_SEQ | HA_FAST_KEY_READ | HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_PRIMARY_KEY_IN_READ_INDEX | 
                    HA_FILE_BASED | HA_CAN_GEOMETRY | HA_AUTO_PART_KEY | HA_TABLE_SCAN_ON_INDEX), 
    changed_rows(0), last_dup_key((uint) - 1), version(0), using_ignore(0),primary_key_offsets(NULL) {
}

static const char *ha_tokudb_exts[] = {
    ha_tokudb_ext,
    NullS
};

/* 
 *  returns NULL terminated file extension string
 */
const char **ha_tokudb::bas_ext() const {
    TOKUDB_DBUG_ENTER("ha_tokudb::bas_ext");
    DBUG_RETURN(ha_tokudb_exts);
}

//
// Returns a bit mask of capabilities of the key or its part specified by 
// the arguments. The capabilities are defined in sql/handler.h.
//
ulong ha_tokudb::index_flags(uint idx, uint part, bool all_parts) const {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_flags");
    ulong flags = (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_KEYREAD_ONLY | HA_READ_RANGE);
    for (uint i = all_parts ? 0 : part; i <= part; i++) {
        KEY_PART_INFO *key_part = table_share->key_info[idx].key_part + i;
        if (key_part->field->type() == FIELD_TYPE_BLOB) {
            /* We can't use BLOBS to shortcut sorts */
            flags &= ~(HA_READ_ORDER | HA_KEYREAD_ONLY | HA_READ_RANGE);
            break;
        }
    }
    DBUG_RETURN(flags);
}

static int tokudb_cmp_hidden_key(DB * file, const DBT * new_key, const DBT * saved_key) {
    ulonglong a = uint5korr((char *) new_key->data);
    ulonglong b = uint5korr((char *) saved_key->data);
    return a < b ? -1 : (a > b ? 1 : 0);
}

/*
    Things that are required for ALL data types:
        key_part->field->null_bit
        key_part->length
        key_part->field->packed_col_length(...)
            DEFAULT: virtual uint packed_col_length(const uchar *to, uint length)
                { return length;}
            All integer types use this.
            String types MIGHT use different one, espescially the varchars
        key_part->field->pack_cmp(...)
            DEFAULT: virtual int pack_cmp(...)
                { return cmp(a,b); }
            All integer types use the obvious one.
            Assume X byte bytestream, int =:
            ((u_int64_t)((u_int8_t)bytes[0])) << 0 | 
            ((u_int64_t)((u_int8_t)bytes[1])) << 8 | 
            ((u_int64_t)((u_int8_t)bytes[2])) << 16 | 
            ((u_int64_t)((u_int8_t)bytes[3])) << 24 | 
            ((u_int64_t)((u_int8_t)bytes[4])) << 32 | 
            ((u_int64_t)((u_int8_t)bytes[5])) << 40 | 
            ((u_int64_t)((u_int8_t)bytes[6])) << 48 | 
            ((u_int64_t)((u_int8_t)bytes[7])) << 56
            If the integer type is < 8 bytes, just skip the unneeded ones.
            Then compare the integers in the obvious way.
        Strings:
            Empty space differences at end are ignored.
            i.e. delete all empty space at end first, and then compare.
    Possible prerequisites:
        key_part->field->cmp
            NO DEFAULT
*/

typedef enum {
    TOKUTRACE_SIGNED_INTEGER   = 0,
    TOKUTRACE_UNSIGNED_INTEGER = 1,
    TOKUTRACE_CHAR = 2
} tokutrace_field_type;

typedef struct {
    tokutrace_field_type    type;
    bool                    null_bit;
    u_int32_t               length;
} tokutrace_field;

typedef struct {
    u_int16_t           version;
    u_int32_t           num_fields;
    tokutrace_field     fields[0];
} tokutrace_cmp_fun;

static int tokutrace_db_get_cmp_byte_stream(DB* db, DBT* byte_stream) {
    int r      = ENOSYS;
    void* data = NULL;
    KEY* key   = NULL;
    if (byte_stream->flags != DB_DBT_MALLOC) { return EINVAL; }
    bzero((void *) byte_stream, sizeof(*byte_stream));

    u_int32_t num_fields = 0;
    if (!db->app_private) { num_fields = 1; }
    else {
        key = (KEY*)db->app_private;
        num_fields = key->key_parts;
    }
    size_t need_size = sizeof(tokutrace_cmp_fun) +
                       num_fields * sizeof(tokutrace_field);

    data = my_malloc(need_size, MYF(MY_FAE | MY_ZEROFILL | MY_WME));
    if (!data) { return ENOMEM; }

    tokutrace_cmp_fun* info = (tokutrace_cmp_fun*)data;
    info->version     = 1;
    info->num_fields  = num_fields;
    
    if (!db->app_private) {
        info->fields[0].type     = TOKUTRACE_UNSIGNED_INTEGER;
        info->fields[0].null_bit = false;
        info->fields[0].length   = 40 / 8;
        goto finish;
    }
    assert(db->app_private);
    assert(key);
    u_int32_t i;
    for (i = 0; i < num_fields; i++) {
        info->fields[i].null_bit = key->key_part[i].null_bit;
        info->fields[i].length   = key->key_part[i].length;
        enum_field_types type    = key->key_part[i].field->type();
        switch (type) {
#ifdef HAVE_LONG_LONG
            case (MYSQL_TYPE_LONGLONG):
#endif
            case (MYSQL_TYPE_LONG):
            case (MYSQL_TYPE_INT24):
            case (MYSQL_TYPE_SHORT):
            case (MYSQL_TYPE_TINY): {
                /* Integer */
                Field_num* field = static_cast<Field_num*>(key->key_part[i].field);
                if (field->unsigned_flag) {
                    info->fields[i].type = TOKUTRACE_UNSIGNED_INTEGER; }
                else {
                    info->fields[i].type = TOKUTRACE_SIGNED_INTEGER; }
                break;
            }
            default: {
                fprintf(stderr, "Cannot save cmp function for type %d.\n", type);
                r = ENOSYS;
                goto cleanup;
            }
        }
    }
finish:
    byte_stream->data = data;
    byte_stream->size = need_size;
    r = 0;
cleanup:
    if (r!=0) {
        if (data) { my_free(data, MYF(0)); }
    }
    return r;
}

static int tokudb_compare_two_keys(KEY *key, const DBT * new_key, const DBT * saved_key, bool cmp_prefix) {
    uchar *new_key_ptr = (uchar *) new_key->data;
    uchar *saved_key_ptr = (uchar *) saved_key->data;
    KEY_PART_INFO *key_part = key->key_part, *end = key_part + key->key_parts;
    uint key_length = new_key->size;
    uint saved_key_length = saved_key->size;

    //DBUG_DUMP("key_in_index", saved_key_ptr, saved_key->size);
    for (; key_part != end && (int) key_length > 0 && (int) saved_key_length > 0; key_part++) {
        int cmp;
        uint new_key_field_length;
        uint saved_key_field_length;
        if (key_part->field->null_bit) {
            assert(new_key_ptr   < (uchar *) new_key->data   + new_key->size);
            assert(saved_key_ptr < (uchar *) saved_key->data + saved_key->size);
            if (*new_key_ptr != *saved_key_ptr) {
                return ((int) *new_key_ptr - (int) *saved_key_ptr); }
            saved_key_ptr++;
            key_length--;
            saved_key_length--;
            if (!*new_key_ptr++) { continue; }
        }
        new_key_field_length     = key_part->field->packed_col_length(new_key_ptr,   key_part->length);
        saved_key_field_length   = key_part->field->packed_col_length(saved_key_ptr, key_part->length);
        assert(      key_length >= new_key_field_length);
        assert(saved_key_length >= saved_key_field_length);
        if ((cmp = key_part->field->pack_cmp(new_key_ptr, saved_key_ptr, key_part->length, 0)))
            return cmp;
        new_key_ptr      += new_key_field_length;
        key_length       -= new_key_field_length;
        saved_key_ptr    += saved_key_field_length;
        saved_key_length -= saved_key_field_length;
    }
    return cmp_prefix ? 0 : key_length - saved_key_length;
}

static int tokudb_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    return tokudb_compare_two_keys(key, keya, keyb, false);
}

static int tokudb_cmp_primary_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->api_internal;
    return tokudb_compare_two_keys(key, keya, keyb, false);
}

//TODO: QQQ Only do one direction for prefix.
static int tokudb_prefix_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    return tokudb_compare_two_keys(key, keya, keyb, true);
}

#if 0
/* Compare key against row */
static bool tokudb_key_cmp(TABLE * table, KEY * key_info, const uchar * key, uint key_length) {
    KEY_PART_INFO *key_part = key_info->key_part, *end = key_part + key_info->key_parts;

    for (; key_part != end && (int) key_length > 0; key_part++) {
        int cmp;
        uint length;
        if (key_part->null_bit) {
            key_length--;
            /*
               With the current usage, the following case will always be FALSE,
               because NULL keys are sorted before any other key
             */
            if (*key != (table->record[0][key_part->null_offset] & key_part->null_bit) ? 0 : 1)
                return 1;
            if (!*key++)        // Null value
                continue;
        }
        /*
           Last argument has to be 0 as we are also using this to function to see
           if a key like 'a  ' matched a row with 'a'
         */
        if ((cmp = key_part->field->pack_cmp(key, key_part->length, 0)))
            return cmp;
        length = key_part->field->packed_col_length(key, key_part->length);
        key += length;
        key_length -= length;
    }
    return 0;                   // Identical keys
}
#endif

int primary_key_part_compare (const void* left, const void* right) {
    PRIM_KEY_PART_INFO* left_part= (PRIM_KEY_PART_INFO *)left;
    PRIM_KEY_PART_INFO* right_part = (PRIM_KEY_PART_INFO *)right;
    return left_part->offset - right_part->offset;
}

//
// Open a secondary table, the key will be a secondary index, the data will be a primary key
//
int ha_tokudb::open_secondary_table(DB** ptr, KEY* key_info, const char* name, int mode, u_int32_t* key_type) {
    int error = ENOSYS;
    char part[MAX_ALIAS_NAME + 10];
    char name_buff[FN_REFLEN];
    uint open_flags = (mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
    char newname[strlen(name) + 32];
    DBT cmp_byte_stream;

    if (tokudb_init_flags & DB_INIT_TXN)
        open_flags += DB_AUTO_COMMIT;

    if ((error = db_create(ptr, db_env, 0))) {
        my_errno = error;
        goto cleanup;
    }
    sprintf(part, "key-%s", key_info->name);
    make_name(newname, name, part);
    fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
    *key_type = key_info->flags & HA_NOSAME ? DB_NOOVERWRITE : DB_YESOVERWRITE;
    (*ptr)->app_private = (void *) (key_info);
    if (tokudb_debug & TOKUDB_DEBUG_SAVE_TRACE) {
        bzero((void *) &cmp_byte_stream, sizeof(cmp_byte_stream));
        cmp_byte_stream.flags = DB_DBT_MALLOC;
        if ((error = tokutrace_db_get_cmp_byte_stream(*ptr, &cmp_byte_stream))) {
            my_errno = error;
            goto cleanup;
        }
        (*ptr)->set_bt_compare(*ptr, tokudb_cmp_packed_key);
        my_free(cmp_byte_stream.data, MYF(0));
    }
    else
        (*ptr)->set_bt_compare(*ptr, tokudb_cmp_packed_key);    
    if (!(key_info->flags & HA_NOSAME)) {
        DBUG_PRINT("info", ("Setting DB_DUP+DB_DUPSORT for key %s\n", key_info->name));
        (*ptr)->set_flags(*ptr, DB_DUP + DB_DUPSORT);
        (*ptr)->api_internal = share->file->app_private;
        (*ptr)->set_dup_compare(*ptr, hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_primary_key);
    }
    if ((error = (*ptr)->open(*ptr, 0, name_buff, NULL, DB_BTREE, open_flags, 0))) {
        my_errno = error;
        goto cleanup;
    }
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("open:%s:file=%p\n", newname, *ptr);
    }
cleanup:
    return error;
}



//
// Creates and opens a handle to a table which already exists in a tokudb
// database.
// Parameters:
//      [in]   name - table name
//             mode - seems to specify if table is read only
//             test_if_locked - unused
// Returns:
//      0 on success
//      1 on error
//
int ha_tokudb::open(const char *name, int mode, uint test_if_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::open %p %s", this, name);
    TOKUDB_OPEN();

    char name_buff[FN_REFLEN];
    uint open_flags = (mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
    uint max_key_length;
    int error;

    if (tokudb_init_flags & DB_INIT_TXN)
        open_flags += DB_AUTO_COMMIT;

    /* Open primary key */
    hidden_primary_key = 0;
    if ((primary_key = table_share->primary_key) >= MAX_KEY) {
        // No primary key
        primary_key = table_share->keys;
        key_used_on_scan = MAX_KEY;
        ref_length = hidden_primary_key = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    } else
        key_used_on_scan = primary_key;

    /* Need some extra memory in case of packed keys */
    max_key_length = table_share->max_key_length + MAX_REF_PARTS * 3;
    if (!(alloc_ptr =
          my_multi_malloc(MYF(MY_WME),
                          &key_buff, max_key_length, 
                          &key_buff2, max_key_length, 
                          &primary_key_buff, (hidden_primary_key ? 0 : table_share->key_info[table_share->primary_key].key_length), 
                          NullS)))
        TOKUDB_DBUG_RETURN(1);
    if (!(rec_buff = (uchar *) my_malloc((alloced_rec_buff_length = table_share->rec_buff_length), MYF(MY_WME)))) {
        my_free(alloc_ptr, MYF(0));
        TOKUDB_DBUG_RETURN(1);
    }

    /* Init shared structure */
    if (!(share = get_share(name, table))) {
        my_free((char *) rec_buff, MYF(0));
        my_free(alloc_ptr, MYF(0));
        TOKUDB_DBUG_RETURN(1);
    }
    /* Make sorted list of primary key parts, if they exist*/
    if (!hidden_primary_key) {
        uint num_prim_key_parts = table_share->key_info[table_share->primary_key].key_parts;
        primary_key_offsets = (PRIM_KEY_PART_INFO *)my_malloc(
            num_prim_key_parts*sizeof(*primary_key_offsets), 
            MYF(MY_WME)
            );
        
        if (!primary_key_offsets) {
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            my_free(alloc_ptr, MYF(0));
            TOKUDB_DBUG_RETURN(1);
        }
        for (uint i = 0; i < table_share->key_info[table_share->primary_key].key_parts; i++) {
            primary_key_offsets[i].offset = table_share->key_info[table_share->primary_key].key_part[i].offset;
            primary_key_offsets[i].part_index = i;
        }
        qsort(
            primary_key_offsets, // start of array
            num_prim_key_parts, //num elements
            sizeof(*primary_key_offsets), //size of each element
            primary_key_part_compare
            );
    }

    thr_lock_data_init(&share->lock, &lock, NULL);
    bzero((void *) &current_row, sizeof(current_row));

    /* Fill in shared structure, if needed */
    pthread_mutex_lock(&share->mutex);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN)
        TOKUDB_TRACE("tokudbopen:%p:share=%p:file=%p:table=%p:table->s=%p:%d\n", 
                     this, share, share->file, table, table->s, share->use_count);
    if (!share->use_count++) {
        DBUG_PRINT("info", ("share->use_count %u", share->use_count));
        DBT cmp_byte_stream;

        if ((error = db_create(&share->file, db_env, 0))) {
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            my_free(alloc_ptr, MYF(0));
            if (primary_key_offsets) my_free(primary_key_offsets, MYF(0));
            my_errno = error;
            TOKUDB_DBUG_RETURN(1);
        }

        if (!hidden_primary_key)
            share->file->app_private = (void *) (table_share->key_info + table_share->primary_key);
        if (tokudb_debug & TOKUDB_DEBUG_SAVE_TRACE) {
            bzero((void *) &cmp_byte_stream, sizeof(cmp_byte_stream));
            cmp_byte_stream.flags = DB_DBT_MALLOC;
            if ((error = tokutrace_db_get_cmp_byte_stream(share->file, &cmp_byte_stream))) {
                free_share(share, table, hidden_primary_key, 1);
                my_free((char *) rec_buff, MYF(0));
                my_free(alloc_ptr, MYF(0));
                if (primary_key_offsets) my_free(primary_key_offsets, MYF(0));
                my_errno = error;
                TOKUDB_DBUG_RETURN(1);
            }
            share->file->set_bt_compare(share->file, (hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_packed_key));
            my_free(cmp_byte_stream.data, MYF(0));
        }
        else
            share->file->set_bt_compare(share->file, (hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_packed_key));
        
        char newname[strlen(name) + 32];
        make_name(newname, name, "main");
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
        if ((error = share->file->open(share->file, 0, name_buff, NULL, DB_BTREE, open_flags, 0))) {
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            my_free(alloc_ptr, MYF(0));
            if (primary_key_offsets) my_free(primary_key_offsets, MYF(0));
            my_errno = error;
            TOKUDB_DBUG_RETURN(1);
        }
        if (tokudb_debug & TOKUDB_DEBUG_OPEN)
            TOKUDB_TRACE("open:%s:file=%p\n", newname, share->file);

        /* Open other keys;  These are part of the share structure */
        share->key_file[primary_key] = share->file;
        share->key_type[primary_key] = hidden_primary_key ? 0 : DB_NOOVERWRITE;

        DB **ptr = share->key_file;
        for (uint i = 0; i < table_share->keys; i++, ptr++) {
            if (i != primary_key) {
                if ((error = open_secondary_table(ptr,&table_share->key_info[i],name,mode,&share->key_type[i]))) {
                    __close(1);
                    TOKUDB_DBUG_RETURN(1);
                }
            }
        }
        /* Calculate pack_length of primary key */
        share->fixed_length_primary_key = 1;
        if (!hidden_primary_key) {
            ref_length = 0;
            KEY_PART_INFO *key_part = table->key_info[primary_key].key_part;
            KEY_PART_INFO *end = key_part + table->key_info[primary_key].key_parts;
            for (; key_part != end; key_part++)
                ref_length += key_part->field->max_packed_col_length(key_part->length);
            share->fixed_length_primary_key = (ref_length == table->key_info[primary_key].key_length);
            share->status |= STATUS_PRIMARY_KEY_INIT;
        }
        share->ref_length = ref_length;
    }
    ref_length = share->ref_length;     // If second open
    pthread_mutex_unlock(&share->mutex);

    transaction = NULL;
    cursor = NULL;
    key_read = false;
    stats.block_size = 1<<20;    // QQQ Tokudb DB block size
    share->fixed_length_row = !(table_share->db_create_options & HA_OPTION_PACK_RECORD);

    // QQQ what happens if get_status fails
    get_status();
    info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

    TOKUDB_DBUG_RETURN(0);
}

//
// Closes a handle to a table. 
//
int ha_tokudb::close(void) {
    TOKUDB_DBUG_ENTER("ha_tokudb::close %p", this);
    TOKUDB_CLOSE();
    TOKUDB_DBUG_RETURN(__close(0));
}

int ha_tokudb::__close(int mutex_is_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::__close %p", this);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) 
        TOKUDB_TRACE("close:%p\n", this);
    my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
    my_free(primary_key_offsets, MYF(MY_ALLOW_ZERO_PTR));
    ha_tokudb::reset();         // current_row buffer
    TOKUDB_DBUG_RETURN(free_share(share, table, hidden_primary_key, mutex_is_locked));
}

//
// Reallocate record buffer (rec_buff) if needed
// If not needed, does nothing
// Parameters:
//          length - size of buffer required for rec_buff
//
bool ha_tokudb::fix_rec_buff_for_blob(ulong length) {
    if (!rec_buff || length > alloced_rec_buff_length) {
        uchar *newptr;
        if (!(newptr = (uchar *) my_realloc((void *) rec_buff, length, MYF(MY_ALLOW_ZERO_PTR))))
            return 1;
        rec_buff = newptr;
        alloced_rec_buff_length = length;
    }
    return 0;
}

/* Calculate max length needed for row */
ulong ha_tokudb::max_row_length(const uchar * buf) {
    ulong length = table_share->reclength + table_share->fields * 2;
    uint *ptr, *end;
    for (ptr = table_share->blob_field, end = ptr + table_share->blob_fields; ptr != end; ptr++) {
        Field_blob *blob = ((Field_blob *) table->field[*ptr]);
        length += blob->get_length((uchar *) (buf + field_offset(blob))) + 2;
    }
    return length;
}

/*
*/
//
// take the row passed in as a DBT*, and convert it into a row in MySQL format in record
// Pack a row for storage.
// If the row is of fixed length, just store the  row 'as is'.
// If not, we will generate a packed row suitable for storage.
// This will only fail if we don't have enough memory to pack the row,
// which may only happen in rows with blobs, as the default row length is
// pre-allocated.
// Parameters:
//      [out]   row - row stored in DBT to be converted
//      [in]    record - row in MySQL format
//

int ha_tokudb::pack_row(DBT * row, const uchar * record) {
    uchar *ptr;
    int r = ENOSYS;
    bzero((void *) row, sizeof(*row));
    uint curr_skip_index;

    KEY *key_info = table->key_info + primary_key;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    //
    // two cases, fixed length row, and variable length row
    // fixed length row is first below
    //
    if (share->fixed_length_row) {
        if (hidden_primary_key) {
            row->data = (void *)record;
            row->size = table_share->reclength;
            r = 0;
            goto cleanup;
        }
        else {
            //
            // if the primary key is not hidden, then it is part of the record
            // because primary key information is already stored in the key
            // that will be passed to the fractal tree, we do not copy
            // components that belong to the primary key
            //
            if (fix_rec_buff_for_blob(table_share->reclength)) {
                r = HA_ERR_OUT_OF_MEM;
                goto cleanup;
            }

            uchar* tmp_dest = rec_buff;
            const uchar* tmp_src = record;
            uint i = 0;
            //
            // say we have 100 bytes in record, and bytes 25-50 and 75-90 belong to the primary key
            // this for loop will do a memcpy [0,25], [51,75] and [90,100]
            //
            for (i =0; i < key_info->key_parts; i++){
                uint curr_index = primary_key_offsets[i].part_index;
                uint bytes_to_copy = record + key_info->key_part[curr_index].offset - tmp_src;
                memcpy(tmp_dest,tmp_src, bytes_to_copy);
                tmp_dest += bytes_to_copy;
                tmp_src = record + key_info->key_part[curr_index].offset + key_info->key_part[curr_index].length;
            }
            memcpy(tmp_dest,tmp_src, record + table_share->reclength - tmp_src);
            tmp_dest += record + table_share->reclength - tmp_src;

            row->data = rec_buff;
            row->size = (size_t) (tmp_dest - rec_buff);

            r = 0;
            goto cleanup;
        }
    }
    
    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(record))) {
            r = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }

    /* Copy null bits */
    memcpy(rec_buff, record, table_share->null_bytes);
    ptr = rec_buff + table_share->null_bytes;

    //
    // assert that when the hidden primary key exists, primary_key_offsets is NULL
    //
    assert( (hidden_primary_key != 0) == (primary_key_offsets == NULL));
    curr_skip_index = 0;
    for (Field ** field = table->field; *field; field++) {
        uint curr_field_offset = field_offset(*field);
        //
        // if the primary key is hidden, primary_key_offsets will be NULL and
        // this clause will not execute
        //
        if (primary_key_offsets) {
            uint curr_skip_offset = primary_key_offsets[curr_skip_index].offset;
            if (curr_skip_offset == curr_field_offset) {
                //
                // we have hit a field that is a portion of the primary key
                //
                uint curr_key_index = primary_key_offsets[curr_skip_index].part_index;
                curr_skip_index++;
                //
                // only choose to continue over the key if the key's length matches the field's length
                // otherwise, we may have a situation where the column is a varchar(10), the
                // key is only the first 3 characters, and we end up losing the last 7 bytes of the
                // column
                //
                if (table->key_info[primary_key].key_part[curr_key_index].length == (*field)->field_length) {
                    continue;
                }
            }
        }
        ptr = (*field)->pack(ptr, (const uchar *)
                             (record + curr_field_offset));
    }

    row->data = rec_buff;
    row->size = (size_t) (ptr - rec_buff);
    r = 0;

cleanup:
    dbug_tmp_restore_column_map(table->write_set, old_map);

    return r;
}

//
// take the row passed in as a DBT*, and convert it into a row in MySQL format in record
// Parameters:
//      [out]   record - row in MySQL format
//      [in]    row - row stored in DBT to be converted
//
void ha_tokudb::unpack_row(uchar * record, DBT* row, DBT* key) {
    //
    // two cases, fixed length row, and variable length row
    // fixed length row is first below
    //
    if (share->fixed_length_row) {
        if (hidden_primary_key) {
            memcpy(record, (void *) row->data, table_share->reclength);
        }
        else {
            my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
            KEY *key_info = table_share->key_info + primary_key;

            uchar* tmp_dest = record;
            uchar* tmp_src = (uchar *)row->data;
            uint i = 0;

            //
            // unpack_key will fill in parts of record that are part of the primary key
            //
            unpack_key(record, key, primary_key);

            //
            // produces the opposite effect to what happened in pack_row
            // first we fill in the parts of record that are not part of the key
            //
            for (i =0; i < key_info->key_parts; i++){
                uint curr_index = primary_key_offsets[i].part_index;
                uint bytes_to_copy = record + key_info->key_part[curr_index].offset - tmp_dest;
                memcpy(tmp_dest,tmp_src, bytes_to_copy);
                tmp_src += bytes_to_copy;
                tmp_dest = record + key_info->key_part[curr_index].offset + key_info->key_part[curr_index].length;
            }
            memcpy(tmp_dest,tmp_src, record + table_share->reclength - tmp_dest);
            dbug_tmp_restore_column_map(table->write_set, old_map);
        }
    }
    else {
        /* Copy null bits */
        my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
        const uchar *ptr = (const uchar *) row->data;
        memcpy(record, ptr, table_share->null_bytes);
        ptr += table_share->null_bytes;
        if (primary_key_offsets) {
            //
            // unpack_key will fill in parts of record that are part of the primary key
            //
            unpack_key(record, key, primary_key);
        }

        //
        // fill in parts of record that are not part of the key
        //
        uint curr_skip_index = 0;
        for (Field ** field = table->field; *field; field++) {
            uint curr_field_offset = field_offset(*field);
            if (primary_key_offsets) {
                uint curr_skip_offset = primary_key_offsets[curr_skip_index].offset;
                if (curr_skip_offset == curr_field_offset) {
                    //
                    // we have hit a field that is a portion of the primary key
                    //
                    uint curr_key_index = primary_key_offsets[curr_skip_index].part_index;
                    curr_skip_index++;
                    //
                    // only choose to continue over the key if the key's length matches the field's length
                    // otherwise, we may have a situation where the column is a varchar(10), the
                    // key is only the first 3 characters, and we end up losing the last 7 bytes of the
                    // column
                    //
                    if (table->key_info[primary_key].key_part[curr_key_index].length == (*field)->field_length) {
                        continue;
                    }
                }
            }
            ptr = (*field)->unpack(record + field_offset(*field), ptr);
        }
        dbug_tmp_restore_column_map(table->write_set, old_map);
    }
}


//
// Store the key and the primary key into the row
// Parameters:
//      [out]   record - key stored in MySQL format
//      [in]    key - key stored in DBT to be converted
//              index -index into key_file that represents the DB 
//                  unpacking a key of
//
void ha_tokudb::unpack_key(uchar * record, DBT * key, uint index) {
    KEY *key_info = table->key_info + index;
    KEY_PART_INFO *key_part = key_info->key_part, *end = key_part + key_info->key_parts;
    uchar *pos = (uchar *) key->data;

    for (; key_part != end; key_part++) {
        if (key_part->null_bit) {
            if (!*pos++) {        // Null value
                /*
                   We don't need to reset the record data as we will not access it
                   if the null data is set
                 */
                record[key_part->null_offset] |= key_part->null_bit;
                continue;
            }
            record[key_part->null_offset] &= ~key_part->null_bit;
        }
        /* tokutek change to make pack_key and unpack_key work for
           decimals */
        uint unpack_length = key_part->length;
        if (key_part->field->type() == MYSQL_TYPE_NEWDECIMAL) {
            Field_new_decimal *field_nd = (Field_new_decimal *) key_part->field;
            unpack_length += field_nd->precision << 8;
        }
        pos = (uchar *) key_part->field->unpack_key(record + field_offset(key_part->field), pos,
#if MYSQL_VERSION_ID < 50123
                                                    unpack_length);
#else
                                                    unpack_length, table->s->db_low_byte_first);
#endif
    }
}


//
// Create a packed key from a row. This key will be written as such
// to the index tree.  This will never fail as the key buffer is pre-allocated.
// Parameters:
//      [out]   key - DBT that holds the key
//      [in]    key_info - holds data about the key, such as it's length and offset into record
//      [out]   buff - buffer that will hold the data for key (unless 
//                  we have a hidden primary key)
//      [in]    record - row from which to create the key
//              key_length - currently set to MAX_KEY_LENGTH, is it size of buff?
// Returns:
//      the parameter key
//

DBT* ha_tokudb::create_dbt_key_from_key(DBT * key, KEY* key_info, uchar * buff, const uchar * record, int key_length) {
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    key->data = buff;
    for (; key_part != end && key_length > 0; key_part++) {
        //
        // accessing key_part->field->null_bit instead off key_part->null_bit
        // because key_part->null_bit is not set in add_index
        // filed ticket 862 to look into this
        //
        if (key_part->field->null_bit) {
            /* Store 0 if the key part is a NULL part */
            if (record[key_part->null_offset] & key_part->field->null_bit) {
                *buff++ = 0;
                //
                // fractal tree does not handle this falg at the moment
                // so commenting out for now
                //
                //key->flags |= DB_DBT_DUPOK;
                continue;
            }
            *buff++ = 1;        // Store NOT NULL marker
        }
        //
        // accessing field_offset(key_part->field) instead off key_part->offset
        // because key_part->offset is SET INCORRECTLY in add_index
        // filed ticket 862 to look into this
        //
        buff = key_part->field->pack_key(buff, (uchar *) (record + field_offset(key_part->field)),
#if MYSQL_VERSION_ID < 50123
                                         key_part->length);
#else
                                         key_part->length, table->s->db_low_byte_first);
#endif
        key_length -= key_part->length;
    }
    key->size = (buff - (uchar *) key->data);
    DBUG_DUMP("key", (uchar *) key->data, key->size);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    return key;
}


//
// Create a packed key from a row. This key will be written as such
// to the index tree.  This will never fail as the key buffer is pre-allocated.
// Parameters:
//      [out]   key - DBT that holds the key
//              keynr - index for which to create the key
//      [out]   buff - buffer that will hold the data for key (unless 
//                  we have a hidden primary key)
//      [in]    record - row from which to create the key
//              key_length - currently set to MAX_KEY_LENGTH, is it size of buff?
// Returns:
//      the parameter key
//
DBT *ha_tokudb::create_dbt_key_from_table(DBT * key, uint keynr, uchar * buff, const uchar * record, int key_length) {
    TOKUDB_DBUG_ENTER("ha_tokudb::create_dbt_key_from_table");
    bzero((void *) key, sizeof(*key));
    if (hidden_primary_key && keynr == primary_key) {
        key->data = current_ident;
        key->size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        DBUG_RETURN(key);
    }
    DBUG_RETURN(create_dbt_key_from_key(key, &table->key_info[keynr],buff,record,key_length));
}


//
// Create a packed key from from a MySQL unpacked key (like the one that is
// sent from the index_read() This key is to be used to read a row
// Parameters:
//      [out]   key - DBT that holds the key
//              keynr - index for which to pack the key
//      [out]   buff - buffer that will hold the data for key
//      [in]    key_ptr - MySQL unpacked key
//              key_length - length of key_ptr
// Returns:
//      the parameter key
//
DBT *ha_tokudb::pack_key(DBT * key, uint keynr, uchar * buff, const uchar * key_ptr, uint key_length) {
    TOKUDB_DBUG_ENTER("ha_tokudb::pack_key");
    KEY *key_info = table->key_info + keynr;
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    bzero((void *) key, sizeof(*key));
    key->data = buff;

    for (; key_part != end && (int) key_length > 0; key_part++) {
        uint offset = 0;
        if (key_part->null_bit) {
            if (!(*buff++ = (*key_ptr == 0)))   // Store 0 if NULL
            {
                key_length -= key_part->store_length;
                key_ptr += key_part->store_length;
                //
                // fractal tree does not handle this falg at the moment
                // so commenting out for now
                //
                //key->flags |= DB_DBT_DUPOK;
                continue;
            }
            offset = 1;         // Data is at key_ptr+1
        }
        buff = key_part->field->pack_key_from_key_image(buff, (uchar *) key_ptr + offset,
#if MYSQL_VERSION_ID < 50123
                                                        key_part->length);
#else
                                                        key_part->length, table->s->db_low_byte_first);
#endif
        key_ptr += key_part->store_length;
        key_length -= key_part->store_length;
    }
    key->size = (buff - (uchar *) key->data);
    DBUG_DUMP("key", (uchar *) key->data, key->size);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    DBUG_RETURN(key);
}

int ha_tokudb::read_last() {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_last");
    int do_commit = 0;
    if (transaction == NULL && (tokudb_init_flags & DB_INIT_TXN)) {
        int r = db_env->txn_begin(db_env, 0, &transaction, 0);
        assert(r == 0);
        do_commit = 1;
    }
    int error = index_init(primary_key, 0);
    if (error == 0)
        error = index_last(table->record[1]);
    index_end();
    if (do_commit) {
        int r = transaction->commit(transaction, 0);
        assert(r == 0);
        transaction = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

/** @brief
    Get status information that is stored in the 'status' sub database
    and the max used value for the hidden primary key.
*/
void ha_tokudb::get_status() {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_status");

    if (!test_all_bits(share->status, (STATUS_PRIMARY_KEY_INIT | STATUS_ROW_COUNT_INIT))) {
        pthread_mutex_lock(&share->mutex);
        if (!(share->status & STATUS_PRIMARY_KEY_INIT)) {
            (void) extra(HA_EXTRA_KEYREAD);
            int error = read_last();
            (void) extra(HA_EXTRA_NO_KEYREAD);
            if (error == 0) {
                share->auto_ident = uint5korr(current_ident);

                // mysql may not initialize the next_number_field here
                // so we do this in the get_auto_increment method
                // index_last uses record[1]
                assert(table->next_number_field == 0);
                if (table->next_number_field) {
                    share->last_auto_increment = table->next_number_field->val_int_offset(table->s->rec_buff_length);
                    if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT) 
                        TOKUDB_TRACE("init auto increment:%lld\n", share->last_auto_increment);
                }
            }
        }

        if (!share->status_block) {
            char name_buff[FN_REFLEN];
            char newname[get_name_length(share->table_name) + 32];
            make_name(newname, share->table_name, "status");
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            uint open_mode = (((table->db_stat & HA_READ_ONLY) ? DB_RDONLY : 0)
                              | DB_THREAD);
            if (tokudb_debug & TOKUDB_DEBUG_OPEN)
                TOKUDB_TRACE("open:%s\n", newname);
            if (!db_create(&share->status_block, db_env, 0)) {
                if (share->status_block->open(share->status_block, NULL, name_buff, NULL, DB_BTREE, open_mode, 0)) {
                    share->status_block->close(share->status_block, 0);
                    share->status_block = 0;
                }
            }
        }

        if (!(share->status & STATUS_ROW_COUNT_INIT) && share->status_block) {
            share->org_rows = share->rows = table_share->max_rows ? table_share->max_rows : HA_TOKUDB_MAX_ROWS;
            DB_TXN *transaction = NULL;
            int r = 0;
            if (tokudb_init_flags & DB_INIT_TXN)
                r = db_env->txn_begin(db_env, 0, &transaction, 0);
            if (r == 0) {
                r = share->status_block->cursor(share->status_block, transaction, &cursor, 0);
                if (r == 0) {
                    DBT row;
                    char rec_buff[64];
                    bzero((void *) &row, sizeof(row));
                    bzero((void *) &last_key, sizeof(last_key));
                    row.data = rec_buff;
                    row.ulen = sizeof(rec_buff);
                    row.flags = DB_DBT_USERMEM;
                    if (!cursor->c_get(cursor, &last_key, &row, DB_FIRST)) {
                        uint i;
                        uchar *pos = (uchar *) row.data;
                        share->org_rows = share->rows = uint4korr(pos);
                        pos += 4;
                        for (i = 0; i < table_share->keys; i++) {
                            share->rec_per_key[i] = uint4korr(pos);
                            pos += 4;
                        }
                    }
                    cursor->c_close(cursor);
                }
                if (transaction) {
                    r = transaction->commit(transaction, 0);
                }
                transaction = NULL;
            }
            cursor = NULL;
        }
        share->status |= STATUS_PRIMARY_KEY_INIT | STATUS_ROW_COUNT_INIT;
        pthread_mutex_unlock(&share->mutex);
    }
    DBUG_VOID_RETURN;
}

static int write_status(DB * status_block, char *buff, uint length) {
    TOKUDB_DBUG_ENTER("write_status");
    DBT row, key;
    int error;
    const char *key_buff = "status";

    bzero((void *) &row, sizeof(row));
    bzero((void *) &key, sizeof(key));
    row.data = buff;
    key.data = (void *) key_buff;
    key.size = sizeof(key_buff);
    row.size = length;
    error = status_block->put(status_block, 0, &key, &row, 0);
    TOKUDB_DBUG_RETURN(error);
}

static void update_status(TOKUDB_SHARE * share, TABLE * table) {
    TOKUDB_DBUG_ENTER("update_status");
    if (share->rows != share->org_rows || (share->status & STATUS_TOKUDB_ANALYZE)) {
        pthread_mutex_lock(&share->mutex);
        if (!share->status_block) {
            /*
               Create sub database 'status' if it doesn't exist from before
               (This '*should*' always exist for table created with MySQL)
             */

            char name_buff[FN_REFLEN];
            char newname[get_name_length(share->table_name) + 32];
            make_name(newname, share->table_name, "status");
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            if (db_create(&share->status_block, db_env, 0))
                goto end;
            share->status_block->set_flags(share->status_block, 0);
            if (share->status_block->open(share->status_block, NULL, name_buff, NULL, DB_BTREE, DB_THREAD | DB_CREATE, my_umask))
                goto end;
        }
        {
            char rec_buff[4 + MAX_KEY * 4], *pos = rec_buff;
            int4store(pos, share->rows);
            pos += 4;
            for (uint i = 0; i < table->s->keys; i++) {
                int4store(pos, share->rec_per_key[i]);
                pos += 4;
            }
            DBUG_PRINT("info", ("updating status for %s", share->table_name));
            (void) write_status(share->status_block, rec_buff, (uint) (pos - rec_buff));
            share->status &= ~STATUS_TOKUDB_ANALYZE;
            share->org_rows = share->rows;
        }
      end:
        pthread_mutex_unlock(&share->mutex);
    }
    DBUG_VOID_RETURN;
}

/** @brief
    Return an estimated of the number of rows in the table.
    Used when sorting to allocate buffers and by the optimizer.
    This is used in filesort.cc. 
*/
ha_rows ha_tokudb::estimate_rows_upper_bound() {
    TOKUDB_DBUG_ENTER("ha_tokudb::estimate_rows_upper_bound");
    DBUG_RETURN(share->rows + HA_TOKUDB_EXTRA_ROWS);
}

int ha_tokudb::cmp_ref(const uchar * ref1, const uchar * ref2) {
    if (hidden_primary_key)
        return memcmp(ref1, ref2, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);

    int result;
    Field *field;
    KEY *key_info = table->key_info + table_share->primary_key;
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;

    for (; key_part != end; key_part++) {
        field = key_part->field;
        result = field->pack_cmp((const uchar *) ref1, (const uchar *) ref2, key_part->length, 0);
        if (result)
            return result;
        ref1 += field->packed_col_length((const uchar *) ref1, key_part->length);
        ref2 += field->packed_col_length((const uchar *) ref2, key_part->length);
    }

    return 0;
}

bool ha_tokudb::check_if_incompatible_data(HA_CREATE_INFO * info, uint table_changes) {
    if (table_changes < IS_EQUAL_YES)
        return COMPATIBLE_DATA_NO;
    return COMPATIBLE_DATA_YES;
}

//
// Stores a row in the table, called when handling an INSERT query
// Parameters:
//      [in]    record - a row in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::write_row(uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::write_row");
    DBT row, prim_key, key;
    int error;

    statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT) {
        table->timestamp_field->set_time();
    }
    if (table->next_number_field && record == table->record[0]) {
        update_auto_increment();
    }
    if ((error = pack_row(&row, (const uchar *) record))){
        TOKUDB_DBUG_RETURN(error);
    }
    
    if (hidden_primary_key) {
        get_auto_primary_key(current_ident);
    }

    u_int32_t put_flags = share->key_type[primary_key];
    THD *thd = ha_thd();
    if (thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
        put_flags = DB_YESOVERWRITE;
    }

    if (table_share->keys + test(hidden_primary_key) == 1) {
        error = share->file->put(share->file, transaction, create_dbt_key_from_table(&prim_key, primary_key, key_buff, record), &row, put_flags);
        last_dup_key = primary_key;
    } else {
        DB_TXN *sub_trans = transaction;
        /* QQQ Don't use sub transactions in temporary tables */
        for (uint retry = 0; retry < tokudb_trans_retry; retry++) {
            key_map changed_keys(0);
            if (!(error = share->file->put(share->file, sub_trans, create_dbt_key_from_table(&prim_key, primary_key, key_buff, record), &row, put_flags))) {
                changed_keys.set_bit(primary_key);
                for (uint keynr = 0; keynr < table_share->keys; keynr++) {
                    if (keynr == primary_key)
                        continue;
                    put_flags = share->key_type[keynr];
                    if (put_flags == DB_NOOVERWRITE && thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS))
                        put_flags = DB_YESOVERWRITE;
                    if ((error = share->key_file[keynr]->put(share->key_file[keynr], sub_trans, create_dbt_key_from_table(&key, keynr, key_buff2, record), &prim_key, put_flags))) {
                        last_dup_key = keynr;
                        break;
                    }
                    changed_keys.set_bit(keynr);
                }
            } 
            else {
                last_dup_key = primary_key;
            }
            if (error) {
                /* Remove inserted row */
                DBUG_PRINT("error", ("Got error %d", error));
                if (using_ignore) {
                    int new_error = 0;
                    if (!changed_keys.is_clear_all()) {
                        new_error = 0;
                        for (uint keynr = 0; keynr < table_share->keys + test(hidden_primary_key); keynr++) {
                            if (changed_keys.is_set(keynr)) {
                                if ((new_error = remove_key(sub_trans, keynr, record, &prim_key)))
                                    break;
                            }
                        }
                    }
                    if (new_error) {
                        error = new_error;      // This shouldn't happen
                        break;
                    }
                }
            }
            if (error != DB_LOCK_DEADLOCK && error != DB_LOCK_NOTGRANTED)
                break;
        }
    }
    if (error == DB_KEYEXIST)
        error = HA_ERR_FOUND_DUPP_KEY;
    else if (!error)
        changed_rows++;
    TOKUDB_DBUG_RETURN(error);
}

/* Compare if a key in a row has changed */
int ha_tokudb::key_cmp(uint keynr, const uchar * old_row, const uchar * new_row) {
    KEY_PART_INFO *key_part = table->key_info[keynr].key_part;
    KEY_PART_INFO *end = key_part + table->key_info[keynr].key_parts;

    for (; key_part != end; key_part++) {
        if (key_part->null_bit) {
            if ((old_row[key_part->null_offset] & key_part->null_bit) != (new_row[key_part->null_offset] & key_part->null_bit))
                return 1;
        }
        if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART)) {

            if (key_part->field->cmp_binary((uchar *) (old_row + key_part->offset), (uchar *) (new_row + key_part->offset), (ulong) key_part->length))
                return 1;
        } else {
            if (memcmp(old_row + key_part->offset, new_row + key_part->offset, key_part->length))
                return 1;
        }
    }
    return 0;
}


/*
  Update a row from one value to another.
  Clobbers key_buff2
*/
int ha_tokudb::update_primary_key(DB_TXN * trans, bool primary_key_changed, const uchar * old_row, DBT * old_key, const uchar * new_row, DBT * new_key, bool local_using_ignore) {
    TOKUDB_DBUG_ENTER("update_primary_key");
    DBT row;
    int error;

    if (primary_key_changed) {
        // Primary key changed or we are updating a key that can have duplicates.
        // Delete the old row and add a new one
        if (!(error = remove_key(trans, primary_key, old_row, old_key))) {
            if (!(error = pack_row(&row, new_row))) {
                if ((error = share->file->put(share->file, trans, new_key, &row, share->key_type[primary_key]))) {
                    // Probably a duplicated key; restore old key and row if needed
                    last_dup_key = primary_key;
                    if (local_using_ignore) {
                        int new_error;
                        if ((new_error = pack_row(&row, old_row)) || (new_error = share->file->put(share->file, trans, old_key, &row, share->key_type[primary_key])))
                            error = new_error;  // fatal error
                    }
                }
            }
        }
    } else {
        // Primary key didn't change;  just update the row data
        if (!(error = pack_row(&row, new_row)))
            error = share->file->put(share->file, trans, new_key, &row, 0);
    }
    TOKUDB_DBUG_RETURN(error);
}

/*
  Restore changed keys, when a non-fatal error aborts the insert/update
  of one row.
  Clobbers keybuff2
*/
int ha_tokudb::restore_keys(DB_TXN * trans, key_map * changed_keys, uint primary_key, const uchar * old_row, DBT * old_key, const uchar * new_row, DBT * new_key) {
    TOKUDB_DBUG_ENTER("restore_keys");
    int error;
    DBT tmp_key;
    uint keynr;

    /* Restore the old primary key, and the old row, but don't ignore
       duplicate key failure */
    if ((error = update_primary_key(trans, TRUE, new_row, new_key, old_row, old_key, FALSE)))
        goto err;

    /* Remove the new key, and put back the old key
       changed_keys is a map of all non-primary keys that need to be
       rolled back.  The last key set in changed_keys is the one that
       triggered the duplicate key error (it wasn't inserted), so for
       that one just put back the old value. */
    if (!changed_keys->is_clear_all()) {
        for (keynr = 0; keynr < table_share->keys + test(hidden_primary_key); keynr++) {
            if (changed_keys->is_set(keynr)) {
                if (changed_keys->is_prefix(1) && (error = remove_key(trans, keynr, new_row, new_key)))
                    break;
                if ((error = share->key_file[keynr]->put(share->key_file[keynr], trans, create_dbt_key_from_table(&tmp_key, keynr, key_buff2, old_row), old_key, share->key_type[keynr])))
                    break;
            }
        }
    }

  err:
    DBUG_ASSERT(error != DB_KEYEXIST);
    TOKUDB_DBUG_RETURN(error);
}

//
// Updates a row in the table, called when handling an UPDATE query
// Parameters:
//      [in]    old_row - row to be updated, in MySQL format
//      [in]    new_row - new row, in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::update_row(const uchar * old_row, uchar * new_row) {
    TOKUDB_DBUG_ENTER("update_row");
    DBT prim_key, key, old_prim_key;
    int error;
    DB_TXN *sub_trans;
    bool primary_key_changed;

    LINT_INIT(error);
    statistic_increment(table->in_use->status_var.ha_update_count, &LOCK_status);
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
        table->timestamp_field->set_time();

    if (hidden_primary_key) {
        primary_key_changed = 0;
        bzero((void *) &prim_key, sizeof(prim_key));
        prim_key.data = (void *) current_ident;
        prim_key.size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        old_prim_key = prim_key;
    } else {
        create_dbt_key_from_table(&prim_key, primary_key, key_buff, new_row);

        if ((primary_key_changed = key_cmp(primary_key, old_row, new_row)))
            create_dbt_key_from_table(&old_prim_key, primary_key, primary_key_buff, old_row);
        else
            old_prim_key = prim_key;
    }

    sub_trans = transaction;
    for (uint retry = 0; retry < tokudb_trans_retry; retry++) {
        key_map changed_keys(0);
        /* Start by updating the primary key */
        if (!(error = update_primary_key(sub_trans, primary_key_changed, old_row, &old_prim_key, new_row, &prim_key, using_ignore))) {
            // Update all other keys
            for (uint keynr = 0; keynr < table_share->keys; keynr++) {
                if (keynr == primary_key)
                    continue;
                if (key_cmp(keynr, old_row, new_row) || primary_key_changed) {
                    if ((error = remove_key(sub_trans, keynr, old_row, &old_prim_key))) {
                        TOKUDB_DBUG_RETURN(error);     // Fatal error
                    }
                    changed_keys.set_bit(keynr);
                    if ((error = share->key_file[keynr]->put(share->key_file[keynr], sub_trans, create_dbt_key_from_table(&key, keynr, key_buff2, new_row), &prim_key, share->key_type[keynr]))) {
                        last_dup_key = keynr;
                        break;
                    }
                }
            }
        }
        if (error) {
            /* Remove inserted row */
            DBUG_PRINT("error", ("Got error %d", error));
            if (using_ignore) {
                int new_error = 0;
                if (!changed_keys.is_clear_all())
                    new_error = restore_keys(transaction, &changed_keys, primary_key, old_row, &old_prim_key, new_row, &prim_key);
                if (new_error) {
                    /* This shouldn't happen */
                    error = new_error;
                    break;
                }
            }
        }
        if (error != DB_LOCK_DEADLOCK && error != DB_LOCK_NOTGRANTED)
            break;
    }
    if (error == DB_KEYEXIST)
        error = HA_ERR_FOUND_DUPP_KEY;
    TOKUDB_DBUG_RETURN(error);
}

//
//
// Delete one key in key_file[keynr]
// This uses key_buff2, when keynr != primary key, so it's important that
// a function that calls this doesn't use this buffer for anything else.
// Parameters:
//      [in]    trans - transaction to be used for the delete
//              keynr - index for which a key needs to be deleted
//      [in]    record - row in MySQL format. Must delete a key for this row
//      [in]    prim_key - key for record in primary table
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::remove_key(DB_TXN * trans, uint keynr, const uchar * record, DBT * prim_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::remove_key");
    int error;
    DBT key;
    DBUG_PRINT("enter", ("index: %d", keynr));
    DBUG_PRINT("primary", ("index: %d", primary_key));
    DBUG_DUMP("prim_key", (uchar *) prim_key->data, prim_key->size);

    if (keynr == active_index && cursor)
        error = cursor->c_del(cursor, 0);
    else if (keynr == primary_key || ((table->key_info[keynr].flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)) {  // Unique key
        DBUG_PRINT("Unique key", ("index: %d", keynr));
        DBUG_ASSERT(keynr == primary_key || prim_key->data != key_buff2);
        error = share->key_file[keynr]->del(share->key_file[keynr], trans, keynr == primary_key ? prim_key : create_dbt_key_from_table(&key, keynr, key_buff2, record), 0);
    } else {
        /* QQQ use toku_db_delboth(key_file[keynr], key, val, trans);
           To delete the not duplicated key, we need to open an cursor on the
           row to find the key to be delete and delete it.
           We will never come here with keynr = primary_key
         */
        DBUG_ASSERT(keynr != primary_key && prim_key->data != key_buff2);
        DBC *tmp_cursor;
        if (!(error = share->key_file[keynr]->cursor(share->key_file[keynr], trans, &tmp_cursor, 0))) {
            if (!(error = tmp_cursor->c_get(tmp_cursor, create_dbt_key_from_table(&key, keynr, key_buff2, record), prim_key, DB_GET_BOTH))) { 
                DBUG_DUMP("cget key", (uchar *) key.data, key.size);
                error = tmp_cursor->c_del(tmp_cursor, 0);
            }
            int result = tmp_cursor->c_close(tmp_cursor);
            if (!error)
                error = result;
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Delete all keys for new_record
// Parameters:
//      [in]    trans - transaction to be used for the delete
//      [in]    record - row in MySQL format. Must delete all keys for this row
//      [in]    prim_key - key for record in primary table
//      [in]    keys - array that states if a key is set, and hence needs 
//                  removal
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::remove_keys(DB_TXN * trans, const uchar * record, DBT * prim_key, key_map * keys) {
    int result = 0;
    for (uint keynr = 0; keynr < table_share->keys + test(hidden_primary_key); keynr++) {
        if (keys->is_set(keynr)) {
            int new_error = remove_key(trans, keynr, record, prim_key);
            if (new_error) {
                result = new_error;     // Return last error
                break;          // Let rollback correct things
            }
        }
    }
    return result;
}

//
// Deletes a row in the table, called when handling a DELETE query
// Parameters:
//      [in]    record - row to be deleted, in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::delete_row(const uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::delete_row");
    int error = ENOSYS;
    DBT prim_key;
    key_map keys = table_share->keys_in_use;
    statistic_increment(table->in_use->status_var.ha_delete_count, &LOCK_status);

    create_dbt_key_from_table(&prim_key, primary_key, key_buff, record);
    if (hidden_primary_key)
        keys.set_bit(primary_key);

    /* Subtransactions may be used in order to retry the delete in
       case we get a DB_LOCK_DEADLOCK error. */
    DB_TXN *sub_trans = transaction;
    for (uint retry = 0; retry < tokudb_trans_retry; retry++) {
        error = remove_keys(sub_trans, record, &prim_key, &keys);
        if (error) {
            DBUG_PRINT("error", ("Got error %d", error));
            break;              // No retry - return error
        }
        if (error != DB_LOCK_DEADLOCK && error != DB_LOCK_NOTGRANTED)
            break;
    }
#ifdef CANT_COUNT_DELETED_ROWS
    if (!error)
        changed_rows--;
#endif
    TOKUDB_DBUG_RETURN(error);
}

//
// Initializes local cursor on DB with index keynr
// Parameters:
//          keynr - key (index) number
//          sorted - 1 if result MUST be sorted according to index
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::index_init(uint keynr, bool sorted) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_init %p %d", this, keynr);
    int error;
    DBUG_PRINT("enter", ("table: '%s'  key: %d", table_share->table_name.str, keynr));

    /*
       Under some very rare conditions (like full joins) we may already have
       an active cursor at this point
     */
    if (cursor) {
        DBUG_PRINT("note", ("Closing active cursor"));
        cursor->c_close(cursor);
    }
    active_index = keynr;
    DBUG_ASSERT(keynr <= table->s->keys);
    DBUG_ASSERT(share->key_file[keynr]);
    if ((error = share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, table->reginfo.lock_type > TL_WRITE_ALLOW_READ ? 0 : 0)))
        cursor = NULL;             // Safety
    bzero((void *) &last_key, sizeof(last_key));
    TOKUDB_DBUG_RETURN(error);
}

//
// closes the local cursor
//
int ha_tokudb::index_end() {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_end %p", this);
    int error = 0;
    if (cursor) {
        DBUG_PRINT("enter", ("table: '%s'", table_share->table_name.str));
        error = cursor->c_close(cursor);
        cursor = NULL;
    }
    active_index = MAX_KEY;
    TOKUDB_DBUG_RETURN(error);
}

//
// The funtion read_row checks whether the row was obtained from the primary table or 
// from an index table. If it was obtained from an index table, it further dereferences on
// the main table. In the end, the read_row function will manage to return the actual row
// of interest in the buf parameter.
//
// Parameters:
//              error - result of preceding DB call
//      [out]   buf - buffer for the row, in MySQL format
//              keynr - index into key_file that represents DB we are currently operating on.
//      [in]    row - the row that has been read from the preceding DB call
//      [in]    found_key - key used to retrieve the row
//              read_next - if true, DB_NOTFOUND and DB_KEYEMPTY map to HA_ERR_END_OF_FILE, 
//                  else HA_ERR_KEY_NOT_FOUND, this is a bad parameter to have and this funcitonality
//                  should not be here
//
int ha_tokudb::read_row(int error, uchar * buf, uint keynr, DBT * row, DBT * found_key, bool read_next) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_row");
    //
    // Disreputable error translation: this makes us all puke
    //
    if (error) {
        if (error == DB_NOTFOUND || error == DB_KEYEMPTY)
            error = read_next ? HA_ERR_END_OF_FILE : HA_ERR_KEY_NOT_FOUND;
        table->status = STATUS_NOT_FOUND;
        TOKUDB_DBUG_RETURN(error);
    }
    //
    // extract hidden primary key to current_ident
    //
    if (hidden_primary_key) {
        if (keynr == primary_key) {
            memcpy_fixed(current_ident, (char *) found_key->data, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        }
        else {
            memcpy_fixed(current_ident, (char *) row->data, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        }
    }
    table->status = 0;
    //
    // if the index shows that the table we read the row from was indexed on the primary key,
    // that means we have our row and can skip
    // this entire if clause. All that is required is to unpack row.
    // if the index shows that what we read was from a table that was NOT indexed on the 
    // primary key, then we must still retrieve the row, as the "row" value is indeed just
    // a primary key, whose row we must still read
    //
    if (keynr != primary_key) {
        if (key_read && found_key) {
            // TOKUDB_DBUG_DUMP("key=", found_key->data, found_key->size);
            unpack_key(buf, found_key, keynr);
            if (!hidden_primary_key) {
                // TOKUDB_DBUG_DUMP("row=", row->data, row->size);
                unpack_key(buf, row, primary_key);
            }
            TOKUDB_DBUG_RETURN(0);
        }
        //
        // create a DBT that has the same data as row,
        //
        DBT key;
        bzero((void *) &key, sizeof(key));
        key.data = key_buff;
        key.size = row->size;
        memcpy(key_buff, row->data, row->size);
        //
        // Read the data into current_row
        //
        current_row.flags = DB_DBT_REALLOC;
        if ((error = share->file->get(share->file, transaction, &key, &current_row, 0))) {
            table->status = STATUS_NOT_FOUND;
            TOKUDB_DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
        }
        // TOKUDB_DBUG_DUMP("key=", key.data, key.size);
        // TOKUDB_DBUG_DUMP("row=", row->data, row->size);
        unpack_row(buf, &current_row, &key);
    }
    else {
        unpack_row(buf, row, found_key);
    }
    if (found_key) { DBUG_DUMP("read row key", (uchar *) found_key->data, found_key->size); }
    TOKUDB_DBUG_RETURN(0);
}

//
// This is only used to read whole keys
// According to InnoDB handlerton: Positions an index cursor to the index 
// specified in keynr. Fetches the row if any
// Parameters:
//      [out]        buf - buffer for the  returned row
//                   keynr - index to use
//      [in]         key - key value, according to InnoDB, if NULL, 
//                              position cursor at start or end of index,
//                              not sure if this is done now
//                     key_len - length of key
//                     find_flag - according to InnoDB, search flags from my_base.h
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found (per InnoDB), 
//      error otherwise
//
int ha_tokudb::index_read_idx(uchar * buf, uint keynr, const uchar * key, uint key_len, enum ha_rkey_function find_flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_read_idx");
    table->in_use->status_var.ha_read_key_count++;
    current_row.flags = DB_DBT_REALLOC;
    active_index = MAX_KEY;
    TOKUDB_DBUG_RETURN(read_row(share->key_file[keynr]->get(share->key_file[keynr], transaction, pack_key(&last_key, keynr, key_buff, key, key_len), &current_row, 0), buf, keynr, &current_row, &last_key, 0));
}

//TODO: QQQ Function to tell if a key+keylen is the entire key (loop through the schema), see comparison function for ideas.
/*
if (full_key) {
    switch (find_flag) {
        case (HA_READ_PREFIX): //Synonym for HA_READ_KEY_EXACT
        case (HA_READ_KEY_EXACT):
            Just c_get DB_SET, return.
        case (HA_READ_AFTER_KEY):
            c_get DB_SET_RANGE.  If EQUAL to query, then do DB_NEXT (or is it DB_NEXT_NODUP?)
        case (HA_READ_KEY_OR_NEXT):
            c_get DB_SET_RANGE
        case (HA_READ_BEFORE_KEY):
            c_get DB_SET_RANGE, then do DB_PREV (or is it DB_PREV_NODUP)?
        case (HA_READ_KEY_OR_PREV):
            c_get DB_SET_RANGE.  If NOT EQUAL to query, then do DB_PREV (or is it DB_PREV_NODUP?)
        case (HA_READ_PREFIX_LAST_OR_PREV):
            c_get DB_SET_RANGE.  If NOT EQUAL to query, then do DB_PREV (or is it DB_PREV_NODUP?)
            if if WAS equal to the query, then
                if (NO_DUP db, just return it) else:
                do DB_NEXT_NODUP
                if found, do DB_PREV and return
                else      do DB_LAST and return
        case (HA_READ_PREFIX_LAST):
            c_get DB_SET.  if !found, return NOT FOUND
            if (NO_DUP db, just return it) else:
                do c_get DB_NEXT_NODUP
                    if found, do DB_PREV and return.
                    else      do DB_LAST and return.
        default: Crash a lot.
     }
else {
// Not full key
    switch (find_flag) {
        case (HA_READ_PREFIX): //Synonym for HA_READ_KEY_EXACT
        case (HA_READ_KEY_EXACT):
            c_get DB_SET_RANGE, then check a prefix
        case (HA_READ_AFTER_KEY):
            c_get DB_SET_RANGE, then:
                while (found && query is prefix of 'dbtfound') do:
                    c_get DB_NEXT_NODUP (Definitely NEXT_NODUP since we care about key only).
        case (HA_READ_KEY_OR_NEXT):
            c_get SET_RANGE
        case (HA_READ_BEFORE_KEY):
            c_get DB_SET_RANGE, then do DB_PREV (or is it DB_PREV_NODUP)?
        case (HA_READ_KEY_OR_PREV):
            c_get DB_SET_RANGE.  If query not a prefix of found, then DB_PREV (or is it DB_PREV_NODUP?)
        case (HA_READ_PREFIX_LAST_OR_PREV):
            c_get DB_SET_RANGE, then:
                if (found && query is prefix of whatever found) do:
                    c_get DB_NEXT till not prefix (and return the one that was)
                if (found originally but was not prefix of whatever found) do:
                    c_get DB_PREV
        case (HA_READ_PREFIX_LAST):
            c_get DB_SET_RANGE.  if !found, or query not prefix of what found, return NOT FOUND
            whlie query is prefix of whatfound, do c_get DB_NEXT till not.. then return the last one that was.
        default: Crash a lot.
     }
}
Note that sometimes if not found, will need things like DB_FIRST or DB_LAST
TODO: QQQ maybe need to pass true/1 as last parameter of read_row (this would make it
return END_OF_FILE instead of just NOT_FOUND
*/

//
// According to InnoDB handlerton: Positions an index cursor to the index 
// specified in keynr. Fetches the row if any
// Parameters:
//      [out]       buf - buffer for the  returned row
//      [in]         key - key value, according to InnoDB, if NULL, 
//                              position cursor at start or end of index,
//                              not sure if this is done now
//                    key_len - length of key
//                    find_flag - according to InnoDB, search flags from my_base.h
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found (per InnoDB), 
//          we seem to return HA_ERR_END_OF_FILE if find_flag != HA_READ_KEY_EXACT
//          TODO: investigate this for correctness
//      error otherwise
//
int ha_tokudb::index_read(uchar * buf, const uchar * key, uint key_len, enum ha_rkey_function find_flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_read %p find %d", this, find_flag);
    // TOKUDB_DBUG_DUMP("key=", key, key_len);
    DBT row;
    int error;

    table->in_use->status_var.ha_read_key_count++;
    bzero((void *) &row, sizeof(row));
    pack_key(&last_key, active_index, key_buff, key, key_len);

    switch (find_flag) {
    case HA_READ_KEY_EXACT: /* Find first record else error */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        if (error == 0) {
            DBT orig_key;
            pack_key(&orig_key, active_index, key_buff2, key, key_len);
            if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key))
                error = DB_NOTFOUND;
        }
        break;
    case HA_READ_AFTER_KEY: /* Find next rec. after key-record */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        if (error == 0) {
            DBT orig_key;
            pack_key(&orig_key, active_index, key_buff2, key, key_len);
            for (;;) {
                if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key) != 0)
                    break;
                error = cursor->c_get(cursor, &last_key, &row, DB_NEXT_NODUP);
                if (error != 0)
                    break;
            }
        }
        break;
    case HA_READ_BEFORE_KEY: /* Find next rec. before key-record */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        if (error == 0)
            error = cursor->c_get(cursor, &last_key, &row, DB_PREV);
        else if (error == DB_NOTFOUND)
            error = cursor->c_get(cursor, &last_key, &row, DB_LAST);
        break;
    case HA_READ_KEY_OR_NEXT: /* Record or next record */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        break;
    case HA_READ_KEY_OR_PREV: /* Record or previous */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        if (error == 0) {
            DBT orig_key; 
            pack_key(&orig_key, active_index, key_buff2, key, key_len);
            if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key) != 0)
                error = cursor->c_get(cursor, &last_key, &row, DB_PREV);
        }
        else if (error == DB_NOTFOUND)
            error = cursor->c_get(cursor, &last_key, &row, DB_LAST);
        break;
    case HA_READ_PREFIX_LAST_OR_PREV: /* Last or prev key with the same prefix */
        error = cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE);
        if (error == 0) {
            DBT orig_key;
            pack_key(&orig_key, active_index, key_buff2, key, key_len);
            for (;;) {
                if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key) != 0)
                    break;
                error = cursor->c_get(cursor, &last_key, &row, DB_NEXT_NODUP);
                if (error != 0)
                    break;
            }
            if (error == 0)
                error = cursor->c_get(cursor, &last_key, &row, DB_PREV);
            else if (error == DB_NOTFOUND)
                error = cursor->c_get(cursor, &last_key, &row, DB_LAST);
        }
        else if (error == DB_NOTFOUND)
            error = cursor->c_get(cursor, &last_key, &row, DB_LAST);
        break;
    default:
        TOKUDB_TRACE("unsupported:%d\n", find_flag);
        error = HA_ERR_UNSUPPORTED;
        break;
    }
    error = read_row(error, buf, active_index, &row, &last_key, 0);
    if (error && (tokudb_debug & TOKUDB_DEBUG_ERROR))
        TOKUDB_TRACE("error:%d:%d\n", error, find_flag);
    TOKUDB_DBUG_RETURN(error);
}

#if 0
/*
  Read last key is solved by reading the next key and then reading
  the previous key
*/
int ha_tokudb::index_read_last(uchar * buf, const uchar * key, uint key_len) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_read_last");
    DBT row;
    int error;
    KEY *key_info = &table->key_info[active_index];

    statistic_increment(table->in_use->status_var.ha_read_key_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));

    /* read of partial key */
    pack_key(&last_key, active_index, key_buff, key, key_len);
    /* Store for compare */
    memcpy(key_buff2, key_buff, (key_len = last_key.size));
    assert(0);
    key_info->handler.bdb_return_if_eq = 1;
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE), buf, active_index, &row, (DBT *) 0, 0);
    key_info->handler.bdb_return_if_eq = 0;
    bzero((void *) &row, sizeof(row));
    if (read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV), buf, active_index, &row, &last_key, 1) || tokudb_key_cmp(table, key_info, key_buff2, key_len))
        error = HA_ERR_KEY_NOT_FOUND;
    TOKUDB_DBUG_RETURN(error);
}
#endif

//
// Reads the next row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_next(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next");
    int error; 
    DBT row;
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT), buf, active_index, &row, &last_key, 1);
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the next row matching to the key, on success, advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
//      [in]     key - key value
//                keylen - length of key
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_next_same(uchar * buf, const uchar * key, uint keylen) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next_same %p", this);
    DBT row;
    int error;
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    /* QQQ NEXT_DUP on nodup returns EINVAL for tokudb */
    if (keylen == table->key_info[active_index].key_length && 
        !(table->key_info[active_index].flags & HA_NOSAME) && 
        !(table->key_info[active_index].flags & HA_END_SPACE_KEY)) {

        error = cursor->c_get(cursor, &last_key, &row, DB_NEXT_DUP);
        error = read_row(error, buf, active_index, &row, &last_key, 1);
    } else {
        error = read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT), buf, active_index, &row, &last_key, 1);
        if (!error &&::key_cmp_if_same(table, key, active_index, keylen))
            error = HA_ERR_END_OF_FILE;
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the previous row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_prev(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_prev");
    int error;
    DBT row;
    statistic_increment(table->in_use->status_var.ha_read_prev_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV), buf, active_index, &row, &last_key, 1);
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the first row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_first(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_first");
    int error;
    DBT row;
    statistic_increment(table->in_use->status_var.ha_read_first_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_FIRST), buf, active_index, &row, &last_key, 1);
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the last row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_last(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_last");
    int error;
    DBT row;
    statistic_increment(table->in_use->status_var.ha_read_last_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_LAST), buf, active_index, &row, &last_key, 1);
    TOKUDB_DBUG_RETURN(error);
}

//
// Initialize a scan of the table (which is why index_init is called on primary_key)
// Parameters:
//          scan - unused
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::rnd_init(bool scan) {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_init");
    int error;
    current_row.flags = DB_DBT_REALLOC;
    if (scan) {
        DB* db = share->key_file[primary_key];
        error = db->pre_acquire_read_lock(db, transaction, db->dbt_neg_infty(), NULL, db->dbt_pos_infty(), NULL);
        if (error) { goto cleanup; }
    }
    error = index_init(primary_key, 0);
cleanup:
    TOKUDB_DBUG_RETURN(error);
}

//
// End a scan of the table
//
int ha_tokudb::rnd_end() {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_end");
    TOKUDB_DBUG_RETURN(index_end());
}

//
// Read the next row in a table scan
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::rnd_next(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::ha_tokudb::rnd_next");
    int error;
    DBT row;
    //
    // The reason we do not just call index_next is that index_next 
    // increments a different variable than we do here
    //
    statistic_increment(table->in_use->status_var.ha_read_rnd_next_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));
    DBUG_DUMP("last_key", (uchar *) last_key.data, last_key.size);
    error = read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT), buf, primary_key, &row, &last_key, 1);
    TOKUDB_DBUG_RETURN(error);
}


DBT *ha_tokudb::get_pos(DBT * to, uchar * pos) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_pos");
    /* We don't need to set app_data here */
    bzero((void *) to, sizeof(*to));

    to->data = pos;
    if (share->fixed_length_primary_key)
        to->size = ref_length;
    else {
        KEY_PART_INFO *key_part = table->key_info[primary_key].key_part;
        KEY_PART_INFO *end = key_part + table->key_info[primary_key].key_parts;

        for (; key_part != end; key_part++)
            pos += key_part->field->packed_col_length(pos, key_part->length);
        to->size = (uint) (pos - (uchar *) to->data);
    }
    DBUG_DUMP("key", (const uchar *) to->data, to->size);
    DBUG_RETURN(to);
}

//
// Retrieves a row with based on the primary key saved in pos
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found
//      error otherwise
//
int ha_tokudb::rnd_pos(uchar * buf, uchar * pos) {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_pos");
    DBT db_pos;
    statistic_increment(table->in_use->status_var.ha_read_rnd_count, &LOCK_status);
    active_index = MAX_KEY;
    DBT* key = get_pos(&db_pos, pos); 
    TOKUDB_DBUG_RETURN(read_row(share->file->get(share->file, transaction, key, &current_row, 0), buf, primary_key, &current_row, key, 0));
}


int ha_tokudb::read_range_first(
    const key_range *start_key,
    const key_range *end_key,
    bool eq_range, 
    bool sorted) 
{
    TOKUDB_DBUG_ENTER("ha_tokudb::read_range_first");
    int error;
    DBT start_dbt_key;
    const DBT* start_dbt_data = NULL;
    DBT end_dbt_key;
    const DBT* end_dbt_data = NULL;
    uchar start_key_buff [table_share->max_key_length + MAX_REF_PARTS * 3];
    uchar end_key_buff [table_share->max_key_length + MAX_REF_PARTS * 3];
    bzero((void *) &start_dbt_key, sizeof(start_dbt_key));
    bzero((void *) &end_dbt_key, sizeof(end_dbt_key));



    if (start_key) {
        pack_key(&start_dbt_key, active_index, start_key_buff, start_key->key, start_key->length);
        switch (start_key->flag) {
        case HA_READ_AFTER_KEY:
            start_dbt_data = share->key_file[active_index]->dbt_pos_infty();
            break;
        default:
            start_dbt_data = share->key_file[active_index]->dbt_neg_infty();
            break;
        }
    }
    else {
        start_dbt_data = share->key_file[active_index]->dbt_neg_infty();
    }

    if (end_key) {
        pack_key(&end_dbt_key, active_index, end_key_buff, end_key->key, end_key->length);
        switch (end_key->flag) {
        case HA_READ_BEFORE_KEY:
            end_dbt_data = share->key_file[active_index]->dbt_neg_infty();
            break;
        default:
            end_dbt_data = share->key_file[active_index]->dbt_pos_infty();
            break;
        }
        
    }
    else {
        end_dbt_data = share->key_file[active_index]->dbt_pos_infty();
    }

    

    error = share->key_file[active_index]->pre_acquire_read_lock(
        share->key_file[active_index], 
        transaction, 
        start_key ? &start_dbt_key : share->key_file[active_index]->dbt_neg_infty(), 
        start_dbt_data, 
        end_key ? &end_dbt_key : share->key_file[active_index]->dbt_pos_infty(), 
        end_dbt_data
        );
    if (error){ goto cleanup; }

    error = handler::read_range_first(start_key, end_key, eq_range, sorted);

cleanup:
    TOKUDB_DBUG_RETURN(error);
}
int ha_tokudb::read_range_next()
{
    return handler::read_range_next();
}



/*
  Set a reference to the current record in (ref,ref_length).

  SYNOPSIS
  ha_tokudb::position()
  record                      The current record buffer

  DESCRIPTION
  The BDB handler stores the primary key in (ref,ref_length).
  There is either an explicit primary key, or an implicit (hidden)
  primary key.
  During open(), 'ref_length' is calculated as the maximum primary
  key length. When an actual key is shorter than that, the rest of
  the buffer must be cleared out. The row cannot be identified, if
  garbage follows behind the end of the key. There is no length
  field for the current key, so that the whole ref_length is used
  for comparison.

  RETURN
  nothing
*/
void ha_tokudb::position(const uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::position");
    DBT key;
    if (hidden_primary_key) {
        DBUG_ASSERT(ref_length == TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        memcpy_fixed(ref, (char *) current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
    } else {
        create_dbt_key_from_table(&key, primary_key, ref, record);
        if (key.size < ref_length)
            bzero(ref + key.size, ref_length - key.size);
    }
    DBUG_VOID_RETURN;
}

//
// Per InnoDB: Returns statistics information of the table to the MySQL interpreter,
// in various fields of the handle object. 
// Return:
//      0, always success
//
int ha_tokudb::info(uint flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::info %p %d %lld %ld", this, flag, share->rows, changed_rows);
    if (flag & HA_STATUS_VARIABLE) {
        // Just to get optimizations right
        stats.records = share->rows + changed_rows;
        stats.deleted = 0;
    }
    if ((flag & HA_STATUS_CONST) || version != share->version) {
        version = share->version;
        for (uint i = 0; i < table_share->keys; i++) {
            table->key_info[i].rec_per_key[table->key_info[i].key_parts - 1] = share->rec_per_key[i];
        }
    }
    /* Don't return key if we got an error for the internal primary key */
    if (flag & HA_STATUS_ERRKEY && last_dup_key < table_share->keys)
        errkey = last_dup_key;
    TOKUDB_DBUG_RETURN(0);
}

//
//  Per InnoDB: Tells something additional to the handler about how to do things.
//
int ha_tokudb::extra(enum ha_extra_function operation) {
    TOKUDB_DBUG_ENTER("extra %p %d", this, operation);
    switch (operation) {
    case HA_EXTRA_RESET_STATE:
        reset();
        break;
    case HA_EXTRA_KEYREAD:
        key_read = 1;           // Query satisfied with key
        break;
    case HA_EXTRA_NO_KEYREAD:
        key_read = 0;
        break;
    case HA_EXTRA_IGNORE_DUP_KEY:
        using_ignore = 1;
        break;
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
        using_ignore = 0;
        break;
    default:
        break;
    }
    TOKUDB_DBUG_RETURN(0);
}

int ha_tokudb::reset(void) {
    TOKUDB_DBUG_ENTER("ha_tokudb::reset");
    key_read = 0;
    using_ignore = 0;
    if (current_row.flags & (DB_DBT_MALLOC | DB_DBT_REALLOC)) {
        current_row.flags = 0;
        if (current_row.data) {
            free(current_row.data);
            current_row.data = 0;
        }
    }
    TOKUDB_DBUG_RETURN(0);
}

/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement to be able to rollback the statement.
  If not, we have to start a master transaction if there doesn't exist
  one from before.
*/
//
// Parameters:
//      [in]    thd - handle to the user thread
//              lock_type - the type of lock
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::external_lock(THD * thd, int lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::external_lock %d", thd_sql_command(thd));
    // QQQ this is here to allow experiments without transactions
    if ((tokudb_init_flags & DB_INIT_TXN) == 0) 
        TOKUDB_DBUG_RETURN(0);
    int error = 0;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        trx = (tokudb_trx_data *)
            my_malloc(sizeof(*trx), MYF(MY_ZEROFILL));
        if (!trx)
            TOKUDB_DBUG_RETURN(1);
        thd_data_set(thd, tokudb_hton->slot, trx);
    }
    if (trx->all == 0)
        trx->sp_level = 0;
    if (lock_type != F_UNLCK) {
        if (!trx->tokudb_lock_count++) {
            DBUG_ASSERT(trx->stmt == 0);
            transaction = NULL;    // Safety
            /* First table lock, start transaction */
            if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK)) && !trx->all) {
                /* QQQ We have to start a master transaction */
                DBUG_PRINT("trans", ("starting transaction all:  options: 0x%lx", (ulong) thd->options));
                if ((error = db_env->txn_begin(db_env, NULL, &trx->all, 0))) {
                    trx->tokudb_lock_count--;      // We didn't get the lock
                    TOKUDB_DBUG_RETURN(error);
                }
                if (tokudb_debug & TOKUDB_DEBUG_TXN)
                    TOKUDB_TRACE("master:%p\n", trx->all);
                trx->sp_level = trx->all;
                trans_register_ha(thd, TRUE, tokudb_hton);
                if (thd->in_lock_tables)
                    TOKUDB_DBUG_RETURN(0);     // Don't create stmt trans
            }
            DBUG_PRINT("trans", ("starting transaction stmt"));
	    if (trx->stmt) 
                if (tokudb_debug & TOKUDB_DEBUG_TXN) 
                    TOKUDB_TRACE("warning:stmt=%p\n", trx->stmt);
            if ((error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, 0))) {
                /* We leave the possible master transaction open */
                trx->tokudb_lock_count--;  // We didn't get the lock
                TOKUDB_DBUG_RETURN(error);
            }
            if (tokudb_debug & TOKUDB_DEBUG_TXN)
                TOKUDB_TRACE("stmt:%p:%p\n", trx->sp_level, trx->stmt);
            trans_register_ha(thd, FALSE, tokudb_hton);
        }
        transaction = trx->stmt;
    } else {
        lock.type = TL_UNLOCK;  // Unlocked
        thread_safe_add(share->rows, changed_rows, &share->mutex);
        changed_rows = 0;
        if (!--trx->tokudb_lock_count) {
            if (trx->stmt) {
                /*
                   F_UNLCK is done without a transaction commit / rollback.
                   This happens if the thread didn't update any rows
                   We must in this case commit the work to keep the row locks
                 */
                DBUG_PRINT("trans", ("commiting non-updating transaction"));
                error = trx->stmt->commit(trx->stmt, 0);
                if (tokudb_debug & TOKUDB_DEBUG_TXN)
                    TOKUDB_TRACE("commit:%p:%d\n", trx->stmt, error);
                trx->stmt = transaction = NULL;
            }
        }
    }
    TOKUDB_DBUG_RETURN(error);
}


/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_tokudb::start_stmt(THD * thd, thr_lock_type lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::start_stmt");
    if (!(tokudb_init_flags & DB_INIT_TXN)) 
        TOKUDB_DBUG_RETURN(0);
    int error = 0;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    DBUG_ASSERT(trx);
    /*
       note that trx->stmt may have been already initialized as start_stmt()
       is called for *each table* not for each storage engine,
       and there could be many bdb tables referenced in the query
     */
    if (!trx->stmt) {
        DBUG_PRINT("trans", ("starting transaction stmt"));
        error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, 0);
        trans_register_ha(thd, FALSE, tokudb_hton);
    }
    transaction = trx->stmt;
    TOKUDB_DBUG_RETURN(error);
}

/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Tokudb DB changes all WRITE locks to TL_WRITE_ALLOW_WRITE (which
  signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.
*/

THR_LOCK_DATA **ha_tokudb::store_lock(THD * thd, THR_LOCK_DATA ** to, enum thr_lock_type lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::store_lock, lock_type=%d", lock_type);
    if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
        /* If we are not doing a LOCK TABLE, then allow multiple writers */
        if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) && !thd->in_lock_tables)
            lock_type = TL_WRITE_ALLOW_WRITE;
        lock.type = lock_type;
    }
    *to++ = &lock;
    DBUG_RETURN(to);
}


static int create_sub_table(const char *table_name, const char *sub_name, DBTYPE type, int flags) {
    TOKUDB_DBUG_ENTER("create_sub_table");
    int error;
    DB *file;
    DBUG_PRINT("enter", ("sub_name: %s  flags: %d", sub_name, flags));

    if (!(error = db_create(&file, db_env, 0))) {
        file->set_flags(file, flags);
        error = (file->open(file, NULL, table_name, sub_name, type, DB_THREAD | DB_CREATE, my_umask));
        if (error) {
            DBUG_PRINT("error", ("Got error: %d when opening table '%s'", error, table_name));
            (void) file->remove(file, table_name, NULL, 0);
        } else
            (void) file->close(file, 0);
    } else {
        DBUG_PRINT("error", ("Got error: %d when creating table", error));
    }
    if (error)
        my_errno = error;
    TOKUDB_DBUG_RETURN(error);
}

static int mkdirpath(char *name, mode_t mode) {
    int r = mkdir(name, mode);
    if (r == -1 && errno == ENOENT) {
        char parent[strlen(name)+1];
        strcpy(parent, name);
        char *cp = strrchr(parent, '/');
        if (cp) {
            *cp = 0;
            r = mkdir(parent, 0755);
            if (r == 0)
                r = mkdir(name, mode);
        }
    }
    return r;
}

#include <dirent.h>

static int rmall(const char *dname) {
    int error = 0;
    DIR *d = opendir(dname);
    if (d) {
        struct dirent *dirent;
        while ((dirent = readdir(d)) != 0) {
            if (0 == strcmp(dirent->d_name, ".") || 0 == strcmp(dirent->d_name, ".."))
                continue;
            char fname[strlen(dname) + 1 + strlen(dirent->d_name) + 1];
            sprintf(fname, "%s/%s", dname, dirent->d_name);
            if (dirent->d_type == DT_DIR) {
                error = rmall(fname);
            } 
            else {
                if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
                    TOKUDB_TRACE("removing:%s\n", fname);
                }
                //
                // if clause checks if the file is a .tokudb file
                //
                if (strlen(fname) >= strlen (ha_tokudb_ext) &&
                    strcmp(fname + (strlen(fname) - strlen(ha_tokudb_ext)), ha_tokudb_ext) == 0) 
                {
                    //
                    // if this fails under low memory conditions, gracefully exit and return error
                    // user will be notified that something went wrong, and he will
                    // have to deal with it
                    //
                    DB* db = NULL;
                    error = db_create(&db, db_env, 0);
                    if (error) {
                        break;
                    }
                    //
                    // it is ok to do db->remove on any .tokudb file, because any such
                    // file was created with db->open
                    //
                    db->remove(db, fname, NULL, 0);
                }
                else {
                    //
                    // in case we have some file that is not .tokudb, we just delete it
                    //
                    error = unlink(fname);
                    if (error != 0) {
                        error = errno;
                        break;
                    }
                }
            }
        }
        closedir(d);
        if (error == 0) {
            error = rmdir(dname);
            if (error != 0)
                error = errno;
        }
    } 
    else {
        error = errno;
    }
    return error;
}

//
// Creates a new table
// Parameters:
//      [in]    name - table name
//      [in]    form - info on table, columns and indexes
//      [in]    create_info - more info on table, CURRENTLY UNUSED
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::create(const char *name, TABLE * form, HA_CREATE_INFO * create_info) {
    TOKUDB_DBUG_ENTER("ha_tokudb::create");
    char name_buff[FN_REFLEN];
    int error;
    char dirname[get_name_length(name) + 32];
    char newname[get_name_length(name) + 32];

    uint i;

    //
    // tracing information about what type of table we are creating
    //
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        for (i = 0; i < form->s->fields; i++) {
            Field *field = form->s->field[i];
            TOKUDB_TRACE("field:%d:%s:type=%d:flags=%x\n", i, field->field_name, field->type(), field->flags);
        }
        for (i = 0; i < form->s->keys; i++) {
            KEY *key = &form->s->key_info[i];
            TOKUDB_TRACE("key:%d:%s:%d\n", i, key->name, key->key_parts);
            uint p;
            for (p = 0; p < key->key_parts; p++) {
                KEY_PART_INFO *key_part = &key->key_part[p];
                Field *field = key_part->field;
                TOKUDB_TRACE("key:%d:%d:length=%d:%s:type=%d:flags=%x\n",
                             i, p, key_part->length, field->field_name, field->type(), field->flags);
            }
        }
    }

    //
    // check if auto increment is properly defined
    // tokudb only supports auto increment on the first field in the primary key
    // or the first field in the row
    //
    int pk_found = 0;
    int ai_found = 0;
    for (i = 0; i < form->s->keys; i++) {
        KEY *key = &form->s->key_info[i];
        int is_primary = (strcmp(key->name, "PRIMARY") == 0);
        if (is_primary) pk_found = 1;
        uint p;
        for (p = 0; p < key->key_parts; p++) {
            KEY_PART_INFO *key_part = &key->key_part[p];
            Field *field = key_part->field;
            if (field->flags & AUTO_INCREMENT_FLAG) {
                ai_found = 1;
                if (is_primary && p > 0) 
                    TOKUDB_DBUG_RETURN(HA_ERR_UNSUPPORTED);
            }
        }
    }

    if (!pk_found && ai_found) {
        Field *field = form->s->field[0];
        if (!(field->flags & AUTO_INCREMENT_FLAG))
            TOKUDB_DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }

    // a table is a directory of dictionaries
    make_name(dirname, name, 0);
    error = mkdirpath(dirname, 0777);
    if (error != 0) {
        TOKUDB_DBUG_RETURN(errno);
    }

    make_name(newname, name, "main");
    fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

    /* Create the main table that will hold the real rows */
    error = create_sub_table(name_buff, NULL, DB_BTREE, 0);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN)
        TOKUDB_TRACE("create:%s:error=%d\n", newname, error);
    if (error) {
        rmall(dirname);
        TOKUDB_DBUG_RETURN(error);
    }

    primary_key = form->s->primary_key;

    /* Create the keys */
    char part[MAX_ALIAS_NAME + 10];
    for (uint i = 0; i < form->s->keys; i++) {
        if (i != primary_key) {
            sprintf(part, "key-%s", form->s->key_info[i].name);
            make_name(newname, name, part);
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            error = create_sub_table(name_buff, NULL, DB_BTREE, (form->key_info[i].flags & HA_NOSAME) ? 0 : DB_DUP + DB_DUPSORT);
            if (tokudb_debug & TOKUDB_DEBUG_OPEN)
                TOKUDB_TRACE("create:%s:flags=%ld:error=%d\n", newname, form->key_info[i].flags, error);
            if (error) {
                rmall(dirname);
                TOKUDB_DBUG_RETURN(error);
            }
        }
    }

    /* Create the status block to save information from last status command */
    /* QQQ Is DB_BTREE the best option here ? (QUEUE can't be used in sub tables) */
    // QQQ what is the status DB used for?

    DB *status_block;
    if (!(error = (db_create(&status_block, db_env, 0)))) {
        make_name(newname, name, "status");
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

        if (!(error = (status_block->open(status_block, NULL, name_buff, NULL, DB_BTREE, DB_CREATE, 0)))) {
            char rec_buff[4 + MAX_KEY * 4];
            uint length = 4 + form->s->keys * 4;
            bzero(rec_buff, length);
            error = write_status(status_block, rec_buff, length);
            status_block->close(status_block, 0);
        }
        if (tokudb_debug & TOKUDB_DEBUG_OPEN)
            TOKUDB_TRACE("create:%s:error=%d\n", newname, error);

    }

    if (error)
        rmall(dirname);
    TOKUDB_DBUG_RETURN(error);
}

//
// Drops table
// Parameters:
//      [in]    name - name of table to be deleted
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::delete_table(const char *name) {
    TOKUDB_DBUG_ENTER("ha_tokudb::delete_table");
    int error;
#if 0 // QQQ single file per table
    char name_buff[FN_REFLEN];
    char newname[strlen(name) + 32];

    sprintf(newname, "%s/main", name);
    fn_format(name_buff, newname, "", ha_tokudb_ext, MY_UNPACK_FILENAME | MY_APPEND_EXT);
    error = db_create(&file, db_env, 0);
    if (error != 0)
        goto exit;
    error = file->remove(file, name_buff, NULL, 0);

    sprintf(newname, "%s/status", name);
    fn_format(name_buff, newname, "", ha_tokudb_ext, MY_UNPACK_FILENAME | MY_APPEND_EXT);
    error = db_create(&file, db_env, 0);
    if (error != 0)
        goto exit;
    error = file->remove(file, name_buff, NULL, 0);

  exit:
    file = 0;                   // Safety
    my_errno = error;
#else
    // remove all of the dictionaries in the table directory 
    char newname[(tokudb_data_dir ? strlen(tokudb_data_dir) : 0) + strlen(name) + 32];
    make_name(newname, name, 0);
    error = rmall(newname);
    my_errno = error;
#endif
    TOKUDB_DBUG_RETURN(error);
}


//
// renames table from "from" to "to"
// Parameters:
//      [in]    name - old name of table
//      [in]    to - new name of table
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::rename_table(const char *from, const char *to) {
    TOKUDB_DBUG_ENTER("%s %s %s", __FUNCTION__, from, to);
    int error;
#if 0 // QQQ single file per table
    char from_buff[FN_REFLEN];
    char to_buff[FN_REFLEN];

    if ((error = db_create(&file, db_env, 0)))
        my_errno = error;
    else {
        /* On should not do a file->close() after rename returns */
        error = file->rename(file,
                             fn_format(from_buff, from, "", ha_tokudb_ext, MY_UNPACK_FILENAME | MY_APPEND_EXT), NULL, fn_format(to_buff, to, "", ha_tokudb_ext, MY_UNPACK_FILENAME | MY_APPEND_EXT), 0);
    }
#else
    int n = get_name_length(from) + 32;
    char newfrom[n];
    make_name(newfrom, from, 0);
    n = get_name_length(to) + 32;
    char newto[n];
    make_name(newto, to, 0);
    error = rename(newfrom, newto);
    if (error != 0)
        error = my_errno = errno;
#endif
    TOKUDB_DBUG_RETURN(error);
}


/*
  Returns estimate on number of seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/
/// QQQ why divide by 3
double ha_tokudb::scan_time() {
    TOKUDB_DBUG_ENTER("ha_tokudb::scan_time");
    double ret_val = stats.records / 3;
    DBUG_RETURN(ret_val);
}

//
// Estimates the number of index records in a range. In case of errors, return
//   HA_TOKUDB_RANGE_COUNT instead of HA_POS_ERROR. This was behavior
//   when we got the handlerton from MySQL.
// Parameters:
//              keynr -index to use 
//      [in]    start_key - low end of the range
//      [in]    end_key - high end of the range
// Returns:
//      0 - There are no matching keys in the given range
//      number > 0 - There are approximately number matching rows in the range
//      HA_POS_ERROR - Something is wrong with the index tree
//
ha_rows ha_tokudb::records_in_range(uint keynr, key_range* start_key, key_range* end_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::records_in_range");
    DBT key;
    ha_rows ret_val = HA_TOKUDB_RANGE_COUNT;
    DB *kfile = share->key_file[keynr];
    u_int64_t less, equal, greater;
    u_int64_t start_rows, end_rows, rows;
    int is_exact;
    int error;

    //
    // get start_rows and end_rows values so that we can estimate range
    //
    if (start_key) {
        error = kfile->key_range64(
            kfile, 
            transaction, 
            pack_key(&key, keynr, key_buff, start_key->key, start_key->length), 
            &less,
            &equal,
            &greater,
            &is_exact
            );
        if (error) {
            ret_val = HA_TOKUDB_RANGE_COUNT;
            goto cleanup;
        }
        if (start_key->flag == HA_READ_KEY_EXACT) {
            start_rows= less;
        }
        else {
            start_rows = less + equal;
        }
    }
    else {
        start_rows= 0;
    }

    if (end_key) {
        error = kfile->key_range64(
            kfile, 
            transaction, 
            pack_key(&key, keynr, key_buff, end_key->key, end_key->length), 
            &less,
            &equal,
            &greater,
            &is_exact
            );
        if (error) {
            ret_val = HA_TOKUDB_RANGE_COUNT;
            goto cleanup;
        }
        if (end_key->flag == HA_READ_BEFORE_KEY) {
            end_rows= less;
        }
        else {
            end_rows= less + equal;
        }
    }
    else {
        end_rows = stats.records;
    }

    rows = end_rows - start_rows;

    //
    // MySQL thinks a return value of 0 means there are exactly 0 rows
    // Therefore, always return non-zero so this assumption is not made
    //
    ret_val = (ha_rows) (rows <= 1 ? 1 : rows);
cleanup:
    TOKUDB_DBUG_RETURN(ret_val);
}

void ha_tokudb::get_auto_increment(ulonglong offset, ulonglong increment, ulonglong nb_desired_values, ulonglong * first_value, ulonglong * nb_reserved_values) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_auto_increment");
    ulonglong nr;

    pthread_mutex_lock(&share->mutex);
    if (!(share->status & STATUS_AUTO_INCREMENT_INIT)) {
        share->status |= STATUS_AUTO_INCREMENT_INIT;
        int error = read_last();
        if (error == 0) {
            share->last_auto_increment = table->next_number_field->val_int_offset(table->s->rec_buff_length);
            if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT) 
                TOKUDB_TRACE("init auto increment:%lld\n", share->last_auto_increment);
        }
    }
    nr = share->last_auto_increment + increment;
    share->last_auto_increment = nr + nb_desired_values - 1;
    pthread_mutex_unlock(&share->mutex);

    if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT)
        TOKUDB_TRACE("get_auto_increment(%lld,%lld,%lld):got:%lld:%lld\n",
                     offset, increment, nb_desired_values, nr, nb_desired_values);
    *first_value = nr;
    *nb_reserved_values = nb_desired_values;
    DBUG_VOID_RETURN;
}

//
// Adds indexes to the table. Takes the array of KEY passed in key_info, and creates
// DB's that will go at the end of share->key_file. THE IMPLICIT ASSUMPTION HERE is
// that the table will be modified and that these added keys will be appended to the end
// of the array table->key_info
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_info - array of KEY's to be added
//              num_of_keys - number of keys to be added, number of elements in key_info
//  Returns:
//      0 on success, error otherwise
//
int ha_tokudb::add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::add_index");
    char name_buff[FN_REFLEN];
    int error;
    char newname[share->table_name_length + 32];
    uint curr_index = 0;
    DBC* tmp_cursor = NULL;
    int cursor_ret_val = 0;
    DBT current_primary_key;
    DBT row;
    DB_TXN* txn = NULL;
    uchar tmp_key_buff[2*table_arg->s->rec_buff_length];
    //
    // these variables are for error handling
    //
    uint num_files_created = 0;
    uint num_DB_opened = 0;
    
    //
    // in unpack_row, MySQL passes a buffer that is this long,
    // so this length should be good enough for us as well
    //
    uchar tmp_record[table_arg->s->rec_buff_length];
    bzero((void *) &current_primary_key, sizeof(current_primary_key));
    bzero((void *) &row, sizeof(row));


    //
    // The files for secondary tables are derived from the name of keys
    // If we try to add a key with the same name as an already existing key,
    // We can crash. So here we check if any of the keys added has the same
    // name of an existing key, and if so, we fail gracefully
    //
    for (uint i = 0; i < num_of_keys; i++) {
        for (uint j = 0; j < table_arg->s->keys; j++) {
            if (strcmp(key_info[i].name, table_arg->s->key_info[j].name) == 0) {
                error = HA_ERR_WRONG_COMMAND;
                goto cleanup;
            }
        }
    }
    
    //
    // first create all the DB's files
    //
    char part[MAX_ALIAS_NAME + 10];
    for (uint i = 0; i < num_of_keys; i++) {
        sprintf(part, "key-%s", key_info[i].name);
        make_name(newname, share->table_name, part);
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
        error = create_sub_table(name_buff, NULL, DB_BTREE, (key_info[i].flags & HA_NOSAME) ? 0 : DB_DUP + DB_DUPSORT);
        if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
            TOKUDB_TRACE("create:%s:flags=%ld:error=%d\n", newname, key_info[i].flags, error);
        }
        if (error) { goto cleanup; }
        num_files_created++;
    }

    //
    // open all the DB files and set the appropriate variables in share
    // they go to the end of share->key_file
    //
    curr_index = table_arg->s->keys;
    for (uint i = 0; i < num_of_keys; i++, curr_index++) {
        error = open_secondary_table(
            &share->key_file[curr_index], 
            &key_info[i],
            share->table_name,
            0,
            &share->key_type[curr_index]
            );
        if (error) { goto cleanup; }
        num_DB_opened++;
    }
    

    //
    // scan primary table, create each secondary key, add to each DB
    //
    
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    assert(error == 0);
    if ((error = share->file->cursor(share->file, txn, &tmp_cursor, 0))) {
        tmp_cursor = NULL;             // Safety
        goto cleanup;
    }

    //
    // for each element in the primary table, insert the proper key value pair in each secondary table
    // that is created
    //
    cursor_ret_val = tmp_cursor->c_get(tmp_cursor, &current_primary_key, &row, DB_NEXT);
    while (cursor_ret_val != DB_NOTFOUND) {
        if (cursor_ret_val) {
            error = cursor_ret_val;
            goto cleanup;
        }
        unpack_row(tmp_record, &row, &current_primary_key);
        for (uint i = 0; i < num_of_keys; i++) {
            DBT secondary_key;
            create_dbt_key_from_key(&secondary_key,&key_info[i], tmp_key_buff, tmp_record);
            uint curr_index = i + table_arg->s->keys;
            u_int32_t put_flags = share->key_type[curr_index];
            
            error = share->key_file[curr_index]->put(share->key_file[curr_index], txn, &secondary_key, &current_primary_key, put_flags);
            if (error) {
                //
                // in the case of any error anywhere, we can just nuke all the files created, so we dont need
                // to be tricky and try to roll back changes. That is why we commit the transaction,
                // which should be fast. The DB is going to go away anyway, so no pt in trying to keep
                // it in a good state.
                //
                txn->commit(txn, 0);
                //
                // found a duplicate in a no_dup DB
                //
                if ( (error == DB_KEYEXIST) && (key_info[i].flags & HA_NOSAME)) {
                    error = HA_ERR_FOUND_DUPP_KEY;
                    last_dup_key = i;
                    memcpy(table_arg->record[0], tmp_record, table_arg->s->rec_buff_length);
                }
                goto cleanup;
            }
        }
        cursor_ret_val = tmp_cursor->c_get(tmp_cursor, &current_primary_key, &row, DB_NEXT);
    }
    error = txn->commit(txn, 0);
    assert(error == 0);
    tmp_cursor->c_close(tmp_cursor);
    
    error = 0;
cleanup:
    if (error) {
        //
        // We need to delete all the files that may have been created
        // The DB's must be closed and removed
        //
        for (uint i = table_arg->s->keys; i < table_arg->s->keys + num_DB_opened; i++) {
            share->key_file[i]->close(share->key_file[i], 0);
            share->key_file[i] = NULL;
        }
        for (uint i = 0; i < num_files_created; i++) {
            DB* tmp;
            sprintf(part, "key-%s", key_info[i].name);
            make_name(newname, share->table_name, part);
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            if (!(db_create(&tmp, db_env, 0))) {
                tmp->remove(tmp, name_buff, NULL, 0);
            }
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Prepares to drop indexes to the table. For each value, i, in the array key_num,
// table->key_info[i] is a key that is to be dropped.
//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_num - array of indexes that specify which keys of the array table->key_info
//                  are to be dropped
//              num_of_keys - size of array, key_num
//  Returns:
//      0 on success, error otherwise
//
int ha_tokudb::prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::prepare_drop_index");
    int error;
    char name_buff[FN_REFLEN];
    char newname[share->table_name_length + 32];
    char part[MAX_ALIAS_NAME + 10];
    DB** dbs_to_remove = NULL;

    //
    // we allocate an array of DB's here to get ready for removal
    // We do this so that all potential memory allocation errors that may occur
    // will do so BEFORE we go about dropping any indexes. This way, we
    // can fail gracefully without losing integrity of data in such cases. If on
    // on the other hand, we started removing DB's, and in the middle, 
    // one failed, it is not immedietely obvious how one would rollback
    //
    dbs_to_remove = (DB **)my_malloc(sizeof(*dbs_to_remove)*num_of_keys, MYF(MY_ZEROFILL));
    if (dbs_to_remove == NULL) {
        error = ENOMEM; 
        goto cleanup;
    }
    for (uint i = 0; i < num_of_keys; i++) {
        error = db_create(&dbs_to_remove[i], db_env, 0);
        if (error) {
            goto cleanup;
        }
    }    
    
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = key_num[i];
        share->key_file[curr_index]->close(share->key_file[curr_index],0);
        share->key_file[curr_index] = NULL;
        
        sprintf(part, "key-%s", table_arg->key_info[curr_index].name);
        make_name(newname, share->table_name, part);
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

        dbs_to_remove[i]->remove(dbs_to_remove[i], name_buff, NULL, 0);
    }
cleanup:
    my_free(dbs_to_remove, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
}


//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments, therefore, we can make
// this function just return
int ha_tokudb::final_drop_index(TABLE *table_arg) {
    TOKUDB_DBUG_ENTER("ha_tokudb::final_drop_index");
    TOKUDB_DBUG_RETURN(0);
}

void ha_tokudb::print_error(int error, myf errflag) {
    if (error == DB_LOCK_DEADLOCK)
        error = HA_ERR_LOCK_DEADLOCK;
    if (error == DB_LOCK_NOTGRANTED)
        error = HA_ERR_LOCK_WAIT_TIMEOUT;
    handler::print_error(error, errflag);
}

#if 0 // QQQ use default
int ha_tokudb::analyze(THD * thd, HA_CHECK_OPT * check_opt) {
    uint i;
    DB_BTREE_STAT *stat = 0;
    DB_TXN_STAT *txn_stat_ptr = 0;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[tokudb_hton->slot];
    DBUG_ASSERT(trx);

    for (i = 0; i < table_share->keys; i++) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((key_file[i]->stat) (key_file[i], trx->all, (void *) &stat, 0))
            goto err;
        share->rec_per_key[i] = (stat->bt_ndata / (stat->bt_nkeys ? stat->bt_nkeys : 1));
    }
    /* A hidden primary key is not in key_file[] */
    if (hidden_primary_key) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((file->stat) (file, trx->all, (void *) &stat, 0))
            goto err;
    }
    pthread_mutex_lock(&share->mutex);
    share->rows = stat->bt_ndata;
    share->status |= STATUS_TOKUDB_ANALYZE;        // Save status on close
    share->version++;           // Update stat in table
    pthread_mutex_unlock(&share->mutex);
    update_status(share, table);        // Write status to file
    if (stat)
        free(stat);
    return ((share->status & STATUS_TOKUDB_ANALYZE) ? HA_ADMIN_FAILED : HA_ADMIN_OK);

  err:
    if (stat)
        free(stat);
    return HA_ADMIN_FAILED;
}
#endif

#if 0 // QQQ use default
int ha_tokudb::optimize(THD * thd, HA_CHECK_OPT * check_opt) {
    return ha_tokudb::analyze(thd, check_opt);
}
#endif

#if 0 // QQQ use default
int ha_tokudb::check(THD * thd, HA_CHECK_OPT * check_opt) {
    TOKUDB_DBUG_ENTER("ha_tokudb::check");
    TOKUDB_DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    // look in old_ha_tokudb.cc for a start of an implementation
}
#endif

ulong ha_tokudb::field_offset(Field *field) {
    if (table->record[0] <= field->ptr && field->ptr < table->record[1])
        return field->offset(table->record[0]);
    assert(0);
    return 0;
}

struct st_mysql_storage_engine storage_engine_structure = { MYSQL_HANDLERTON_INTERFACE_VERSION };

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

static MYSQL_SYSVAR_UINT(cache_memory_percent, tokudb_cache_memory_percent, PLUGIN_VAR_READONLY, "Default percent of physical memory in the TokuDB cache table", NULL, NULL, tokudb_cache_memory_percent, 0, 100, 0);

static MYSQL_SYSVAR_ULONG(max_lock, tokudb_max_lock, PLUGIN_VAR_READONLY, "TokuDB Max Locks", NULL, NULL, 8 * 1024, 0, ~0L, 0);

static MYSQL_SYSVAR_ULONG(debug, tokudb_debug, PLUGIN_VAR_READONLY, "TokuDB Debug", NULL, NULL, 0, 0, ~0L, 0);

static MYSQL_SYSVAR_STR(log_dir, tokudb_log_dir, PLUGIN_VAR_READONLY, "TokuDB Log Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(data_dir, tokudb_data_dir, PLUGIN_VAR_READONLY, "TokuDB Data Directory", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(version, tokudb_version, PLUGIN_VAR_READONLY, "TokuDB Version", NULL, NULL, NULL);

static MYSQL_SYSVAR_UINT(init_flags, tokudb_init_flags, PLUGIN_VAR_READONLY, "Sets TokuDB DB_ENV->open flags", NULL, NULL, tokudb_init_flags, 0, ~0, 0);

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
    MYSQL_SYSVAR(cache_memory_percent),
    MYSQL_SYSVAR(max_lock),
    MYSQL_SYSVAR(data_dir),
    MYSQL_SYSVAR(log_dir),
    MYSQL_SYSVAR(debug),
    MYSQL_SYSVAR(commit_sync),
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(init_flags),
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
    "Fractal trees, transactions, row level locks",
    PLUGIN_LICENSE_PROPRIETARY,        /* QQQ license? */
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    0x0200,                    /* QQQ 2.0 */
    NULL,                      /* status variables */
    tokudb_system_variables,   /* system variables */
    NULL                       /* config options */
}
mysql_declare_plugin_end;
