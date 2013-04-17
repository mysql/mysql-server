#ifndef _HATOKU_DEF
#define _HATOKU_DEF

#include "mysql_version.h"
#if MYSQL_VERSION_ID < 50506
#include "mysql_priv.h"
#else
#include "sql_table.h"
#include "handler.h"
#include "table.h"
#include "log.h"
#include "sql_class.h"
#include "sql_show.h"
#include "discover.h"
#endif

#include "db.h"
#include "toku_os.h"

#ifdef USE_PRAGMA_INTERFACE
#pragma interface               /* gcc class implementation */
#endif

// In MariaDB 5.3, thread progress reporting was introduced.
// Only include that functionality if we're using maria 5.3 +
#ifdef MARIADB_BASE_VERSION
#if MYSQL_VERSION_ID >= 50300
#define HA_TOKUDB_HAS_THD_PROGRESS
#endif
#endif

#if defined(TOKUDB_PATCHES) && TOKUDB_PATCHES == 0

#elif 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
#define TOKU_INCLUDE_ALTER_56 0
#define TOKU_INCLUDE_ALTER_55 0
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 0
#define TOKU_INCLUDE_XA 1
#define TOKU_PARTITION_WRITE_FRM_DATA 0
#define TOKU_INCLUDE_WRITE_FRM_DATA 0

#elif 50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699
#define TOKU_INCLUDE_ALTER_56 1
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 1
#define TOKU_INCLUDE_XA 1
#define TOKU_PARTITION_WRITE_FRM_DATA 1
#define TOKU_INCLUDE_WRITE_FRM_DATA 1
#define TOKU_INCLUDE_UPSERT 1
#if DB_TYPE_TOKUDB_DEFINED
#define TOKU_INCLUDE_EXTENDED_KEYS 1
#endif
#define TOKU_INCLUDE_ANALYZE 1

#elif 50500 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50599
#define TOKU_INCLUDE_ALTER_56 1
#define TOKU_INCLUDE_ALTER_55 1
#define TOKU_INCLUDE_ROW_TYPE_COMPRESSION 1
#define TOKU_INCLUDE_XA 1
#define TOKU_PARTITION_WRITE_FRM_DATA 1
#define TOKU_INCLUDE_WRITE_FRM_DATA 1
#define TOKU_INCLUDE_UPSERT 1
#if defined(MARIADB_BASE_VERSION) && DB_TYPE_TOKUDB_DEFINED
#define TOKU_INCLUDE_EXTENDED_KEYS 1
#endif
#define TOKU_INCLUDE_ANALYZE 1

#else

#error

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

extern ulong tokudb_debug;

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

// tokudb debug tracing
#define TOKUDB_DEBUG_INIT 1
#define TOKUDB_DEBUG_OPEN 2
#define TOKUDB_DEBUG_ENTER 4
#define TOKUDB_DEBUG_RETURN 8
#define TOKUDB_DEBUG_ERROR 16
#define TOKUDB_DEBUG_TXN 32
#define TOKUDB_DEBUG_AUTO_INCREMENT 64
#define TOKUDB_DEBUG_LOCK 256
#define TOKUDB_DEBUG_LOCKRETRY 512
#define TOKUDB_DEBUG_CHECK_KEY 1024
#define TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS 2048
#define TOKUDB_DEBUG_ALTER_TABLE_INFO 4096
#define TOKUDB_DEBUG_UPSERT 8192
#define TOKUDB_DEBUG_CHECK (1<<14)
#define TOKUDB_DEBUG_ANALYZE (1<<15)

#define TOKUDB_TRACE(f, ...) \
    printf("%d:%s:%d:" f, my_tid(), __FILE__, __LINE__, ##__VA_ARGS__);


static inline unsigned int my_tid() {
    return (unsigned int)toku_os_gettid();
}



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
} tokudb_trx_data;

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

static inline void commit_txn(DB_TXN* txn, uint32_t flags) {
    if (tokudb_debug & TOKUDB_DEBUG_TXN)
        TOKUDB_TRACE("commit_txn %p\n", txn);
    int r = txn->commit(txn, flags);
    if (r != 0) {
        sql_print_error("tried committing transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

static inline void abort_txn(DB_TXN* txn) {
    if (tokudb_debug & TOKUDB_DEBUG_TXN)
        TOKUDB_TRACE("abort_txn %p\n", txn);
    int r = txn->abort(txn);
    if (r != 0) {
        sql_print_error("tried aborting transaction %p and got error code %d", txn, r);
    }
    assert(r == 0);
}

/* The purpose of this file is to define assert() for use by the handlerton.
 * The intention is for a failed handlerton assert to invoke a failed assert
 * in the fractal tree layer, which dumps engine status to the error log.
 */

void toku_hton_assert_fail(const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/, int/*errno*/) __attribute__((__visibility__("default"))) __attribute__((__noreturn__));

#undef assert  
#define assert(expr)      ((expr)      ? (void)0 : toku_hton_assert_fail(#expr, __FUNCTION__, __FILE__, __LINE__, errno))

#endif
