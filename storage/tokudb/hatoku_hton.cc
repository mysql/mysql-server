/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* -*- mode: C; c-basic-offset: 4 -*- */
#define MYSQL_SERVER 1
#include "hatoku_defines.h"
#include <db.h>
#include <ctype.h>

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

#if TOKU_INCLUDE_OPTION_STRUCTS
#ifdef HA_TOPTION_SYSVAR
ha_create_table_option tokudb_table_options[] = {
    HA_TOPTION_SYSVAR("compression", row_format, row_format),
    HA_TOPTION_END
};
#else
ha_create_table_option tokudb_table_options[]=
{
  HA_TOPTION_ENUM("compression", row_format,
                  "TOKUDB_UNCOMPRESSED,TOKUDB_ZLIB,TOKUDB_QUICKLZ,"
                  "TOKUDB_LZMA,TOKUDB_FAST,TOKUDB_SMALL", 1),
  HA_TOPTION_END
};
#endif

ha_create_table_option tokudb_index_options[] = {
    HA_IOPTION_BOOL("clustering", clustering, 0),
    HA_IOPTION_END
};
#endif

static uchar *tokudb_get_key(TOKUDB_SHARE * share, size_t * length, my_bool not_used __attribute__ ((unused))) {
    *length = share->table_name_length;
    return (uchar *) share->table_name;
}

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root);


static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer);
static void tokudb_cleanup_log_files(void);
static int tokudb_end(handlerton * hton, ha_panic_function type);
static bool tokudb_flush_logs(handlerton * hton);
static bool tokudb_show_status(handlerton * hton, THD * thd, stat_print_fn * print, enum ha_stat_type);
#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
static void tokudb_handle_fatal_signal(handlerton *hton, THD *thd, int sig);
#endif
static int tokudb_close_connection(handlerton * hton, THD * thd);
static int tokudb_commit(handlerton * hton, THD * thd, bool all);
static int tokudb_rollback(handlerton * hton, THD * thd, bool all);
#if TOKU_INCLUDE_XA
static int tokudb_xa_prepare(handlerton* hton, THD* thd, bool all);
static int tokudb_xa_recover(handlerton* hton, XID*  xid_list, uint  len);
static int tokudb_commit_by_xid(handlerton* hton, XID* xid);
static int tokudb_rollback_by_xid(handlerton* hton, XID*  xid);
#endif

static int tokudb_rollback_to_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_savepoint(handlerton * hton, THD * thd, void *savepoint);
static int tokudb_release_savepoint(handlerton * hton, THD * thd, void *savepoint);
#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
static int tokudb_discover_table(handlerton *hton, THD* thd, TABLE_SHARE *ts);
static int tokudb_discover_table_existence(handlerton *hton, const char *db, const char *name);
#endif
static int tokudb_discover(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen);
static int tokudb_discover2(handlerton *hton, THD* thd, const char *db, const char *name, bool translate_name, uchar **frmblob, size_t *frmlen);
static int tokudb_discover3(handlerton *hton, THD* thd, const char *db, const char *name, char *path, uchar **frmblob, size_t *frmlen);
handlerton *tokudb_hton;

const char *ha_tokudb_ext = ".tokudb";
char *tokudb_data_dir;
ulong tokudb_debug;
DB_ENV *db_env;
HASH tokudb_open_tables;
pthread_mutex_t tokudb_mutex;

#if TOKU_THDVAR_MEMALLOC_BUG
static pthread_mutex_t tokudb_map_mutex;
static TREE tokudb_map;
struct tokudb_map_pair {
    THD *thd;
    char *last_lock_timeout;
};
#if 50500 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50599
static int tokudb_map_pair_cmp(void *custom_arg, const void *a, const void *b) {
#else
static int tokudb_map_pair_cmp(const void *custom_arg, const void *a, const void *b) {
#endif
    const struct tokudb_map_pair *a_key = (const struct tokudb_map_pair *) a;
    const struct tokudb_map_pair *b_key = (const struct tokudb_map_pair *) b;
    if (a_key->thd < b_key->thd)
        return -1;
    else if (a_key->thd > b_key->thd)
        return +1;
    else
        return 0;
};
#endif

#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
static my_bool tokudb_gdb_on_fatal;
static char *tokudb_gdb_path;
#endif

static PARTITIONED_COUNTER tokudb_primary_key_bytes_inserted;
void toku_hton_update_primary_key_bytes_inserted(uint64_t row_size) {
    increment_partitioned_counter(tokudb_primary_key_bytes_inserted, row_size);
}

static void tokudb_lock_timeout_callback(DB *db, uint64_t requesting_txnid, const DBT *left_key, const DBT *right_key, uint64_t blocking_txnid);
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
static my_bool tokudb_checkpoint_on_flush_logs = FALSE;
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
static uint32_t tokudb_fsync_log_period;
uint32_t tokudb_write_status_frequency;
uint32_t tokudb_read_status_frequency;

#ifdef TOKUDB_VERSION
#define tokudb_stringify_2(x) #x
#define tokudb_stringify(x) tokudb_stringify_2(x)
#define TOKUDB_VERSION_STR tokudb_stringify(TOKUDB_VERSION)
#else
#define TOKUDB_VERSION_STR NULL
#endif
char *tokudb_version = (char *) TOKUDB_VERSION_STR;
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
static void create_tokudb_hton_intialized_lock(void) {
    my_rwlock_init(&tokudb_hton_initialized_lock, 0);
}

static void destroy_tokudb_hton_initialized_lock(void) __attribute__((destructor));
static void destroy_tokudb_hton_initialized_lock(void) {
    rwlock_destroy(&tokudb_hton_initialized_lock);
}

static SHOW_VAR *toku_global_status_variables = NULL;
static uint64_t toku_global_status_max_rows;
static TOKU_ENGINE_STATUS_ROW_S* toku_global_status_rows = NULL;

static void handle_ydb_error(int error) {
    switch (error) {
    case TOKUDB_HUGE_PAGES_ENABLED:
        sql_print_error("************************************************************");
        sql_print_error("                                                            ");
        sql_print_error("                        @@@@@@@@@@@                         ");
        sql_print_error("                      @@'         '@@                       ");
        sql_print_error("                     @@    _     _  @@                      ");
        sql_print_error("                     |    (.)   (.)  |                      ");
        sql_print_error("                     |             ` |                      ");
        sql_print_error("                     |        >    ' |                      ");
        sql_print_error("                     |     .----.    |                      ");
        sql_print_error("                     ..   |.----.|  ..                      ");
        sql_print_error("                      ..  '      ' ..                       ");
        sql_print_error("                        .._______,.                         ");
        sql_print_error("                                                            ");
        sql_print_error("%s will not run with transparent huge pages enabled.        ", tokudb_hton_name);
        sql_print_error("Please disable them to continue.                            ");
        sql_print_error("(echo never > /sys/kernel/mm/transparent_hugepage/enabled)  ");
        sql_print_error("                                                            ");
        sql_print_error("************************************************************");
        break;
    case TOKUDB_UPGRADE_FAILURE:
        sql_print_error("%s upgrade failed. A clean shutdown of the previous version is required.", tokudb_hton_name);
        break;
    default:
        sql_print_error("%s unknown error %d", tokudb_hton_name, error);
        break;
    }
}

static int tokudb_set_product_name(void) {
    size_t n = strlen(tokudb_hton_name);
    char tokudb_product_name[n+1];
    memset(tokudb_product_name, 0, sizeof tokudb_product_name);
    for (size_t i = 0; i < n; i++)
        tokudb_product_name[i] = tolower(tokudb_hton_name[i]);
    int r = db_env_set_toku_product_name(tokudb_product_name);
    return r;
}

static int tokudb_init_func(void *p) {
    TOKUDB_DBUG_ENTER("%p", p);
    int r;

    // 3938: lock the handlerton's initialized status flag for writing
    r = rw_wrlock(&tokudb_hton_initialized_lock);
    assert(r == 0);

    db_env = NULL;
    tokudb_hton = (handlerton *) p;

#if TOKUDB_CHECK_JEMALLOC
    if (tokudb_check_jemalloc && dlsym(RTLD_DEFAULT, "mallctl") == NULL) {
        sql_print_error("%s is not initialized because jemalloc is not loaded", tokudb_hton_name);
        goto error;
    }
#endif

    r = tokudb_set_product_name();
    if (r) {
        sql_print_error("%s can not set product name error %d", tokudb_hton_name, r);
        goto error;
    }

    tokudb_pthread_mutex_init(&tokudb_mutex, MY_MUTEX_INIT_FAST);
    (void) my_hash_init(&tokudb_open_tables, table_alias_charset, 32, 0, 0, (my_hash_get_key) tokudb_get_key, 0, 0);

    tokudb_hton->state = SHOW_OPTION_YES;
    // tokudb_hton->flags= HTON_CAN_RECREATE;  // QQQ this came from skeleton
    tokudb_hton->flags = HTON_CLOSE_CURSORS_AT_COMMIT | HTON_EXTENDED_KEYS;

#if defined(TOKU_INCLUDE_EXTENDED_KEYS) && TOKU_INCLUDE_EXTENDED_KEYS
#if defined(HTON_SUPPORTS_EXTENDED_KEYS)
    tokudb_hton->flags |= HTON_SUPPORTS_EXTENDED_KEYS;
#endif
#if defined(HTON_EXTENDED_KEYS)
    tokudb_hton->flags |= HTON_EXTENDED_KEYS;
#endif
#endif
#if defined(HTON_SUPPORTS_CLUSTERED_KEYS)
    tokudb_hton->flags |= HTON_SUPPORTS_CLUSTERED_KEYS;
#endif

#if defined(TOKU_USE_DB_TYPE_TOKUDB) && TOKU_USE_DB_TYPE_TOKUDB
    tokudb_hton->db_type = DB_TYPE_TOKUDB;
#elif defined(TOKU_USE_DB_TYPE_UNKNOWN) && TOKU_USE_DB_TYPE_UNKNOWN
    tokudb_hton->db_type = DB_TYPE_UNKNOWN;
#else
#error
#endif

    tokudb_hton->create = tokudb_create_handler;
    tokudb_hton->close_connection = tokudb_close_connection;

    tokudb_hton->savepoint_offset = sizeof(SP_INFO_T);
    tokudb_hton->savepoint_set = tokudb_savepoint;
    tokudb_hton->savepoint_rollback = tokudb_rollback_to_savepoint;
    tokudb_hton->savepoint_release = tokudb_release_savepoint;

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
    tokudb_hton->discover_table = tokudb_discover_table;
    tokudb_hton->discover_table_existence = tokudb_discover_table_existence;
#else
    tokudb_hton->discover = tokudb_discover;
#if defined(MYSQL_HANDLERTON_INCLUDE_DISCOVER2)
    tokudb_hton->discover2 = tokudb_discover2;
#endif
#endif
    tokudb_hton->commit = tokudb_commit;
    tokudb_hton->rollback = tokudb_rollback;
#if TOKU_INCLUDE_XA
    tokudb_hton->prepare=tokudb_xa_prepare;
    tokudb_hton->recover=tokudb_xa_recover;
    tokudb_hton->commit_by_xid=tokudb_commit_by_xid;
    tokudb_hton->rollback_by_xid=tokudb_rollback_by_xid;
#endif

    tokudb_hton->table_options= tokudb_table_options;
    tokudb_hton->index_options= tokudb_index_options;

    tokudb_hton->panic = tokudb_end;
    tokudb_hton->flush_logs = tokudb_flush_logs;
    tokudb_hton->show_status = tokudb_show_status;
#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
    tokudb_hton->handle_fatal_signal = tokudb_handle_fatal_signal;
#endif

#if TOKU_INCLUDE_OPTION_STRUCTS
    tokudb_hton->table_options = tokudb_table_options;
    tokudb_hton->index_options = tokudb_index_options;
#endif

    if (!tokudb_home)
        tokudb_home = mysql_real_data_home;
    DBUG_PRINT("info", ("tokudb_home: %s", tokudb_home));

    if ((r = db_env_create(&db_env, 0))) {
        DBUG_PRINT("info", ("db_env_create %d\n", r));
        handle_ydb_error(r);
        goto error;
    }

    DBUG_PRINT("info", ("tokudb_env_flags: 0x%x\n", tokudb_env_flags));
    r = db_env->set_flags(db_env, tokudb_env_flags, 1);
    if (r) { // QQQ
        if (tokudb_debug & TOKUDB_DEBUG_INIT) 
            TOKUDB_TRACE("WARNING: flags=%x r=%d", tokudb_env_flags, r); 
        // goto error;
    }

    // config error handling
    db_env->set_errcall(db_env, tokudb_print_error);
    db_env->set_errpfx(db_env, tokudb_hton_name);

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
    if (tokudb_debug & TOKUDB_DEBUG_INIT) 
        TOKUDB_TRACE("tokudb_cache_size=%lld r=%d", ((unsigned long long) gbytes << 30) + bytes, r);

    if (db_env->set_redzone) {
        r = db_env->set_redzone(db_env, tokudb_fs_reserve_percent);
        if (tokudb_debug & TOKUDB_DEBUG_INIT)
            TOKUDB_TRACE("set_redzone r=%d", r);
    }

    if (tokudb_debug & TOKUDB_DEBUG_INIT) 
        TOKUDB_TRACE("env open:flags=%x", tokudb_init_flags);

    r = db_env->set_generate_row_callback_for_put(db_env,generate_row_for_put);
    assert(r == 0);
    r = db_env->set_generate_row_callback_for_del(db_env,generate_row_for_del);
    assert(r == 0);
    db_env->set_update(db_env, tokudb_update_fun);
    db_env_set_direct_io(tokudb_directio == TRUE);
    db_env->change_fsync_log_period(db_env, tokudb_fsync_log_period);
    db_env->set_lock_timeout_callback(db_env, tokudb_lock_timeout_callback);
    db_env->set_loader_memory_size(db_env, tokudb_get_loader_memory_size_callback);

    r = db_env->open(db_env, tokudb_home, tokudb_init_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (tokudb_debug & TOKUDB_DEBUG_INIT) 
        TOKUDB_TRACE("env opened:return=%d", r);

    if (r) {
        DBUG_PRINT("info", ("env->open %d", r));
        handle_ydb_error(r);
        goto error;
    }

    r = db_env->checkpointing_set_period(db_env, tokudb_checkpointing_period);
    assert(r == 0);
    r = db_env->cleaner_set_period(db_env, tokudb_cleaner_period);
    assert(r == 0);
    r = db_env->cleaner_set_iterations(db_env, tokudb_cleaner_iterations);
    assert(r == 0);

    r = db_env->set_lock_timeout(db_env, DEFAULT_TOKUDB_LOCK_TIMEOUT, tokudb_get_lock_wait_time_callback);
    assert(r == 0);

    db_env->set_killed_callback(db_env, DEFAULT_TOKUDB_KILLED_TIME, tokudb_get_killed_time_callback, tokudb_killed_callback);

    r = db_env->get_engine_status_num_rows (db_env, &toku_global_status_max_rows);
    assert(r == 0);

    {
        const myf mem_flags = MY_FAE|MY_WME|MY_ZEROFILL|MY_ALLOW_ZERO_PTR|MY_FREE_ON_ERROR;
        toku_global_status_variables = (SHOW_VAR*)tokudb_my_malloc(sizeof(*toku_global_status_variables)*toku_global_status_max_rows, mem_flags);
        toku_global_status_rows = (TOKU_ENGINE_STATUS_ROW_S*)tokudb_my_malloc(sizeof(*toku_global_status_rows)*toku_global_status_max_rows, mem_flags);
    }

    tokudb_primary_key_bytes_inserted = create_partitioned_counter();

#if TOKU_THDVAR_MEMALLOC_BUG
    tokudb_pthread_mutex_init(&tokudb_map_mutex, MY_MUTEX_INIT_FAST);
    init_tree(&tokudb_map, 0, 0, 0, tokudb_map_pair_cmp, true, NULL, NULL);
#endif

    //3938: succeeded, set the init status flag and unlock
    tokudb_hton_initialized = 1;
    rw_unlock(&tokudb_hton_initialized_lock);
    DBUG_RETURN(false);

error:
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
    TOKUDB_DBUG_ENTER("");
    tokudb_my_free(toku_global_status_variables);
    toku_global_status_variables = NULL;
    tokudb_my_free(toku_global_status_rows);
    toku_global_status_rows = NULL;
    my_hash_free(&tokudb_open_tables);
    tokudb_pthread_mutex_destroy(&tokudb_mutex);
    TOKUDB_DBUG_RETURN(0);
}

static handler *tokudb_create_handler(handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root) {
    return new(mem_root) ha_tokudb(hton, table);
}

int tokudb_end(handlerton * hton, ha_panic_function type) {
    TOKUDB_DBUG_ENTER("");
    int error = 0;
    
    // 3938: if we finalize the storage engine plugin, it is no longer
    // initialized. grab a writer lock for the duration of the
    // call, so we can drop the flag and destroy the mutexes
    // in isolation.
    rw_wrlock(&tokudb_hton_initialized_lock);
    assert(tokudb_hton_initialized);

    if (db_env) {
        if (tokudb_init_flags & DB_INIT_LOG)
            tokudb_cleanup_log_files();
        long total_prepared = 0; // count the total number of prepared txn's that we discard
#if TOKU_INCLUDE_XA
        while (1) {
            // get xid's 
            const long n_xid = 1;
            TOKU_XA_XID xids[n_xid];
            long n_prepared = 0;
            error = db_env->txn_xa_recover(db_env, xids, n_xid, &n_prepared, total_prepared == 0 ? DB_FIRST : DB_NEXT);
            assert(error == 0);
            if (n_prepared == 0) 
                break;
            // discard xid's
            for (long i = 0; i < n_xid; i++) {
                DB_TXN *txn = NULL;
                error = db_env->get_txn_from_xid(db_env, &xids[i], &txn);
                assert(error == 0);
                error = txn->discard(txn, 0);
                assert(error == 0);
            }
            total_prepared += n_prepared;
        }
#endif
        error = db_env->close(db_env, total_prepared > 0 ? TOKUFT_DIRTY_SHUTDOWN : 0);
#if TOKU_INCLUDE_XA
        if (error != 0 && total_prepared > 0) {
            sql_print_error("%s: %ld prepared txns still live, please shutdown, error %d", tokudb_hton_name, total_prepared, error);
        } else
#endif
        assert(error == 0);
        db_env = NULL;
    }

    if (tokudb_primary_key_bytes_inserted) {
        destroy_partitioned_counter(tokudb_primary_key_bytes_inserted);
        tokudb_primary_key_bytes_inserted = NULL;
    }

#if TOKU_THDVAR_MEMALLOC_BUG
    tokudb_pthread_mutex_destroy(&tokudb_map_mutex);
    delete_tree(&tokudb_map);
#endif

    // 3938: drop the initialized flag and unlock
    tokudb_hton_initialized = 0;
    rw_unlock(&tokudb_hton_initialized_lock);

    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_close_connection(handlerton * hton, THD * thd) {
    int error = 0;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_get_ha_data(thd, tokudb_hton);
    if (trx && trx->checkpoint_lock_taken) {
        error = db_env->checkpointing_resume(db_env);
    }
    tokudb_my_free(trx);
#if TOKU_THDVAR_MEMALLOC_BUG
    tokudb_pthread_mutex_lock(&tokudb_map_mutex);
    struct tokudb_map_pair key = { thd, NULL };
    struct tokudb_map_pair *found_key = (struct tokudb_map_pair *) tree_search(&tokudb_map, &key, NULL);
    if (found_key) {
        tokudb_my_free(found_key->last_lock_timeout);
        tree_delete(&tokudb_map, found_key, sizeof *found_key, NULL);
    }
    tokudb_pthread_mutex_unlock(&tokudb_map_mutex);
#endif
    return error;
}

bool tokudb_flush_logs(handlerton * hton) {
    TOKUDB_DBUG_ENTER("");
    int error;
    bool result = 0;

    if (tokudb_checkpoint_on_flush_logs) {
        //
        // take the checkpoint
        //
        error = db_env->txn_checkpoint(db_env, 0, 0, 0);
        if (error) {
            my_error(ER_ERROR_DURING_CHECKPOINT, MYF(0), error);
            result = 1;
            goto exit;
        }
    }
    else {
        error = db_env->log_flush(db_env, NULL);
        assert(error == 0);
    }

    result = 0;
exit:
    TOKUDB_DBUG_RETURN(result);
}


typedef struct txn_progress_info {
    char status[200];
    THD* thd;
} *TXN_PROGRESS_INFO;

static void txn_progress_func(TOKU_TXN_PROGRESS progress, void* extra) {
    TXN_PROGRESS_INFO progress_info = (TXN_PROGRESS_INFO)extra;
    int r = sprintf(progress_info->status, 
                    "%sprocessing %s of transaction, %" PRId64 " out of %" PRId64,
                    progress->stalled_on_checkpoint ? "Writing committed changes to disk, " : "",
                    progress->is_commit ? "commit" : "abort",
                    progress->entries_processed, 
                    progress->entries_total
                    ); 
    assert(r >= 0);
    thd_proc_info(progress_info->thd, progress_info->status);
}

static void commit_txn_with_progress(DB_TXN* txn, uint32_t flags, THD* thd) {
    const char *orig_proc_info = tokudb_thd_get_proc_info(thd);
    struct txn_progress_info info;
    info.thd = thd;
    int r = txn->commit_with_progress(txn, flags, txn_progress_func, &info);
    if (r != 0) {
        sql_print_error("%s: tried committing transaction %p and got error code %d", tokudb_hton_name, txn, r);
    }
    assert(r == 0);
    thd_proc_info(thd, orig_proc_info);
}

static void abort_txn_with_progress(DB_TXN* txn, THD* thd) {
    const char *orig_proc_info = tokudb_thd_get_proc_info(thd);
    struct txn_progress_info info;
    info.thd = thd;
    int r = txn->abort_with_progress(txn, txn_progress_func, &info);
    if (r != 0) {
        sql_print_error("%s: tried aborting transaction %p and got error code %d", tokudb_hton_name, txn, r);
    }
    assert(r == 0);
    thd_proc_info(thd, orig_proc_info);
}

static void tokudb_cleanup_handlers(tokudb_trx_data *trx, DB_TXN *txn) {
    LIST *e;
    while ((e = trx->handlers)) {
        trx->handlers = list_delete(trx->handlers, e);
        ha_tokudb *handler = (ha_tokudb *) e->data;
        handler->cleanup_txn(txn);
    }
}

static int tokudb_commit(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("");
    DBUG_PRINT("trans", ("ending transaction %s", all ? "all" : "stmt"));
    uint32_t syncflag = THDVAR(thd, commit_sync) ? 0 : DB_TXN_NOSYNC;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    DB_TXN *this_txn = *txn;
    if (this_txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("commit trx %u txn %p", all, this_txn);
        }
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_commit_before", DBUG_SUICIDE(););
        tokudb_cleanup_handlers(trx, this_txn);
        commit_txn_with_progress(this_txn, syncflag, thd);
        // test hook to induce a crash on a debug build
        DBUG_EXECUTE_IF("tokudb_crash_commit_after", DBUG_SUICIDE(););
        if (this_txn == trx->sp_level) {
            trx->sp_level = 0;
        }
        *txn = 0;
        trx->sub_sp_level = NULL;
    } 
    else if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("nothing to commit %d", all);
    }
    reset_stmt_progress(&trx->stmt_progress);
    TOKUDB_DBUG_RETURN(0);
}

static int tokudb_rollback(handlerton * hton, THD * thd, bool all) {
    TOKUDB_DBUG_ENTER("");
    DBUG_PRINT("trans", ("aborting transaction %s", all ? "all" : "stmt"));
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
    DB_TXN **txn = all ? &trx->all : &trx->stmt;
    DB_TXN *this_txn = *txn;
    if (this_txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("rollback %u txn %p", all, this_txn);
        }
        tokudb_cleanup_handlers(trx, this_txn);
        abort_txn_with_progress(this_txn, thd);
        if (this_txn == trx->sp_level) {
            trx->sp_level = 0;
        }
        *txn = 0;
        trx->sub_sp_level = NULL;
    } 
    else {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("abort0");
        }
    }
    reset_stmt_progress(&trx->stmt_progress);
    TOKUDB_DBUG_RETURN(0);
}

#if TOKU_INCLUDE_XA

static int tokudb_xa_prepare(handlerton* hton, THD* thd, bool all) {
    TOKUDB_DBUG_ENTER("");
    int r = 0;

    /* if support_xa is disable, just return */
    if (!THDVAR(thd, support_xa)) {
        TOKUDB_DBUG_RETURN(r);
    }

    DBUG_PRINT("trans", ("preparing transaction %s", all ? "all" : "stmt"));
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
    DB_TXN* txn = all ? trx->all : trx->stmt;
    if (txn) {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("doing txn prepare:%d:%p", all, txn);
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
        TOKUDB_TRACE("nothing to prepare %d", all);
    }
    TOKUDB_DBUG_RETURN(r);
}

static int tokudb_xa_recover(handlerton* hton, XID* xid_list, uint len) {
    TOKUDB_DBUG_ENTER("");
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
    TOKUDB_DBUG_ENTER("");
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
    TOKUDB_DBUG_ENTER("");
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
    TOKUDB_DBUG_ENTER("");
    int error;
    SP_INFO save_info = (SP_INFO)savepoint;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
    if (thd->in_sub_stmt) {
        assert(trx->stmt);
        error = txn_begin(db_env, trx->sub_sp_level, &(save_info->txn), DB_INHERIT_ISOLATION, thd);
        if (error) {
            goto cleanup;
        }
        trx->sub_sp_level = save_info->txn;
        save_info->in_sub_stmt = true;
    }
    else {
        error = txn_begin(db_env, trx->sp_level, &(save_info->txn), DB_INHERIT_ISOLATION, thd);
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
    TOKUDB_DBUG_ENTER("");
    int error;
    SP_INFO save_info = (SP_INFO)savepoint;
    DB_TXN* parent = NULL;
    DB_TXN* txn_to_rollback = save_info->txn;

    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
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
    TOKUDB_DBUG_ENTER("");
    int error;

    SP_INFO save_info = (SP_INFO)savepoint;
    DB_TXN* parent = NULL;
    DB_TXN* txn_to_commit = save_info->txn;

    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, hton);
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

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
static int tokudb_discover_table(handlerton *hton, THD* thd, TABLE_SHARE *ts) {
    uchar *frmblob = 0;
    size_t frmlen;
    int res= tokudb_discover3(hton, thd, ts->db.str, ts->table_name.str,
                              ts->normalized_path.str, &frmblob, &frmlen);
    if (!res)
        res= ts->init_from_binary_frm_image(thd, true, frmblob, frmlen);
    
    my_free(frmblob);
    // discover_table should returns HA_ERR_NO_SUCH_TABLE for "not exists"
    return res == ENOENT ? HA_ERR_NO_SUCH_TABLE : res;
}

static int tokudb_discover_table_existence(handlerton *hton, const char *db, const char *name) {
    uchar *frmblob = 0;
    size_t frmlen;
    int res= tokudb_discover(hton, current_thd, db, name, &frmblob, &frmlen);
    my_free(frmblob);
    return res != ENOENT;
}
#endif

static int tokudb_discover(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen) {
    return tokudb_discover2(hton, thd, db, name, true, frmblob, frmlen);
}

static int tokudb_discover2(handlerton *hton, THD* thd, const char *db, const char *name, bool translate_name,
                            uchar **frmblob, size_t *frmlen) {
    char path[FN_REFLEN + 1];
    build_table_filename(path, sizeof(path) - 1, db, name, "", translate_name ? 0 : FN_IS_TMP);
    return tokudb_discover3(hton, thd, db, name, path, frmblob, frmlen);
}

static int tokudb_discover3(handlerton *hton, THD* thd, const char *db, const char *name, char *path,
                            uchar **frmblob, size_t *frmlen) {
    TOKUDB_DBUG_ENTER("%s %s %s", db, name, path);
    int error;
    DB* status_db = NULL;
    DB_TXN* txn = NULL;
    HA_METADATA_KEY curr_key = hatoku_frm_data;
    DBT key = {};
    DBT value = {};
    bool do_commit = false;

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
    tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, tokudb_hton);
    if (thd_sql_command(thd) == SQLCOM_CREATE_TABLE && trx && trx->sub_sp_level) {
        do_commit = false;
        txn = trx->sub_sp_level;
    } else {
        error = txn_begin(db_env, 0, &txn, 0, thd);
        if (error) { goto cleanup; }
        do_commit = true;
    }
#else
    error = txn_begin(db_env, 0, &txn, 0, thd);
    if (error) { goto cleanup; }
    do_commit = true;
#endif

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
    if (do_commit && txn) {
        commit_txn(txn, 0);
    }
    TOKUDB_DBUG_RETURN(error);    
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
    TOKUDB_DBUG_ENTER("");
    int error;
    uint64_t panic;
    const int panic_string_len = 1024;
    char panic_string[panic_string_len] = {'\0'};
    uint64_t num_rows;
    uint64_t max_rows;
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
    error = db_env->get_engine_status_num_rows (db_env, &max_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[max_rows];
    error = db_env->get_engine_status (db_env, mystat, max_rows, &num_rows, &redzone_state, &panic, panic_string, panic_string_len, TOKU_ENGINE_STATUS);

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
                snprintf(buf, bufsiz, "%" PRIu64 "", mystat[row].value.num);
                break;
            case UINT64:
                snprintf(buf, bufsiz, "%" PRIu64 "", mystat[row].value.num);
                break;
            case CHARSTR:
                snprintf(buf, bufsiz, "%s", mystat[row].value.str);
                break;
            case UNIXTIME: {
                time_t t = mystat[row].value.num;
                char tbuf[26];
                snprintf(buf, bufsiz, "%.24s", ctime_r(&t, tbuf));
                break;
            }
            case TOKUTIME: {
                double t = tokutime_to_seconds(mystat[row].value.num);
                snprintf(buf, bufsiz, "%.6f", t);
                break;
            }
            case PARCOUNT: {
                uint64_t v = read_partitioned_counter(mystat[row].value.parcount);
                snprintf(buf, bufsiz, "%" PRIu64, v);
                break;
            }
            case DOUBLE:
                snprintf(buf, bufsiz, "%.6f", mystat[row].value.dnum);
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
    const char *old_proc_info;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_get_ha_data(thd, tokudb_hton);
    if (!trx) {
        error = create_tokudb_trx_data_instance(&trx);
        //
        // can only fail due to memory allocation, so ok to assert
        //
        assert(!error);
        thd_set_ha_data(thd, tokudb_hton, trx);
    }
    
    if (trx->checkpoint_lock_taken) {
        goto cleanup;
    }
    //
    // This can only fail if environment is not created, which is not possible
    // in handlerton
    //
    old_proc_info = tokudb_thd_get_proc_info(thd);
    thd_proc_info(thd, "Trying to grab checkpointing lock.");
    error = db_env->checkpointing_postpone(db_env);
    assert(!error);
    thd_proc_info(thd, old_proc_info);

    trx->checkpoint_lock_taken = true;
cleanup:
    return;
}

static void tokudb_checkpoint_unlock(THD * thd) {
    int error;
    const char *old_proc_info;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_get_ha_data(thd, tokudb_hton);
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
    old_proc_info = tokudb_thd_get_proc_info(thd);
    thd_proc_info(thd, "Trying to release checkpointing lock.");
    error = db_env->checkpointing_resume(db_env);
    assert(!error);
    thd_proc_info(thd, old_proc_info);

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

#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
static void tokudb_handle_fatal_signal(handlerton *hton __attribute__ ((__unused__)), THD *thd __attribute__ ((__unused__)), int sig) {
    if (tokudb_gdb_on_fatal) {
        db_env_try_gdb_stack_trace(tokudb_gdb_path);
    }
}
#endif

static void tokudb_print_error(const DB_ENV * db_env, const char *db_errpfx, const char *buffer) {
    sql_print_error("%s: %s", db_errpfx, buffer);
}

static void tokudb_cleanup_log_files(void) {
    TOKUDB_DBUG_ENTER("");
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
                TOKUDB_TRACE("cleanup:%s", *np);
#else
            my_delete(*np, MYF(MY_WME));
#endif
        }

        free(names);
    }

    DBUG_VOID_RETURN;
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
static void tokudb_cleaner_period_update(THD * thd, struct st_mysql_sys_var * sys_var, void * var, const void * save) {
    ulong * cleaner_period = (ulong *) var;
    *cleaner_period = *(const ulonglong *) save;
    int r = db_env->cleaner_set_period(db_env, *cleaner_period);
    assert(r == 0);
}

#define DEFAULT_CLEANER_PERIOD 1

static MYSQL_SYSVAR_ULONG(cleaner_period, tokudb_cleaner_period,
    0, "TokuDB cleaner_period", 
    NULL, tokudb_cleaner_period_update, DEFAULT_CLEANER_PERIOD,
    0, ~0UL, 0);

static void tokudb_cleaner_iterations_update(THD * thd, struct st_mysql_sys_var * sys_var, void * var, const void * save) {
    ulong * cleaner_iterations = (ulong *) var;
    *cleaner_iterations = *(const ulonglong *) save;
    int r = db_env->cleaner_set_iterations(db_env, *cleaner_iterations);
    assert(r == 0);
}

#define DEFAULT_CLEANER_ITERATIONS 5

static MYSQL_SYSVAR_ULONG(cleaner_iterations, tokudb_cleaner_iterations,
    0, "TokuDB cleaner_iterations", 
    NULL, tokudb_cleaner_iterations_update, DEFAULT_CLEANER_ITERATIONS,
    0, ~0UL, 0);

static void tokudb_checkpointing_period_update(THD * thd, struct st_mysql_sys_var * sys_var, void * var, const void * save) {
    uint * checkpointing_period = (uint *) var;
    *checkpointing_period = *(const ulonglong *) save;
    int r = db_env->checkpointing_set_period(db_env, *checkpointing_period);
    assert(r == 0);
}

static MYSQL_SYSVAR_UINT(checkpointing_period, tokudb_checkpointing_period, 
    0, "TokuDB Checkpointing period", 
    NULL, tokudb_checkpointing_period_update, 60, 
    0, ~0U, 0);

static MYSQL_SYSVAR_BOOL(directio, tokudb_directio,
    PLUGIN_VAR_READONLY,
    "TokuDB Enable Direct I/O ",
    NULL, NULL, FALSE);
static MYSQL_SYSVAR_BOOL(checkpoint_on_flush_logs, tokudb_checkpoint_on_flush_logs,
    0,
    "TokuDB Checkpoint on Flush Logs ",
    NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONGLONG(cache_size, tokudb_cache_size,
    PLUGIN_VAR_READONLY, "TokuDB cache table size", NULL, NULL, 0,
    0, ~0ULL, 0);

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

#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
static MYSQL_SYSVAR_STR(gdb_path, tokudb_gdb_path, PLUGIN_VAR_READONLY|PLUGIN_VAR_RQCMDARG, "TokuDB path to gdb for extra debug info on fatal signal", NULL, NULL, "/usr/bin/gdb");
static MYSQL_SYSVAR_BOOL(gdb_on_fatal, tokudb_gdb_on_fatal, 0, "TokuDB enable gdb debug info on fatal signal", NULL, NULL, true);
#endif

static void tokudb_fsync_log_period_update(THD *thd, struct st_mysql_sys_var *sys_var, void *var, const void *save) {
    uint32 *period = (uint32 *) var;
    *period = *(const ulonglong *) save;
    db_env->change_fsync_log_period(db_env, *period);
}

static MYSQL_SYSVAR_UINT(fsync_log_period, tokudb_fsync_log_period, 0, "TokuDB fsync log period", NULL, tokudb_fsync_log_period_update, 0, 0, ~0U, 0);

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
    MYSQL_SYSVAR(alter_print_error),
    MYSQL_SYSVAR(create_index_online),
    MYSQL_SYSVAR(disable_prefetching),
    MYSQL_SYSVAR(version),
    MYSQL_SYSVAR(init_flags),
    MYSQL_SYSVAR(checkpointing_period),
    MYSQL_SYSVAR(prelock_empty),
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
    MYSQL_SYSVAR(checkpoint_on_flush_logs),
#if TOKU_INCLUDE_UPSERT
    MYSQL_SYSVAR(disable_slow_update),
    MYSQL_SYSVAR(disable_slow_upsert),
#endif
    MYSQL_SYSVAR(analyze_time),
    MYSQL_SYSVAR(fsync_log_period),
#if TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL
    MYSQL_SYSVAR(gdb_path),
    MYSQL_SYSVAR(gdb_on_fatal),
#endif
    MYSQL_SYSVAR(last_lock_timeout),
    MYSQL_SYSVAR(lock_timeout_debug),
    MYSQL_SYSVAR(loader_memory_size),
    MYSQL_SYSVAR(hide_default_row_format),
    MYSQL_SYSVAR(killed_time),
    MYSQL_SYSVAR(empty_scan),
#if TOKUDB_CHECK_JEMALLOC
    MYSQL_SYSVAR(check_jemalloc),
#endif
    MYSQL_SYSVAR(bulk_fetch),
#if TOKU_INCLUDE_XA
    MYSQL_SYSVAR(support_xa),
#endif
    MYSQL_SYSVAR(rpl_unique_checks),
    MYSQL_SYSVAR(rpl_unique_checks_delay),
    MYSQL_SYSVAR(rpl_lookup_rows),
    MYSQL_SYSVAR(rpl_lookup_rows_delay),
    NULL
};

// Split ./database/table-dictionary into database, table and dictionary strings
static void tokudb_split_dname(const char *dname, String &database_name, String &table_name, String &dictionary_name) {
    const char *splitter = strchr(dname, '/');
    if (splitter) {
        const char *database_ptr = splitter+1;
        const char *table_ptr = strchr(database_ptr, '/');
        if (table_ptr) {
            database_name.append(database_ptr, table_ptr - database_ptr);
            table_ptr += 1;
            const char *dictionary_ptr = strchr(table_ptr, '-');
            if (dictionary_ptr) {
                table_name.append(table_ptr, dictionary_ptr - table_ptr);
                dictionary_ptr += 1;
                dictionary_name.append(dictionary_ptr);
            }
        }
    }
}

struct st_mysql_storage_engine tokudb_storage_engine = { MYSQL_HANDLERTON_INTERFACE_VERSION };

static struct st_mysql_information_schema tokudb_file_map_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_file_map_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_schema", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

static int tokudb_file_map(TABLE *table, THD *thd) {
    int error;
    DB_TXN* txn = NULL;
    DBC* tmp_cursor = NULL;
    DBT curr_key;
    DBT curr_val;
    memset(&curr_key, 0, sizeof curr_key); 
    memset(&curr_val, 0, sizeof curr_val);
    error = txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED, thd);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(tmp_cursor, &curr_key, &curr_val, DB_NEXT);
        if (!error) {
            // We store the NULL terminator in the directory so it's included in the size.
            // See #5789
            // Recalculate and check just to be safe.
            const char *dname = (const char *) curr_key.data;
            size_t dname_len = strlen(dname);
            assert(dname_len == curr_key.size - 1);
            table->field[0]->store(dname, dname_len, system_charset_info);

            const char *iname = (const char *) curr_val.data;
            size_t iname_len = strlen(iname);
            assert(iname_len == curr_val.size - 1);
            table->field[1]->store(iname, iname_len, system_charset_info);

            // split the dname
            String database_name, table_name, dictionary_name;
            tokudb_split_dname(dname, database_name, table_name, dictionary_name);
            table->field[2]->store(database_name.c_ptr(), database_name.length(), system_charset_info);
            table->field[3]->store(table_name.c_ptr(), table_name.length(), system_charset_info);
            table->field[4]->store(dictionary_name.c_ptr(), dictionary_name.length(), system_charset_info);

            error = schema_table_store_record(thd, table);
        }
        if (!error && thd_killed(thd))
            error = ER_QUERY_INTERRUPTED;
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r == 0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_file_map_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_file_map_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    TABLE *table = tables->table;

    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        error = tokudb_file_map(table, thd);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_file_map_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_file_map_field_info;
    schema->fill_table = tokudb_file_map_fill_table;
    return 0;
}

static int tokudb_file_map_done(void *p) {
    return 0;
}

static struct st_mysql_information_schema tokudb_fractal_tree_info_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_fractal_tree_info_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_num_blocks_allocated", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_num_blocks_in_use", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_size_allocated", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"bt_size_in_use", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_schema", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

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
        assert(dname_len == dname->size - 1);
        table->field[0]->store((char *)dname->data, dname_len, system_charset_info);
        size_t iname_len = strlen((const char *)iname->data);
        assert(iname_len == iname->size - 1);
        table->field[1]->store((char *)iname->data, iname_len, system_charset_info);
    }
    table->field[2]->store(bt_num_blocks_allocated, false);
    table->field[3]->store(bt_num_blocks_in_use, false);
    table->field[4]->store(bt_size_allocated, false);
    table->field[5]->store(bt_size_in_use, false);

    // split the dname
    {
        String database_name, table_name, dictionary_name;
        tokudb_split_dname((const char *)dname->data, database_name, table_name, dictionary_name);
        table->field[6]->store(database_name.c_ptr(), database_name.length(), system_charset_info);
        table->field[7]->store(table_name.c_ptr(), table_name.length(), system_charset_info);
        table->field[8]->store(dictionary_name.c_ptr(), dictionary_name.length(), system_charset_info);
    }
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
    error = txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED, thd);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(tmp_cursor, &curr_key, &curr_val, DB_NEXT);
        if (!error) {
            error = tokudb_report_fractal_tree_info_for_db(&curr_key, &curr_val, table, thd);
        }
        if (!error && thd_killed(thd))
            error = ER_QUERY_INTERRUPTED;
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r == 0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_fractal_tree_info_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_fractal_tree_info_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        error = tokudb_fractal_tree_info(table, thd);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
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

static struct st_mysql_information_schema tokudb_fractal_tree_block_map_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_fractal_tree_block_map_field_info[] = {
    {"dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"internal_file_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"checkpoint_count", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"blocknum", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"offset", 0, MYSQL_TYPE_LONGLONG, 0, MY_I_S_MAYBE_NULL, NULL, SKIP_OPEN_TABLE },
    {"size", 0, MYSQL_TYPE_LONGLONG, 0, MY_I_S_MAYBE_NULL, NULL, SKIP_OPEN_TABLE },
    {"table_schema", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"table_dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

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
        e->checkpoint_counts = (uint64_t *) tokudb_my_malloc(num_rows * (sizeof *e->checkpoint_counts), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->blocknums = (int64_t *) tokudb_my_malloc(num_rows * (sizeof *e->blocknums), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->diskoffs = (int64_t *) tokudb_my_malloc(num_rows * (sizeof *e->diskoffs), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
        e->sizes = (int64_t *) tokudb_my_malloc(num_rows * (sizeof *e->sizes), MYF(MY_WME|MY_ZEROFILL|MY_FAE));
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
    struct tokudb_report_fractal_tree_block_map_iterator_extra e = {}; // avoid struct initializers so that we can compile with older gcc versions

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
        assert(dname_len == dname->size - 1);
        table->field[0]->store((char *)dname->data, dname_len, system_charset_info);

        size_t iname_len = strlen((const char *)iname->data);
        assert(iname_len == iname->size - 1);
        table->field[1]->store((char *)iname->data, iname_len, system_charset_info);

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

        // split the dname
        String database_name, table_name, dictionary_name;
        tokudb_split_dname((const char *)dname->data, database_name, table_name,dictionary_name);
        table->field[6]->store(database_name.c_ptr(), database_name.length(), system_charset_info);
        table->field[7]->store(table_name.c_ptr(), table_name.length(), system_charset_info);
        table->field[8]->store(dictionary_name.c_ptr(), dictionary_name.length(), system_charset_info);

        error = schema_table_store_record(thd, table);
    }

exit:
    if (e.checkpoint_counts != NULL) {
        tokudb_my_free(e.checkpoint_counts);
        e.checkpoint_counts = NULL;
    }
    if (e.blocknums != NULL) {
        tokudb_my_free(e.blocknums);
        e.blocknums = NULL;
    }
    if (e.diskoffs != NULL) {
        tokudb_my_free(e.diskoffs);
        e.diskoffs = NULL;
    }
    if (e.sizes != NULL) {
        tokudb_my_free(e.sizes);
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
    error = txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED, thd);
    if (error) {
        goto cleanup;
    }
    error = db_env->get_cursor_for_directory(db_env, txn, &tmp_cursor);
    if (error) {
        goto cleanup;
    }
    while (error == 0) {
        error = tmp_cursor->c_get(tmp_cursor, &curr_key, &curr_val, DB_NEXT);
        if (!error) {
            error = tokudb_report_fractal_tree_block_map_for_db(&curr_key, &curr_val, table, thd);
        }
        if (!error && thd_killed(thd))
            error = ER_QUERY_INTERRUPTED;
    }
    if (error == DB_NOTFOUND) {
        error = 0;
    }
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r == 0);
    }
    if (txn) {
        commit_txn(txn, 0);
    }
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_fractal_tree_block_map_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_fractal_tree_block_map_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    TABLE *table = tables->table;

    // 3938: Get a read lock on the status flag, since we must
    // read it before safely proceeding
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        error = tokudb_fractal_tree_block_map(table, thd);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    //3938: unlock the status flag lock
    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
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

static void tokudb_pretty_key(const DB *db, const DBT *key, const char *default_key, String *out) {
    if (key->data == NULL) {
        out->append(default_key);
    } else {
        bool do_hexdump = true;
        if (do_hexdump) {
            // hexdump the key
            const unsigned char *data = reinterpret_cast<const unsigned char *>(key->data);
            for (size_t i = 0; i < key->size; i++) {
                char str[3];
                snprintf(str, sizeof str, "%2.2x", data[i]);
                out->append(str);
            }
        }
    }
}

static void tokudb_pretty_left_key(const DB *db, const DBT *key, String *out) {
    tokudb_pretty_key(db, key, "-infinity", out);
}

static void tokudb_pretty_right_key(const DB *db, const DBT *key, String *out) {
    tokudb_pretty_key(db, key, "+infinity", out);
}

static const char *tokudb_get_index_name(DB *db) {
    if (db != NULL) {
        return db->get_dname(db);
    } else {
        return "$ydb_internal";
    }
}

static int tokudb_equal_key(const DBT *left_key, const DBT *right_key) {
    if (left_key->data == NULL || right_key->data == NULL || left_key->size != right_key->size)
        return 0;
    else
        return memcmp(left_key->data, right_key->data, left_key->size) == 0;
}

static void tokudb_lock_timeout_callback(DB *db, uint64_t requesting_txnid, const DBT *left_key, const DBT *right_key, uint64_t blocking_txnid) {
    THD *thd = current_thd;
    if (!thd)
        return;
    ulong lock_timeout_debug = THDVAR(thd, lock_timeout_debug);
    if (lock_timeout_debug != 0) {
        // generate a JSON document with the lock timeout info
        String log_str;
        log_str.append("{");
        log_str.append("\"mysql_thread_id\":");
        log_str.append_ulonglong(thd->thread_id);
        log_str.append(", \"dbname\":");
        log_str.append("\""); log_str.append(tokudb_get_index_name(db)); log_str.append("\"");
        log_str.append(", \"requesting_txnid\":");
        log_str.append_ulonglong(requesting_txnid);
        log_str.append(", \"blocking_txnid\":");
        log_str.append_ulonglong(blocking_txnid);
        if (tokudb_equal_key(left_key, right_key)) {
            String key_str;
            tokudb_pretty_key(db, left_key, "?", &key_str);
            log_str.append(", \"key\":");
            log_str.append("\""); log_str.append(key_str); log_str.append("\"");
        } else {
            String left_str;
            tokudb_pretty_left_key(db, left_key, &left_str);
            log_str.append(", \"key_left\":");
            log_str.append("\""); log_str.append(left_str); log_str.append("\"");
            String right_str;
            tokudb_pretty_right_key(db, right_key, &right_str);
            log_str.append(", \"key_right\":");
            log_str.append("\""); log_str.append(right_str); log_str.append("\"");
        }
        log_str.append("}");
        // set last_lock_timeout
        if (lock_timeout_debug & 1) {
            char *old_lock_timeout = THDVAR(thd, last_lock_timeout);
            char *new_lock_timeout = tokudb_my_strdup(log_str.c_ptr(), MY_FAE);
            THDVAR(thd, last_lock_timeout) = new_lock_timeout;
            tokudb_my_free(old_lock_timeout);
#if TOKU_THDVAR_MEMALLOC_BUG
            tokudb_pthread_mutex_lock(&tokudb_map_mutex);
            struct tokudb_map_pair old_key = { thd, old_lock_timeout };
            tree_delete(&tokudb_map, &old_key, sizeof old_key, NULL);
            struct tokudb_map_pair new_key = { thd, new_lock_timeout };
            tree_insert(&tokudb_map, &new_key, sizeof new_key, NULL);
            tokudb_pthread_mutex_unlock(&tokudb_map_mutex);
#endif
        }
        // dump to stderr
        if (lock_timeout_debug & 2) {
            sql_print_error("%s: %s", tokudb_hton_name, log_str.c_ptr());
        }
    }
}

static struct st_mysql_information_schema tokudb_trx_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_trx_field_info[] = {
    {"trx_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"trx_mysql_thread_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

struct tokudb_trx_extra {
    THD *thd;
    TABLE *table;
};

static int tokudb_trx_callback(uint64_t txn_id, uint64_t client_id, iterate_row_locks_callback iterate_locks, void *locks_extra, void *extra) {
    struct tokudb_trx_extra *e = reinterpret_cast<struct tokudb_trx_extra *>(extra);
    THD *thd = e->thd;
    TABLE *table = e->table;
    table->field[0]->store(txn_id, false);
    table->field[1]->store(client_id, false);
    int error = schema_table_store_record(thd, table);
    if (!error && thd_killed(thd))
        error = ER_QUERY_INTERRUPTED;
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_trx_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_trx_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        struct tokudb_trx_extra e = { thd, tables->table };
        error = db_env->iterate_live_transactions(db_env, tokudb_trx_callback, &e);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_trx_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_trx_field_info;
    schema->fill_table = tokudb_trx_fill_table;
    return 0;
}

static int tokudb_trx_done(void *p) {
    return 0;
}

static struct st_mysql_information_schema tokudb_lock_waits_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_lock_waits_field_info[] = {
    {"requesting_trx_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"blocking_trx_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_dname", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_key_left", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_key_right", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_start_time", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_table_schema", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"lock_waits_table_dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

struct tokudb_lock_waits_extra {
    THD *thd;
    TABLE *table;
};

static int tokudb_lock_waits_callback(DB *db, uint64_t requesting_txnid, const DBT *left_key, const DBT *right_key, 
                                      uint64_t blocking_txnid, uint64_t start_time, void *extra) {
    struct tokudb_lock_waits_extra *e = reinterpret_cast<struct tokudb_lock_waits_extra *>(extra);
    THD *thd = e->thd;
    TABLE *table = e->table;
    table->field[0]->store(requesting_txnid, false);
    table->field[1]->store(blocking_txnid, false);
    const char *dname = tokudb_get_index_name(db);
    size_t dname_length = strlen(dname);
    table->field[2]->store(dname, dname_length, system_charset_info);
    String left_str;
    tokudb_pretty_left_key(db, left_key, &left_str);
    table->field[3]->store(left_str.ptr(), left_str.length(), system_charset_info);
    String right_str;
    tokudb_pretty_right_key(db, right_key, &right_str);
    table->field[4]->store(right_str.ptr(), right_str.length(), system_charset_info);
    table->field[5]->store(start_time, false);

    String database_name, table_name, dictionary_name;
    tokudb_split_dname(dname, database_name, table_name, dictionary_name);
    table->field[6]->store(database_name.c_ptr(), database_name.length(), system_charset_info);
    table->field[7]->store(table_name.c_ptr(), table_name.length(), system_charset_info);
    table->field[8]->store(dictionary_name.c_ptr(), dictionary_name.length(), system_charset_info);

    int error = schema_table_store_record(thd, table);

    if (!error && thd_killed(thd))
        error = ER_QUERY_INTERRUPTED;

    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_lock_waits_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_lock_waits_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        struct tokudb_lock_waits_extra e = { thd, tables->table };
        error = db_env->iterate_pending_lock_requests(db_env, tokudb_lock_waits_callback, &e);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_lock_waits_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_lock_waits_field_info;
    schema->fill_table = tokudb_lock_waits_fill_table;
    return 0;
}

static int tokudb_lock_waits_done(void *p) {
    return 0;
}

static struct st_mysql_information_schema tokudb_locks_information_schema = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

static ST_FIELD_INFO tokudb_locks_field_info[] = {
    {"locks_trx_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_mysql_thread_id", 0, MYSQL_TYPE_LONGLONG, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_dname", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_key_left", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_key_right", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_table_schema", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_table_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {"locks_table_dictionary_name", 256, MYSQL_TYPE_STRING, 0, 0, NULL, SKIP_OPEN_TABLE },
    {NULL, 0, MYSQL_TYPE_NULL, 0, 0, NULL, SKIP_OPEN_TABLE}
};

struct tokudb_locks_extra {
    THD *thd;
    TABLE *table;
};

static int tokudb_locks_callback(uint64_t txn_id, uint64_t client_id, iterate_row_locks_callback iterate_locks, void *locks_extra, void *extra) {
    struct tokudb_locks_extra *e = reinterpret_cast<struct tokudb_locks_extra *>(extra);
    THD *thd = e->thd;
    TABLE *table = e->table;
    int error = 0;
    DB *db;
    DBT left_key, right_key;
    while (error == 0 && iterate_locks(&db, &left_key, &right_key, locks_extra) == 0) {
        table->field[0]->store(txn_id, false);
        table->field[1]->store(client_id, false);

        const char *dname = tokudb_get_index_name(db);
        size_t dname_length = strlen(dname);
        table->field[2]->store(dname, dname_length, system_charset_info);

        String left_str;
        tokudb_pretty_left_key(db, &left_key, &left_str);
        table->field[3]->store(left_str.ptr(), left_str.length(), system_charset_info);

        String right_str;
        tokudb_pretty_right_key(db, &right_key, &right_str);
        table->field[4]->store(right_str.ptr(), right_str.length(), system_charset_info);

        String database_name, table_name, dictionary_name;
        tokudb_split_dname(dname, database_name, table_name, dictionary_name);
        table->field[5]->store(database_name.c_ptr(), database_name.length(), system_charset_info);
        table->field[6]->store(table_name.c_ptr(), table_name.length(), system_charset_info);
        table->field[7]->store(dictionary_name.c_ptr(), dictionary_name.length(), system_charset_info);

        error = schema_table_store_record(thd, table);

        if (!error && thd_killed(thd))
            error = ER_QUERY_INTERRUPTED;
    }
    return error;
}

#if MYSQL_VERSION_ID >= 50600
static int tokudb_locks_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
#else
static int tokudb_locks_fill_table(THD *thd, TABLE_LIST *tables, COND *cond) {
#endif
    TOKUDB_DBUG_ENTER("");
    int error;
    
    rw_rdlock(&tokudb_hton_initialized_lock);

    if (!tokudb_hton_initialized) {
        error = ER_PLUGIN_IS_NOT_LOADED;
        my_error(error, MYF(0), tokudb_hton_name);
    } else {
        struct tokudb_locks_extra e = { thd, tables->table };
        error = db_env->iterate_live_transactions(db_env, tokudb_locks_callback, &e);
        if (error)
            my_error(ER_GET_ERRNO, MYF(0), error, tokudb_hton_name);
    }

    rw_unlock(&tokudb_hton_initialized_lock);
    TOKUDB_DBUG_RETURN(error);
}

static int tokudb_locks_init(void *p) {
    ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
    schema->fields_info = tokudb_locks_field_info;
    schema->fill_table = tokudb_locks_fill_table;
    return 0;
}

static int tokudb_locks_done(void *p) {
    return 0;
}

// Retrieves variables for information_schema.global_status.
// Names (columnname) are automatically converted to upper case, and prefixed with "TOKUDB_"
static int show_tokudb_vars(THD *thd, SHOW_VAR *var, char *buff) {
    TOKUDB_DBUG_ENTER("");

    int error;
    uint64_t panic;
    const int panic_string_len = 1024;
    char panic_string[panic_string_len] = {'\0'};
    fs_redzone_state redzone_state;

    uint64_t num_rows;
    error = db_env->get_engine_status (db_env, toku_global_status_rows, toku_global_status_max_rows, &num_rows, &redzone_state, &panic, panic_string, panic_string_len, TOKU_GLOBAL_STATUS);
    //TODO: Maybe do something with the panic output?
    if (error == 0) {
        assert(num_rows <= toku_global_status_max_rows);
        //TODO: Maybe enable some of the items here: (copied from engine status

        //TODO: (optionally) add redzone state, panic, panic string, etc. Right now it's being ignored.

        for (uint64_t row = 0; row < num_rows; row++) {
            SHOW_VAR &status_var = toku_global_status_variables[row];
            TOKU_ENGINE_STATUS_ROW_S &status_row = toku_global_status_rows[row];

            status_var.name = status_row.columnname;
            switch (status_row.type) {
            case FS_STATE:
            case UINT64:
                status_var.type = SHOW_LONGLONG;
                status_var.value = (char*)&status_row.value.num;
                break;
            case CHARSTR:
                status_var.type = SHOW_CHAR;
                status_var.value = (char*)status_row.value.str;
                break;
            case UNIXTIME: {
                status_var.type = SHOW_CHAR;
                time_t t = status_row.value.num;
                char tbuf[26];
                // Reuse the memory in status_row. (It belongs to us).
                snprintf(status_row.value.datebuf, sizeof(status_row.value.datebuf), "%.24s", ctime_r(&t, tbuf));
                status_var.value = (char*)&status_row.value.datebuf[0];
                break;
            }
            case TOKUTIME:
                status_var.type = SHOW_DOUBLE;
                // Reuse the memory in status_row. (It belongs to us).
                status_row.value.dnum = tokutime_to_seconds(status_row.value.num);
                status_var.value = (char*)&status_row.value.dnum;
                break;
            case PARCOUNT: {
                status_var.type = SHOW_LONGLONG;
                uint64_t v = read_partitioned_counter(status_row.value.parcount);
                // Reuse the memory in status_row. (It belongs to us).
                status_row.value.num = v;
                status_var.value = (char*)&status_row.value.num;
                break;
            }
            case DOUBLE:
                status_var.type = SHOW_DOUBLE;
                status_var.value = (char*) &status_row.value.dnum;
                break;
            default:
                status_var.type = SHOW_CHAR;
                // Reuse the memory in status_row.datebuf. (It belongs to us).
                // UNKNOWN TYPE: %d fits in 26 bytes (sizeof datebuf) for any integer.
                snprintf(status_row.value.datebuf, sizeof(status_row.value.datebuf), "UNKNOWN TYPE: %d", status_row.type);
                status_var.value = (char*)&status_row.value.datebuf[0];
                break;
            }
        }
        // Sentinel value at end.
        toku_global_status_variables[num_rows].type = SHOW_LONG;
        toku_global_status_variables[num_rows].value = (char*)NullS;
        toku_global_status_variables[num_rows].name = (char*)NullS;

        var->type= SHOW_ARRAY;
        var->value= (char *) toku_global_status_variables;
    }
    if (error) { my_errno = error; }
    TOKUDB_DBUG_RETURN(error);
}

static SHOW_VAR toku_global_status_variables_export[]= {
    {"Tokudb", (char*)&show_tokudb_vars, SHOW_FUNC},
    {NullS, NullS, SHOW_LONG}
};

#if TOKU_INCLUDE_BACKTRACE
#include <execinfo.h>
static void tokudb_backtrace(void) {
    const int N_POINTERS = 30;
    void *backtrace_pointers[N_POINTERS];
    int n = backtrace(backtrace_pointers, N_POINTERS);
    backtrace_symbols_fd(backtrace_pointers, n, fileno(stderr));
}
#endif

#if defined(TOKUDB_VERSION_MAJOR) && defined(TOKUDB_VERSION_MINOR)
#define TOKUDB_PLUGIN_VERSION ((TOKUDB_VERSION_MAJOR << 8) + TOKUDB_VERSION_MINOR)
#else
#define TOKUDB_PLUGIN_VERSION 0
#endif

#ifdef MARIA_PLUGIN_INTERFACE_VERSION
maria_declare_plugin(tokudb) 
#else
mysql_declare_plugin(tokudb) 
#endif
{
    MYSQL_STORAGE_ENGINE_PLUGIN, 
    &tokudb_storage_engine, 
    tokudb_hton_name, 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_init_func,          /* plugin init */
    tokudb_done_func,          /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    toku_global_status_variables_export,  /* status variables */
    tokudb_system_variables,   /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_trx_information_schema,
    "TokuDB_trx", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_trx_init,     /* plugin init */
    tokudb_trx_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_lock_waits_information_schema,
    "TokuDB_lock_waits", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_lock_waits_init,     /* plugin init */
    tokudb_lock_waits_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_locks_information_schema,
    "TokuDB_locks", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_locks_init,     /* plugin init */
    tokudb_locks_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_file_map_information_schema, 
    "TokuDB_file_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_file_map_init,     /* plugin init */
    tokudb_file_map_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_info_information_schema, 
    "TokuDB_fractal_tree_info", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_fractal_tree_info_init,     /* plugin init */
    tokudb_fractal_tree_info_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
},
{
    MYSQL_INFORMATION_SCHEMA_PLUGIN, 
    &tokudb_fractal_tree_block_map_information_schema, 
    "TokuDB_fractal_tree_block_map", 
    "Tokutek Inc", 
    "Tokutek TokuDB Storage Engine with Fractal Tree(tm) Technology",
    PLUGIN_LICENSE_GPL,
    tokudb_fractal_tree_block_map_init,     /* plugin init */
    tokudb_fractal_tree_block_map_done,     /* plugin deinit */
    TOKUDB_PLUGIN_VERSION,
    NULL,                      /* status variables */
    NULL,                      /* system variables */
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
    tokudb_version,
    MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
#else
    NULL,                      /* config options */
    0,                         /* flags */
#endif
}
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
maria_declare_plugin_end;
#else
mysql_declare_plugin_end;
#endif
