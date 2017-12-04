#ifndef HANDLER_INCLUDED
#define HANDLER_INCLUDED

/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Definitions for parameters to do with handler-routines */

#include "my_global.h"
#include "ft_global.h"         // ft_hints
#include "my_thread_local.h"   // my_errno
#include "thr_lock.h"          // thr_lock_type
#include "discrete_interval.h" // Discrete_interval
#include "key.h"               // KEY
#include "mysqld.h"            // lower_case_table_names
#include "sql_const.h"         // SHOW_COMP_OPTION
#include "sql_list.h"          // SQL_I_List
#include "sql_plugin_ref.h"    // plugin_ref

#include "mysql/psi/psi.h"

#include <algorithm>
#include <string>

class Alter_info;
class SE_cost_constants;     // see opt_costconstants.h
class String;
struct TABLE_LIST;
typedef struct st_bitmap MY_BITMAP;
typedef struct st_hash HASH;
typedef struct st_key_cache KEY_CACHE;
typedef struct xid_t XID;
class partition_info;
class Partition_handler;
typedef my_bool (*qc_engine_callback)(THD *thd, char *table_key,
                                      uint key_length,
                                      ulonglong *engine_data);


// the following is for checking tables

#define HA_ADMIN_ALREADY_DONE	  1
#define HA_ADMIN_OK               0
#define HA_ADMIN_NOT_IMPLEMENTED -1
#define HA_ADMIN_FAILED		 -2
#define HA_ADMIN_CORRUPT         -3
#define HA_ADMIN_INTERNAL_ERROR  -4
#define HA_ADMIN_INVALID         -5
#define HA_ADMIN_REJECT          -6
#define HA_ADMIN_TRY_ALTER       -7
#define HA_ADMIN_WRONG_CHECKSUM  -8
#define HA_ADMIN_NOT_BASE_TABLE  -9
#define HA_ADMIN_NEEDS_UPGRADE  -10
#define HA_ADMIN_NEEDS_ALTER    -11
#define HA_ADMIN_NEEDS_CHECK    -12
/** Needs ALTER TABLE t UPGRADE PARTITIONING. */
#define HA_ADMIN_NEEDS_UPG_PART -13
/** Needs to dump and re-create to fix pre 5.0 decimal types */
#define HA_ADMIN_NEEDS_DUMP_UPGRADE -14

/**
   Return values for check_if_supported_inplace_alter().

   @see check_if_supported_inplace_alter() for description of
   the individual values.
*/
enum enum_alter_inplace_result {
  HA_ALTER_ERROR,
  HA_ALTER_INPLACE_NOT_SUPPORTED,
  HA_ALTER_INPLACE_EXCLUSIVE_LOCK,
  HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE,
  HA_ALTER_INPLACE_SHARED_LOCK,
  HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE,
  HA_ALTER_INPLACE_NO_LOCK
};

/* Bits in table_flags() to show what database can do */

#define HA_NO_TRANSACTIONS     (1 << 0) /* Doesn't support transactions */
#define HA_PARTIAL_COLUMN_READ (1 << 1) /* read may not return all columns */
#define HA_TABLE_SCAN_ON_INDEX (1 << 2) /* No separate data/index file */
/*
  The following should be set if the following is not true when scanning
  a table with rnd_next()
  - We will see all rows (including deleted ones)
  - Row positions are 'table->s->db_record_offset' apart
  If this flag is not set, filesort will do a position() call for each matched
  row to be able to find the row later.
*/
#define HA_REC_NOT_IN_SEQ      (1 << 3)
#define HA_CAN_GEOMETRY        (1 << 4)
/*
  Reading keys in random order is as fast as reading keys in sort order
  (Used in records.cc to decide if we should use a record cache and by
  filesort to decide if we should sort key + data or key + pointer-to-row
*/
#define HA_FAST_KEY_READ       (1 << 5)
/*
  Set the following flag if we on delete should force all key to be read
  and on update read all keys that changes
*/
#define HA_REQUIRES_KEY_COLUMNS_FOR_DELETE (1 << 6)
#define HA_NULL_IN_KEY         (1 << 7) /* One can have keys with NULL */
#define HA_DUPLICATE_POS       (1 << 8)    /* position() gives dup row */
#define HA_NO_BLOBS            (1 << 9) /* Doesn't support blobs */
#define HA_CAN_INDEX_BLOBS     (1 << 10)
#define HA_AUTO_PART_KEY       (1 << 11) /* auto-increment in multi-part key */
#define HA_REQUIRE_PRIMARY_KEY (1 << 12) /* .. and can't create a hidden one */
#define HA_STATS_RECORDS_IS_EXACT (1 << 13) /* stats.records is exact */
/// Not in use.
#define HA_UNUSED  (1 << 14)
/*
  If we get the primary key columns for free when we do an index read
  (usually, it also implies that HA_PRIMARY_KEY_REQUIRED_FOR_POSITION
  flag is set).
*/
#define HA_PRIMARY_KEY_IN_READ_INDEX (1 << 15)
/*
  If HA_PRIMARY_KEY_REQUIRED_FOR_POSITION is set, it means that to position()
  uses a primary key given by the record argument.
  Without primary key, we can't call position().
  If not set, the position is returned as the current rows position
  regardless of what argument is given.
*/ 
#define HA_PRIMARY_KEY_REQUIRED_FOR_POSITION (1 << 16) 
#define HA_CAN_RTREEKEYS       (1 << 17)
#define HA_NOT_DELETE_WITH_CACHE (1 << 18)
/*
  The following is we need to a primary key to delete (and update) a row.
  If there is no primary key, all columns needs to be read on update and delete
*/
#define HA_PRIMARY_KEY_REQUIRED_FOR_DELETE (1 << 19)
#define HA_NO_PREFIX_CHAR_KEYS (1 << 20)
#define HA_CAN_FULLTEXT        (1 << 21)
#define HA_CAN_SQL_HANDLER     (1 << 22)
#define HA_NO_AUTO_INCREMENT   (1 << 23)
#define HA_HAS_CHECKSUM        (1 << 24)
/* Table data are stored in separate files (for lower_case_table_names) */
#define HA_FILE_BASED	       (1 << 26)
#define HA_NO_VARCHAR	       (1 << 27)
#define HA_CAN_BIT_FIELD       (1 << 28) /* supports bit fields */
#define HA_ANY_INDEX_MAY_BE_UNIQUE (1 << 30)
#define HA_NO_COPY_ON_ALTER    (1LL << 31)
#define HA_HAS_RECORDS	       (1LL << 32) /* records() gives exact count*/
/* Has it's own method of binlog logging */
#define HA_HAS_OWN_BINLOGGING  (1LL << 33)
/*
  Engine is capable of row-format and statement-format logging,
  respectively
*/
#define HA_BINLOG_ROW_CAPABLE  (1LL << 34)
#define HA_BINLOG_STMT_CAPABLE (1LL << 35)
/*
    When a multiple key conflict happens in a REPLACE command mysql
    expects the conflicts to be reported in the ascending order of
    key names.

    For e.g.

    CREATE TABLE t1 (a INT, UNIQUE (a), b INT NOT NULL, UNIQUE (b), c INT NOT
                     NULL, INDEX(c));

    REPLACE INTO t1 VALUES (1,1,1),(2,2,2),(2,1,3);

    MySQL expects the conflict with 'a' to be reported before the conflict with
    'b'.

    If the underlying storage engine does not report the conflicting keys in
    ascending order, it causes unexpected errors when the REPLACE command is
    executed.

    This flag helps the underlying SE to inform the server that the keys are not
    ordered.
*/
#define HA_DUPLICATE_KEY_NOT_IN_ORDER    (1LL << 36)
/*
  Engine supports REPAIR TABLE. Used by CHECK TABLE FOR UPGRADE if an
  incompatible table is detected. If this flag is set, CHECK TABLE FOR UPGRADE
  will report ER_TABLE_NEEDS_UPGRADE, otherwise ER_TABLE_NEED_REBUILD.
*/
#define HA_CAN_REPAIR                    (1LL << 37)

/*
  Set of all binlog flags. Currently only contain the capabilities
  flags.
 */
#define HA_BINLOG_FLAGS (HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE)

/**
  The handler supports read before write removal optimization

  Read before write removal may be used for storage engines which support
  write without previous read of the row to be updated. Handler returning
  this flag must implement start_read_removal() and end_read_removal().
  The handler may return "fake" rows constructed from the key of the row
  asked for. This is used to optimize UPDATE and DELETE by reducing the
  numer of roundtrips between handler and storage engine.
  
  Example:
  UPDATE a=1 WHERE pk IN (<keys>)

  mysql_update()
  {
    if (<conditions for starting read removal>)
      start_read_removal()
      -> handler returns true if read removal supported for this table/query

    while(read_record("pk=<key>"))
      -> handler returns fake row with column "pk" set to <key>

      ha_update_row()
      -> handler sends write "a=1" for row with "pk=<key>"

    end_read_removal()
    -> handler returns the number of rows actually written
  }

  @note This optimization in combination with batching may be used to
        remove even more roundtrips.
*/
#define HA_READ_BEFORE_WRITE_REMOVAL  (1LL << 38)

/*
  Engine supports extended fulltext API
 */
#define HA_CAN_FULLTEXT_EXT              (1LL << 39)

/*
  Storage engine doesn't synchronize result set with expected table contents.
  Used by replication slave to check if it is possible to retrieve rows from
  the table when deciding whether to do a full table scan, index scan or hash
  scan while applying a row event.
 */
#define HA_READ_OUT_OF_SYNC              (1LL << 40)

/*
  Storage engine supports table export using the
  FLUSH TABLE <table_list> FOR EXPORT statement.
 */
#define HA_CAN_EXPORT                 (1LL << 41)

/*
  The handler don't want accesses to this table to 
  be const-table optimized
*/
#define HA_BLOCK_CONST_TABLE          (1LL << 42)

/*
  Handler supports FULLTEXT hints
*/
#define HA_CAN_FULLTEXT_HINTS         (1LL << 43)

/**
  Storage engine doesn't support LOCK TABLE ... READ LOCAL locks
  but doesn't want to use handler::store_lock() API for upgrading
  them to LOCK TABLE ... READ locks, for example, because it doesn't
  use THR_LOCK locks at all.
*/
#define HA_NO_READ_LOCAL_LOCK         (1LL << 44)

/**
  A storage engine is compatible with the attachable transaction requirements
  means that

    - either SE detects the fact that THD::ha_data was reset and starts a new
      attachable transaction, closes attachable transaction on close_connection
      and resumes regular (outer) transaction when THD::ha_data is restored;

    - or SE completely ignores THD::ha_data and close_connection like MyISAM
      does.
*/
#define HA_ATTACHABLE_TRX_COMPATIBLE  (1LL << 45)

/**
  Handler supports Generated Columns
*/
#define HA_GENERATED_COLUMNS            (1LL << 46)

/**
  Supports index on virtual generated column
*/
#define HA_CAN_INDEX_VIRTUAL_GENERATED_COLUMN (1LL << 47)

/* bits in index_flags(index_number) for what you can do with index */
#define HA_READ_NEXT            1       /* TODO really use this flag */
#define HA_READ_PREV            2       /* supports ::index_prev */
#define HA_READ_ORDER           4       /* index_next/prev follow sort order */
#define HA_READ_RANGE           8       /* can find all records in a range */
#define HA_ONLY_WHOLE_INDEX	16	/* Can't use part key searches */
#define HA_KEYREAD_ONLY         64	/* Support HA_EXTRA_KEYREAD */
/*
  Index scan will not return records in rowid order. Not guaranteed to be
  set for unordered (e.g. HASH) indexes.
*/
#define HA_KEY_SCAN_NOT_ROR     128 
#define HA_DO_INDEX_COND_PUSHDOWN  256 /* Supports Index Condition Pushdown */

/* operations for disable/enable indexes */
#define HA_KEY_SWITCH_NONUNIQ      0
#define HA_KEY_SWITCH_ALL          1
#define HA_KEY_SWITCH_NONUNIQ_SAVE 2
#define HA_KEY_SWITCH_ALL_SAVE     3

/*
  Note: the following includes binlog and closing 0.
  so: innodb + bdb + ndb + binlog + myisam + myisammrg + archive +
      example + csv + heap + blackhole + federated + 0
  (yes, the sum is deliberately inaccurate)
  TODO remove the limit, use dynarrays
*/
#define MAX_HA 15

/*
  Use this instead of 0 as the initial value for the slot number of
  handlerton, so that we can distinguish uninitialized slot number
  from slot 0.
*/
#define HA_SLOT_UNDEF ((uint)-1)

/*
  Parameters for open() (in register form->filestat)
  HA_GET_INFO does an implicit HA_ABORT_IF_LOCKED
*/

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a handler::info() after open */
#define HA_READ_ONLY		16	/* File opened as readonly */
/* Try readonly if can't open with read and write */
#define HA_TRY_READ_ONLY	32
#define HA_WAIT_IF_LOCKED	64	/* Wait if locked on open */
#define HA_ABORT_IF_LOCKED	128	/* skip if locked on open.*/
#define HA_BLOCK_LOCK		256	/* unlock when reading some records */
#define HA_OPEN_TEMPORARY	512

	/* Some key definitions */
#define HA_KEY_NULL_LENGTH	1
#define HA_KEY_BLOB_LENGTH	2

#define HA_LEX_CREATE_TMP_TABLE	1
#define HA_LEX_CREATE_IF_NOT_EXISTS 2
#define HA_LEX_CREATE_TABLE_LIKE 4
#define HA_LEX_CREATE_INTERNAL_TMP_TABLE 8
#define HA_OPTION_NO_CHECKSUM	(1L << 17)
#define HA_OPTION_NO_DELAY_KEY_WRITE (1L << 18)
#define HA_MAX_REC_LENGTH	65535U

/* Table caching type */
#define HA_CACHE_TBL_NONTRANSACT 0
#define HA_CACHE_TBL_NOCACHE     1
#define HA_CACHE_TBL_ASKTRANSACT 2
#define HA_CACHE_TBL_TRANSACT    4

/**
  Options for the START TRANSACTION statement.

  Note that READ ONLY and READ WRITE are logically mutually exclusive.
  This is enforced by the parser and depended upon by trans_begin().

  We need two flags instead of one in order to differentiate between
  situation when no READ WRITE/ONLY clause were given and thus transaction
  is implicitly READ WRITE and the case when READ WRITE clause was used
  explicitly.
*/

// WITH CONSISTENT SNAPSHOT option
static const uint MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT = 1;
// READ ONLY option
static const uint MYSQL_START_TRANS_OPT_READ_ONLY          = 2;
// READ WRITE option
static const uint MYSQL_START_TRANS_OPT_READ_WRITE         = 4;
// HIGH PRIORITY option
static const uint MYSQL_START_TRANS_OPT_HIGH_PRIORITY      = 8;

enum legacy_db_type
{
  DB_TYPE_UNKNOWN=0,DB_TYPE_DIAB_ISAM=1,
  DB_TYPE_HASH,DB_TYPE_MISAM,DB_TYPE_PISAM,
  DB_TYPE_RMS_ISAM, DB_TYPE_HEAP, DB_TYPE_ISAM,
  DB_TYPE_MRG_ISAM, DB_TYPE_MYISAM, DB_TYPE_MRG_MYISAM,
  DB_TYPE_BERKELEY_DB, DB_TYPE_INNODB,
  DB_TYPE_GEMINI, DB_TYPE_NDBCLUSTER,
  DB_TYPE_EXAMPLE_DB, DB_TYPE_ARCHIVE_DB, DB_TYPE_CSV_DB,
  DB_TYPE_FEDERATED_DB,
  DB_TYPE_BLACKHOLE_DB,
  DB_TYPE_PARTITION_DB,
  DB_TYPE_BINLOG,
  DB_TYPE_SOLID,
  DB_TYPE_PBXT,
  DB_TYPE_TABLE_FUNCTION,
  DB_TYPE_MEMCACHE,
  DB_TYPE_FALCON,
  DB_TYPE_MARIA,
  /** Performance schema engine. */
  DB_TYPE_PERFORMANCE_SCHEMA,
  DB_TYPE_FIRST_DYNAMIC=42,
  DB_TYPE_DEFAULT=127 // Must be last
};

enum row_type { ROW_TYPE_NOT_USED=-1, ROW_TYPE_DEFAULT, ROW_TYPE_FIXED,
		ROW_TYPE_DYNAMIC, ROW_TYPE_COMPRESSED,
		ROW_TYPE_REDUNDANT, ROW_TYPE_COMPACT,
                /** Unused. Reserved for future versions. */
                ROW_TYPE_PAGE };

/* Specifies data storage format for individual columns */
enum column_format_type {
  COLUMN_FORMAT_TYPE_DEFAULT=   0, /* Not specified (use engine default) */
  COLUMN_FORMAT_TYPE_FIXED=     1, /* FIXED format */
  COLUMN_FORMAT_TYPE_DYNAMIC=   2  /* DYNAMIC format */
};

enum enum_binlog_func {
  BFN_RESET_LOGS=        1,
  BFN_RESET_SLAVE=       2,
  BFN_BINLOG_WAIT=       3,
  BFN_BINLOG_END=        4,
  BFN_BINLOG_PURGE_FILE= 5
};

enum enum_binlog_command {
  LOGCOM_CREATE_TABLE,
  LOGCOM_ALTER_TABLE,
  LOGCOM_RENAME_TABLE,
  LOGCOM_DROP_TABLE,
  LOGCOM_CREATE_DB,
  LOGCOM_ALTER_DB,
  LOGCOM_DROP_DB,
  LOGCOM_ACL_NOTIFY
};

/* struct to hold information about the table that should be created */

/* Bits in used_fields */
#define HA_CREATE_USED_AUTO             (1L << 0)
#define HA_CREATE_USED_RAID             (1L << 1) //RAID is no longer availble
#define HA_CREATE_USED_UNION            (1L << 2)
#define HA_CREATE_USED_INSERT_METHOD    (1L << 3)
#define HA_CREATE_USED_MIN_ROWS         (1L << 4)
#define HA_CREATE_USED_MAX_ROWS         (1L << 5)
#define HA_CREATE_USED_AVG_ROW_LENGTH   (1L << 6)
#define HA_CREATE_USED_PACK_KEYS        (1L << 7)
#define HA_CREATE_USED_CHARSET          (1L << 8)
#define HA_CREATE_USED_DEFAULT_CHARSET  (1L << 9)
#define HA_CREATE_USED_DATADIR          (1L << 10)
#define HA_CREATE_USED_INDEXDIR         (1L << 11)
#define HA_CREATE_USED_ENGINE           (1L << 12)
#define HA_CREATE_USED_CHECKSUM         (1L << 13)
#define HA_CREATE_USED_DELAY_KEY_WRITE  (1L << 14)
#define HA_CREATE_USED_ROW_FORMAT       (1L << 15)
#define HA_CREATE_USED_COMMENT          (1L << 16)
#define HA_CREATE_USED_PASSWORD         (1L << 17)
#define HA_CREATE_USED_CONNECTION       (1L << 18)
#define HA_CREATE_USED_KEY_BLOCK_SIZE   (1L << 19)
/** Unused. Reserved for future versions. */
#define HA_CREATE_USED_TRANSACTIONAL    (1L << 20)
/** Unused. Reserved for future versions. */
#define HA_CREATE_USED_PAGE_CHECKSUM    (1L << 21)
/** This is set whenever STATS_PERSISTENT=0|1|default has been
specified in CREATE/ALTER TABLE. See also HA_OPTION_STATS_PERSISTENT in
include/my_base.h. It is possible to distinguish whether
STATS_PERSISTENT=default has been specified or no STATS_PERSISTENT= is
given at all. */
#define HA_CREATE_USED_STATS_PERSISTENT (1L << 22)
/**
   This is set whenever STATS_AUTO_RECALC=0|1|default has been
   specified in CREATE/ALTER TABLE. See enum_stats_auto_recalc.
   It is possible to distinguish whether STATS_AUTO_RECALC=default
   has been specified or no STATS_AUTO_RECALC= is given at all.
*/
#define HA_CREATE_USED_STATS_AUTO_RECALC (1L << 23)
/**
   This is set whenever STATS_SAMPLE_PAGES=N|default has been
   specified in CREATE/ALTER TABLE. It is possible to distinguish whether
   STATS_SAMPLE_PAGES=default has been specified or no STATS_SAMPLE_PAGES= is
   given at all.
*/
#define HA_CREATE_USED_STATS_SAMPLE_PAGES (1L << 24)

/**
   This is set whenever a 'TABLESPACE=...' phrase is used on CREATE TABLE
*/
#define HA_CREATE_USED_TABLESPACE       (1L << 25)

/** COMPRESSION="zlib|lz4|none" used during table create. */
#define HA_CREATE_USED_COMPRESS         (1L << 26)

/*
  This is master database for most of system tables. However there
  can be other databases which can hold system tables. Respective
  storage engines define their own system database names.
*/
extern const char *mysqld_system_database;

/*
  Structure to hold list of database_name.table_name.
  This is used at both mysqld and storage engine layer.
*/
struct st_handler_tablename
{
  const char *db;
  const char *tablename;
};

#define MAXGTRIDSIZE 64
#define MAXBQUALSIZE 64

#define COMPATIBLE_DATA_YES 0
#define COMPATIBLE_DATA_NO  1

namespace AQP {
  class Join_plan;
};

/** ENCRYPTION="Y" used during table create. */
#define HA_CREATE_USED_ENCRYPT          (1L << 27)

/*
  These structures are used to pass information from a set of SQL commands
  on add/drop/change tablespace definitions to the proper hton.
*/
#define UNDEF_NODEGROUP 65535
enum ts_command_type
{
  TS_CMD_NOT_DEFINED = -1,
  CREATE_TABLESPACE = 0,
  ALTER_TABLESPACE = 1,
  CREATE_LOGFILE_GROUP = 2,
  ALTER_LOGFILE_GROUP = 3,
  DROP_TABLESPACE = 4,
  DROP_LOGFILE_GROUP = 5,
  CHANGE_FILE_TABLESPACE = 6,
  ALTER_ACCESS_MODE_TABLESPACE = 7
};

enum ts_alter_tablespace_type
{
  TS_ALTER_TABLESPACE_TYPE_NOT_DEFINED = -1,
  ALTER_TABLESPACE_ADD_FILE = 1,
  ALTER_TABLESPACE_DROP_FILE = 2
};

enum tablespace_access_mode
{
  TS_NOT_DEFINED= -1,
  TS_READ_ONLY = 0,
  TS_READ_WRITE = 1,
  TS_NOT_ACCESSIBLE = 2
};

struct handlerton;
class st_alter_tablespace : public Sql_alloc
{
  public:
  const char *tablespace_name;
  const char *logfile_group_name;
  enum ts_command_type ts_cmd_type;
  enum ts_alter_tablespace_type ts_alter_tablespace_type;
  const char *data_file_name;
  const char *undo_file_name;
  const char *redo_file_name;
  ulonglong extent_size;
  ulonglong undo_buffer_size;
  ulonglong redo_buffer_size;
  ulonglong initial_size;
  ulonglong autoextend_size;
  ulonglong max_size;
  ulonglong file_block_size;
  uint nodegroup_id;
  handlerton *storage_engine;
  bool wait_until_completed;
  const char *ts_comment;
  enum tablespace_access_mode ts_access_mode;
  bool is_tablespace_command()
  {
    return ts_cmd_type == CREATE_TABLESPACE      ||
            ts_cmd_type == ALTER_TABLESPACE       ||
            ts_cmd_type == DROP_TABLESPACE        ||
            ts_cmd_type == CHANGE_FILE_TABLESPACE ||
            ts_cmd_type == ALTER_ACCESS_MODE_TABLESPACE;
  }

  /** Default constructor */
  st_alter_tablespace()
  {
    tablespace_name= NULL;
    logfile_group_name= "DEFAULT_LG"; //Default log file group
    ts_cmd_type= TS_CMD_NOT_DEFINED;
    data_file_name= NULL;
    undo_file_name= NULL;
    redo_file_name= NULL;
    extent_size= 1024*1024;        // Default 1 MByte
    undo_buffer_size= 8*1024*1024; // Default 8 MByte
    redo_buffer_size= 8*1024*1024; // Default 8 MByte
    initial_size= 128*1024*1024;   // Default 128 MByte
    autoextend_size= 0;            // No autoextension as default
    max_size= 0;                   // Max size == initial size => no extension
    storage_engine= NULL;
    file_block_size= 0;            // 0=default or must be a valid Page Size
    nodegroup_id= UNDEF_NODEGROUP;
    wait_until_completed= TRUE;
    ts_comment= NULL;
    ts_access_mode= TS_NOT_DEFINED;
  }
};

/* The handler for a table type.  Will be included in the TABLE structure */

struct TABLE;

/*
  Make sure that the order of schema_tables and enum_schema_tables are the same.
*/
enum enum_schema_tables
{
  SCH_CHARSETS= 0,
  SCH_COLLATIONS,
  SCH_COLLATION_CHARACTER_SET_APPLICABILITY,
  SCH_COLUMNS,
  SCH_COLUMN_PRIVILEGES,
  SCH_ENGINES,
  SCH_EVENTS,
  SCH_FILES,
  SCH_GLOBAL_STATUS,
  SCH_GLOBAL_VARIABLES,
  SCH_KEY_COLUMN_USAGE,
  SCH_OPEN_TABLES,
  SCH_OPTIMIZER_TRACE,
  SCH_PARAMETERS,
  SCH_PARTITIONS,
  SCH_PLUGINS,
  SCH_PROCESSLIST,
  SCH_PROFILES,
  SCH_REFERENTIAL_CONSTRAINTS,
  SCH_PROCEDURES,
  SCH_SCHEMATA,
  SCH_SCHEMA_PRIVILEGES,
  SCH_SESSION_STATUS,
  SCH_SESSION_VARIABLES,
  SCH_STATISTICS,
  SCH_STATUS,
  SCH_TABLES,
  SCH_TABLESPACES,
  SCH_TABLE_CONSTRAINTS,
  SCH_TABLE_NAMES,
  SCH_TABLE_PRIVILEGES,
  SCH_TRIGGERS,
  SCH_USER_PRIVILEGES,
  SCH_VARIABLES,
  SCH_VIEWS
};

struct TABLE_SHARE;
struct st_foreign_key_info;
typedef struct st_foreign_key_info FOREIGN_KEY_INFO;
typedef bool (stat_print_fn)(THD *thd, const char *type, size_t type_len,
                             const char *file, size_t file_len,
                             const char *status, size_t status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };
enum ha_notification_type { HA_NOTIFY_PRE_EVENT, HA_NOTIFY_POST_EVENT };

extern st_plugin_int *hton2plugin[MAX_HA];

class handler;
/*
  handlerton is a singleton structure - one instance per storage engine -
  to provide access to storage engine functionality that works on the
  "global" level (unlike handler class that works on a per-table basis)

  usually handlerton instance is defined statically in ha_xxx.cc as

  static handlerton { ... } xxx_hton;

  savepoint_*, prepare, recover, and *_by_xid pointers can be 0.
*/
struct handlerton
{
  /*
    Historical marker for if the engine is available of not
  */
  SHOW_COMP_OPTION state;

  /*
    Historical number used for frm file to determine the correct storage engine.
    This is going away and new engines will just use "name" for this.
  */
  enum legacy_db_type db_type;
  /*
    each storage engine has it's own memory area (actually a pointer)
    in the thd, for storing per-connection information.
    It is accessed as

      thd->ha_data[xxx_hton.slot]

   slot number is initialized by MySQL after xxx_init() is called.
   */
   uint slot;
   /*
     to store per-savepoint data storage engine is provided with an area
     of a requested size (0 is ok here).
     savepoint_offset must be initialized statically to the size of
     the needed memory to store per-savepoint information.
     After xxx_init it is changed to be an offset to savepoint storage
     area and need not be used by storage engine.
     see binlog_hton and binlog_savepoint_set/rollback for an example.
   */
   uint savepoint_offset;
   /*
     handlerton methods:

     close_connection is only called if
     thd->ha_data[xxx_hton.slot] is non-zero, so even if you don't need
     this storage area - set it to something, so that MySQL would know
     this storage engine was accessed in this connection
   */
   int  (*close_connection)(handlerton *hton, THD *thd);
   /* Terminate connection/statement notification. */
   void (*kill_connection)(handlerton *hton, THD *thd);
   /*
     sv points to an uninitialized storage area of requested size
     (see savepoint_offset description)
   */
   int  (*savepoint_set)(handlerton *hton, THD *thd, void *sv);
   /*
     sv points to a storage area, that was earlier passed
     to the savepoint_set call
   */
   int  (*savepoint_rollback)(handlerton *hton, THD *thd, void *sv);
   /**
     Check if storage engine allows to release metadata locks which were
     acquired after the savepoint if rollback to savepoint is done.
     @return true  - If it is safe to release MDL locks.
             false - If it is not.
   */
   bool (*savepoint_rollback_can_release_mdl)(handlerton *hton, THD *thd);
   int  (*savepoint_release)(handlerton *hton, THD *thd, void *sv);
   /*
     'all' is true if it's a real commit, that makes persistent changes
     'all' is false if it's not in fact a commit but an end of the
     statement that is part of the transaction.
     NOTE 'all' is also false in auto-commit mode where 'end of statement'
     and 'real commit' mean the same event.
   */
   int  (*commit)(handlerton *hton, THD *thd, bool all);
   int  (*rollback)(handlerton *hton, THD *thd, bool all);
   int  (*prepare)(handlerton *hton, THD *thd, bool all);
   int  (*recover)(handlerton *hton, XID *xid_list, uint len);
   int  (*commit_by_xid)(handlerton *hton, XID *xid);
   int  (*rollback_by_xid)(handlerton *hton, XID *xid);
   handler *(*create)(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root);
   void (*drop_database)(handlerton *hton, char* path);
   int (*panic)(handlerton *hton, enum ha_panic_function flag);
   int (*start_consistent_snapshot)(handlerton *hton, THD *thd);
   /**
     Flush the log(s) of storage engine(s).

     @param hton Handlerton of storage engine.
     @param binlog_group_flush true if we got invoked by binlog group
     commit during flush stage, false in other cases.
     @retval false Succeed
     @retval true Error
   */
   bool (*flush_logs)(handlerton *hton, bool binlog_group_flush);
   bool (*show_status)(handlerton *hton, THD *thd, stat_print_fn *print, enum ha_stat_type stat);
   /*
     The flag values are defined in sql_partition.h.
     If this function is set, then it implies that the handler supports
     partitioned tables.
     If this function exists, then handler::get_partition_handler must also be
     implemented.
   */
   uint (*partition_flags)();


  /**
    Get the tablespace name from the SE for the given schema and table.

    @param       thd              Thread context.
    @param       db_name          Name of the relevant schema.
    @param       table_name       Name of the relevant table.
    @param [out] tablespace_name  Name of the tablespace containing the table.

    @return Operation status.
      @retval == 0  Success.
      @retval != 0  Error (handler error code returned).
   */

  int (*get_tablespace)(THD* thd, LEX_CSTRING db_name, LEX_CSTRING table_name,
                        LEX_CSTRING *tablespace_name);

   int (*alter_tablespace)(handlerton *hton, THD *thd, st_alter_tablespace *ts_info);
   int (*fill_is_table)(handlerton *hton, THD *thd, TABLE_LIST *tables, 
                        class Item *cond, 
                        enum enum_schema_tables);
   uint32 flags;                                /* global handler flags */
   /*
      Those handlerton functions below are properly initialized at handler
      init.
   */
   int (*binlog_func)(handlerton *hton, THD *thd, enum_binlog_func fn, void *arg);
   void (*binlog_log_query)(handlerton *hton, THD *thd, 
                            enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name);
   int (*release_temporary_latches)(handlerton *hton, THD *thd);

   int (*discover)(handlerton *hton, THD* thd, const char *db, 
                   const char *name,
                   uchar **frmblob, 
                   size_t *frmlen);
   int (*find_files)(handlerton *hton, THD *thd,
                     const char *db,
                     const char *path,
                     const char *wild, bool dir, List<LEX_STRING> *files);
   int (*table_exists_in_engine)(handlerton *hton, THD* thd, const char *db,
                                 const char *name);
   int (*make_pushed_join)(handlerton *hton, THD* thd, 
                           const AQP::Join_plan* plan);

  /**
    List of all system tables specific to the SE.
    Array element would look like below,
     { "<database_name>", "<system table name>" },
    The last element MUST be,
     { (const char*)NULL, (const char*)NULL }

    @see ha_example_system_tables in ha_example.cc

    This interface is optional, so every SE need not implement it.
  */
  const char* (*system_database)();

  /**
    Check if the given db.tablename is a system table for this SE.

    @param db                         Database name to check.
    @param table_name                 table name to check.
    @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                      layer system table.

    @see example_is_supported_system_table in ha_example.cc

    is_sql_layer_system_table is supplied to make more efficient
    checks possible for SEs that support all SQL layer tables.

    This interface is optional, so every SE need not implement it.
  */
  bool (*is_supported_system_table)(const char *db,
                                    const char *table_name,
                                    bool is_sql_layer_system_table);

  /**
    Retrieve cost constants to be used for this storage engine.

    A storage engine that wants to provide its own cost constants to
    be used in the optimizer cost model, should implement this function.
    The server will call this function to get a cost constant object
    that will be used for tables stored in this storage engine instead
    of using the default cost constants.

    Life cycle for the cost constant object: The storage engine must
    allocate the cost constant object on the heap. After the function
    returns, the server takes over the ownership of this object.
    The server will eventually delete the object by calling delete.

    @note In the initial version the storage_category parameter will
    not be used. The only valid value this will have is DEFAULT_STORAGE_CLASS
    (see declartion in opt_costconstants.h).

    @param storage_category the storage type that the cost constants will
                            be used for

    @return a pointer to the cost constant object, if NULL is returned
            the default cost constants will be used
  */

  SE_cost_constants *(*get_cost_constants)(uint storage_category);

  /**
    @param[in,out]  thd          pointer to THD
    @param[in]      new_trx_arg  pointer to replacement transaction
    @param[out]     ptr_trx_arg  double pointer to being replaced transaction

    Associated with THD engine's native transaction is replaced
    with @c new_trx_arg. The old value is returned through a buffer if non-null
    pointer is provided with @c ptr_trx_arg.
    The method is adapted by XA start and XA prepare handlers to
    handle XA transaction that is logged as two parts by slave applier.

    This interface concerns engines that are aware of XA transaction.
  */
  void (*replace_native_transaction_in_thd)(THD *thd, void *new_trx_arg,
                                            void **ptr_trx_arg);


  /**
    Notify/get permission from storage engine before acquisition or after
    release of exclusive metadata lock on object represented by key.

    @param thd                Thread context.
    @param mdl_key            MDL key identifying object on which exclusive
                              lock is to be acquired/was released.
    @param notification_type  Indicates whether this is pre-acquire or
                              post-release notification.
    @param victimized        'true' if locking failed as we were selected
                              as a victim in order to avoid possible deadlocks.

    @note Notification is done only for objects from TABLESPACE, SCHEMA,
          TABLE, FUNCTION, PROCEDURE, TRIGGER and EVENT namespaces.

    @note Problems during notification are to be reported as warnings, MDL
          subsystem will report generic error if pre-acquire notification
          fails/SE refuses lock acquisition.
    @note Return value is ignored/error is not reported in case of
          post-release notification.

    @note In some cases post-release notification might happen even if
          there were no prior pre-acquire notification. For example,
          when SE was loaded after exclusive lock acquisition, or when
          we need notify SEs which permitted lock acquisition that it
          didn't happen because one of SEs didn't allow it (in such case
          we will do post-release notification for all SEs for simplicity).

    @return False - if notification was successful/lock can be acquired,
            True - if it has failed/lock should not be acquired.
  */
  bool (*notify_exclusive_mdl)(THD *thd, const MDL_key *mdl_key,
                               ha_notification_type notification_type,
                               bool *victimized);

  /**
    Notify/get permission from storage engine before or after execution of
    ALTER TABLE operation on the table identified by the MDL key.

    @param thd                Thread context.
    @param mdl_key            MDL key identifying table which is going to be
                              or was ALTERed.
    @param notification_type  Indicates whether this is pre-ALTER TABLE or
                              post-ALTER TABLE notification.

    @note This hook is necessary because for ALTER TABLE upgrade to X
          metadata lock happens fairly late during the execution process,
          so it can be expensive to abort ALTER TABLE operation at this
          stage by returning failure from notify_exclusive_mdl() hook.

    @note This hook follows the same error reporting convention as
          @see notify_exclusive_mdl().

    @note Similarly to notify_exclusive_mdl() in some cases post-ALTER
          notification might happen even if there were no prior pre-ALTER
          notification.

    @note Post-ALTER notification can happen before post-release notification
          for exclusive metadata lock acquired by this ALTER TABLE.

    @return False - if notification was successful/ALTER TABLE can proceed.
            True - if it has failed/ALTER TABLE should be aborted.
  */
  bool (*notify_alter_table)(THD *thd, const MDL_key *mdl_key,
                             ha_notification_type notification_type);


  /**
    @brief
    Initiate master key rotation

    @returns false on success,
             true on failure
  */
  bool (*rotate_encryption_master_key)(void);

   uint32 license; /* Flag for Engine License */
   void *data; /* Location for engines to keep personal structures */
};


/* Possible flags of a handlerton (there can be 32 of them) */
#define HTON_NO_FLAGS                 0
#define HTON_CLOSE_CURSORS_AT_COMMIT (1 << 0)
#define HTON_ALTER_NOT_SUPPORTED     (1 << 1) //Engine does not support alter
#define HTON_CAN_RECREATE            (1 << 2) //Delete all is used fro truncate
#define HTON_HIDDEN                  (1 << 3) //Engine does not appear in lists
#define HTON_FLUSH_AFTER_RENAME      (1 << 4)
#define HTON_NOT_USER_SELECTABLE     (1 << 5)
#define HTON_TEMPORARY_NOT_SUPPORTED (1 << 6) //Having temporary tables not supported
#define HTON_SUPPORT_LOG_TABLES      (1 << 7) //Engine supports log tables
#define HTON_NO_PARTITION            (1 << 8) //You can not partition these tables

/*
  This flag should be set when deciding that the engine does not allow row based
  binary logging (RBL) optimizations.

  Currently, setting this flag, means that table's read/write_set will be left 
  untouched when logging changes to tables in this engine. In practice this 
  means that the server will not mess around with table->write_set and/or 
  table->read_set when using RBL and deciding whether to log full or minimal rows.

  It's valuable for instance for virtual tables, eg: Performance Schema which have
  no meaning for replication.
*/
#define HTON_NO_BINLOG_ROW_OPT       (1 << 9)

/**
  Engine supports extended keys. The flag allows to
  use 'extended key' feature if the engine is able to
  do it (has primary key values in the secondary key).
  Note that handler flag HA_PRIMARY_KEY_IN_READ_INDEX is
  actually partial case of HTON_SUPPORTS_EXTENDED_KEYS.
*/

#define HTON_SUPPORTS_EXTENDED_KEYS  (1 << 10)

// Engine support foreign key constraint.

#define HTON_SUPPORTS_FOREIGN_KEYS   (1 << 11)

enum enum_tx_isolation { ISO_READ_UNCOMMITTED, ISO_READ_COMMITTED,
			 ISO_REPEATABLE_READ, ISO_SERIALIZABLE};

typedef struct {
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong delete_length;
  ha_rows records;
  ulong mean_rec_length;
  ulong create_time;
  ulong check_time;
  ulong update_time;
  ulonglong check_sum;
} PARTITION_STATS;

#define UNDEF_NODEGROUP 65535
class Item;
struct st_table_log_memory_entry;

enum enum_ha_unused { HA_CHOICE_UNDEF, HA_CHOICE_NO, HA_CHOICE_YES };

enum enum_stats_auto_recalc { HA_STATS_AUTO_RECALC_DEFAULT= 0,
                              HA_STATS_AUTO_RECALC_ON,
                              HA_STATS_AUTO_RECALC_OFF };

typedef struct st_ha_create_information
{
  const CHARSET_INFO *table_charset, *default_table_charset;
  LEX_STRING connect_string;
  const char *password, *tablespace;
  LEX_STRING comment;

  /**
  Algorithm (and possible options) to be used for InnoDB's transparent
  page compression. If this attribute is set then it is hint to the
  storage engine to try and compress the data using the specified algorithm
  where possible. Note: this value is interpreted by the storage engine only.
  and ignored by the Server layer. */

  LEX_STRING compress;

  /**
  This attibute is used for InnoDB's transparent page encryption.
  If this attribute is set then it is hint to the storage engine to encrypt
  the data. Note: this value is interpreted by the storage engine only.
  and ignored by the Server layer. */

  LEX_STRING encrypt_type;

  const char *data_file_name, *index_file_name;
  const char *alias;
  ulonglong max_rows,min_rows;
  ulonglong auto_increment_value;
  ulong table_options;
  ulong avg_row_length;
  ulong used_fields;
  ulong key_block_size;
  uint stats_sample_pages;		/* number of pages to sample during
					stats estimation, if used, otherwise 0. */
  enum_stats_auto_recalc stats_auto_recalc;
  SQL_I_List<TABLE_LIST> merge_list;
  handlerton *db_type;
  /**
    Row type of the table definition.

    Defaults to ROW_TYPE_DEFAULT for all non-ALTER statements.
    For ALTER TABLE defaults to ROW_TYPE_NOT_USED (means "keep the current").

    Can be changed either explicitly by the parser.
    If nothing specified inherits the value of the original table (if present).
  */
  enum row_type row_type;
  uint null_bits;                       /* NULL bits at start of record */
  uint options;				/* OR of HA_CREATE_ options */
  uint merge_insert_method;
  uint extra_size;                      /* length of extra data segment */
  bool varchar;                         /* 1 if table has a VARCHAR */
  enum ha_storage_media storage_media;  /* DEFAULT, DISK or MEMORY */
} HA_CREATE_INFO;


/**
  Structure describing changes to an index to be caused by ALTER TABLE.
*/

struct KEY_PAIR
{
  /**
    Pointer to KEY object describing old version of index in
    TABLE::key_info array for TABLE instance representing old
    version of table.
  */
  KEY *old_key;
  /**
    Pointer to KEY object describing new version of index in
    Alter_inplace_info::key_info_buffer array.
  */
  KEY *new_key;
};


/**
  In-place alter handler context.

  This is a superclass intended to be subclassed by individual handlers
  in order to store handler unique context between in-place alter API calls.

  The handler is responsible for creating the object. This can be done
  as early as during check_if_supported_inplace_alter().

  The SQL layer is responsible for destroying the object.
  The class extends Sql_alloc so the memory will be mem root allocated.

  @see Alter_inplace_info
*/

class inplace_alter_handler_ctx : public Sql_alloc
{
public:
  inplace_alter_handler_ctx() {}

  virtual ~inplace_alter_handler_ctx() {}
};


/**
  Class describing changes to be done by ALTER TABLE.
  Instance of this class is passed to storage engine in order
  to determine if this ALTER TABLE can be done using in-place
  algorithm. It is also used for executing the ALTER TABLE
  using in-place algorithm.
*/

class Alter_inplace_info
{
public:
  /**
     Bits to show in detail what operations the storage engine is
     to execute.

     All these operations are supported as in-place operations by the
     SQL layer. This means that operations that by their nature must
     be performed by copying the table to a temporary table, will not
     have their own flags here (e.g. ALTER TABLE FORCE, ALTER TABLE
     ENGINE).

     We generally try to specify handler flags only if there are real
     changes. But in cases when it is cumbersome to determine if some
     attribute has really changed we might choose to set flag
     pessimistically, for example, relying on parser output only.
  */
  typedef ulonglong HA_ALTER_FLAGS;

  // Add non-unique, non-primary index
  static const HA_ALTER_FLAGS ADD_INDEX                  = 1ULL << 0;

  // Drop non-unique, non-primary index
  static const HA_ALTER_FLAGS DROP_INDEX                 = 1ULL << 1;

  // Add unique, non-primary index
  static const HA_ALTER_FLAGS ADD_UNIQUE_INDEX           = 1ULL << 2;

  // Drop unique, non-primary index
  static const HA_ALTER_FLAGS DROP_UNIQUE_INDEX          = 1ULL << 3;

  // Add primary index
  static const HA_ALTER_FLAGS ADD_PK_INDEX               = 1ULL << 4;

  // Drop primary index
  static const HA_ALTER_FLAGS DROP_PK_INDEX              = 1ULL << 5;

  // Add column

  // Virtual generated column
  static const HA_ALTER_FLAGS ADD_VIRTUAL_COLUMN         = 1ULL << 6;
  // Stored base (non-generated) column
  static const HA_ALTER_FLAGS ADD_STORED_BASE_COLUMN     = 1ULL << 7;
  // Stored generated column
  static const HA_ALTER_FLAGS ADD_STORED_GENERATED_COLUMN= 1ULL << 8;
  // Add generic column (convience constant).
  static const HA_ALTER_FLAGS ADD_COLUMN= ADD_VIRTUAL_COLUMN |
                                          ADD_STORED_BASE_COLUMN |
                                          ADD_STORED_GENERATED_COLUMN;

  // Drop column
  static const HA_ALTER_FLAGS DROP_VIRTUAL_COLUMN        = 1ULL << 9;
  static const HA_ALTER_FLAGS DROP_STORED_COLUMN         = 1ULL << 10;
  static const HA_ALTER_FLAGS DROP_COLUMN= DROP_VIRTUAL_COLUMN |
                                           DROP_STORED_COLUMN;

  // Rename column
  static const HA_ALTER_FLAGS ALTER_COLUMN_NAME          = 1ULL << 11;

  // Change column datatype
  static const HA_ALTER_FLAGS ALTER_VIRTUAL_COLUMN_TYPE         = 1ULL << 12;
  static const HA_ALTER_FLAGS ALTER_STORED_COLUMN_TYPE          = 1ULL << 13;

  /**
    Change column datatype in such way that new type has compatible
    packed representation with old type, so it is theoretically
    possible to perform change by only updating data dictionary
    without changing table rows.
  */
  static const HA_ALTER_FLAGS ALTER_COLUMN_EQUAL_PACK_LENGTH = 1ULL << 14;

  /// A virtual column has changed its position
  static const HA_ALTER_FLAGS ALTER_VIRTUAL_COLUMN_ORDER     = 1ULL << 15;

  /// A stored column has changed its position (disregarding virtual columns)
  static const HA_ALTER_FLAGS ALTER_STORED_COLUMN_ORDER      = 1ULL << 16;

  // Change column from NOT NULL to NULL
  static const HA_ALTER_FLAGS ALTER_COLUMN_NULLABLE      = 1ULL << 17;

  // Change column from NULL to NOT NULL
  static const HA_ALTER_FLAGS ALTER_COLUMN_NOT_NULLABLE  = 1ULL << 18;

  // Set or remove default column value
  static const HA_ALTER_FLAGS ALTER_COLUMN_DEFAULT       = 1ULL << 19;

  // Change column generation expression
  static const HA_ALTER_FLAGS ALTER_VIRTUAL_GCOL_EXPR    = 1ULL << 20;
  static const HA_ALTER_FLAGS ALTER_STORED_GCOL_EXPR     = 1ULL << 21;

  // Add foreign key
  static const HA_ALTER_FLAGS ADD_FOREIGN_KEY            = 1ULL << 22;

  // Drop foreign key
  static const HA_ALTER_FLAGS DROP_FOREIGN_KEY           = 1ULL << 23;

  // table_options changed, see HA_CREATE_INFO::used_fields for details.
  static const HA_ALTER_FLAGS CHANGE_CREATE_OPTION       = 1ULL << 24;

  // Table is renamed
  static const HA_ALTER_FLAGS ALTER_RENAME               = 1ULL << 25;

  // Change the storage type of column
  static const HA_ALTER_FLAGS ALTER_COLUMN_STORAGE_TYPE = 1ULL << 26;

  // Change the column format of column
  static const HA_ALTER_FLAGS ALTER_COLUMN_COLUMN_FORMAT = 1ULL << 27;

  // Add partition
  static const HA_ALTER_FLAGS ADD_PARTITION              = 1ULL << 28;

  // Drop partition
  static const HA_ALTER_FLAGS DROP_PARTITION             = 1ULL << 29;

  // Changing partition options
  static const HA_ALTER_FLAGS ALTER_PARTITION            = 1ULL << 30;

  // Coalesce partition
  static const HA_ALTER_FLAGS COALESCE_PARTITION         = 1ULL << 31;

  // Reorganize partition ... into
  static const HA_ALTER_FLAGS REORGANIZE_PARTITION       = 1ULL << 32;

  // Reorganize partition
  static const HA_ALTER_FLAGS ALTER_TABLE_REORG          = 1ULL << 33;

  // Remove partitioning
  static const HA_ALTER_FLAGS ALTER_REMOVE_PARTITIONING  = 1ULL << 34;

  // Partition operation with ALL keyword
  static const HA_ALTER_FLAGS ALTER_ALL_PARTITION        = 1ULL << 35;

  /**
    Rename index. Note that we set this flag only if there are no other
    changes to the index being renamed. Also for simplicity we don't
    detect renaming of indexes which is done by dropping index and then
    re-creating index with identical definition under different name.
  */
  static const HA_ALTER_FLAGS RENAME_INDEX               = 1ULL << 36;

  /**
    Recreate the table for ALTER TABLE FORCE, ALTER TABLE ENGINE
    and OPTIMIZE TABLE operations.
  */
  static const HA_ALTER_FLAGS RECREATE_TABLE             = 1ULL << 37;

  // Add spatial index
  static const HA_ALTER_FLAGS ADD_SPATIAL_INDEX          = 1ULL << 38;

  // Alter index comment
  static const HA_ALTER_FLAGS ALTER_INDEX_COMMENT        = 1ULL << 39;

  // Upgrade partitioning
  static const HA_ALTER_FLAGS ALTER_UPGRADE_PARTITIONING  = 1ULL << 40;

  // New/changed virtual generated column require validation
  static const HA_ALTER_FLAGS VALIDATE_VIRTUAL_COLUMN    = 1ULL << 41;

  /**
    Create options (like MAX_ROWS) for the new version of table.

    @note The referenced instance of HA_CREATE_INFO object was already
          used to create new .FRM file for table being altered. So it
          has been processed by mysql_prepare_create_table() already.
          For example, this means that it has HA_OPTION_PACK_RECORD
          flag in HA_CREATE_INFO::table_options member correctly set.
  */
  HA_CREATE_INFO *create_info;

  /**
    Alter options, fields and keys for the new version of table.

    @note The referenced instance of Alter_info object was already
          used to create new .FRM file for table being altered. So it
          has been processed by mysql_prepare_create_table() already.
          In particular, this means that in Create_field objects for
          fields which were present in some form in the old version
          of table, Create_field::field member points to corresponding
          Field instance for old version of table.
  */
  Alter_info *alter_info;

  /**
    Array of KEYs for new version of table - including KEYs to be added.

    @note Currently this array is produced as result of
          mysql_prepare_create_table() call.
          This means that it follows different convention for
          KEY_PART_INFO::fieldnr values than objects in TABLE::key_info
          array.

    @todo This is mainly due to the fact that we need to keep compatibility
          with removed handler::add_index() call. We plan to switch to
          TABLE::key_info numbering later.

    KEYs are sorted - see sort_keys().
  */
  KEY  *key_info_buffer;

  /** Size of key_info_buffer array. */
  uint key_count;

  /** Size of index_drop_buffer array. */
  uint index_drop_count;

  /**
     Array of pointers to KEYs to be dropped belonging to the TABLE instance
     for the old version of the table.
  */
  KEY  **index_drop_buffer;

  /** Size of index_add_buffer array. */
  uint index_add_count;

  /**
     Array of indexes into key_info_buffer for KEYs to be added,
     sorted in increasing order.
  */
  uint *index_add_buffer;

  /** Size of index_rename_buffer array. */
  uint index_rename_count;

  /**
    Array of KEY_PAIR objects describing indexes being renamed.
    For each index renamed it contains object with KEY_PAIR::old_key
    pointing to KEY object belonging to the TABLE instance for old
    version of table representing old version of index and with
    KEY_PAIR::new_key pointing to KEY object for new version of
    index in key_info_buffer member.
  */
  KEY_PAIR  *index_rename_buffer;

  /**
     Context information to allow handlers to keep context between in-place
     alter API calls.

     @see inplace_alter_handler_ctx for information about object lifecycle.
  */
  inplace_alter_handler_ctx *handler_ctx;

  /**
    If the table uses several handlers, like ha_partition uses one handler
    per partition, this contains a Null terminated array of ctx pointers
    that should all be committed together.
    Or NULL if only handler_ctx should be committed.
    Set to NULL if the low level handler::commit_inplace_alter_table uses it,
    to signal to the main handler that everything was committed as atomically.

    @see inplace_alter_handler_ctx for information about object lifecycle.
  */
  inplace_alter_handler_ctx **group_commit_ctx;

  /**
     Flags describing in detail which operations the storage engine is to execute.
  */
  HA_ALTER_FLAGS handler_flags;

  /**
     Partition_info taking into account the partition changes to be performed.
     Contains all partitions which are present in the old version of the table
     with partitions to be dropped or changed marked as such + all partitions
     to be added in the new version of table marked as such.
  */
  partition_info *modified_part_info;

  /** true for online operation (LOCK=NONE) */
  bool online;

  /**
     Can be set by handler to describe why a given operation cannot be done
     in-place (HA_ALTER_INPLACE_NOT_SUPPORTED) or why it cannot be done
     online (HA_ALTER_INPLACE_NO_LOCK or HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE)
     If set, it will be used with ER_ALTER_OPERATION_NOT_SUPPORTED_REASON if
     results from handler::check_if_supported_inplace_alter() doesn't match
     requirements set by user. If not set, the more generic
     ER_ALTER_OPERATION_NOT_SUPPORTED will be used.

     Please set to a properly localized string, for example using
     my_get_err_msg(), so that the error message as a whole is localized.
  */
  const char *unsupported_reason;

  Alter_inplace_info(HA_CREATE_INFO *create_info_arg,
                     Alter_info *alter_info_arg,
                     KEY *key_info_arg, uint key_count_arg,
                     partition_info *modified_part_info_arg)
    : create_info(create_info_arg),
    alter_info(alter_info_arg),
    key_info_buffer(key_info_arg),
    key_count(key_count_arg),
    index_drop_count(0),
    index_drop_buffer(NULL),
    index_add_count(0),
    index_add_buffer(NULL),
    index_rename_count(0),
    index_rename_buffer(NULL),
    handler_ctx(NULL),
    group_commit_ctx(NULL),
    handler_flags(0),
    modified_part_info(modified_part_info_arg),
    online(false),
    unsupported_reason(NULL)
  {}

  ~Alter_inplace_info()
  {
    delete handler_ctx;
  }

  /**
    Used after check_if_supported_inplace_alter() to report
    error if the result does not match the LOCK/ALGORITHM
    requirements set by the user.

    @param not_supported  Part of statement that was not supported.
    @param try_instead    Suggestion as to what the user should
                          replace not_supported with.
  */
  void report_unsupported_error(const char *not_supported,
                                const char *try_instead);

  /** Add old and new version of key to array of indexes to be renamed. */
  void add_renamed_key(KEY *old_key, KEY *new_key)
  {
    KEY_PAIR *key_pair= index_rename_buffer + index_rename_count++;
    key_pair->old_key= old_key;
    key_pair->new_key= new_key;
    DBUG_PRINT("info", ("index renamed: '%s' to '%s'",
                        old_key->name, new_key->name));
  }

  /**
    Add old and new version of modified key to arrays of indexes to
    be dropped and added (correspondingly).
  */
  void add_modified_key(KEY *old_key, KEY *new_key)
  {
    index_drop_buffer[index_drop_count++]= old_key;
    index_add_buffer[index_add_count++]= (uint) (new_key - key_info_buffer);
    DBUG_PRINT("info", ("index changed: '%s'", old_key->name));
  }

  /** Drop key to array of indexes to be dropped. */
  void add_dropped_key(KEY *old_key)
  {
    index_drop_buffer[index_drop_count++]= old_key;
    DBUG_PRINT("info", ("index dropped: '%s'", old_key->name));
  }

  /** Add key to array of indexes to be added. */
  void add_added_key(KEY *new_key)
  {
    index_add_buffer[index_add_count++]= (uint) (new_key - key_info_buffer);
    DBUG_PRINT("info", ("index added: '%s'", new_key->name));
  }
};


typedef struct st_key_create_information
{
  enum ha_key_alg algorithm;
  ulong block_size;
  LEX_STRING parser_name;
  LEX_STRING comment;
  /**
    A flag to determine if we will check for duplicate indexes.
    This typically means that the key information was specified
    directly by the user (set by the parser) or a column
    associated with it was dropped.
  */
  bool check_for_duplicate_indexes;
} KEY_CREATE_INFO;


/*
  Class for maintaining hooks used inside operations on tables such
  as: create table functions, delete table functions, and alter table
  functions.

  Class is using the Template Method pattern to separate the public
  usage interface from the private inheritance interface.  This
  imposes no overhead, since the public non-virtual function is small
  enough to be inlined.

  The hooks are usually used for functions that does several things,
  e.g., create_table_from_items(), which both create a table and lock
  it.
 */
class TABLEOP_HOOKS
{
public:
  TABLEOP_HOOKS() {}
  virtual ~TABLEOP_HOOKS() {}

  inline void prelock(TABLE **tables, uint count)
  {
    do_prelock(tables, count);
  }

  inline int postlock(TABLE **tables, uint count)
  {
    return do_postlock(tables, count);
  }
private:
  /* Function primitive that is called prior to locking tables */
  virtual void do_prelock(TABLE **tables, uint count)
  {
    /* Default is to do nothing */
  }

  /**
     Primitive called after tables are locked.

     If an error is returned, the tables will be unlocked and error
     handling start.

     @return Error code or zero.
   */
  virtual int do_postlock(TABLE **tables, uint count)
  {
    return 0;                           /* Default is to do nothing */
  }
};

typedef struct st_savepoint SAVEPOINT;
extern ulong savepoint_alloc_size;
extern KEY_CREATE_INFO default_key_create_info;

typedef struct st_ha_check_opt
{
  st_ha_check_opt() {}                        /* Remove gcc warning */
  uint flags;       /* isam layer flags (e.g. for myisamchk) */
  uint sql_flags;   /* sql layer flags - for something myisamchk cannot do */
  KEY_CACHE *key_cache;	/* new key cache when changing key cache */
  void init();
} HA_CHECK_OPT;



/*
  This is a buffer area that the handler can use to store rows.
  'end_of_used_area' should be kept updated after calls to
  read-functions so that other parts of the code can use the
  remaining area (until next read calls is issued).
*/

typedef struct st_handler_buffer
{
  uchar *buffer;         /* Buffer one can start using */
  uchar *buffer_end;     /* End of buffer */
  uchar *end_of_used_area;     /* End of area that was used by handler */
} HANDLER_BUFFER;

typedef struct system_status_var SSV;


typedef void *range_seq_t;

typedef struct st_range_seq_if
{
  /*
    Initialize the traversal of range sequence
    
    SYNOPSIS
      init()
        init_params  The seq_init_param parameter 
        n_ranges     The number of ranges obtained 
        flags        A combination of HA_MRR_SINGLE_POINT, HA_MRR_FIXED_KEY

    RETURN
      An opaque value to be used as RANGE_SEQ_IF::next() parameter
  */
  range_seq_t (*init)(void *init_params, uint n_ranges, uint flags);


  /*
    Get the next range in the range sequence

    SYNOPSIS
      next()
        seq    The value returned by RANGE_SEQ_IF::init()
        range  OUT Information about the next range
    
    RETURN
      0 - Ok, the range structure filled with info about the next range
      1 - No more ranges
  */
  uint (*next) (range_seq_t seq, KEY_MULTI_RANGE *range);

  /*
    Check whether range_info orders to skip the next record

    SYNOPSIS
      skip_record()
        seq         The value returned by RANGE_SEQ_IF::init()
        range_info  Information about the next range 
                    (Ignored if MRR_NO_ASSOCIATION is set)
        rowid       Rowid of the record to be checked (ignored if set to 0)
    
    RETURN
      1 - Record with this range_info and/or this rowid shall be filtered
          out from the stream of records returned by multi_range_read_next()
      0 - The record shall be left in the stream
  */ 
  bool (*skip_record) (range_seq_t seq, char *range_info, uchar *rowid);

  /*
    Check if the record combination matches the index condition
    SYNOPSIS
      skip_index_tuple()
        seq         The value returned by RANGE_SEQ_IF::init()
        range_info  Information about the next range 
    
    RETURN
      0 - The record combination satisfies the index condition
      1 - Otherwise
  */ 
  bool (*skip_index_tuple) (range_seq_t seq, char *range_info);
} RANGE_SEQ_IF;

uint16 &mrr_persistent_flag_storage(range_seq_t seq, uint idx);
char* &mrr_get_ptr_by_idx(range_seq_t seq, uint idx);

/**
  Used to store optimizer cost estimates.

  The class consists of PODs only: default operator=, copy constructor
  and destructor are used.
 */
class Cost_estimate
{ 
private:
  double io_cost;                               ///< cost of I/O operations
  double cpu_cost;                              ///< cost of CPU operations
  double import_cost;                           ///< cost of remote operations
  double mem_cost;                              ///< memory used (bytes)
  
public:
  Cost_estimate() :
    io_cost(0),
    cpu_cost(0),
    import_cost(0),
    mem_cost(0)
  {}

  /// Returns sum of time-consuming costs, i.e., not counting memory cost
  double total_cost() const  { return io_cost + cpu_cost + import_cost; }
  double get_io_cost()     const { return io_cost; }
  double get_cpu_cost()    const { return cpu_cost; }
  double get_import_cost() const { return import_cost; }
  double get_mem_cost()    const { return mem_cost; }

  /**
    Whether or not all costs in the object are zero
    
    @return true if all costs are zero, false otherwise
  */
  bool is_zero() const
  { 
    return !(io_cost || cpu_cost || import_cost || mem_cost);
  }
  /**
    Whether or not the total cost is the maximal double
    
    @return true if total cost is the maximal double, false otherwise
  */
  bool is_max_cost()  const { return io_cost == DBL_MAX; }
  /// Reset all costs to zero
  void reset()
  {
    io_cost= cpu_cost= import_cost= mem_cost= 0;
  }
  /// Set current cost to the maximal double
  void set_max_cost()
  {
    reset();
    io_cost= DBL_MAX;
  }

  /// Multiply io, cpu and import costs by parameter
  void multiply(double m)
  {
    DBUG_ASSERT(!is_max_cost());

    io_cost *= m;
    cpu_cost *= m;
    import_cost *= m;
    /* Don't multiply mem_cost */
  }

  Cost_estimate& operator+= (const Cost_estimate &other)
  {
    DBUG_ASSERT(!is_max_cost() && !other.is_max_cost());

    io_cost+= other.io_cost;
    cpu_cost+= other.cpu_cost;
    import_cost+= other.import_cost;
    mem_cost+= other.mem_cost;

    return *this;
  }

  Cost_estimate operator+ (const Cost_estimate &other)
  {
    Cost_estimate result= *this;
    result+= other;

    return result;
  }

  Cost_estimate operator- (const Cost_estimate &other)
  {
    Cost_estimate result;

    DBUG_ASSERT(!other.is_max_cost());

    result.io_cost= io_cost - other.io_cost;
    result.cpu_cost= cpu_cost - other.cpu_cost;
    result.import_cost= import_cost - other.import_cost;
    result.mem_cost= mem_cost - other.mem_cost;
    return result;
  }

  bool operator> (const Cost_estimate &other) const
  {
    return total_cost() > other.total_cost() ? true : false;
  }

  bool operator< (const Cost_estimate &other) const
  {
    return other > *this ? true : false;
  }

  /// Add to IO cost
  void add_io(double add_io_cost)
  {
    DBUG_ASSERT(!is_max_cost());
    io_cost+= add_io_cost;
  }

  /// Add to CPU cost
  void add_cpu(double add_cpu_cost)
  {
    DBUG_ASSERT(!is_max_cost());
    cpu_cost+= add_cpu_cost;
  }

  /// Add to import cost
  void add_import(double add_import_cost)
  {
    DBUG_ASSERT(!is_max_cost());
    import_cost+= add_import_cost;
  }

  /// Add to memory cost
  void add_mem(double add_mem_cost)
  {
    DBUG_ASSERT(!is_max_cost());
    mem_cost+= add_mem_cost;
  }
};

void get_sweep_read_cost(TABLE *table, ha_rows nrows, bool interrupted, 
                         Cost_estimate *cost);

/*
  The below two are not used (and not handled) in this milestone of this WL
  entry because there seems to be no use for them at this stage of
  implementation.
*/
#define HA_MRR_SINGLE_POINT 1
#define HA_MRR_FIXED_KEY  2

/* 
  Indicates that RANGE_SEQ_IF::next(&range) doesn't need to fill in the
  'range' parameter.
*/
#define HA_MRR_NO_ASSOCIATION 4

/* 
  The MRR user will provide ranges in key order, and MRR implementation
  must return rows in key order.
  Passing this flag to multi_read_range_init() may cause the
  default MRR handler to be used even if HA_MRR_USE_DEFAULT_IMPL
  was not specified.
  (If the native MRR impl. can not provide SORTED result)
*/
#define HA_MRR_SORTED 8

/* MRR implementation doesn't have to retrieve full records */
#define HA_MRR_INDEX_ONLY 16

/* 
  The passed memory buffer is of maximum possible size, the caller can't
  assume larger buffer.
*/
#define HA_MRR_LIMITS 32


/*
  Flag set <=> default MRR implementation is used
  (The choice is made by **_info[_const]() function which may set this
   flag. SQL layer remembers the flag value and then passes it to
   multi_read_range_init().
*/
#define HA_MRR_USE_DEFAULT_IMPL 64

/*
  Used only as parameter to multi_range_read_info():
  Flag set <=> the caller guarantees that the bounds of the scanned ranges
  will not have NULL values.
*/
#define HA_MRR_NO_NULL_ENDPOINTS 128

/*
  Set by the MRR implementation to signal that it will natively
  produced sorted result if multi_range_read_init() is called with
  the HA_MRR_SORTED flag - Else multi_range_read_init(HA_MRR_SORTED)
  will revert to use the default MRR implementation. 
*/
#define HA_MRR_SUPPORT_SORTED 256


class ha_statistics
{
public:
  ulonglong data_file_length;		/* Length off data file */
  ulonglong max_data_file_length;	/* Length off data file */
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;		/* Free bytes */
  ulonglong auto_increment_value;
  /*
    The number of records in the table. 
      0    - means the table has exactly 0 rows
    other  - if (table_flags() & HA_STATS_RECORDS_IS_EXACT)
               the value is the exact number of records in the table
             else
               it is an estimate
  */
  ha_rows records;
  ha_rows deleted;			/* Deleted records */
  ulong mean_rec_length;		/* physical reclength */
  time_t create_time;			/* When table was created */
  ulong check_time;
  ulong update_time;
  uint block_size;			/* index block size */
  
  /*
    number of buffer bytes that native mrr implementation needs,
  */
  uint mrr_length_per_rec;

  /**
    Estimate for how much of the table that is availabe in a memory
    buffer. Valid range is [0..1]. If it has the special value
    IN_MEMORY_ESTIMATE_UNKNOWN (defined in structs.h), it means that
    the storage engine has not supplied any value for it.
  */
  double table_in_mem_estimate;

  ha_statistics():
    data_file_length(0), max_data_file_length(0),
    index_file_length(0), delete_length(0), auto_increment_value(0),
    records(0), deleted(0), mean_rec_length(0), create_time(0),
    check_time(0), update_time(0), block_size(0),
    table_in_mem_estimate(IN_MEMORY_ESTIMATE_UNKNOWN)
  {}
};

/**
  Calculates length of key.

  Given a key index and a map of key parts return length of buffer used by key
  parts.

  @param  table        Table containing the key
  @param  key          Key index
  @param  keypart_map  which key parts that is used

  @return Length of used key parts.
*/
uint calculate_key_len(TABLE *table, uint key,
                       key_part_map keypart_map);
/*
  bitmap with first N+1 bits set
  (keypart_map for a key prefix of [0..N] keyparts)
*/
#define make_keypart_map(N) (((key_part_map)2 << (N)) - 1)
/*
  bitmap with first N bits set
  (keypart_map for a key prefix of [0..N-1] keyparts)
*/
#define make_prev_keypart_map(N) (((key_part_map)1 << (N)) - 1)


/** Base class to be used by handlers different shares */
class Handler_share
{
public:
  Handler_share() {}
  virtual ~Handler_share() {}
};


/**
  Wrapper for struct ft_hints.
*/

class Ft_hints: public Sql_alloc
{
private:
  struct ft_hints hints;

public:
  Ft_hints(uint ft_flags)
  {
    hints.flags= ft_flags;
    hints.op_type= FT_OP_UNDEFINED;
    hints.op_value= 0.0;
    hints.limit= HA_POS_ERROR;
  }

  /**
    Set comparison operation type and and value for master MATCH function.

     @param type   comparison operation type
     @param value  comparison operation value
  */
  void set_hint_op(enum ft_operation type, double value)
  {
    hints.op_type= type;
    hints.op_value= value;
  }

  /**
    Set Ft_hints flag.

    @param ft_flag Ft_hints flag
  */
  void set_hint_flag(uint ft_flag)
  {
    hints.flags|= ft_flag;
  }

  /**
    Set Ft_hints limit.

    @param Ft_hints limit
  */
  void set_hint_limit(ha_rows ft_limit)
  {
    hints.limit= ft_limit;
  }

  /**
    Get Ft_hints limit.

    @return Ft_hints limit
  */
  ha_rows get_limit()
  {
    return hints.limit;
  }

  /**
    Get Ft_hints operation value.

    @return operation value
  */
  double get_op_value()
  {
    return hints.op_value;
  }

  /**
    Get Ft_hints operation type.

    @return operation type
  */
  enum ft_operation get_op_type()
  {
    return hints.op_type;
  }

  /**
    Get Ft_hints flags.

    @return Ft_hints flags
  */
  uint get_flags()
  {
    return hints.flags;
  }

 /**
    Get ft_hints struct.

    @return pointer to ft_hints struct
  */
  struct ft_hints* get_hints()
  {
    return &hints;
  }
};


/**
  The handler class is the interface for dynamically loadable
  storage engines. Do not add ifdefs and take care when adding or
  changing virtual functions to avoid vtable confusion

  Functions in this class accept and return table columns data. Two data
  representation formats are used:
  1. TableRecordFormat - Used to pass [partial] table records to/from
     storage engine

  2. KeyTupleFormat - used to pass index search tuples (aka "keys") to
     storage engine. See opt_range.cc for description of this format.

  TableRecordFormat
  =================
  [Warning: this description is work in progress and may be incomplete]
  The table record is stored in a fixed-size buffer:
   
    record: null_bytes, column1_data, column2_data, ...
  
  The offsets of the parts of the buffer are also fixed: every column has 
  an offset to its column{i}_data, and if it is nullable it also has its own
  bit in null_bytes. 

  The record buffer only includes data about columns that are marked in the
  relevant column set (table->read_set and/or table->write_set, depending on
  the situation). 
  <not-sure>It could be that it is required that null bits of non-present
  columns are set to 1</not-sure>

  VARIOUS EXCEPTIONS AND SPECIAL CASES

  f the table has no nullable columns, then null_bytes is still 
  present, its length is one byte <not-sure> which must be set to 0xFF 
  at all times. </not-sure>
  
  If the table has columns of type BIT, then certain bits from those columns
  may be stored in null_bytes as well. Grep around for Field_bit for
  details.

  For blob columns (see Field_blob), the record buffer stores length of the 
  data, following by memory pointer to the blob data. The pointer is owned 
  by the storage engine and is valid until the next operation.

  If a blob column has NULL value, then its length and blob data pointer
  must be set to 0.
*/

class handler :public Sql_alloc
{
  friend class Partition_handler;
public:
  typedef ulonglong Table_flags;
protected:
  TABLE_SHARE *table_share;             /* The table definition */
  TABLE *table;                         /* The current open table */
  Table_flags cached_table_flags;       /* Set on init() and open() */

  ha_rows estimation_rows_to_insert;
public:
  handlerton *ht;                 /* storage engine of this handler */
  uchar *ref;				/* Pointer to current row */
  uchar *dup_ref;			/* Pointer to duplicate row */

  ha_statistics stats;
  
  /* MultiRangeRead-related members: */
  range_seq_t mrr_iter;    /* Interator to traverse the range sequence */
  RANGE_SEQ_IF mrr_funcs;  /* Range sequence traversal functions */
  HANDLER_BUFFER *multi_range_buffer; /* MRR buffer info */
  uint ranges_in_seq; /* Total number of ranges in the traversed sequence */
  /* TRUE <=> source MRR ranges and the output are ordered */
  bool mrr_is_output_sorted;
  
  /* TRUE <=> we're currently traversing a range in mrr_cur_range. */
  bool mrr_have_range;
  /* Current range (the one we're now returning rows from) */
  KEY_MULTI_RANGE mrr_cur_range;

  /*
    The direction of the current range or index scan. This is used by
    the ICP implementation to determine if it has reached the end
    of the current range.
  */
  enum enum_range_scan_direction {
    RANGE_SCAN_ASC,
    RANGE_SCAN_DESC
  };
private:
  /*
    Storage space for the end range value. Should only be accessed using
    the end_range pointer. The content is invalid when end_range is NULL.
  */
  key_range save_end_range;
  enum_range_scan_direction range_scan_direction;
  int key_compare_result_on_equal;

protected:
  KEY_PART_INFO *range_key_part;
  bool eq_range;
  /* 
    TRUE <=> the engine guarantees that returned records are within the range
    being scanned.
  */
  bool in_range_check_pushed_down;

public:  
  /*
    End value for a range scan. If this is NULL the range scan has no
    end value. Should also be NULL when there is no ongoing range scan.
    Used by the read_range() functions and also evaluated by pushed
    index conditions.
  */
  key_range *end_range;
  uint errkey;				/* Last dup key */
  uint key_used_on_scan;
  uint active_index;
  /** Length of ref (1-8 or the clustered key length) */
  uint ref_length;
  FT_INFO *ft_handler;
  enum {NONE=0, INDEX, RND} inited;
  bool implicit_emptied;                /* Can be !=0 only if HEAP */
  const Item *pushed_cond;

  Item *pushed_idx_cond;
  uint pushed_idx_cond_keyno;  /* The index which the above condition is for */

  /**
    next_insert_id is the next value which should be inserted into the
    auto_increment column: in a inserting-multi-row statement (like INSERT
    SELECT), for the first row where the autoinc value is not specified by the
    statement, get_auto_increment() called and asked to generate a value,
    next_insert_id is set to the next value, then for all other rows
    next_insert_id is used (and increased each time) without calling
    get_auto_increment().
  */
  ulonglong next_insert_id;
  /**
    insert id for the current row (*autogenerated*; if not
    autogenerated, it's 0).
    At first successful insertion, this variable is stored into
    THD::first_successful_insert_id_in_cur_stmt.
  */
  ulonglong insert_id_for_cur_row;
  /**
    Interval returned by get_auto_increment() and being consumed by the
    inserter.
  */
  Discrete_interval auto_inc_interval_for_cur_row;
  /**
     Number of reserved auto-increment intervals. Serves as a heuristic
     when we have no estimation of how many records the statement will insert:
     the more intervals we have reserved, the bigger the next one. Reset in
     handler::ha_release_auto_increment().
  */
  uint auto_inc_intervals_count;

  /**
    Instrumented table associated with this handler.
  */
  PSI_table *m_psi;

private:
  /** Internal state of the batch instrumentation. */
  enum batch_mode_t
  {
    /** Batch mode not used. */
    PSI_BATCH_MODE_NONE,
    /** Batch mode used, before first table io. */
    PSI_BATCH_MODE_STARTING,
    /** Batch mode used, after first table io. */
    PSI_BATCH_MODE_STARTED
  };
  /**
    Batch mode state.
    @sa start_psi_batch_mode.
    @sa end_psi_batch_mode.
  */
  batch_mode_t m_psi_batch_mode;
  /**
    The number of rows in the batch.
    @sa start_psi_batch_mode.
    @sa end_psi_batch_mode.
  */
  ulonglong m_psi_numrows;
  /**
    The current event in a batch.
    @sa start_psi_batch_mode.
    @sa end_psi_batch_mode.
  */
  PSI_table_locker *m_psi_locker;
  /**
    Storage for the event in a batch.
    @sa start_psi_batch_mode.
    @sa end_psi_batch_mode.
  */
  PSI_table_locker_state m_psi_locker_state;

public:
  virtual void unbind_psi();
  virtual void rebind_psi();
  /**
    Put the handler in 'batch' mode when collecting
    table io instrumented events.
    When operating in batch mode:
    - a single start event is generated in the performance schema.
    - all table io performed between @c start_psi_batch_mode
      and @c end_psi_batch_mode is not instrumented:
      the number of rows affected is counted instead in @c m_psi_numrows.
    - a single end event is generated in the performance schema
      when the batch mode ends with @c end_psi_batch_mode.
  */
  void start_psi_batch_mode();
  /** End a batch started with @c start_psi_batch_mode. */
  void end_psi_batch_mode();

private:
  friend class DsMrr_impl;
  /**
    The lock type set by when calling::ha_external_lock(). This is 
    propagated down to the storage engine. The reason for also storing 
    it here, is that when doing MRR we need to create/clone a second handler
    object. This cloned handler object needs to know about the lock_type used.
  */
  int m_lock_type;
  /**
    Pointer where to store/retrieve the Handler_share pointer.
    For non partitioned handlers this is &TABLE_SHARE::ha_share.
  */
  Handler_share **ha_share;

  /**
    Some non-virtual ha_* functions, responsible for reading rows,
    like ha_rnd_pos(), must ensure that virtual generated columns are
    calculated before they return. For that, they should set this
    member to true at their start, and check it before they return: if
    the member is still true, it means they should calculate; if it's
    false, it means the calculation has been done by some called
    lower-level function and does not need to be re-done (which is why
    we need this status flag: to avoid redundant calculations, for
    performance).
  */
  bool m_update_generated_read_fields;

public:
  handler(handlerton *ht_arg, TABLE_SHARE *share_arg)
    :table_share(share_arg), table(0),
    estimation_rows_to_insert(0), ht(ht_arg),
    ref(0), range_scan_direction(RANGE_SCAN_ASC),
    in_range_check_pushed_down(false), end_range(NULL),
    key_used_on_scan(MAX_KEY), active_index(MAX_KEY),
    ref_length(sizeof(my_off_t)),
    ft_handler(0), inited(NONE),
    implicit_emptied(0),
    pushed_cond(0), pushed_idx_cond(NULL), pushed_idx_cond_keyno(MAX_KEY),
    next_insert_id(0), insert_id_for_cur_row(0),
    auto_inc_intervals_count(0),
    m_psi(NULL),
    m_psi_batch_mode(PSI_BATCH_MODE_NONE),
    m_psi_numrows(0),
    m_psi_locker(NULL),
    m_lock_type(F_UNLCK), ha_share(NULL), m_update_generated_read_fields(false)
    {
      DBUG_PRINT("info",
                 ("handler created F_UNLCK %d F_RDLCK %d F_WRLCK %d",
                  F_UNLCK, F_RDLCK, F_WRLCK));
    }
  virtual ~handler(void)
  {
    DBUG_ASSERT(m_psi == NULL);
    DBUG_ASSERT(m_psi_batch_mode == PSI_BATCH_MODE_NONE);
    DBUG_ASSERT(m_psi_locker == NULL);
    DBUG_ASSERT(m_lock_type == F_UNLCK);
    DBUG_ASSERT(inited == NONE);
  }
  /* TODO: reorganize the methods and have proper public/protected/private qualifiers!!! */
  virtual handler *clone(const char *name, MEM_ROOT *mem_root);
  /** This is called after create to allow us to set up cached variables */
  void init()
  {
    cached_table_flags= table_flags();
  }
  /* ha_ methods: public wrappers for private virtual API */

  int ha_open(TABLE *table, const char *name, int mode, int test_if_locked);
  int ha_close(void);
  int ha_index_init(uint idx, bool sorted);
  int ha_index_end();
  int ha_rnd_init(bool scan);
  int ha_rnd_end();
  int ha_rnd_next(uchar *buf);
  int ha_rnd_pos(uchar * buf, uchar *pos);
  int ha_index_read_map(uchar *buf, const uchar *key,
                        key_part_map keypart_map,
                        enum ha_rkey_function find_flag);
  int ha_index_read_last_map(uchar * buf, const uchar * key,
                             key_part_map keypart_map);
  int ha_index_read_idx_map(uchar *buf, uint index, const uchar *key,
                           key_part_map keypart_map,
                           enum ha_rkey_function find_flag);
  int ha_index_next(uchar * buf);
  int ha_index_prev(uchar * buf);
  int ha_index_first(uchar * buf);
  int ha_index_last(uchar * buf);
  int ha_index_next_same(uchar *buf, const uchar *key, uint keylen);
  int ha_reset();
  /* this is necessary in many places, e.g. in HANDLER command */
  int ha_index_or_rnd_end()
  {
    return inited == INDEX ? ha_index_end() : inited == RND ? ha_rnd_end() : 0;
  }
  /**
    The cached_table_flags is set at ha_open and ha_external_lock
  */
  Table_flags ha_table_flags() const { return cached_table_flags; }
  /**
    These functions represent the public interface to *users* of the
    handler class, hence they are *not* virtual. For the inheritance
    interface, see the (private) functions write_row(), update_row(),
    and delete_row() below.
  */
  int ha_external_lock(THD *thd, int lock_type);
  int ha_write_row(uchar * buf);
  int ha_update_row(const uchar * old_data, uchar * new_data);
  int ha_delete_row(const uchar * buf);
  void ha_release_auto_increment();

  int check_collation_compatibility();
  int ha_check_for_upgrade(HA_CHECK_OPT *check_opt);
  /** to be actually called to get 'check()' functionality*/
  int ha_check(THD *thd, HA_CHECK_OPT *check_opt);
  int ha_repair(THD* thd, HA_CHECK_OPT* check_opt);
  void ha_start_bulk_insert(ha_rows rows);
  int ha_end_bulk_insert();
  int ha_bulk_update_row(const uchar *old_data, uchar *new_data,
                         uint *dup_key_found);
  int ha_delete_all_rows();
  int ha_truncate();
  int ha_optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int ha_analyze(THD* thd, HA_CHECK_OPT* check_opt);
  bool ha_check_and_repair(THD *thd);
  int ha_disable_indexes(uint mode);
  int ha_enable_indexes(uint mode);
  int ha_discard_or_import_tablespace(my_bool discard);
  int ha_rename_table(const char *from, const char *to);
  int ha_delete_table(const char *name);
  void ha_drop_table(const char *name);

  int ha_create(const char *name, TABLE *form, HA_CREATE_INFO *info);

  int ha_create_handler_files(const char *name, const char *old_name,
                              int action_flag, HA_CREATE_INFO *info);

  void adjust_next_insert_id_after_explicit_value(ulonglong nr);
  int update_auto_increment();
  virtual void print_error(int error, myf errflag);
  virtual bool get_error_message(int error, String *buf);
  uint get_dup_key(int error);
  /**
    Retrieves the names of the table and the key for which there was a
    duplicate entry in the case of HA_ERR_FOREIGN_DUPLICATE_KEY.

    If any of the table or key name is not available this method will return
    false and will not change any of child_table_name or child_key_name.

    @param child_table_name[out]    Table name
    @param child_table_name_len[in] Table name buffer size
    @param child_key_name[out]      Key name
    @param child_key_name_len[in]   Key name buffer size

    @retval  true                  table and key names were available
                                   and were written into the corresponding
                                   out parameters.
    @retval  false                 table and key names were not available,
                                   the out parameters were not touched.
  */
  virtual bool get_foreign_dup_key(char *child_table_name,
                                   uint child_table_name_len,
                                   char *child_key_name,
                                   uint child_key_name_len)
  { DBUG_ASSERT(false); return(false); }
  virtual void change_table_ptr(TABLE *table_arg, TABLE_SHARE *share)
  {
    table= table_arg;
    table_share= share;
  }
  /* Estimates calculation */

  /**
    @deprecated This function is deprecated and will be removed in a future
                version. Use table_scan_cost() instead.
  */

  virtual double scan_time()
  { return ulonglong2double(stats.data_file_length) / IO_SIZE + 2; }

  /**
    The cost of reading a set of ranges from the table using an index
    to access it.

    @deprecated This function is deprecated and will be removed in a future
                version. Use read_cost() instead.
   
    @param index  The index number.
    @param ranges The number of ranges to be read.
    @param rows   Total number of rows to be read.
   
    This method can be used to calculate the total cost of scanning a table
    using an index by calling it using read_time(index, 1, table_size).
  */

  virtual double read_time(uint index, uint ranges, ha_rows rows)
  { return rows2double(ranges+rows); }

  /**
    @deprecated This function is deprecated and will be removed in a future
                version. Use index_scan_cost() instead.
  */

  virtual double index_only_read_time(uint keynr, double records);

  /**
    Cost estimate for doing a complete table scan.

    @note For this version it is recommended that storage engines continue
    to override scan_time() instead of this function.

    @returns the estimated cost
  */

  virtual Cost_estimate table_scan_cost();

  /**
    Cost estimate for reading a number of ranges from an index.

    The cost estimate will only include the cost of reading data that
    is contained in the index. If the records need to be read, use
    read_cost() instead.

    @note The ranges parameter is currently ignored and is not taken
    into account in the cost estimate.

    @note For this version it is recommended that storage engines continue
    to override index_only_read_time() instead of this function.
 
    @param index  the index number
    @param ranges the number of ranges to be read
    @param rows   total number of rows to be read

    @returns the estimated cost
  */
  
  virtual Cost_estimate index_scan_cost(uint index, double ranges, double rows);

  /**
    Cost estimate for reading a set of ranges from the table using an index
    to access it.

    @note For this version it is recommended that storage engines continue
    to override read_time() instead of this function.

    @param index  the index number
    @param ranges the number of ranges to be read
    @param rows   total number of rows to be read

    @returns the estimated cost
  */

  virtual Cost_estimate read_cost(uint index, double ranges, double rows);
  
  /**
    Return an estimate on the amount of memory the storage engine will
    use for caching data in memory. If this is unknown or the storage
    engine does not cache data in memory -1 is returned.
  */
  virtual longlong get_memory_buffer_size() const { return -1; }

  /**
    Return an estimate of how much of the table that is currently stored
    in main memory.

    This estimate should be the fraction of the table that currently
    is available in a main memory buffer. The estimate should be in the
    range from 0.0 (nothing in memory) to 1.0 (entire table in memory).

    @return The fraction of the table in main memory buffer
  */

  double table_in_memory_estimate() const;

  /**
    Return an estimate of how much of the index that is currently stored
    in main memory.

    This estimate should be the fraction of the index that currently
    is available in a main memory buffer. The estimate should be in the
    range from 0.0 (nothing in memory) to 1.0 (entire index in memory).

    @param keyno the index to get an estimate for

    @return The fraction of the index in main memory buffer
  */

  double index_in_memory_estimate(uint keyno) const;

private:
  /**
    Make a guestimate for how much of a table or index is in a memory
    buffer in the case where the storage engine has not provided any
    estimate for this.

    @param table_index_size size of the table or index

    @return The fraction of the table or index in main memory buffer
  */

  double estimate_in_memory_buffer(ulonglong table_index_size) const;

public:
  virtual ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                              void *seq_init_param, 
                                              uint n_ranges, uint *bufsz,
                                              uint *flags, 
                                              Cost_estimate *cost);
  virtual ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                        uint *bufsz, uint *flags, 
                                        Cost_estimate *cost);
  virtual int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                    uint n_ranges, uint mode,
                                    HANDLER_BUFFER *buf);
  virtual int multi_range_read_next(char **range_info);


  virtual const key_map *keys_to_use_for_scanning() { return &key_map_empty; }
  bool has_transactions()
  { return (ha_table_flags() & HA_NO_TRANSACTIONS) == 0; }
  virtual uint extra_rec_buf_length() const { return 0; }

  /**
    @brief Determine whether an error can be ignored or not.

    @details This method is used to analyze the error to see whether the
    error is ignorable or not. Such errors will be reported as warnings
    instead of errors for IGNORE statements. This means that the statement
    will not abort, but instead continue to the next row.

    HA_ERR_FOUND_DUP_UNIQUE is a special case in MyISAM that means the
    same thing as HA_ERR_FOUND_DUP_KEY, but can in some cases lead to
    a slightly different error message.

    @param error  error code received from the handler interface (HA_ERR_...)

    @return   whether the error is ignorablel or not
      @retval true  the error is ignorable
      @retval false the error is not ignorable
  */

  virtual bool is_ignorable_error(int error);

  /**
    @brief Determine whether an error is fatal or not.

    @details This method is used to analyze the error to see whether the
    error is fatal or not. A fatal error is an error that will not be
    possible to handle with SP handlers and will not be subject to
    retry attempts on the slave.

    @param error  error code received from the handler interface (HA_ERR_...)

    @return   whether the error is fatal or not
      @retval true  the error is fatal
      @retval false the error is not fatal
  */

  virtual bool is_fatal_error(int error);

protected:
  /**
    Number of rows in table. It will only be called if
    (table_flags() & (HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT)) != 0
    @param[out]  num_rows number of rows in table.
    @retval 0 for OK, one of the HA_xxx values in case of error.
  */
  virtual int records(ha_rows *num_rows)
  {
    *num_rows= stats.records;
    return 0;
  }

public:
 /**
   Public function wrapping the actual handler call, and doing error checking.
    @param[out]  num_rows number of rows in table.
    @retval 0 for OK, one of the HA_xxx values in case of error.
 */
  int ha_records(ha_rows *num_rows)
  {
    int error= records(num_rows);
    // A return value of HA_POS_ERROR was previously used to indicate error.
    if (error != 0)
      DBUG_ASSERT(*num_rows == HA_POS_ERROR);
    if (*num_rows == HA_POS_ERROR)
      DBUG_ASSERT(error != 0);
    if (error != 0)
    {
      /*
        ha_innobase::records may have rolled back internally.
        In this case, thd_mark_transaction_to_rollback() will have been called.
        For the errors below, we need to abort right away.
      */
      switch (error) {
      case HA_ERR_LOCK_DEADLOCK:
      case HA_ERR_LOCK_TABLE_FULL:
      case HA_ERR_LOCK_WAIT_TIMEOUT:
      case HA_ERR_QUERY_INTERRUPTED:
        print_error(error, MYF(0));
        return error;
      default:
        return error;
      }
    }
    return 0;
  }

  /**
    Return upper bound of current number of records in the table
    (max. of how many records one will retrieve when doing a full table scan)
    If upper bound is not known, HA_POS_ERROR should be returned as a max
    possible upper bound.
  */
  virtual ha_rows estimate_rows_upper_bound()
  { return stats.records+EXTRA_RECORDS; }

  /**
    Get the row type from the storage engine.  If this method returns
    ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
  */
  virtual enum row_type get_row_type() const { return ROW_TYPE_NOT_USED; }

  virtual const char *index_type(uint key_number) { DBUG_ASSERT(0); return "";}


  /**
    Signal that the table->read_set and table->write_set table maps changed
    The handler is allowed to set additional bits in the above map in this
    call. Normally the handler should ignore all calls until we have done
    a ha_rnd_init() or ha_index_init(), write_row(), update_row or delete_row()
    as there may be several calls to this routine.
  */
  virtual void column_bitmaps_signal();
  uint get_index(void) const { return active_index; }

  /**
    @retval  0   Bulk update used by handler
    @retval  1   Bulk update not used, normal operation used
  */
  virtual bool start_bulk_update() { return 1; }
  /**
    @retval  0   Bulk delete used by handler
    @retval  1   Bulk delete not used, normal operation used
  */
  virtual bool start_bulk_delete() { return 1; }
  /**
    After this call all outstanding updates must be performed. The number
    of duplicate key errors are reported in the duplicate key parameter.
    It is allowed to continue to the batched update after this call, the
    handler has to wait until end_bulk_update with changing state.

    @param    dup_key_found       Number of duplicate keys found

    @retval  0           Success
    @retval  >0          Error code
  */
  virtual int exec_bulk_update(uint *dup_key_found)
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
  /**
    Perform any needed clean-up, no outstanding updates are there at the
    moment.
  */
  virtual void end_bulk_update() { return; }
  /**
    Execute all outstanding deletes and close down the bulk delete.

    @retval 0             Success
    @retval >0            Error code
  */
  virtual int end_bulk_delete()
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
protected:
  /**
     @brief
     Positions an index cursor to the index specified in the handle
     ('active_index'). Fetches the row if available. If the key value is null,
     begin at the first key of the index.
     @returns 0 if success (found a record, and function has set table->status
     to 0); non-zero if no record (function has set table->status to
     STATUS_NOT_FOUND).
  */
  virtual int index_read_map(uchar * buf, const uchar * key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag)
  {
    uint key_len= calculate_key_len(table, active_index, keypart_map);
    return  index_read(buf, key, key_len, find_flag);
  }
  /**
     @brief
     Positions an index cursor to the index specified in argument. Fetches
     the row if available. If the key value is null, begin at the first key of
     the index.
     @returns @see index_read_map().
  */
  virtual int index_read_idx_map(uchar * buf, uint index, const uchar * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag);
  /// @returns @see index_read_map().
  virtual int index_next(uchar * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  /// @returns @see index_read_map().
  virtual int index_prev(uchar * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  /// @returns @see index_read_map().
  virtual int index_first(uchar * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  /// @returns @see index_read_map().
  virtual int index_last(uchar * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  /// @returns @see index_read_map().
  virtual int index_next_same(uchar *buf, const uchar *key, uint keylen);
  /**
     @brief
     The following functions works like index_read, but it find the last
     row with the current key value or prefix.
     @returns @see index_read_map().
  */
  virtual int index_read_last_map(uchar * buf, const uchar * key,
                                  key_part_map keypart_map)
  {
    uint key_len= calculate_key_len(table, active_index, keypart_map);
    return index_read_last(buf, key, key_len);
  }
public:
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  virtual int read_range_next();

  /**
    Set the end position for a range scan. This is used for checking
    for when to end the range scan and by the ICP code to determine
    that the next record is within the current range.

    @param range     The end value for the range scan
    @param direction Direction of the range scan
  */
  void set_end_range(const key_range* range,
                     enum_range_scan_direction direction);
  int compare_key(key_range *range);
  int compare_key_icp(const key_range *range) const;
  int compare_key_in_buffer(const uchar *buf) const;
  virtual int ft_init() { return HA_ERR_WRONG_COMMAND; }
  void ft_end() { ft_handler=NULL; }
  virtual FT_INFO *ft_init_ext(uint flags, uint inx,String *key)
    { return NULL; }
  virtual FT_INFO *ft_init_ext_with_hints(uint inx, String *key,
                                          Ft_hints *hints)
  {
    return ft_init_ext(hints->get_flags(), inx, key);
  }
  virtual int ft_read(uchar *buf) { return HA_ERR_WRONG_COMMAND; }
protected:
  /// @returns @see index_read_map().
  virtual int rnd_next(uchar *buf)=0;
  /// @returns @see index_read_map().
  virtual int rnd_pos(uchar * buf, uchar *pos)=0;
public:
  /**
    This function only works for handlers having
    HA_PRIMARY_KEY_REQUIRED_FOR_POSITION set.
    It will return the row with the PK given in the record argument.
  */
  virtual int rnd_pos_by_record(uchar *record)
    {
      int error;
      DBUG_ASSERT(table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION);

      error = ha_rnd_init(FALSE);
      if (error != 0)
            return error;

      position(record);
      error = ha_rnd_pos(record, ref);
      ha_rnd_end();
      return error;

    }
  virtual int read_first_row(uchar *buf, uint primary_key);
  virtual ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key)
    { return (ha_rows) 10; }
  /*
    If HA_PRIMARY_KEY_REQUIRED_FOR_POSITION is set, then it sets ref
    (reference to the row, aka position, with the primary key given in
    the record).
    Otherwise it set ref to the current row.
  */
  virtual void position(const uchar *record)=0;
  virtual int info(uint)=0; // see my_base.h for full description
  virtual uint32 calculate_key_hash_value(Field **field_array)
  { DBUG_ASSERT(0); return 0; }
  virtual int extra(enum ha_extra_function operation)
  { return 0; }
  virtual int extra_opt(enum ha_extra_function operation, ulong cache_size)
  { return extra(operation); }

  /**
    Start read (before write) removal on the current table.
    @see HA_READ_BEFORE_WRITE_REMOVAL
  */
  virtual bool start_read_removal(void)
  { DBUG_ASSERT(0); return false; }

  /**
    End read (before write) removal and return the number of rows
    really written
    @see HA_READ_BEFORE_WRITE_REMOVAL
  */
  virtual ha_rows end_read_removal(void)
  { DBUG_ASSERT(0); return (ha_rows) 0; }

  /**
    In an UPDATE or DELETE, if the row under the cursor was locked by another
    transaction, and the engine used an optimistic read of the last
    committed row value under the cursor, then the engine returns 1 from this
    function. MySQL must NOT try to update this optimistic value. If the
    optimistic value does not match the WHERE condition, MySQL can decide to
    skip over this row. Currently only works for InnoDB. This can be used to
    avoid unnecessary lock waits.

    If this method returns nonzero, it will also signal the storage
    engine that the next read will be a locking re-read of the row.
  */
  virtual bool was_semi_consistent_read() { return 0; }
  /**
    Tell the engine whether it should avoid unnecessary lock waits.
    If yes, in an UPDATE or DELETE, if the row under the cursor was locked
    by another transaction, the engine may try an optimistic read of
    the last committed row value under the cursor.
  */
  virtual void try_semi_consistent_read(bool) {}
  virtual void unlock_row() {}
  virtual int start_stmt(THD *thd, thr_lock_type lock_type) {return 0;}
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  void set_next_insert_id(ulonglong id)
  {
    DBUG_PRINT("info",("auto_increment: next value %lu", (ulong)id));
    next_insert_id= id;
  }
  void restore_auto_increment(ulonglong prev_insert_id)
  {
    /*
      Insertion of a row failed, re-use the lastly generated auto_increment
      id, for the next row. This is achieved by resetting next_insert_id to
      what it was before the failed insertion (that old value is provided by
      the caller). If that value was 0, it was the first row of the INSERT;
      then if insert_id_for_cur_row contains 0 it means no id was generated
      for this first row, so no id was generated since the INSERT started, so
      we should set next_insert_id to 0; if insert_id_for_cur_row is not 0, it
      is the generated id of the first and failed row, so we use it.
    */
    next_insert_id= (prev_insert_id > 0) ? prev_insert_id :
      insert_id_for_cur_row;
  }

  virtual void update_create_info(HA_CREATE_INFO *create_info) {}
  int check_old_types();
  virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int preload_keys(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  /* end of the list of admin commands */

  virtual int indexes_are_disabled(void) {return 0;}
  virtual void append_create_info(String *packet) {}
  /**
    If index == MAX_KEY then a check for table is made and if index <
    MAX_KEY then a check is made if the table has foreign keys and if
    a foreign key uses this index (and thus the index cannot be dropped).

    @param  index            Index to check if foreign key uses it

    @retval   TRUE            Foreign key defined on table or index
    @retval   FALSE           No foreign key defined
  */
  virtual bool is_fk_defined_on_table_or_index(uint index)
  { return FALSE; }
  virtual char* get_foreign_key_create_info()
  { return(NULL);}  /* gets foreign key create string from InnoDB */
  /**
    Used in ALTER TABLE to check if changing storage engine is allowed.

    @note Called without holding thr_lock.c lock.

    @retval true   Changing storage engine is allowed.
    @retval false  Changing storage engine not allowed.
  */
  virtual bool can_switch_engines() { return true; }
  /**
    Get the list of foreign keys in this table.

    @remark Returns the set of foreign keys where this table is the
            dependent or child table.

    @param thd  The thread handle.
    @param f_key_list[out]  The list of foreign keys.

    @return The handler error code or zero for success.
  */
  virtual int
  get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
  { return 0; }
  /**
    Get the list of foreign keys referencing this table.

    @remark Returns the set of foreign keys where this table is the
            referenced or parent table.

    @param thd  The thread handle.
    @param f_key_list[out]  The list of foreign keys.

    @return The handler error code or zero for success.
  */
  virtual int
  get_parent_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
  { return 0; }
  /**
    Get the list of tables which are direct or indirect parents in foreign
    key with cascading actions for this table.

    @remarks Returns the set of parent tables connected by FK clause that
    can modify the given table.

    @param      thd             The thread handle.
    @param[out] fk_table_list   List of parent tables (including indirect parents).
                                Elements of the list as well as buffers for database
                                and schema names are allocated from the current
                                memory root. 

    @return The handler error code or zero for success
  */
  virtual int
  get_cascade_foreign_key_table_list(THD *thd,
                                     List<st_handler_tablename> *fk_table_list)
  { return 0; }
  virtual uint referenced_by_foreign_key() { return 0;}
  virtual void init_table_handle_for_HANDLER()
  { return; }       /* prepare InnoDB for HANDLER */
  virtual void free_foreign_key_create_info(char* str) {}
  /** The following can be called without an open handler */
  virtual const char *table_type() const =0;
  /**
    If frm_error() is called then we will use this to find out what file
    extentions exist for the storage engine. This is also used by the default
    rename_table and delete_table method in handler.cc.

    For engines that have two file name extentions (separate meta/index file
    and data file), the order of elements is relevant. First element of engine
    file name extentions array should be meta/index file extention. Second
    element - data file extention. This order is assumed by
    prepare_for_repair() when REPAIR TABLE ... USE_FRM is issued.
  */
  virtual const char **bas_ext() const =0;

  virtual ulong index_flags(uint idx, uint part, bool all_parts) const =0;

  uint max_record_length() const
  {
    return std::min(HA_MAX_REC_LENGTH, max_supported_record_length());
  }
  uint max_keys() const
  {
    return std::min<uint>(MAX_KEY, max_supported_keys());
  }
  uint max_key_parts() const
  {
    return std::min(MAX_REF_PARTS, max_supported_key_parts());
  }
  uint max_key_length() const
  {
    return std::min(MAX_KEY_LENGTH, max_supported_key_length());
  }
  uint max_key_part_length() const
  {
    return std::min(MAX_KEY_LENGTH, max_supported_key_part_length());
  }

  virtual uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  virtual uint max_supported_keys() const { return 0; }
  virtual uint max_supported_key_parts() const { return MAX_REF_PARTS; }
  virtual uint max_supported_key_length() const { return MAX_KEY_LENGTH; }
  virtual uint max_supported_key_part_length() const { return 255; }
  virtual uint min_record_length(uint options) const { return 1; }

  virtual bool low_byte_first() const { return 1; }
  virtual ha_checksum checksum() const { return 0; }
  virtual bool is_crashed() const  { return 0; }
  virtual bool auto_repair() const { return 0; }


#define CHF_CREATE_FLAG 0
#define CHF_DELETE_FLAG 1
#define CHF_RENAME_FLAG 2
#define CHF_INDEX_FLAG  3


  /**
    @note lock_count() can return > 1 if the table is MERGE or partitioned.
  */
  virtual uint lock_count(void) const { return 1; }
  /**
    Is not invoked for non-transactional temporary tables.

    @note store_lock() can return more than one lock if the table is MERGE
    or partitioned.

    @note that one can NOT rely on table->in_use in store_lock().  It may
    refer to a different thread if called from mysql_lock_abort_for_thread().

    @note If the table is MERGE, store_lock() can return less locks
    than lock_count() claimed. This can happen when the MERGE children
    are not attached when this is called from another thread.
  */
  virtual THR_LOCK_DATA **store_lock(THD *thd,
				     THR_LOCK_DATA **to,
				     enum thr_lock_type lock_type)=0;

  /** Type of table for caching query */
  virtual uint8 table_cache_type() { return HA_CACHE_TBL_NONTRANSACT; }


  /**
    @brief Register a named table with a call back function to the query cache.

    @param thd The thread handle
    @param table_key A pointer to the table name in the table cache
    @param key_length The length of the table name
    @param[out] engine_callback The pointer to the storage engine call back
      function
    @param[out] engine_data Storage engine specific data which could be
      anything

    This method offers the storage engine, the possibility to store a reference
    to a table name which is going to be used with query cache. 
    The method is called each time a statement is written to the cache and can
    be used to verify if a specific statement is cachable. It also offers
    the possibility to register a generic (but static) call back function which
    is called each time a statement is matched against the query cache.

    @note If engine_data supplied with this function is different from
      engine_data supplied with the callback function, and the callback returns
      FALSE, a table invalidation on the current table will occur.

    @return Upon success the engine_callback will point to the storage engine
      call back function, if any, and engine_data will point to any storage
      engine data used in the specific implementation.
      @retval TRUE Success
      @retval FALSE The specified table or current statement should not be
        cached
  */

  virtual my_bool register_query_cache_table(THD *thd, char *table_key,
                                             size_t key_length,
                                             qc_engine_callback
                                             *engine_callback,
                                             ulonglong *engine_data)
  {
    *engine_callback= 0;
    return TRUE;
  }


 /**
   Check if the primary key is clustered or not.

   @retval true  Primary key (if there is one) is a clustered
                 key covering all fields
   @retval false otherwise
 */

 virtual bool primary_key_is_clustered() const { return false; }
 virtual int cmp_ref(const uchar *ref1, const uchar *ref2)
 {
   return memcmp(ref1, ref2, ref_length);
 }

 /*
   Condition pushdown to storage engines
 */

 /**
   Push condition down to the table handler.

   @param  cond   Condition to be pushed. The condition tree must not be
                  modified by the by the caller.

   @return
     The 'remainder' condition that caller must use to filter out records.
     NULL means the handler will not return rows that do not match the
     passed condition.

   @note
   The pushed conditions form a stack (from which one can remove the
   last pushed condition using cond_pop).
   The table handler filters out rows using (pushed_cond1 AND pushed_cond2 
   AND ... AND pushed_condN)
   or less restrictive condition, depending on handler's capabilities.

   handler->ha_reset() call empties the condition stack.
   Calls to rnd_init/rnd_end, index_init/index_end etc do not affect the
   condition stack.
 */ 
 virtual const Item *cond_push(const Item *cond) { return cond; };
 /**
   Pop the top condition from the condition stack of the handler instance.

   Pops the top if condition stack, if stack is not empty.
 */
 virtual void cond_pop() { return; }

 /**
   Push down an index condition to the handler.

   The server will use this method to push down a condition it wants
   the handler to evaluate when retrieving records using a specified
   index. The pushed index condition will only refer to fields from
   this handler that is contained in the index (but it may also refer
   to fields in other handlers). Before the handler evaluates the
   condition it must read the content of the index entry into the 
   record buffer.

   The handler is free to decide if and how much of the condition it
   will take responsibility for evaluating. Based on this evaluation
   it should return the part of the condition it will not evaluate.
   If it decides to evaluate the entire condition it should return
   NULL. If it decides not to evaluate any part of the condition it
   should return a pointer to the same condition as given as argument.

   @param keyno    the index number to evaluate the condition on
   @param idx_cond the condition to be evaluated by the handler

   @return The part of the pushed condition that the handler decides
           not to evaluate
  */

 virtual Item *idx_cond_push(uint keyno, Item* idx_cond) { return idx_cond; }

 /** Reset information about pushed index conditions */
 virtual void cancel_pushed_idx_cond()
 {
   pushed_idx_cond= NULL;
   pushed_idx_cond_keyno= MAX_KEY;
   in_range_check_pushed_down= false;
 }

  /**
    Reports #tables included in pushed join which this
    handler instance is part of. ==0 -> Not pushed
  */
  virtual uint number_of_pushed_joins() const
  { return 0; }

  /**
    If this handler instance is part of a pushed join sequence
    returned TABLE instance being root of the pushed query?
  */
  virtual const TABLE* root_of_pushed_join() const
  { return NULL; }

  /**
    If this handler instance is a child in a pushed join sequence
    returned TABLE instance being my parent?
  */
  virtual const TABLE* parent_of_pushed_join() const
  { return NULL; }

  virtual int index_read_pushed(uchar * buf, const uchar * key,
                             key_part_map keypart_map)
  { return  HA_ERR_WRONG_COMMAND; }

  virtual int index_next_pushed(uchar * buf)
  { return  HA_ERR_WRONG_COMMAND; }

 /**
   Part of old, deprecated in-place ALTER API.
 */
 virtual bool check_if_incompatible_data(HA_CREATE_INFO *create_info,
					 uint table_changes)
 { return COMPATIBLE_DATA_NO; }

 /* On-line/in-place ALTER TABLE interface. */

 /*
   Here is an outline of on-line/in-place ALTER TABLE execution through
   this interface.

   Phase 1 : Initialization
   ========================
   During this phase we determine which algorithm should be used
   for execution of ALTER TABLE and what level concurrency it will
   require.

   *) This phase starts by opening the table and preparing description
      of the new version of the table.
   *) Then we check if it is impossible even in theory to carry out
      this ALTER TABLE using the in-place algorithm. For example, because
      we need to change storage engine or the user has explicitly requested
      usage of the "copy" algorithm.
   *) If in-place ALTER TABLE is theoretically possible, we continue
      by compiling differences between old and new versions of the table
      in the form of HA_ALTER_FLAGS bitmap. We also build a few
      auxiliary structures describing requested changes and store
      all these data in the Alter_inplace_info object.
   *) Then the handler::check_if_supported_inplace_alter() method is called
      in order to find if the storage engine can carry out changes requested
      by this ALTER TABLE using the in-place algorithm. To determine this,
      the engine can rely on data in HA_ALTER_FLAGS/Alter_inplace_info
      passed to it as well as on its own checks. If the in-place algorithm
      can be used for this ALTER TABLE, the level of required concurrency for
      its execution is also returned.
      If any errors occur during the handler call, ALTER TABLE is aborted
      and no further handler functions are called.
   *) Locking requirements of the in-place algorithm are compared to any
      concurrency requirements specified by user. If there is a conflict
      between them, we either switch to the copy algorithm or emit an error.

   Phase 2 : Execution
   ===================

   In this phase the operations are executed.

   *) As the first step, we acquire a lock corresponding to the concurrency
      level which was returned by handler::check_if_supported_inplace_alter()
      and requested by the user. This lock is held for most of the
      duration of in-place ALTER (if HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE
      or HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE were returned we acquire an
      exclusive lock for duration of the next step only).
   *) After that we call handler::ha_prepare_inplace_alter_table() to give the
      storage engine a chance to update its internal structures with a higher
      lock level than the one that will be used for the main step of algorithm.
      After that we downgrade the lock if it is necessary.
   *) After that, the main step of this phase and algorithm is executed.
      We call the handler::ha_inplace_alter_table() method, which carries out the
      changes requested by ALTER TABLE but does not makes them visible to other
      connections yet.
   *) We ensure that no other connection uses the table by upgrading our
      lock on it to exclusive.
   *) a) If the previous step succeeds, handler::ha_commit_inplace_alter_table() is
         called to allow the storage engine to do any final updates to its structures,
         to make all earlier changes durable and visible to other connections.
      b) If we have failed to upgrade lock or any errors have occured during the
         handler functions calls (including commit), we call
         handler::ha_commit_inplace_alter_table()
         to rollback all changes which were done during previous steps.

  Phase 3 : Final
  ===============

  In this phase we:

  *) Update SQL-layer data-dictionary by installing .FRM file for the new version
     of the table.
  *) Inform the storage engine about this change by calling the
     handler::ha_notify_table_changed() method.
  *) Destroy the Alter_inplace_info and handler_ctx objects.

 */

 /**
    Check if a storage engine supports a particular alter table in-place

    @param    altered_table     TABLE object for new version of table.
    @param    ha_alter_info     Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.

    @retval   HA_ALTER_ERROR                  Unexpected error.
    @retval   HA_ALTER_INPLACE_NOT_SUPPORTED  Not supported, must use copy.
    @retval   HA_ALTER_INPLACE_EXCLUSIVE_LOCK Supported, but requires X lock.
    @retval   HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE
                                              Supported, but requires SNW lock
                                              during main phase. Prepare phase
                                              requires X lock.
    @retval   HA_ALTER_INPLACE_SHARED_LOCK    Supported, but requires SNW lock.
    @retval   HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE
                                              Supported, concurrent reads/writes
                                              allowed. However, prepare phase
                                              requires X lock.
    @retval   HA_ALTER_INPLACE_NO_LOCK        Supported, concurrent
                                              reads/writes allowed.

    @note The default implementation uses the old in-place ALTER API
    to determine if the storage engine supports in-place ALTER or not.

    @note Called without holding thr_lock.c lock.
 */
 virtual enum_alter_inplace_result
 check_if_supported_inplace_alter(TABLE *altered_table,
                                  Alter_inplace_info *ha_alter_info);


 /**
    Public functions wrapping the actual handler call.
    @see prepare_inplace_alter_table()
 */
 bool ha_prepare_inplace_alter_table(TABLE *altered_table,
                                     Alter_inplace_info *ha_alter_info);


 /**
    Public function wrapping the actual handler call.
    @see inplace_alter_table()
 */
 bool ha_inplace_alter_table(TABLE *altered_table,
                             Alter_inplace_info *ha_alter_info)
 {
   return inplace_alter_table(altered_table, ha_alter_info);
 }


 /**
    Public function wrapping the actual handler call.
    Allows us to enforce asserts regardless of handler implementation.
    @see commit_inplace_alter_table()
 */
 bool ha_commit_inplace_alter_table(TABLE *altered_table,
                                    Alter_inplace_info *ha_alter_info,
                                    bool commit);


 /**
    Public function wrapping the actual handler call.
    @see notify_table_changed()
 */
 void ha_notify_table_changed()
 {
   notify_table_changed();
 }


protected:
 /**
    Allows the storage engine to update internal structures with concurrent
    writes blocked. If check_if_supported_inplace_alter() returns
    HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE or
    HA_ALTER_INPLACE_SHARED_AFTER_PREPARE, this function is called with
    exclusive lock otherwise the same level of locking as for
    inplace_alter_table() will be used.

    @note Storage engines are responsible for reporting any errors by
    calling my_error()/print_error()

    @note If this function reports error, commit_inplace_alter_table()
    will be called with commit= false.

    @note For partitioning, failing to prepare one partition, means that
    commit_inplace_alter_table() will be called to roll back changes for
    all partitions. This means that commit_inplace_alter_table() might be
    called without prepare_inplace_alter_table() having been called first
    for a given partition.

    @param    altered_table     TABLE object for new version of table.
    @param    ha_alter_info     Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.

    @retval   true              Error
    @retval   false             Success
 */
 virtual bool prepare_inplace_alter_table(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info)
 { return false; }


 /**
    Alter the table structure in-place with operations specified using HA_ALTER_FLAGS
    and Alter_inplace_info. The level of concurrency allowed during this
    operation depends on the return value from check_if_supported_inplace_alter().

    @note Storage engines are responsible for reporting any errors by
    calling my_error()/print_error()

    @note If this function reports error, commit_inplace_alter_table()
    will be called with commit= false.

    @param    altered_table     TABLE object for new version of table.
    @param    ha_alter_info     Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.

    @retval   true              Error
    @retval   false             Success
 */
 virtual bool inplace_alter_table(TABLE *altered_table,
                                  Alter_inplace_info *ha_alter_info)
 { return false; }


 /**
    Commit or rollback the changes made during prepare_inplace_alter_table()
    and inplace_alter_table() inside the storage engine.
    Note that in case of rollback the allowed level of concurrency during
    this operation will be the same as for inplace_alter_table() and thus
    might be higher than during prepare_inplace_alter_table(). (For example,
    concurrent writes were blocked during prepare, but might not be during
    rollback).

    @note Storage engines are responsible for reporting any errors by
    calling my_error()/print_error()

    @note If this function with commit= true reports error, it will be called
    again with commit= false.

    @note In case of partitioning, this function might be called for rollback
    without prepare_inplace_alter_table() having been called first.
    Also partitioned tables sets ha_alter_info->group_commit_ctx to a NULL
    terminated array of the partitions handlers and if all of them are
    committed as one, then group_commit_ctx should be set to NULL to indicate
    to the partitioning handler that all partitions handlers are committed.
    @see prepare_inplace_alter_table().

    @param    altered_table     TABLE object for new version of table.
    @param    ha_alter_info     Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.
    @param    commit            True => Commit, False => Rollback.

    @retval   true              Error
    @retval   false             Success
 */
 virtual bool commit_inplace_alter_table(TABLE *altered_table,
                                         Alter_inplace_info *ha_alter_info,
                                         bool commit)
{
  /* Nothing to commit/rollback, mark all handlers committed! */
  ha_alter_info->group_commit_ctx= NULL;
  return false;
}


 /**
    Notify the storage engine that the table structure (.FRM) has been updated.

    @note No errors are allowed during notify_table_changed().
 */
 virtual void notify_table_changed();

public:
 /* End of On-line/in-place ALTER TABLE interface. */


  /**
    use_hidden_primary_key() is called in case of an update/delete when
    (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
    but we don't have a primary key
  */
  virtual void use_hidden_primary_key();

protected:
  /* Service methods for use by storage engines. */
  void ha_statistic_increment(ulonglong SSV::*offset) const;
  THD *ha_thd(void) const;

  /**
    Acquire the instrumented table information from a table share.
    @param share a table share
    @return an instrumented table share, or NULL.
  */
  PSI_table_share *ha_table_share_psi(const TABLE_SHARE *share) const;

  /**
    Default rename_table() and delete_table() rename/delete files with a
    given name and extensions from bas_ext().

    These methods can be overridden, but their default implementation
    provide useful functionality.
  */
  virtual int rename_table(const char *from, const char *to);
  /**
    Delete a table in the engine. Called for base as well as temporary
    tables.
  */
  virtual int delete_table(const char *name);
private:
  /* Private helpers */
  void mark_trx_read_write();
  /*
    Low-level primitives for storage engines.  These should be
    overridden by the storage engine class. To call these methods, use
    the corresponding 'ha_*' method above.
  */

  virtual int open(const char *name, int mode, uint test_if_locked)=0;
  virtual int close(void)=0;
  virtual int index_init(uint idx, bool sorted) { active_index= idx; return 0; }
  virtual int index_end() { active_index= MAX_KEY; return 0; }
  /**
    rnd_init() can be called two times without rnd_end() in between
    (it only makes sense if scan=1).
    then the second call should prepare for the new table scan (e.g
    if rnd_init allocates the cursor, second call should position it
    to the start of the table, no need to deallocate and allocate it again
  */
  virtual int rnd_init(bool scan)= 0;
  virtual int rnd_end() { return 0; }
  /**
    Write a row.

    write_row() inserts a row. buf is a byte array of data, normally
    record[0].

    You can use the field information to extract the data from the native byte
    array type.

    Example of this would be:
    for (Field **field=table->field ; *field ; field++)
    {
      ...
    }

    @param buf  Buffer to write from.

    @return Operation status.
      @retval    0  Success.
      @retval != 0  Error code.
  */
  virtual int write_row(uchar *buf MY_ATTRIBUTE((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }

  /**
    Update a single row.

    Note: If HA_ERR_FOUND_DUPP_KEY is returned, the handler must read
    all columns of the row so MySQL can create an error message. If
    the columns required for the error message are not read, the error
    message will contain garbage.
  */
  virtual int update_row(const uchar *old_data MY_ATTRIBUTE((unused)),
                         uchar *new_data MY_ATTRIBUTE((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }

  virtual int delete_row(const uchar *buf MY_ATTRIBUTE((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }
  /**
    Reset state of file to after 'open'.
    This function is called after every statement for all tables used
    by that statement.
  */
  virtual int reset() { return 0; }
  virtual Table_flags table_flags(void) const= 0;
  /**
    Is not invoked for non-transactional temporary tables.

    Tells the storage engine that we intend to read or write data
    from the table. This call is prefixed with a call to handler::store_lock()
    and is invoked only for those handler instances that stored the lock.

    Calls to rnd_init/index_init are prefixed with this call. When table
    IO is complete, we call external_lock(F_UNLCK).
    A storage engine writer should expect that each call to
    ::external_lock(F_[RD|WR]LOCK is followed by a call to
    ::external_lock(F_UNLCK). If it is not, it is a bug in MySQL.

    The name and signature originate from the first implementation
    in MyISAM, which would call fcntl to set/clear an advisory
    lock on the data file in this method.

    @param   lock_type    F_RDLCK, F_WRLCK, F_UNLCK

    @return  non-0 in case of failure, 0 in case of success.
    When lock_type is F_UNLCK, the return value is ignored.
  */
  virtual int external_lock(THD *thd MY_ATTRIBUTE((unused)),
                            int lock_type MY_ATTRIBUTE((unused)))
  {
    return 0;
  }
  virtual void release_auto_increment() { return; };
  /** admin commands - called from mysql_admin_table */
  virtual int check_for_upgrade(HA_CHECK_OPT *check_opt)
  { return 0; }
  virtual int check(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }

  /**
     In this method check_opt can be modified
     to specify CHECK option to use to call check()
     upon the table.
  */
  virtual int repair(THD* thd, HA_CHECK_OPT* check_opt)
  {
    DBUG_ASSERT(!(ha_table_flags() & HA_CAN_REPAIR));
    return HA_ADMIN_NOT_IMPLEMENTED;
  }
  virtual void start_bulk_insert(ha_rows rows) {}
  virtual int end_bulk_insert() { return 0; }
protected:
  virtual int index_read(uchar * buf, const uchar * key, uint key_len,
                         enum ha_rkey_function find_flag)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_read_last(uchar * buf, const uchar * key, uint key_len)
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    return HA_ERR_WRONG_COMMAND;
  }
public:
  /**
    This method is similar to update_row, however the handler doesn't need
    to execute the updates at this point in time. The handler can be certain
    that another call to bulk_update_row will occur OR a call to
    exec_bulk_update before the set of updates in this query is concluded.

    Note: If HA_ERR_FOUND_DUPP_KEY is returned, the handler must read
    all columns of the row so MySQL can create an error message. If
    the columns required for the error message are not read, the error
    message will contain garbage.

    @param    old_data       Old record
    @param    new_data       New record
    @param    dup_key_found  Number of duplicate keys found

  */
  virtual int bulk_update_row(const uchar *old_data, uchar *new_data,
                              uint *dup_key_found)
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
  /**
    This is called to delete all rows in a table
    If the handler don't support this, then this function will
    return HA_ERR_WRONG_COMMAND and MySQL will delete the rows one
    by one.
  */
  virtual int delete_all_rows()
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    return HA_ERR_WRONG_COMMAND;
  }
  /**
    Quickly remove all rows from a table.

    @remark This method is responsible for implementing MySQL's TRUNCATE
            TABLE statement, which is a DDL operation. As such, a engine
            can bypass certain integrity checks and in some cases avoid
            fine-grained locking (e.g. row locks) which would normally be
            required for a DELETE statement.

    @remark Typically, truncate is not used if it can result in integrity
            violation. For example, truncate is not used when a foreign
            key references the table, but it might be used if foreign key
            checks are disabled.

    @remark Engine is responsible for resetting the auto-increment counter.

    @remark The table is locked in exclusive mode.
  */
  virtual int truncate()
  { return HA_ERR_WRONG_COMMAND; }
  virtual int optimize(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual bool check_and_repair(THD *thd) { return TRUE; }
  virtual int disable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int enable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int discard_or_import_tablespace(my_bool discard)
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    return HA_ERR_WRONG_COMMAND;
  }
  virtual void drop_table(const char *name);
  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info)=0;

  virtual int create_handler_files(const char *name, const char *old_name,
                                   int action_flag, HA_CREATE_INFO *info)
  { return FALSE; }

  virtual bool set_ha_share_ref(Handler_share **arg_ha_share)
  {
    DBUG_ASSERT(!ha_share);
    DBUG_ASSERT(arg_ha_share);
    if (ha_share || !arg_ha_share)
      return true;
    ha_share= arg_ha_share;
    return false;
  }
  int get_lock_type() const { return m_lock_type; }

  /**
    Callback function that will be called by my_prepare_gcolumn_template
    once the table has been opened.
  */
  typedef void (*my_gcolumn_template_callback_t)(const TABLE*, void*);
  static bool my_prepare_gcolumn_template(THD *thd,
                                          const char *db_name,
                                          const char *table_name,
                                          my_gcolumn_template_callback_t myc,
                                          void *ib_table);
  static bool my_eval_gcolumn_expr_with_open(THD *thd,
                                             const char *db_name,
                                             const char *table_name,
                                             const MY_BITMAP *const fields,
                                             uchar *record);

  /**
   Callback for computing generated column values.

   Storage engines that need to have virtual column values for a row
   can use this function to get the values computed. The storage
   engine must have filled in the values for the base columns that
   the virutal columns depend on.

   @param  thd	        thread handle
   @param  table	table object
   @param  fields	bitmap of field index of evaluated generated
			column
   @param  record	buff of base columns generated column depends.
			After calling this function, it will be
			used to return the value of the generated
			columns.

   @retval true in case of error
   @retval false on success.
  */
  static bool my_eval_gcolumn_expr(THD *thd, TABLE *table,
				   const MY_BITMAP *const fields,
                                   uchar *record);

  /* This must be implemented if the handlerton's partition_flags() is set. */
  virtual Partition_handler *get_partition_handler()
  { return NULL; }

protected:
  Handler_share *get_ha_share_ptr();
  void set_ha_share_ptr(Handler_share *arg_ha_share);
  void lock_shared_ha_data();
  void unlock_shared_ha_data();
};


bool key_uses_partial_cols(TABLE *table, uint keyno);

/*
  A Disk-Sweep MRR interface implementation

  This implementation makes range (and, in the future, 'ref') scans to read
  table rows in disk sweeps. 
  
  Currently it is used by MyISAM and InnoDB. Potentially it can be used with
  any table handler that has non-clustered indexes and on-disk rows.
*/

class DsMrr_impl
{
public:
  typedef void (handler::*range_check_toggle_func_t)(bool on);

  DsMrr_impl() : h2(NULL) {}

  ~DsMrr_impl()
  {
    /*
      If ha_reset() has not been called then the h2 dialog might still
      exist. This must be closed and deleted (this is the case for
      internally created temporary tables).
    */
    if (h2)
      reset();
    DBUG_ASSERT(h2 == NULL);
  }
  
  /*
    The "owner" handler object (the one that calls dsmrr_XXX functions.
    It is used to retrieve full table rows by calling rnd_pos().
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */
private:
  /* Secondary handler object.  It is used for scanning the index */
  handler *h2;

  /* Buffer to store rowids, or (rowid, range_id) pairs */
  uchar *rowids_buf;
  uchar *rowids_buf_cur;   /* Current position when reading/writing */
  uchar *rowids_buf_last;  /* When reading: end of used buffer space */
  uchar *rowids_buf_end;   /* End of the buffer */

  bool dsmrr_eof; /* TRUE <=> We have reached EOF when reading index tuples */

  /* TRUE <=> need range association, buffer holds {rowid, range_id} pairs */
  bool is_mrr_assoc;

  bool use_default_impl; /* TRUE <=> shortcut all calls to default MRR impl */
public:
  /**
    Initialize the DsMrr_impl object.

    This object is used for both doing default MRR scans and DS-MRR scans.
    This function just initializes the object. To do a DS-MRR scan,
    this must also be initialized by calling dsmrr_init().

    @param h_arg     pointer to the handler that owns this object
    @param table_arg pointer to the TABLE that owns the handler
  */

  void init(handler *h_arg, TABLE *table_arg)
  {
    DBUG_ASSERT(h_arg != NULL);
    DBUG_ASSERT(table_arg != NULL);
    h= h_arg; 
    table= table_arg;
  }

  int dsmrr_init(handler *h, RANGE_SEQ_IF *seq_funcs, void *seq_init_param, 
                 uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  void dsmrr_close();

  /**
    Resets the DS-MRR object to the state it had after being intialized.

    If there is an open scan then this will be closed.
    
    This function should be called by handler::ha_reset() which is called
    when a statement is completed in order to make the handler object ready
    for re-use by a different statement.
  */

  void reset();
  int dsmrr_fill_buffer();
  int dsmrr_next(char **range_info);

  ha_rows dsmrr_info(uint keyno, uint n_ranges, uint keys, uint *bufsz,
                     uint *flags, Cost_estimate *cost);

  ha_rows dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq, 
                            void *seq_init_param, uint n_ranges, uint *bufsz,
                            uint *flags, Cost_estimate *cost);
private:
  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       Cost_estimate *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, Cost_estimate *cost);
};
	/* Some extern variables used with handlers */

extern const char *ha_row_type[];
extern MYSQL_PLUGIN_IMPORT const char *tx_isolation_names[];
extern MYSQL_PLUGIN_IMPORT const char *binlog_format_names[];
extern TYPELIB tx_isolation_typelib;
extern const char *myisam_stats_method_names[];
extern ulong total_ha, total_ha_2pc;

/* lookups */
handlerton *ha_default_handlerton(THD *thd);
handlerton *ha_default_temp_handlerton(THD *thd);
/**
  Resolve handlerton plugin by name, without checking for "DEFAULT" or
  HTON_NOT_USER_SELECTABLE.

  @param thd  Thread context.
  @param name Plugin name.

  @return plugin or NULL if not found.
*/
plugin_ref ha_resolve_by_name_raw(THD *thd, const LEX_CSTRING &name);
plugin_ref ha_resolve_by_name(THD *thd, const LEX_STRING *name,
                              bool is_temp_table);
plugin_ref ha_lock_engine(THD *thd, const handlerton *hton);
handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type);
handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         handlerton *db_type);
handlerton *ha_checktype(THD *thd, enum legacy_db_type database_type,
                          bool no_substitute, bool report_error);


static inline enum legacy_db_type ha_legacy_type(const handlerton *db_type)
{
  return (db_type == NULL) ? DB_TYPE_UNKNOWN : db_type->db_type;
}

const char *ha_resolve_storage_engine_name(const handlerton *db_type);

static inline bool ha_check_storage_engine_flag(const handlerton *db_type, uint32 flag)
{
  return db_type == NULL ? FALSE : MY_TEST(db_type->flags & flag);
}

static inline bool ha_storage_engine_is_enabled(const handlerton *db_type)
{
  return (db_type && db_type->create) ?
         (db_type->state == SHOW_OPTION_YES) : FALSE;
}

static inline bool is_ha_partition_handlerton(const handlerton *db_type)
{
  return (db_type->db_type == DB_TYPE_PARTITION_DB);
}

/* basic stuff */
int ha_init_errors(void);
int ha_init(void);
void ha_end();
int ha_initialize_handlerton(st_plugin_int *plugin);
int ha_finalize_handlerton(st_plugin_int *plugin);

TYPELIB* ha_known_exts();
int ha_panic(enum ha_panic_function flag);
void ha_close_connection(THD* thd);
void ha_kill_connection(THD *thd);
/**
  Flush the log(s) of storage engine(s).

  @param hton Handlerton of storage engine.
  @param binlog_group_flush true if we got invoked by binlog group
  commit during flush stage, false in other cases.
  @retval false Succeed
  @retval true Error
*/
bool ha_flush_logs(handlerton *db_type, bool binlog_group_flush= false);
void ha_drop_database(char* path);
int ha_create_table(THD *thd, const char *path,
                    const char *db, const char *table_name,
                    HA_CREATE_INFO *create_info,
		                bool update_create_info,
                    bool is_temp_table= false);

int ha_delete_table(THD *thd, handlerton *db_type, const char *path,
                    const char *db, const char *alias, bool generate_warning);

/* statistics and info */
bool ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat);

/* discovery */
int ha_create_table_from_engine(THD* thd, const char *db, const char *name);
bool ha_check_if_table_exists(THD* thd, const char *db, const char *name,
                             bool *exists);
int ha_discover(THD* thd, const char* dbname, const char* name,
                uchar** frmblob, size_t* frmlen);
int ha_find_files(THD *thd,const char *db,const char *path,
                  const char *wild, bool dir, List<LEX_STRING>* files);
int ha_table_exists_in_engine(THD* thd, const char* db, const char* name);
bool ha_check_if_supported_system_table(handlerton *hton, const char* db, 
                                        const char* table_name);

/* key cache */
extern "C" int ha_init_key_cache(const char *name, KEY_CACHE *key_cache);
int ha_resize_key_cache(KEY_CACHE *key_cache);
int ha_change_key_cache(KEY_CACHE *old_key_cache, KEY_CACHE *new_key_cache);

/* report to InnoDB that control passes to the client */
int ha_release_temporary_latches(THD *thd);

/* transactions: interface to handlerton functions */
int ha_start_consistent_snapshot(THD *thd);
int ha_commit_trans(THD *thd, bool all, bool ignore_global_read_lock= false);
int ha_commit_attachable(THD *thd);
int ha_rollback_trans(THD *thd, bool all);
int ha_prepare(THD *thd);


/**
  recover() step of xa.

  @note
    there are three modes of operation:
    - automatic recover after a crash
    in this case commit_list != 0, tc_heuristic_recover==TC_HEURISTIC_NOT_USED
    all xids from commit_list are committed, others are rolled back
    - manual (heuristic) recover
    in this case commit_list==0, tc_heuristic_recover != TC_HEURISTIC_NOT_USED
    DBA has explicitly specified that all prepared transactions should
    be committed (or rolled back).
    - no recovery (MySQL did not detect a crash)
    in this case commit_list==0, tc_heuristic_recover == TC_HEURISTIC_NOT_USED
    there should be no prepared transactions in this case.
*/

int ha_recover(HASH *commit_list);

/*
 transactions: interface to low-level handlerton functions. These are
 intended to be used by the transaction coordinators to
 commit/prepare/rollback transactions in the engines.
*/
int ha_commit_low(THD *thd, bool all, bool run_after_commit= true);
int ha_prepare_low(THD *thd, bool all);
int ha_rollback_low(THD *thd, bool all);

/* transactions: these functions never call handlerton functions directly */
int ha_enable_transaction(THD *thd, bool on);

/* savepoints */
int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv);
bool ha_rollback_to_savepoint_can_release_mdl(THD *thd);
int ha_savepoint(THD *thd, SAVEPOINT *sv);
int ha_release_savepoint(THD *thd, SAVEPOINT *sv);

/* Build pushed joins in handlers implementing this feature */
int ha_make_pushed_joins(THD *thd, const AQP::Join_plan* plan);

/* these are called by storage engines */
void trans_register_ha(THD *thd, bool all, handlerton *ht,
                       const ulonglong *trxid);
/*
  Storage engine has to assume the transaction will end up with 2pc if
   - there is more than one 2pc-capable storage engine available
   - in the current transaction 2pc was not disabled yet
*/
#define trans_need_2pc(thd, all)                   ((total_ha_2pc > 1) && \
        !((all ? &thd->transaction.all : &thd->transaction.stmt)->no_2pc))

int ha_reset_logs(THD *thd);
int ha_binlog_index_purge_file(THD *thd, const char *file);
void ha_reset_slave(THD *thd);
void ha_binlog_log_query(THD *thd, handlerton *db_type,
                         enum_binlog_command binlog_command,
                         const char *query, size_t query_length,
                         const char *db, const char *table_name);
void ha_binlog_wait(THD *thd);

/* It is required by basic binlog features on both MySQL server and libmysqld */
int ha_binlog_end(THD *thd);

const char *ha_legacy_type_name(legacy_db_type legacy_type);
const char *get_canonical_filename(handler *file, const char *path,
                                   char *tmp_path);

inline const char *table_case_name(HA_CREATE_INFO *info, const char *name)
{
  return ((lower_case_table_names == 2 && info->alias) ? info->alias : name);
}

void print_keydup_error(TABLE *table, KEY *key, const char *msg, myf errflag);
void print_keydup_error(TABLE *table, KEY *key, myf errflag);

void ha_set_normalized_disabled_se_str(const std::string &disabled_se_str);
bool ha_is_storage_engine_disabled(handlerton *se_engine);

bool ha_notify_exclusive_mdl(THD *thd, const MDL_key *mdl_key,
                             ha_notification_type notification_type,
                             bool *victimized);
bool ha_notify_alter_table(THD *thd, const MDL_key *mdl_key,
                           ha_notification_type notification_type);

int commit_owned_gtids(THD *thd, bool all, bool *need_clear_ptr);
int commit_owned_gtid_by_partial_command(THD *thd);
bool set_tx_isolation(THD *thd,
                      enum_tx_isolation tx_isolation,
                      bool one_shot);

#endif /* HANDLER_INCLUDED */
