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

#ifndef _TOKUDB_CONFIG_H
#define _TOKUDB_CONFIG_H

#ifdef USE_PRAGMA_INTERFACE
#pragma interface               /* gcc class implementation */
#endif

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
// mariadb 10
#define TOKU_USE_DB_TYPE_TOKUDB 1
#define TOKU_INCLUDE_ALTER_56 1
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 0
#define TOKU_INCLUDE_XA 1
#define TOKU_INCLUDE_WRITE_FRM_DATA 0
#if defined(MARIADB_BASE_VERSION)
#define TOKU_INCLUDE_EXTENDED_KEYS 1
#endif

#elif 50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799
// mysql 5.7 with no patches
#define TOKU_USE_DB_TYPE_UNKNOWN 1
#define TOKU_INCLUDE_ALTER_56 1
#define TOKU_PARTITION_WRITE_FRM_DATA 0

#elif 50613 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699
// mysql 5.6
#define TOKU_USE_DB_TYPE_TOKUDB 1
#define TOKU_INCLUDE_ALTER_56 1
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 1
#define TOKU_INCLUDE_XA 1
#define TOKU_PARTITION_WRITE_FRM_DATA 0
#define TOKU_INCLUDE_WRITE_FRM_DATA 0
#define TOKU_INCLUDE_UPSERT 1
#if defined(HTON_SUPPORTS_EXTENDED_KEYS)
#define TOKU_INCLUDE_EXTENDED_KEYS 1
#endif

#elif 50500 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50599
#define TOKU_USE_DB_TYPE_TOKUDB 1
#define TOKU_INCLUDE_ALTER_56 0         /* MariaDB 5.5 */
#define TOKU_INCLUDE_ALTER_55 0         /* MariaDB 5.5 */
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 1
#define TOKU_INCLUDE_XA 1
#define TOKU_PARTITION_WRITE_FRM_DATA 0 /* MariaDB 5.5 */
#define TOKU_INCLUDE_WRITE_FRM_DATA 0   /* MariaDB 5.5 */
#define TOKU_INCLUDE_UPSERT 0           /* MariaDB 5.5 */
#if defined(MARIADB_BASE_VERSION)
#define TOKU_INCLUDE_EXTENDED_KEYS 1
#endif
#define TOKU_INCLUDE_HANDLERTON_HANDLE_FATAL_SIGNAL 0 /* MariaDB 5.5 */

#else
#error

#endif

#ifdef MARIADB_BASE_VERSION
// In MariaDB 5.3, thread progress reporting was introduced.
// Only include that functionality if we're using maria 5.3 +
#define HA_TOKUDB_HAS_THD_PROGRESS 1

// MariaDB supports thdvar memalloc correctly
#define TOKU_THDVAR_MEMALLOC_BUG 0
#else
// MySQL does not support thdvar memalloc correctly
// see http://bugs.mysql.com/bug.php?id=71759
#define TOKU_THDVAR_MEMALLOC_BUG 1
#endif

#if !defined(HA_CLUSTERING)
#define HA_CLUSTERING 0
#endif

#if !defined(HA_CLUSTERED_INDEX)
#define HA_CLUSTERED_INDEX 0
#endif

#if !defined(HA_CAN_WRITE_DURING_OPTIMIZE)
#define HA_CAN_WRITE_DURING_OPTIMIZE 0
#endif

// In older (< 5.5) versions of MySQL and MariaDB, it is necessary to 
// use a read/write lock on the key_file array in a table share, 
// because table locks do not protect the race of some thread closing 
// a table and another calling ha_tokudb::info()
//
// In version 5.5 and, a higher layer "metadata lock" was introduced
// to synchronize threads that open, close, call info(), etc on tables.
// In these versions, we don't need the key_file lock
#if MYSQL_VERSION_ID < 50500
#define HA_TOKUDB_NEEDS_KEY_FILE_LOCK
#endif

//
// returns maximum length of dictionary name, such as key-NAME
// NAME_CHAR_LEN is max length of the key name, and have upper bound of 10 for key-
//
#define MAX_DICT_NAME_LEN NAME_CHAR_LEN + 10

// QQQ how to tune these?
#define HA_TOKUDB_RANGE_COUNT   100
/* extra rows for estimate_rows_upper_bound() */
#define HA_TOKUDB_EXTRA_ROWS    100

/* Bits for share->status */
#define STATUS_PRIMARY_KEY_INIT 0x1

#endif // _TOKUDB_CONFIG_H

#ifndef _TOKUDB_DEBUG_H
#define _TOKUDB_DEBUG_H

extern ulong tokudb_debug;

// tokudb debug tracing
#define TOKUDB_DEBUG_INIT 1
#define TOKUDB_DEBUG_OPEN 2
#define TOKUDB_DEBUG_ENTER 4
#define TOKUDB_DEBUG_RETURN 8
#define TOKUDB_DEBUG_ERROR 16
#define TOKUDB_DEBUG_TXN 32
#define TOKUDB_DEBUG_AUTO_INCREMENT 64
#define TOKUDB_DEBUG_LOCK 256
#define TOKUDB_DEBUG_CHECK_KEY 1024
#define TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS 2048
#define TOKUDB_DEBUG_ALTER_TABLE 4096
#define TOKUDB_DEBUG_UPSERT 8192
#define TOKUDB_DEBUG_CHECK (1<<14)
#define TOKUDB_DEBUG_ANALYZE (1<<15)

#define TOKUDB_TRACE(f, ...) { \
    fprintf(stderr, "%u %s:%u %s " f "\n", my_tid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
}

static inline unsigned int my_tid() {
    return (unsigned int)toku_os_gettid();
}

#define TOKUDB_DBUG_ENTER(f, ...) { \
    if (tokudb_debug & TOKUDB_DEBUG_ENTER) { \
        TOKUDB_TRACE(f, ##__VA_ARGS__);       \
    } \
} \
    DBUG_ENTER(__FUNCTION__);

#define TOKUDB_DBUG_RETURN(r) { \
    int rr = (r); \
    if ((tokudb_debug & TOKUDB_DEBUG_RETURN) || (rr != 0 && (tokudb_debug & TOKUDB_DEBUG_ERROR))) { \
        TOKUDB_TRACE("return %d", rr); \
    } \
    DBUG_RETURN(rr); \
}

#define TOKUDB_HANDLER_TRACE(f, ...) \
    fprintf(stderr, "%u %p %s:%u ha_tokudb::%s " f "\n", my_tid(), this, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);

#define TOKUDB_HANDLER_DBUG_ENTER(f, ...) { \
    if (tokudb_debug & TOKUDB_DEBUG_ENTER) { \
        TOKUDB_HANDLER_TRACE(f, ##__VA_ARGS__); \
    } \
} \
    DBUG_ENTER(__FUNCTION__);

#define TOKUDB_HANDLER_DBUG_RETURN(r) { \
    int rr = (r); \
    if ((tokudb_debug & TOKUDB_DEBUG_RETURN) || (rr != 0 && (tokudb_debug & TOKUDB_DEBUG_ERROR))) { \
        TOKUDB_HANDLER_TRACE("return %d", rr); \
    } \
    DBUG_RETURN(rr); \
}

#define TOKUDB_HANDLER_DBUG_VOID_RETURN { \
    if (tokudb_debug & TOKUDB_DEBUG_RETURN) { \
        TOKUDB_HANDLER_TRACE("return");       \
    } \
    DBUG_VOID_RETURN; \
}

#define TOKUDB_DBUG_DUMP(s, p, len) \
{ \
    TOKUDB_TRACE("%s", s); \
    uint i;                                                             \
    for (i=0; i<len; i++) {                                             \
        fprintf(stderr, "%2.2x", ((uchar*)p)[i]);                       \
    }                                                                   \
    fprintf(stderr, "\n");                                              \
}

/* The purpose of this file is to define assert() for use by the handlerton.
 * The intention is for a failed handlerton assert to invoke a failed assert
 * in the fractal tree layer, which dumps engine status to the error log.
 */

void toku_hton_assert_fail(const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/, int/*errno*/) __attribute__((__visibility__("default"))) __attribute__((__noreturn__));

#undef assert  
#define assert(expr)      ((expr)      ? (void)0 : toku_hton_assert_fail(#expr, __FUNCTION__, __FILE__, __LINE__, errno))

#endif // _TOKUDB_DEBUG_H

#ifndef _TOKUDB_TXN_H
#define _TOKUDB_TXN_H

typedef enum {
    hatoku_iso_not_set = 0,
    hatoku_iso_read_uncommitted,
    hatoku_iso_read_committed,
    hatoku_iso_repeatable_read,
    hatoku_iso_serializable
} HA_TOKU_ISO_LEVEL;



typedef struct st_tokudb_stmt_progress {
    ulonglong inserted;
    ulonglong updated;
    ulonglong deleted;
    ulonglong queried;
    bool using_loader;
} tokudb_stmt_progress;


typedef struct st_tokudb_trx_data {
    DB_TXN *all;
    DB_TXN *stmt;
    DB_TXN *sp_level;
    DB_TXN *sub_sp_level;
    uint tokudb_lock_count;
    tokudb_stmt_progress stmt_progress;
    bool checkpoint_lock_taken;
    LIST *handlers;
} tokudb_trx_data;

struct ha_table_option_struct
{
  uint row_format;
};

extern char *tokudb_data_dir;
extern const char *ha_tokudb_ext;

static inline void reset_stmt_progress (tokudb_stmt_progress* val) {
    val->deleted = 0;
    val->inserted = 0;
    val->updated = 0;
    val->queried = 0;
}

static inline int get_name_length(const char *name) {
    int n = 0;
    const char *newname = name;
    n += strlen(newname);
    n += strlen(ha_tokudb_ext);
    return n;
}

//
// returns maximum length of path to a dictionary
//
static inline int get_max_dict_name_path_length(const char *tablename) {
    int n = 0;
    n += get_name_length(tablename);
    n += 1; //for the '-'
    n += MAX_DICT_NAME_LEN;
    return n;
}

static inline void make_name(char *newname, const char *tablename, const char *dictname) {
    const char *newtablename = tablename;
    char *nn = newname;
    assert(tablename);
    assert(dictname);
    nn += sprintf(nn, "%s", newtablename);
    nn += sprintf(nn, "-%s", dictname);
}

static inline int txn_begin(DB_ENV *env, DB_TXN *parent, DB_TXN **txn, uint32_t flags, THD *thd) {
    *txn = NULL;
    int r = env->txn_begin(env, parent, txn, flags);
    if (r == 0 && thd) {
        DB_TXN *this_txn = *txn;
        this_txn->set_client_id(this_txn, thd_get_thread_id(thd));
    }
    if ((tokudb_debug & TOKUDB_DEBUG_TXN)) {
        TOKUDB_TRACE("begin txn %p %p %u r=%d", parent, *txn, flags, r);
    }
    return r;
}

static inline void commit_txn(DB_TXN* txn, uint32_t flags) {
    if (tokudb_debug & TOKUDB_DEBUG_TXN)
        TOKUDB_TRACE("commit txn %p", txn);
    int r = txn->commit(txn, flags);
    if (r != 0) {
        sql_print_error("tried committing transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

static inline void abort_txn(DB_TXN* txn) {
    if (tokudb_debug & TOKUDB_DEBUG_TXN)
        TOKUDB_TRACE("abort txn %p", txn);
    int r = txn->abort(txn);
    if (r != 0) {
        sql_print_error("tried aborting transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

#endif // _TOKUDB_TXN_H

#ifndef _TOKUDB_PORTABILITY_H
#define _TOKUDB_PORTABILITY_H

static inline void *tokudb_my_malloc(size_t s, myf flags) {
#if 50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799
    return my_malloc(0, s, flags);
#else
    return my_malloc(s, flags);
#endif
}

static inline void *tokudb_my_realloc(void *p, size_t s, myf flags) {
#if 50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799
    return my_realloc(0, p, s, flags);
#else
    return my_realloc(p, s, flags);
#endif
}

static inline void tokudb_my_free(void *ptr) {
    my_free(ptr);
}

static inline char *tokudb_my_strdup(const char *p, myf flags) {
#if 50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799
    return my_strdup(0, p, flags);
#else
    return my_strdup(p, flags);
#endif
}

static inline void* tokudb_my_multi_malloc(myf myFlags, ...) {
  va_list args;
  char **ptr,*start,*res;
  size_t tot_length,length;

  va_start(args,myFlags);
  tot_length=0;
  while ((ptr=va_arg(args, char **))) {
      length=va_arg(args,uint);
      tot_length+=ALIGN_SIZE(length);
  }
  va_end(args);

  if (!(start=(char *) tokudb_my_malloc(tot_length,myFlags))) {
      return 0;
  }

  va_start(args,myFlags);
  res=start;
  while ((ptr=va_arg(args, char **))) {
      *ptr=res;
      length=va_arg(args,uint);
      res+=ALIGN_SIZE(length);
  }
  va_end(args);
  return start;
}

static inline void tokudb_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    int r = pthread_mutex_init(mutex, attr);
    assert(r == 0);
}

static inline void tokudb_pthread_mutex_destroy(pthread_mutex_t *mutex) {
    int r = pthread_mutex_destroy(mutex);
    assert(r == 0);
}

static inline void tokudb_pthread_mutex_lock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_lock(mutex);
    assert(r == 0);
}

static inline void tokudb_pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_unlock(mutex);
    assert(r == 0);
}

static inline void tokudb_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    int r = pthread_cond_init(cond, attr);
    assert(r == 0);
}

static inline void tokudb_pthread_cond_destroy(pthread_cond_t *cond) {
    int r = pthread_cond_destroy(cond);
    assert(r == 0);
}

static inline void tokudb_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    int r = pthread_cond_wait(cond, mutex);
    assert(r == 0);
}

static inline void tokudb_pthread_cond_broadcast(pthread_cond_t *cond) {
    int r = pthread_cond_broadcast(cond);
    assert(r == 0);
}

// mysql 5.6.15 removed the test macro, so we define our own
#define tokudb_test(e) ((e) ? 1 : 0)

static const char *tokudb_thd_get_proc_info(THD *thd) {
    return thd->proc_info;
}

// uint3korr reads 4 bytes and valgrind reports an error, so we use this function instead
static uint tokudb_uint3korr(const uchar *a) {
    uchar b[4] = {};
    memcpy(b, a, 3);
    return uint3korr(b);
}

#endif // _TOKUDB_PORTABILITY_H
