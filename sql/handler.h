/* Copyright (C) 2000,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Definitions for parameters to do with handler-routines */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <ft_global.h>
#include <keycache.h>

#ifndef NO_HASH
#define NO_HASH				/* Not yet implemented */
#endif

#define USING_TRANSACTIONS

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

/* Bits in table_flags() to show what database can do */

#define HA_NO_TRANSACTIONS     (1 << 0) /* Doesn't support transactions */
#define HA_PARTIAL_COLUMN_READ (1 << 1) /* read may not return all columns */
#define HA_TABLE_SCAN_ON_INDEX (1 << 2) /* No separate data/index file */
/*
  The following should be set if the following is not true when scanning
  a table with rnd_next()
  - We will see all rows (including deleted ones)
  - Row positions are 'table->s->db_record_offset' apart
  If this flag is not set, filesort will do a postion() call for each matched
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
#define HA_DUPLICATE_POS       (1 << 8)    /* ha_position() gives dup row */
#define HA_NO_BLOBS            (1 << 9) /* Doesn't support blobs */
#define HA_CAN_INDEX_BLOBS     (1 << 10)
#define HA_AUTO_PART_KEY       (1 << 11) /* auto-increment in multi-part key */
#define HA_REQUIRE_PRIMARY_KEY (1 << 12) /* .. and can't create a hidden one */
#define HA_STATS_RECORDS_IS_EXACT (1 << 13) /* stats.records is exact */
/*
  INSERT_DELAYED only works with handlers that uses MySQL internal table
  level locks
*/
#define HA_CAN_INSERT_DELAYED  (1 << 14)
/*
  If we get the primary key columns for free when we do an index read
  It also implies that we have to retrive the primary key when using
  position() and rnd_pos().
*/
#define HA_PRIMARY_KEY_IN_READ_INDEX (1 << 15)
/*
  If HA_PRIMARY_KEY_REQUIRED_FOR_POSITION is set, it means that to position()
  uses a primary key. Without primary key, we can't call position().
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
#define HA_NEED_READ_RANGE_BUFFER (1 << 29) /* for read_multi_range */
#define HA_ANY_INDEX_MAY_BE_UNIQUE (1 << 30)
#define HA_NO_COPY_ON_ALTER    (LL(1) << 31)
#define HA_HAS_RECORDS	       (LL(1) << 32) /* records() gives exact count*/
/* Has it's own method of binlog logging */
#define HA_HAS_OWN_BINLOGGING  (LL(1) << 33)

/* bits in index_flags(index_number) for what you can do with index */
#define HA_READ_NEXT            1       /* TODO really use this flag */
#define HA_READ_PREV            2       /* supports ::index_prev */
#define HA_READ_ORDER           4       /* index_next/prev follow sort order */
#define HA_READ_RANGE           8       /* can find all records in a range */
#define HA_ONLY_WHOLE_INDEX	16	/* Can't use part key searches */
#define HA_KEYREAD_ONLY         64	/* Support HA_EXTRA_KEYREAD */

/*
  bits in alter_table_flags:
*/
/*
  These bits are set if different kinds of indexes can be created
  off-line without re-create of the table (but with a table lock).
*/
#define HA_ONLINE_ADD_INDEX_NO_WRITES           (1L << 0) /*add index w/lock*/
#define HA_ONLINE_DROP_INDEX_NO_WRITES          (1L << 1) /*drop index w/lock*/
#define HA_ONLINE_ADD_UNIQUE_INDEX_NO_WRITES    (1L << 2) /*add unique w/lock*/
#define HA_ONLINE_DROP_UNIQUE_INDEX_NO_WRITES   (1L << 3) /*drop uniq. w/lock*/
#define HA_ONLINE_ADD_PK_INDEX_NO_WRITES        (1L << 4) /*add prim. w/lock*/
#define HA_ONLINE_DROP_PK_INDEX_NO_WRITES       (1L << 5) /*drop prim. w/lock*/
/*
  These are set if different kinds of indexes can be created on-line
  (without a table lock). If a handler is capable of one or more of
  these, it should also set the corresponding *_NO_WRITES bit(s).
*/
#define HA_ONLINE_ADD_INDEX                     (1L << 6) /*add index online*/
#define HA_ONLINE_DROP_INDEX                    (1L << 7) /*drop index online*/
#define HA_ONLINE_ADD_UNIQUE_INDEX              (1L << 8) /*add unique online*/
#define HA_ONLINE_DROP_UNIQUE_INDEX             (1L << 9) /*drop uniq. online*/
#define HA_ONLINE_ADD_PK_INDEX                  (1L << 10)/*add prim. online*/
#define HA_ONLINE_DROP_PK_INDEX                 (1L << 11)/*drop prim. online*/
/*
  HA_PARTITION_FUNCTION_SUPPORTED indicates that the function is
  supported at all.
  HA_FAST_CHANGE_PARTITION means that optimised variants of the changes
  exists but they are not necessarily done online.

  HA_ONLINE_DOUBLE_WRITE means that the handler supports writing to both
  the new partition and to the old partitions when updating through the
  old partitioning schema while performing a change of the partitioning.
  This means that we can support updating of the table while performing
  the copy phase of the change. For no lock at all also a double write
  from new to old must exist and this is not required when this flag is
  set.
  This is actually removed even before it was introduced the first time.
  The new idea is that handlers will handle the lock level already in
  store_lock for ALTER TABLE partitions.

  HA_PARTITION_ONE_PHASE is a flag that can be set by handlers that take
  care of changing the partitions online and in one phase. Thus all phases
  needed to handle the change are implemented inside the storage engine.
  The storage engine must also support auto-discovery since the frm file
  is changed as part of the change and this change must be controlled by
  the storage engine. A typical engine to support this is NDB (through
  WL #2498).
*/
#define HA_PARTITION_FUNCTION_SUPPORTED         (1L << 12)
#define HA_FAST_CHANGE_PARTITION                (1L << 13)
#define HA_PARTITION_ONE_PHASE                  (1L << 14)

/*
  Index scan will not return records in rowid order. Not guaranteed to be
  set for unordered (e.g. HASH) indexes.
*/
#define HA_KEY_SCAN_NOT_ROR     128 

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
  Parameters for open() (in register form->filestat)
  HA_GET_INFO does an implicit HA_ABORT_IF_LOCKED
*/

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a ha_info() after open */
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
#define HA_OPTION_NO_CHECKSUM	(1L << 17)
#define HA_OPTION_NO_DELAY_KEY_WRITE (1L << 18)
#define HA_MAX_REC_LENGTH	65535

/* Table caching type */
#define HA_CACHE_TBL_NONTRANSACT 0
#define HA_CACHE_TBL_NOCACHE     1
#define HA_CACHE_TBL_ASKTRANSACT 2
#define HA_CACHE_TBL_TRANSACT    4

/* Options of START TRANSACTION statement (and later of SET TRANSACTION stmt) */
#define MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT 1

/* Flags for method is_fatal_error */
#define HA_CHECK_DUP_KEY 1
#define HA_CHECK_DUP_UNIQUE 2
#define HA_CHECK_DUP (HA_CHECK_DUP_KEY + HA_CHECK_DUP_UNIQUE)

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
  DB_TYPE_FIRST_DYNAMIC=32,
  DB_TYPE_DEFAULT=127 // Must be last
};

enum row_type { ROW_TYPE_NOT_USED=-1, ROW_TYPE_DEFAULT, ROW_TYPE_FIXED,
		ROW_TYPE_DYNAMIC, ROW_TYPE_COMPRESSED,
		ROW_TYPE_REDUNDANT, ROW_TYPE_COMPACT, ROW_TYPE_PAGES };

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
  LOGCOM_DROP_DB
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

typedef ulonglong my_xid; // this line is the same as in log_event.h
#define MYSQL_XID_PREFIX "MySQLXid"
#define MYSQL_XID_PREFIX_LEN 8 // must be a multiple of 8
#define MYSQL_XID_OFFSET (MYSQL_XID_PREFIX_LEN+sizeof(server_id))
#define MYSQL_XID_GTRID_LEN (MYSQL_XID_OFFSET+sizeof(my_xid))

#define XIDDATASIZE 128
#define MAXGTRIDSIZE 64
#define MAXBQUALSIZE 64

#define COMPATIBLE_DATA_YES 0
#define COMPATIBLE_DATA_NO  1

struct xid_t {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[XIDDATASIZE];  // not \0-terminated !

  xid_t() {}                                /* Remove gcc warning */  
  bool eq(struct xid_t *xid)
  { return eq(xid->gtrid_length, xid->bqual_length, xid->data); }
  bool eq(long g, long b, const char *d)
  { return g == gtrid_length && b == bqual_length && !memcmp(d, data, g+b); }
  void set(struct xid_t *xid)
  { memcpy(this, xid, xid->length()); }
  void set(long f, const char *g, long gl, const char *b, long bl)
  {
    formatID= f;
    memcpy(data, g, gtrid_length= gl);
    memcpy(data+gl, b, bqual_length= bl);
  }
  void set(ulonglong xid)
  {
    my_xid tmp;
    formatID= 1;
    set(MYSQL_XID_PREFIX_LEN, 0, MYSQL_XID_PREFIX);
    memcpy(data+MYSQL_XID_PREFIX_LEN, &server_id, sizeof(server_id));
    tmp= xid;
    memcpy(data+MYSQL_XID_OFFSET, &tmp, sizeof(tmp));
    gtrid_length=MYSQL_XID_GTRID_LEN;
  }
  void set(long g, long b, const char *d)
  {
    formatID= 1;
    gtrid_length= g;
    bqual_length= b;
    memcpy(data, d, g+b);
  }
  bool is_null() { return formatID == -1; }
  void null() { formatID= -1; }
  my_xid quick_get_my_xid()
  {
    my_xid tmp;
    memcpy(&tmp, data+MYSQL_XID_OFFSET, sizeof(tmp));
    return tmp;
  }
  my_xid get_my_xid()
  {
    return gtrid_length == MYSQL_XID_GTRID_LEN && bqual_length == 0 &&
           !memcmp(data+MYSQL_XID_PREFIX_LEN, &server_id, sizeof(server_id)) &&
           !memcmp(data, MYSQL_XID_PREFIX, MYSQL_XID_PREFIX_LEN) ?
           quick_get_my_xid() : 0;
  }
  uint length()
  {
    return sizeof(formatID)+sizeof(gtrid_length)+sizeof(bqual_length)+
           gtrid_length+bqual_length;
  }
  byte *key()
  {
    return (byte *)&gtrid_length;
  }
  uint key_length()
  {
    return sizeof(gtrid_length)+sizeof(bqual_length)+gtrid_length+bqual_length;
  }
};
typedef struct xid_t XID;

/* for recover() handlerton call */
#define MIN_XID_LIST_SIZE  128
#ifdef SAFEMALLOC
#define MAX_XID_LIST_SIZE  256
#else
#define MAX_XID_LIST_SIZE  (1024*128)
#endif

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
  uint nodegroup_id;
  const handlerton *storage_engine;
  bool wait_until_completed;
  const char *ts_comment;
  enum tablespace_access_mode ts_access_mode;
  st_alter_tablespace()
  {
    tablespace_name= NULL;
    logfile_group_name= "DEFAULT_LG"; //Default log file group
    ts_cmd_type= TS_CMD_NOT_DEFINED;
    data_file_name= NULL;
    undo_file_name= NULL;
    redo_file_name= NULL;
    extent_size= 1024*1024;        //Default 1 MByte
    undo_buffer_size= 8*1024*1024; //Default 8 MByte
    redo_buffer_size= 8*1024*1024; //Default 8 MByte
    initial_size= 128*1024*1024;   //Default 128 MByte
    autoextend_size= 0;            //No autoextension as default
    max_size= 0;                   //Max size == initial size => no extension
    storage_engine= NULL;
    nodegroup_id= UNDEF_NODEGROUP;
    wait_until_completed= TRUE;
    ts_comment= NULL;
    ts_access_mode= TS_NOT_DEFINED;
  }
};

/* The handler for a table type.  Will be included in the TABLE structure */

struct st_table;
typedef struct st_table TABLE;
typedef struct st_table_share TABLE_SHARE;
struct st_foreign_key_info;
typedef struct st_foreign_key_info FOREIGN_KEY_INFO;
typedef bool (stat_print_fn)(THD *thd, const char *type, uint type_len,
                             const char *file, uint file_len,
                             const char *status, uint status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };
extern st_plugin_int *hton2plugin[MAX_HA];

/* Transaction log maintains type definitions */
enum log_status
{
  HA_LOG_STATUS_FREE= 0,      /* log is free and can be deleted */
  HA_LOG_STATUS_INUSE= 1,     /* log can't be deleted because it is in use */
  HA_LOG_STATUS_NOSUCHLOG= 2  /* no such log (can't be returned by
                                the log iterator status) */
};
/*
  Function for signaling that the log file changed its state from
  LOG_STATUS_INUSE to LOG_STATUS_FREE

  Now it do nothing, will be implemented as part of new transaction
  log management for engines.
  TODO: implement the function.
*/
void signal_log_not_needed(struct handlerton, char *log_file);
/*
  Data of transaction log iterator.
*/
struct handler_log_file_data {
  LEX_STRING filename;
  enum log_status status;
};


enum handler_iterator_type
{
  /* request of transaction log iterator */
  HA_TRANSACTLOG_ITERATOR= 1
};
enum handler_create_iterator_result
{
  HA_ITERATOR_OK,          /* iterator created */
  HA_ITERATOR_UNSUPPORTED, /* such type of iterator is not supported */
  HA_ITERATOR_ERROR        /* error during iterator creation */
};

/*
  Iterator structure. Can be used by handler/handlerton for different purposes.

  Iterator should be created in the way to point "before" the first object
  it iterate, so next() call move it to the first object or return !=0 if
  there is nothing to iterate through.
*/
struct handler_iterator {
  /*
    Moves iterator to next record and return 0 or return !=0
    if there is no records.
    iterator_object will be filled by this function if next() returns 0.
    Content of the iterator_object depend on iterator type.
  */
  int (*next)(struct handler_iterator *, void *iterator_object);
  /*
    Free resources allocated by iterator, after this call iterator
    is not usable.
  */
  void (*destroy)(struct handler_iterator *);
  /*
    Pointer to buffer for the iterator to use.
    Should be allocated by function which created the iterator and
    destroied by freed by above "destroy" call
  */
  void *buffer;
};

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
   int  (*close_connection)(THD *thd);
   /*
     sv points to an uninitialized storage area of requested size
     (see savepoint_offset description)
   */
   int  (*savepoint_set)(THD *thd, void *sv);
   /*
     sv points to a storage area, that was earlier passed
     to the savepoint_set call
   */
   int  (*savepoint_rollback)(THD *thd, void *sv);
   int  (*savepoint_release)(THD *thd, void *sv);
   /*
     'all' is true if it's a real commit, that makes persistent changes
     'all' is false if it's not in fact a commit but an end of the
     statement that is part of the transaction.
     NOTE 'all' is also false in auto-commit mode where 'end of statement'
     and 'real commit' mean the same event.
   */
   int  (*commit)(THD *thd, bool all);
   int  (*rollback)(THD *thd, bool all);
   int  (*prepare)(THD *thd, bool all);
   int  (*recover)(XID *xid_list, uint len);
   int  (*commit_by_xid)(XID *xid);
   int  (*rollback_by_xid)(XID *xid);
   void *(*create_cursor_read_view)();
   void (*set_cursor_read_view)(void *);
   void (*close_cursor_read_view)(void *);
   handler *(*create)(TABLE_SHARE *table, MEM_ROOT *mem_root);
   void (*drop_database)(char* path);
   int (*panic)(enum ha_panic_function flag);
   int (*start_consistent_snapshot)(THD *thd);
   bool (*flush_logs)();
   bool (*show_status)(THD *thd, stat_print_fn *print, enum ha_stat_type stat);
   uint (*partition_flags)();
   uint (*alter_table_flags)(uint flags);
   int (*alter_tablespace)(THD *thd, st_alter_tablespace *ts_info);
   int (*fill_files_table)(THD *thd,
                           struct st_table_list *tables,
                           class Item *cond);
   uint32 flags;                                /* global handler flags */
   /*
      Those handlerton functions below are properly initialized at handler
      init.
   */
   int (*binlog_func)(THD *thd, enum_binlog_func fn, void *arg);
   void (*binlog_log_query)(THD *thd, enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name);
   int (*release_temporary_latches)(THD *thd);

   /*
     Get log status.
     If log_status is null then the handler do not support transaction
     log information (i.e. log iterator can't be created).
     (see example of implementation in handler.cc, TRANS_LOG_MGM_EXAMPLE_CODE)

   */
   enum log_status (*get_log_status)(char *log);

   /*
     Iterators creator.
     Presence of the pointer should be checked before using
   */
   enum handler_create_iterator_result
     (*create_iterator)(enum handler_iterator_type type,
                        struct handler_iterator *fill_this_in);
};


/* Possible flags of a handlerton */
#define HTON_NO_FLAGS                 0
#define HTON_CLOSE_CURSORS_AT_COMMIT (1 << 0)
#define HTON_ALTER_NOT_SUPPORTED     (1 << 1) //Engine does not support alter
#define HTON_CAN_RECREATE            (1 << 2) //Delete all is used fro truncate
#define HTON_HIDDEN                  (1 << 3) //Engine does not appear in lists
#define HTON_FLUSH_AFTER_RENAME      (1 << 4)
#define HTON_NOT_USER_SELECTABLE     (1 << 5)
#define HTON_TEMPORARY_NOT_SUPPORTED (1 << 6) //Having temporary tables not supported
#define HTON_ALTER_CANNOT_CREATE     (1 << 7) //Cannot use alter to create

typedef struct st_thd_trans
{
  /* number of entries in the ht[] */
  uint        nht;
  /* true is not all entries in the ht[] support 2pc */
  bool        no_2pc;
  /* storage engines that registered themselves for this transaction */
  handlerton *ht[MAX_HA];
} THD_TRANS;

enum enum_tx_isolation { ISO_READ_UNCOMMITTED, ISO_READ_COMMITTED,
			 ISO_REPEATABLE_READ, ISO_SERIALIZABLE};


enum ndb_distribution { ND_KEYHASH= 0, ND_LINHASH= 1 };


typedef struct {
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong delete_length;
  ha_rows records;
  ulong mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  ulonglong check_sum;
} PARTITION_INFO;

#define UNDEF_NODEGROUP 65535
class Item;
struct st_table_log_memory_entry;

class partition_info;

struct st_partition_iter;
#define NOT_A_PARTITION_ID ((uint32)-1)


typedef struct st_ha_create_information
{
  CHARSET_INFO *table_charset, *default_table_charset;
  LEX_STRING connect_string;
  const char *comment,*password, *tablespace;
  const char *data_file_name, *index_file_name;
  const char *alias;
  ulonglong max_rows,min_rows;
  ulonglong auto_increment_value;
  ulong table_options;
  ulong avg_row_length;
  ulong used_fields;
  ulong key_block_size;
  SQL_LIST merge_list;
  handlerton *db_type;
  enum row_type row_type;
  uint null_bits;                       /* NULL bits at start of record */
  uint options;				/* OR of HA_CREATE_ options */
  uint merge_insert_method;
  uint extra_size;                      /* length of extra data segment */
  bool table_existed;			/* 1 in create if table existed */
  bool frm_only;                        /* 1 if no ha_create_table() */
  bool varchar;                         /* 1 if table has a VARCHAR */
  bool store_on_disk;                   /* 1 if table stored on disk */
} HA_CREATE_INFO;


typedef struct st_key_create_information
{
  enum ha_key_alg algorithm;
  ulong block_size;
  LEX_STRING parser_name;
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
  inline void prelock(TABLE **tables, uint count)
  {
    do_prelock(tables, count);
  }
  virtual ~TABLEOP_HOOKS() {}
  TABLEOP_HOOKS() {}

private:
  /* Function primitive that is called prior to locking tables */
  virtual void do_prelock(TABLE **tables, uint count)
  {
    /* Default is to do nothing */
  }
};

typedef struct st_savepoint SAVEPOINT;
extern ulong savepoint_alloc_size;
extern KEY_CREATE_INFO default_key_create_info;

/* Forward declaration for condition pushdown to storage engine */
typedef class Item COND;

typedef struct st_ha_check_opt
{
  st_ha_check_opt() {}                        /* Remove gcc warning */
  ulong sort_buffer_size;
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
  const byte *buffer;         /* Buffer one can start using */
  const byte *buffer_end;     /* End of buffer */
  byte *end_of_used_area;     /* End of area that was used by handler */
} HANDLER_BUFFER;

typedef struct system_status_var SSV;

class ha_statistics
{
public:
  ulonglong data_file_length;		/* Length off data file */
  ulonglong max_data_file_length;	/* Length off data file */
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;		/* Free bytes */
  ulonglong auto_increment_value;
  ha_rows records;			/* Estimated records in table */
  ha_rows deleted;			/* Deleted records */
  ulong mean_rec_length;		/* physical reclength */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  uint block_size;			/* index block size */

  ha_statistics():
    data_file_length(0), max_data_file_length(0),
    index_file_length(0), delete_length(0), auto_increment_value(0),
    records(0), deleted(0), mean_rec_length(0), create_time(0),
    check_time(0), update_time(0), block_size(0)
  {}
};

/*
  The handler class is the interface for dynamically loadable
  storage engines. Do not add ifdefs and take care when adding or
  changing virtual functions to avoid vtable confusion
 */

class handler :public Sql_alloc
{
  friend class ha_partition;

 protected:
  struct st_table_share *table_share;   /* The table definition */
  struct st_table *table;               /* The current open table */
  ulonglong cached_table_flags;         /* Set on init() and open() */

  virtual int index_init(uint idx, bool sorted) { active_index=idx; return 0; }
  virtual int index_end() { active_index=MAX_KEY; return 0; }
  /*
    rnd_init() can be called two times without rnd_end() in between
    (it only makes sense if scan=1).
    then the second call should prepare for the new table scan (e.g
    if rnd_init allocates the cursor, second call should position it
    to the start of the table, no need to deallocate and allocate it again
  */
  virtual int rnd_init(bool scan) =0;
  virtual int rnd_end() { return 0; }
  virtual ulonglong table_flags(void) const =0;
  void ha_statistic_increment(ulong SSV::*offset) const;

  ha_rows estimation_rows_to_insert;
  virtual void start_bulk_insert(ha_rows rows) {}
  virtual int end_bulk_insert() {return 0; }
public:
  const handlerton *ht;                 /* storage engine of this handler */
  byte *ref;				/* Pointer to current row */
  byte *dup_ref;			/* Pointer to duplicate row */

  ha_statistics stats;

  /* The following are for read_multi_range */
  bool multi_range_sorted;
  KEY_MULTI_RANGE *multi_range_curr;
  KEY_MULTI_RANGE *multi_range_end;
  HANDLER_BUFFER *multi_range_buffer;

  /* The following are for read_range() */
  key_range save_end_range, *end_range;
  KEY_PART_INFO *range_key_part;
  int key_compare_result_on_equal;
  bool eq_range;

  uint errkey;				/* Last dup key */
  uint key_used_on_scan;
  uint active_index;
  /* Length of ref (1-8 or the clustered key length) */
  uint ref_length;
  FT_INFO *ft_handler;
  enum {NONE=0, INDEX, RND} inited;
  bool implicit_emptied;                /* Can be !=0 only if HEAP */
  const COND *pushed_cond;
  /*
    next_insert_id is the next value which should be inserted into the
    auto_increment column: in a inserting-multi-row statement (like INSERT
    SELECT), for the first row where the autoinc value is not specified by the
    statement, get_auto_increment() called and asked to generate a value,
    next_insert_id is set to the next value, then for all other rows
    next_insert_id is used (and increased each time) without calling
    get_auto_increment().
  */
  ulonglong next_insert_id;
  /*
    insert id for the current row (*autogenerated*; if not
    autogenerated, it's 0).
    At first successful insertion, this variable is stored into
    THD::first_successful_insert_id_in_cur_stmt.
  */
  ulonglong insert_id_for_cur_row;
  /*
    Interval returned by get_auto_increment() and being consumed by the
    inserter.
  */
  Discrete_interval auto_inc_interval_for_cur_row;

  handler(const handlerton *ht_arg, TABLE_SHARE *share_arg)
    :table_share(share_arg), estimation_rows_to_insert(0), ht(ht_arg),
    ref(0), key_used_on_scan(MAX_KEY), active_index(MAX_KEY),
    ref_length(sizeof(my_off_t)),
    ft_handler(0), inited(NONE), implicit_emptied(0),
    pushed_cond(NULL), next_insert_id(0), insert_id_for_cur_row(0)
    {}
  virtual ~handler(void)
  {
    /* TODO: DBUG_ASSERT(inited == NONE); */
  }
  /* This is called after create to allow us to set up cached variables */
  void init()
  {
    cached_table_flags= table_flags();
  }
  /*
    Check whether a handler allows to lock the table.

    SYNOPSIS
      check_if_locking_is_allowed()
        thd     Handler of the thread, trying to lock the table
        table   Table handler to check
        count   Number of locks already granted to the table

    DESCRIPTION
      Check whether a handler allows to lock the table. For instance,
      MyISAM does not allow to lock mysql.proc along with other tables.
      This limitation stems from the fact that MyISAM does not support
      row-level locking and we have to add this limitation to avoid
      deadlocks.

    RETURN
      TRUE      Locking is allowed
      FALSE     Locking is not allowed. The error was thrown.
  */
  virtual bool check_if_locking_is_allowed(uint sql_command,
                                           ulong type, TABLE *table,
                                           uint count,
                                           bool called_by_logger_thread)
  {
    return TRUE;
  }
  bool check_if_log_table_locking_is_allowed(uint sql_command,
                                             ulong type, TABLE *table);
  int ha_open(TABLE *table, const char *name, int mode, int test_if_locked);
  void adjust_next_insert_id_after_explicit_value(ulonglong nr);
  bool update_auto_increment();
  void print_keydup_error(uint key_nr, const char *msg);
  virtual void print_error(int error, myf errflag);
  virtual bool get_error_message(int error, String *buf);
  uint get_dup_key(int error);
  void change_table_ptr(TABLE *table_arg, TABLE_SHARE *share)
  {
    table= table_arg;
    table_share= share;
  }
  virtual double scan_time()
  { return ulonglong2double(stats.data_file_length) / IO_SIZE + 2; }
  virtual double read_time(uint index, uint ranges, ha_rows rows)
  { return rows2double(ranges+rows); }
  virtual const key_map *keys_to_use_for_scanning() { return &key_map_empty; }
  bool has_transactions()
  { return (ha_table_flags() & HA_NO_TRANSACTIONS) == 0; }
  virtual uint extra_rec_buf_length() const { return 0; }

  /*
    This method is used to analyse the error to see whether the error
    is ignorable or not, certain handlers can have more error that are
    ignorable than others. E.g. the partition handler can get inserts
    into a range where there is no partition and this is an ignorable
    error.
    HA_ERR_FOUND_DUP_UNIQUE is a special case in MyISAM that means the
    same thing as HA_ERR_FOUND_DUP_KEY but can in some cases lead to
    a slightly different error message.
  */
  virtual bool is_fatal_error(int error, uint flags)
  {
    if (!error ||
        ((flags & HA_CHECK_DUP_KEY) &&
         (error == HA_ERR_FOUND_DUPP_KEY ||
          error == HA_ERR_FOUND_DUPP_UNIQUE)))
      return FALSE;
    return TRUE;
  }

  /*
    Number of rows in table. It will only be called if
    (table_flags() & (HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT)) != 0
  */
  virtual ha_rows records() { return stats.records; }
  /*
    Return upper bound of current number of records in the table
    (max. of how many records one will retrieve when doing a full table scan)
    If upper bound is not known, HA_POS_ERROR should be returned as a max
    possible upper bound.
  */
  virtual ha_rows estimate_rows_upper_bound()
  { return stats.records+EXTRA_RECORDS; }

  /*
    Get the row type from the storage engine.  If this method returns
    ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
  */
  virtual enum row_type get_row_type() const { return ROW_TYPE_NOT_USED; }

  virtual const char *index_type(uint key_number) { DBUG_ASSERT(0); return "";}

  int ha_index_init(uint idx, bool sorted)
  {
    DBUG_ENTER("ha_index_init");
    DBUG_ASSERT(inited==NONE);
    inited=INDEX;
    DBUG_RETURN(index_init(idx, sorted));
  }
  int ha_index_end()
  {
    DBUG_ENTER("ha_index_end");
    DBUG_ASSERT(inited==INDEX);
    inited=NONE;
    DBUG_RETURN(index_end());
  }
  int ha_rnd_init(bool scan)
  {
    DBUG_ENTER("ha_rnd_init");
    DBUG_ASSERT(inited==NONE || (inited==RND && scan));
    inited=RND;
    DBUG_RETURN(rnd_init(scan));
  }
  int ha_rnd_end()
  {
    DBUG_ENTER("ha_rnd_end");
    DBUG_ASSERT(inited==RND);
    inited=NONE;
    DBUG_RETURN(rnd_end());
  }
  int ha_reset();

  /* this is necessary in many places, e.g. in HANDLER command */
  int ha_index_or_rnd_end()
  {
    return inited == INDEX ? ha_index_end() : inited == RND ? ha_rnd_end() : 0;
  }
  longlong ha_table_flags() { return cached_table_flags; }

  /*
    Signal that the table->read_set and table->write_set table maps changed
    The handler is allowed to set additional bits in the above map in this
    call. Normally the handler should ignore all calls until we have done
    a ha_rnd_init() or ha_index_init(), write_row(), update_row or delete_row()
    as there may be several calls to this routine.
  */
  virtual void column_bitmaps_signal();
  uint get_index(void) const { return active_index; }
  virtual int open(const char *name, int mode, uint test_if_locked)=0;
  virtual int close(void)=0;

  /*
    These functions represent the public interface to *users* of the
    handler class, hence they are *not* virtual. For the inheritance
    interface, see the (private) functions write_row(), update_row(),
    and delete_row() below.
   */
  int ha_external_lock(THD *thd, int lock_type);
  int ha_write_row(byte * buf);
  int ha_update_row(const byte * old_data, byte * new_data);
  int ha_delete_row(const byte * buf);

  /*
    SYNOPSIS
      start_bulk_update()
    RETURN
      0   Bulk update used by handler
      1   Bulk update not used, normal operation used
  */
  virtual bool start_bulk_update() { return 1; }
  /*
    SYNOPSIS
      start_bulk_delete()
    RETURN
      0   Bulk delete used by handler
      1   Bulk delete not used, normal operation used
  */
  virtual bool start_bulk_delete() { return 1; }
  /*
    SYNOPSIS
    This method is similar to update_row, however the handler doesn't need
    to execute the updates at this point in time. The handler can be certain
    that another call to bulk_update_row will occur OR a call to
    exec_bulk_update before the set of updates in this query is concluded.

      bulk_update_row()
        old_data       Old record
        new_data       New record
        dup_key_found  Number of duplicate keys found
    RETURN
      0   Bulk delete used by handler
      1   Bulk delete not used, normal operation used
  */
  virtual int bulk_update_row(const byte *old_data, byte *new_data,
                              uint *dup_key_found)
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
  /*
    SYNOPSIS
    After this call all outstanding updates must be performed. The number
    of duplicate key errors are reported in the duplicate key parameter.
    It is allowed to continue to the batched update after this call, the
    handler has to wait until end_bulk_update with changing state.

      exec_bulk_update()
        dup_key_found       Number of duplicate keys found
    RETURN
      0           Success
      >0          Error code
  */
  virtual int exec_bulk_update(uint *dup_key_found)
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
  /*
    SYNOPSIS
    Perform any needed clean-up, no outstanding updates are there at the
    moment.

      end_bulk_update()
    RETURN
      Nothing
  */
  virtual void end_bulk_update() { return; }
  /*
    SYNOPSIS
    Execute all outstanding deletes and close down the bulk delete.

      end_bulk_delete()
    RETURN
    0             Success
    >0            Error code
  */
  virtual int end_bulk_delete()
  {
    DBUG_ASSERT(FALSE);
    return HA_ERR_WRONG_COMMAND;
  }
  virtual int index_read(byte * buf, const byte * key,
			 uint key_len, enum ha_rkey_function find_flag)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_read_idx(byte * buf, uint index, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag);
  virtual int index_next(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_prev(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_first(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_last(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_next_same(byte *buf, const byte *key, uint keylen);
  virtual int index_read_last(byte * buf, const byte * key, uint key_len)
   { return (my_errno=HA_ERR_WRONG_COMMAND); }
  virtual int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                     KEY_MULTI_RANGE *ranges, uint range_count,
                                     bool sorted, HANDLER_BUFFER *buffer);
  virtual int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  virtual int read_range_next();
  int compare_key(key_range *range);
  virtual int ft_init() { return HA_ERR_WRONG_COMMAND; }
  void ft_end() { ft_handler=NULL; }
  virtual FT_INFO *ft_init_ext(uint flags, uint inx,String *key)
    { return NULL; }
  virtual int ft_read(byte *buf) { return HA_ERR_WRONG_COMMAND; }
  virtual int rnd_next(byte *buf)=0;
  virtual int rnd_pos(byte * buf, byte *pos)=0;
  virtual int read_first_row(byte *buf, uint primary_key);
  /*
    The following function is only needed for tables that may be temporary
    tables during joins
  */
  virtual int restart_rnd_next(byte *buf, byte *pos)
    { return HA_ERR_WRONG_COMMAND; }
  virtual int rnd_same(byte *buf, uint inx)
    { return HA_ERR_WRONG_COMMAND; }
  virtual ha_rows records_in_range(uint inx, key_range *min_key,
                                   key_range *max_key)
    { return (ha_rows) 10; }
  virtual void position(const byte *record)=0;
  virtual void info(uint)=0; // see my_base.h for full description
  virtual void get_dynamic_partition_info(PARTITION_INFO *stat_info,
                                          uint part_id);
  virtual int extra(enum ha_extra_function operation)
  { return 0; }
  virtual int extra_opt(enum ha_extra_function operation, ulong cache_size)
  { return extra(operation); }

  /*
    Reset state of file to after 'open'
    This function is called after every statement for all tables used
    by that statement.
  */
  virtual int reset() { return 0; }
  /*
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
  /*
    Tell the engine whether it should avoid unnecessary lock waits.
    If yes, in an UPDATE or DELETE, if the row under the cursor was locked
    by another transaction, the engine may try an optimistic read of
    the last committed row value under the cursor.
  */
  virtual void try_semi_consistent_read(bool) {}
  virtual void unlock_row() {}
  virtual int start_stmt(THD *thd, thr_lock_type lock_type) {return 0;}
  /*
    This is called to delete all rows in a table
    If the handler don't support this, then this function will
    return HA_ERR_WRONG_COMMAND and MySQL will delete the rows one
    by one.
  */
  virtual int delete_all_rows()
  { return (my_errno=HA_ERR_WRONG_COMMAND); }
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
private:
  virtual void release_auto_increment() { return; };
public:
  void ha_release_auto_increment();
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
  /*
    Reset the auto-increment counter to the given value, i.e. the next row
    inserted will get the given value. This is called e.g. after TRUNCATE
    is emulated by doing a 'DELETE FROM t'. HA_ERR_WRONG_COMMAND is
    returned by storage engines that don't support this operation.
  */
  virtual int reset_auto_increment(ulonglong value)
  { return HA_ERR_WRONG_COMMAND; }

  virtual void update_create_info(HA_CREATE_INFO *create_info) {}
protected:
  /* to be implemented in handlers */

  /* admin commands - called from mysql_admin_table */
  virtual int check(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }

  /*
     in these two methods check_opt can be modified
     to specify CHECK option to use to call check()
     upon the table
  */
  virtual int check_for_upgrade(HA_CHECK_OPT *check_opt)
  { return 0; }
public:
  int ha_check_for_upgrade(HA_CHECK_OPT *check_opt);
  int check_old_types();
  /* to be actually called to get 'check()' functionality*/
  int ha_check(THD *thd, HA_CHECK_OPT *check_opt);
   
  virtual int backup(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  /*
    restore assumes .frm file must exist, and that generate_table() has been
    called; It will just copy the data file and run repair.
  */
  virtual int restore(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
protected:
  virtual int repair(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
public:
  int ha_repair(THD* thd, HA_CHECK_OPT* check_opt);
  virtual int optimize(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int preload_keys(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  /* end of the list of admin commands */

  virtual bool check_and_repair(THD *thd) { return HA_ERR_WRONG_COMMAND; }
  virtual int dump(THD* thd, int fd = -1) { return HA_ERR_WRONG_COMMAND; }
  virtual int disable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int enable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int indexes_are_disabled(void) {return 0;}
  void ha_start_bulk_insert(ha_rows rows)
  {
    estimation_rows_to_insert= rows;
    start_bulk_insert(rows);
  }
  int ha_end_bulk_insert()
  {
    estimation_rows_to_insert= 0;
    return end_bulk_insert();
  }
  virtual int discard_or_import_tablespace(my_bool discard)
  {return HA_ERR_WRONG_COMMAND;}
  virtual int net_read_dump(NET* net) { return HA_ERR_WRONG_COMMAND; }
  virtual char *update_table_comment(const char * comment)
  { return (char*) comment;}
  virtual void append_create_info(String *packet) {}
  /*
    SYNOPSIS
      is_fk_defined_on_table_or_index()
      index            Index to check if foreign key uses it
    RETURN VALUE
       TRUE            Foreign key defined on table or index
       FALSE           No foreign key defined
    DESCRIPTION
      If index == MAX_KEY then a check for table is made and if index <
      MAX_KEY then a check is made if the table has foreign keys and if
      a foreign key uses this index (and thus the index cannot be dropped).
  */
  virtual bool is_fk_defined_on_table_or_index(uint index)
  { return FALSE; }
  virtual char* get_foreign_key_create_info()
  { return(NULL);}  /* gets foreign key create string from InnoDB */
  virtual char* get_tablespace_name(THD *thd)
  { return(NULL);}  /* gets tablespace name from handler */
  /* used in ALTER TABLE; 1 if changing storage engine is allowed */
  virtual bool can_switch_engines() { return 1; }
  /* used in REPLACE; is > 0 if table is referred by a FOREIGN KEY */
  virtual int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
  { return 0; }
  virtual uint referenced_by_foreign_key() { return 0;}
  virtual void init_table_handle_for_HANDLER()
  { return; }       /* prepare InnoDB for HANDLER */
  virtual void free_foreign_key_create_info(char* str) {}
  /* The following can be called without an open handler */
  virtual const char *table_type() const =0;
  virtual const char **bas_ext() const =0;

  virtual int get_default_no_partitions(HA_CREATE_INFO *info) { return 1;}
  virtual void set_auto_partitions(partition_info *part_info) { return; }
  virtual bool get_no_parts(const char *name,
                            uint *no_parts)
  {
    *no_parts= 0;
    return 0;
  }
  virtual void set_part_info(partition_info *part_info) {return;}

  virtual ulong index_flags(uint idx, uint part, bool all_parts) const =0;

  virtual void prepare_for_alter() { return; }
  virtual int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys)
  { return (HA_ERR_WRONG_COMMAND); }
  virtual int prepare_drop_index(TABLE *table_arg, uint *key_num,
                                 uint num_of_keys)
  { return (HA_ERR_WRONG_COMMAND); }
  virtual int final_drop_index(TABLE *table_arg)
  { return (HA_ERR_WRONG_COMMAND); }

  uint max_record_length() const
  { return min(HA_MAX_REC_LENGTH, max_supported_record_length()); }
  uint max_keys() const
  { return min(MAX_KEY, max_supported_keys()); }
  uint max_key_parts() const
  { return min(MAX_REF_PARTS, max_supported_key_parts()); }
  uint max_key_length() const
  { return min(MAX_KEY_LENGTH, max_supported_key_length()); }
  uint max_key_part_length() const
  { return min(MAX_KEY_LENGTH, max_supported_key_part_length()); }

  virtual uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  virtual uint max_supported_keys() const { return 0; }
  virtual uint max_supported_key_parts() const { return MAX_REF_PARTS; }
  virtual uint max_supported_key_length() const { return MAX_KEY_LENGTH; }
  virtual uint max_supported_key_part_length() const { return 255; }
  virtual uint min_record_length(uint options) const { return 1; }

  virtual bool low_byte_first() const { return 1; }
  virtual uint checksum() const { return 0; }
  virtual bool is_crashed() const  { return 0; }
  virtual bool auto_repair() const { return 0; }

  /*
    default rename_table() and delete_table() rename/delete files with a
    given name and extensions from bas_ext()
  */
  virtual int rename_table(const char *from, const char *to);
  virtual int delete_table(const char *name);
  virtual void drop_table(const char *name);
  
  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info)=0;

#define CHF_CREATE_FLAG 0
#define CHF_DELETE_FLAG 1
#define CHF_RENAME_FLAG 2
#define CHF_INDEX_FLAG  3

  virtual int create_handler_files(const char *name, const char *old_name,
                                   int action_flag, HA_CREATE_INFO *info)
  { return FALSE; }

  virtual int change_partitions(HA_CREATE_INFO *create_info,
                                const char *path,
                                ulonglong *copied,
                                ulonglong *deleted,
                                const void *pack_frm_data,
                                uint pack_frm_len)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int drop_partitions(const char *path)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int rename_partitions(const char *path)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int optimize_partitions(THD *thd)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int analyze_partitions(THD *thd)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int check_partitions(THD *thd)
  { return HA_ERR_WRONG_COMMAND; }
  virtual int repair_partitions(THD *thd)
  { return HA_ERR_WRONG_COMMAND; }

  /* lock_count() can be more than one if the table is a MERGE */
  virtual uint lock_count(void) const { return 1; }
  virtual THR_LOCK_DATA **store_lock(THD *thd,
				     THR_LOCK_DATA **to,
				     enum thr_lock_type lock_type)=0;

  /* Type of table for caching query */
  virtual uint8 table_cache_type() { return HA_CACHE_TBL_NONTRANSACT; }
  /* ask handler about permission to cache table when query is to be cached */
  virtual my_bool register_query_cache_table(THD *thd, char *table_key,
					     uint key_length,
					     qc_engine_callback 
					     *engine_callback,
					     ulonglong *engine_data)
  {
    *engine_callback= 0;
    return 1;
  }
 /*
  RETURN
    true  Primary key (if there is one) is clustered key covering all fields
    false otherwise
 */
 virtual bool primary_key_is_clustered() { return FALSE; }
 virtual int cmp_ref(const byte *ref1, const byte *ref2)
 {
   return memcmp(ref1, ref2, ref_length);
 }
 
 /*
   Condition pushdown to storage engines
 */

 /*
   Push condition down to the table handler.
   SYNOPSIS
     cond_push()
     cond   Condition to be pushed. The condition tree must not be            
     modified by the by the caller.

   RETURN
     The 'remainder' condition that caller must use to filter out records.
     NULL means the handler will not return rows that do not match the
     passed condition.

   NOTES
   The pushed conditions form a stack (from which one can remove the
   last pushed condition using cond_pop).
   The table handler filters out rows using (pushed_cond1 AND pushed_cond2 
   AND ... AND pushed_condN)
   or less restrictive condition, depending on handler's capabilities.
   
   handler->ha_reset() call empties the condition stack.
   Calls to rnd_init/rnd_end, index_init/index_end etc do not affect the
   condition stack.
 */ 
 virtual const COND *cond_push(const COND *cond) { return cond; };
 /*
   Pop the top condition from the condition stack of the handler instance.
   SYNOPSIS
     cond_pop()
     Pops the top if condition stack, if stack is not empty
 */
 virtual void cond_pop() { return; };
 virtual bool check_if_incompatible_data(HA_CREATE_INFO *create_info,
					 uint table_changes)
 { return COMPATIBLE_DATA_NO; }

 /* These are only called from sql_select for internal temporary tables */
  virtual int write_row(byte *buf __attribute__((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }

  virtual int update_row(const byte *old_data __attribute__((unused)),
                         byte *new_data __attribute__((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }

  virtual int delete_row(const byte *buf __attribute__((unused)))
  {
    return HA_ERR_WRONG_COMMAND;
  }
  /*
    use_hidden_primary_key() is called in case of an update/delete when
    (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
    but we don't have a primary key
  */
  virtual void use_hidden_primary_key();

private:
  /*
    Row-level primitives for storage engines.  These should be
    overridden by the storage engine class. To call these methods, use
    the corresponding 'ha_*' method above.
  */
  virtual int external_lock(THD *thd __attribute__((unused)),
                            int lock_type __attribute__((unused)))
  {
    return 0;
  }
};

	/* Some extern variables used with handlers */

extern handlerton *sys_table_types[];
extern const char *ha_row_type[];
extern TYPELIB tx_isolation_typelib;
extern TYPELIB myisam_stats_method_typelib;
extern ulong total_ha, total_ha_2pc;

	/* Wrapper functions */
#define ha_commit_stmt(thd) (ha_commit_trans((thd), FALSE))
#define ha_rollback_stmt(thd) (ha_rollback_trans((thd), FALSE))
#define ha_commit(thd) (ha_commit_trans((thd), TRUE))
#define ha_rollback(thd) (ha_rollback_trans((thd), TRUE))

/* lookups */
handlerton *ha_default_handlerton(THD *thd);
handlerton *ha_resolve_by_name(THD *thd, const LEX_STRING *name);
handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type);
const char *ha_get_storage_engine(enum legacy_db_type db_type);
handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         handlerton *db_type);
handlerton *ha_checktype(THD *thd, enum legacy_db_type database_type,
                          bool no_substitute, bool report_error);


static inline enum legacy_db_type ha_legacy_type(const handlerton *db_type)
{
  return (db_type == NULL) ? DB_TYPE_UNKNOWN : db_type->db_type;
}

static inline const char *ha_resolve_storage_engine_name(const handlerton *db_type)
{
  return db_type == NULL ? "UNKNOWN" : hton2plugin[db_type->slot]->name.str;
}

static inline bool ha_check_storage_engine_flag(const handlerton *db_type, uint32 flag)
{
  return db_type == NULL ? FALSE : test(db_type->flags & flag);
}

static inline bool ha_storage_engine_is_enabled(const handlerton *db_type)
{
  return (db_type && db_type->create) ?
         (db_type->state == SHOW_OPTION_YES) : FALSE;
}

/* basic stuff */
int ha_init(void);
int ha_initialize_handlerton(st_plugin_int *plugin);
int ha_finalize_handlerton(st_plugin_int *plugin);

TYPELIB *ha_known_exts(void);
int ha_panic(enum ha_panic_function flag);
void ha_close_connection(THD* thd);
bool ha_flush_logs(handlerton *db_type);
void ha_drop_database(char* path);
int ha_create_table(THD *thd, const char *path,
                    const char *db, const char *table_name,
                    HA_CREATE_INFO *create_info,
		    bool update_create_info);
int ha_delete_table(THD *thd, handlerton *db_type, const char *path,
                    const char *db, const char *alias, bool generate_warning);

/* statistics and info */
bool ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat);

/* discovery */
int ha_create_table_from_engine(THD* thd, const char *db, const char *name);
int ha_discover(THD* thd, const char* dbname, const char* name,
                const void** frmblob, uint* frmlen);
int ha_find_files(THD *thd,const char *db,const char *path,
                  const char *wild, bool dir,List<char>* files);
int ha_table_exists_in_engine(THD* thd, const char* db, const char* name);

/* key cache */
int ha_init_key_cache(const char *name, KEY_CACHE *key_cache);
int ha_resize_key_cache(KEY_CACHE *key_cache);
int ha_change_key_cache_param(KEY_CACHE *key_cache);
int ha_change_key_cache(KEY_CACHE *old_key_cache, KEY_CACHE *new_key_cache);
int ha_end_key_cache(KEY_CACHE *key_cache);

/* report to InnoDB that control passes to the client */
int ha_release_temporary_latches(THD *thd);

/* transactions: interface to handlerton functions */
int ha_start_consistent_snapshot(THD *thd);
int ha_commit_or_rollback_by_xid(XID *xid, bool commit);
int ha_commit_one_phase(THD *thd, bool all);
int ha_rollback_trans(THD *thd, bool all);
int ha_prepare(THD *thd);
int ha_recover(HASH *commit_list);

/* transactions: these functions never call handlerton functions directly */
int ha_commit_trans(THD *thd, bool all);
int ha_autocommit_or_rollback(THD *thd, int error);
int ha_enable_transaction(THD *thd, bool on);

/* savepoints */
int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv);
int ha_savepoint(THD *thd, SAVEPOINT *sv);
int ha_release_savepoint(THD *thd, SAVEPOINT *sv);

/* these are called by storage engines */
void trans_register_ha(THD *thd, bool all, handlerton *ht);

/*
  Storage engine has to assume the transaction will end up with 2pc if
   - there is more than one 2pc-capable storage engine available
   - in the current transaction 2pc was not disabled yet
*/
#define trans_need_2pc(thd, all)                   ((total_ha_2pc > 1) && \
        !((all ? &thd->transaction.all : &thd->transaction.stmt)->no_2pc))

#ifdef HAVE_NDB_BINLOG
int ha_reset_logs(THD *thd);
int ha_binlog_index_purge_file(THD *thd, const char *file);
void ha_reset_slave(THD *thd);
void ha_binlog_log_query(THD *thd, const handlerton *db_type,
                         enum_binlog_command binlog_command,
                         const char *query, uint query_length,
                         const char *db, const char *table_name);
void ha_binlog_wait(THD *thd);
int ha_binlog_end(THD *thd);
#else
#define ha_reset_logs(a) do {} while (0)
#define ha_binlog_index_purge_file(a,b) do {} while (0)
#define ha_reset_slave(a) do {} while (0)
#define ha_binlog_log_query(a,b,c,d,e,f,g) do {} while (0)
#define ha_binlog_wait(a) do {} while (0)
#define ha_binlog_end(a)  do {} while (0)
#endif
