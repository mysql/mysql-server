/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_CLASS_INCLUDED
#define SQL_CLASS_INCLUDED

/* Classes in mysql */

#include "my_global.h"

#include "dur_prop.h"                     // durability_properties
#include "mysql/mysql_lex_string.h"       // LEX_STRING
#include "mysql_com.h"                    // Item_result
#include "mysql_com_server.h"             // NET_SERVER
#include "auth/sql_security_ctx.h"        // Security_context
#include "derror.h"                       // ER_THD
#include "discrete_interval.h"            // Discrete_interval
#include "handler.h"                      // KEY_CREATE_INFO
#include "opt_trace_context.h"            // Opt_trace_context
#include "protocol.h"                     // Protocol
#include "protocol_classic.h"             // Protocol_text
#include "rpl_context.h"                  // Rpl_thd_context
#include "rpl_gtid.h"                     // Gtid_specification
#include "session_tracker.h"              // Session_tracker
#include "sql_alloc.h"                    // Sql_alloc
#include "sql_digest_stream.h"            // sql_digest_state
#include "sql_lex.h"                      // keytype
#include "sql_locale.h"                   // MY_LOCALE
#include "sql_profile.h"                  // PROFILING
#include "sys_vars_resource_mgr.h"        // Session_sysvar_resource_manager
#include "transaction_info.h"             // Ha_trx_info

#include <pfs_stage_provider.h>
#include <mysql/psi/mysql_stage.h>

#include <pfs_statement_provider.h>
#include <mysql/psi/mysql_statement.h>

#include <memory>
#include "mysql/thread_type.h"

#include "violite.h"                       /* SSL_handle */

class Reprepare_observer;
class sp_cache;
class Rows_log_event;
struct st_thd_timer;
typedef struct st_log_info LOG_INFO;
typedef struct st_columndef MI_COLUMNDEF;
typedef struct st_mysql_lex_string LEX_STRING;
typedef struct user_conn USER_CONN;

/**
  The meat of thd_proc_info(THD*, char*), a macro that packs the last
  three calling-info parameters.
*/
extern "C"
const char *set_thd_proc_info(MYSQL_THD thd_arg, const char *info,
                              const char *calling_func,
                              const char *calling_file,
                              const unsigned int calling_line);

#define thd_proc_info(thd, msg) \
  set_thd_proc_info(thd, msg, __func__, __FILE__, __LINE__)

extern "C"
void set_thd_stage_info(void *thd,
                        const PSI_stage_info *new_stage,
                        PSI_stage_info *old_stage,
                        const char *calling_func,
                        const char *calling_file,
                        const unsigned int calling_line);
extern "C"
void thd_enter_cond(void *opaque_thd, mysql_cond_t *cond, mysql_mutex_t *mutex,
                    const PSI_stage_info *stage, PSI_stage_info *old_stage,
                    const char *src_function, const char *src_file,
                    int src_line);
extern "C"
void thd_exit_cond(void *opaque_thd, const PSI_stage_info *stage,
                   const char *src_function, const char *src_file,
                   int src_line);

#define THD_STAGE_INFO(thd, stage) \
  (thd)->enter_stage(& stage, NULL, __func__, __FILE__, __LINE__)

enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };
enum enum_rbr_exec_mode { RBR_EXEC_MODE_STRICT,
                          RBR_EXEC_MODE_IDEMPOTENT,
                          RBR_EXEC_MODE_LAST_BIT };
enum enum_transaction_write_set_hashing_algorithm { HASH_ALGORITHM_OFF= 0,
                                                    HASH_ALGORITHM_MURMUR32= 1,
                                                    HASH_ALGORITHM_XXHASH64= 2};
enum enum_slave_type_conversions { SLAVE_TYPE_CONVERSIONS_ALL_LOSSY,
                                   SLAVE_TYPE_CONVERSIONS_ALL_NON_LOSSY,
                                   SLAVE_TYPE_CONVERSIONS_ALL_UNSIGNED,
                                   SLAVE_TYPE_CONVERSIONS_ALL_SIGNED};
enum enum_slave_rows_search_algorithms { SLAVE_ROWS_TABLE_SCAN = (1U << 0),
                                         SLAVE_ROWS_INDEX_SCAN = (1U << 1),
                                         SLAVE_ROWS_HASH_SCAN  = (1U << 2)};
enum enum_binlog_row_image {
  /** PKE in the before image and changed columns in the after image */
  BINLOG_ROW_IMAGE_MINIMAL= 0,
  /** Whenever possible, before and after image contain all columns except blobs. */
  BINLOG_ROW_IMAGE_NOBLOB= 1,
  /** All columns in both before and after image. */
  BINLOG_ROW_IMAGE_FULL= 2
};

enum enum_session_track_gtids {
  OFF= 0,
  OWN_GTID= 1,
  ALL_GTIDS= 2
};

enum enum_binlog_format {
  BINLOG_FORMAT_MIXED= 0, ///< statement if safe, otherwise row - autodetected
  BINLOG_FORMAT_STMT=  1, ///< statement-based
  BINLOG_FORMAT_ROW=   2, ///< row-based
  BINLOG_FORMAT_UNSPEC=3  ///< thd_binlog_format() returns it when binlog is closed
};

/* Bits for different SQL modes modes (including ANSI mode) */
#define MODE_REAL_AS_FLOAT              1
#define MODE_PIPES_AS_CONCAT            2
#define MODE_ANSI_QUOTES                4
#define MODE_IGNORE_SPACE               8
#define MODE_NOT_USED                   16
#define MODE_ONLY_FULL_GROUP_BY         32
#define MODE_NO_UNSIGNED_SUBTRACTION    64
#define MODE_NO_DIR_IN_CREATE           128
#define MODE_POSTGRESQL                 256
#define MODE_ORACLE                     512
#define MODE_MSSQL                      1024
#define MODE_DB2                        2048
#define MODE_MAXDB                      4096
#define MODE_NO_KEY_OPTIONS             8192
#define MODE_NO_TABLE_OPTIONS           16384
#define MODE_NO_FIELD_OPTIONS           32768
#define MODE_MYSQL323                   65536L
#define MODE_MYSQL40                    (MODE_MYSQL323*2)
#define MODE_ANSI                       (MODE_MYSQL40*2)
#define MODE_NO_AUTO_VALUE_ON_ZERO      (MODE_ANSI*2)
#define MODE_NO_BACKSLASH_ESCAPES       (MODE_NO_AUTO_VALUE_ON_ZERO*2)
#define MODE_STRICT_TRANS_TABLES        (MODE_NO_BACKSLASH_ESCAPES*2)
#define MODE_STRICT_ALL_TABLES          (MODE_STRICT_TRANS_TABLES*2)
/*
 * NO_ZERO_DATE, NO_ZERO_IN_DATE and ERROR_FOR_DIVISION_BY_ZERO modes are
 * removed in 5.7 and their functionality is merged with STRICT MODE.
 * However, For backward compatibility during upgrade, these modes are kept
 * but they are not used. Setting these modes in 5.7 will give warning and
 * have no effect.
 */
#define MODE_NO_ZERO_IN_DATE            (MODE_STRICT_ALL_TABLES*2)
#define MODE_NO_ZERO_DATE               (MODE_NO_ZERO_IN_DATE*2)
#define MODE_INVALID_DATES              (MODE_NO_ZERO_DATE*2)
#define MODE_ERROR_FOR_DIVISION_BY_ZERO (MODE_INVALID_DATES*2)
#define MODE_TRADITIONAL                (MODE_ERROR_FOR_DIVISION_BY_ZERO*2)
#define MODE_NO_AUTO_CREATE_USER        (MODE_TRADITIONAL*2)
#define MODE_HIGH_NOT_PRECEDENCE        (MODE_NO_AUTO_CREATE_USER*2)
#define MODE_NO_ENGINE_SUBSTITUTION     (MODE_HIGH_NOT_PRECEDENCE*2)
#define MODE_PAD_CHAR_TO_FULL_LENGTH    (1ULL << 31)

/*
  Replication uses 8 bytes to store SQL_MODE in the binary log. The day you
  use strictly more than 64 bits by adding one more define above, you should
  contact the replication team because the replication code should then be
  updated (to store more bytes on disk).

  NOTE: When adding new SQL_MODE types, make sure to also add them to
  the scripts used for creating the MySQL system tables
  in scripts/mysql_system_tables.sql and scripts/mysql_system_tables_fix.sql

*/

extern char internal_table_name[2];
extern char empty_c_string[1];
extern LEX_STRING EMPTY_STR;
extern LEX_STRING NULL_STR;
extern LEX_CSTRING EMPTY_CSTR;
extern LEX_CSTRING NULL_CSTR;

extern "C" LEX_CSTRING thd_query_unsafe(MYSQL_THD thd);
extern "C" size_t thd_query_safe(MYSQL_THD thd, char *buf, size_t buflen);

/**
  @class CSET_STRING
  @brief Character set armed LEX_CSTRING
*/
class CSET_STRING
{
private:
  LEX_CSTRING string;
  const CHARSET_INFO *cs;
public:
  CSET_STRING() : cs(&my_charset_bin)
  {
    string.str= NULL;
    string.length= 0;
  }
  CSET_STRING(const char *str_arg, size_t length_arg, const CHARSET_INFO *cs_arg) :
  cs(cs_arg)
  {
    DBUG_ASSERT(cs_arg != NULL);
    string.str= str_arg;
    string.length= length_arg;
  }

  inline const char *str() const { return string.str; }
  inline size_t length() const { return string.length; }
  const CHARSET_INFO *charset() const { return cs; }
};


#define TC_LOG_PAGE_SIZE   8192
#define TC_LOG_MIN_SIZE    (3*TC_LOG_PAGE_SIZE)

typedef struct st_user_var_events
{
  user_var_entry *user_var_event;
  char *value;
  ulong length;
  Item_result type;
  uint charset_number;
  bool unsigned_flag;
} BINLOG_USER_VAR_EVENT;


class Key_part_spec :public Sql_alloc {
public:
  LEX_STRING field_name;
  uint length;
  Key_part_spec(const LEX_STRING &name, uint len)
    : field_name(name), length(len)
  {}
  Key_part_spec(const char *name, const size_t name_len, uint len)
    : length(len)
  { field_name.str= (char *)name; field_name.length= name_len; }
  bool operator==(const Key_part_spec& other) const;
  /**
    Construct a copy of this Key_part_spec. field_name is copied
    by-pointer as it is known to never change. At the same time
    'length' may be reset in mysql_prepare_create_table, and this
    is why we supply it with a copy.

    @return If out of memory, 0 is returned and an error is set in
    THD.
  */
  Key_part_spec *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Key_part_spec(*this); }
};


class Key :public Sql_alloc {
public:
  keytype type;
  KEY_CREATE_INFO key_create_info;
  List<Key_part_spec> columns;
  LEX_STRING name;
  bool generated;

  Key(keytype type_par, const LEX_STRING &name_arg,
      KEY_CREATE_INFO *key_info_arg,
      bool generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
  Key(keytype type_par, const char *name_arg, size_t name_len_arg,
      KEY_CREATE_INFO *key_info_arg, bool generated_arg,
      List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    generated(generated_arg)
  {
    name.str= (char *)name_arg;
    name.length= name_len_arg;
  }
  Key(const Key &rhs, MEM_ROOT *mem_root);
  virtual ~Key() {}
  /* Equality comparison of keys (ignoring name) */
  friend bool foreign_key_prefix(Key *a, Key *b);
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  virtual Key *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Key(*this, mem_root); }
};

class Table_ident;

class Foreign_key: public Key {
public:

  LEX_CSTRING ref_db;
  LEX_CSTRING ref_table;
  List<Key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  Foreign_key(const LEX_STRING &name_arg, List<Key_part_spec> &cols,
	      const LEX_CSTRING &ref_db_arg, const LEX_CSTRING &ref_table_arg,
              List<Key_part_spec> &ref_cols,
	      uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key(KEYTYPE_FOREIGN, name_arg, &default_key_create_info, 0, cols),
    ref_db(ref_db_arg), ref_table(ref_table_arg), ref_columns(ref_cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  {
    // We don't check for duplicate FKs.
    key_create_info.check_for_duplicate_indexes= false;
  }
  Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root);
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  virtual Key *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Foreign_key(*this, mem_root); }
  /* Used to validate foreign key options */
  bool validate(List<Create_field> &table_fields);
};

typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;


/**
  To be used for pool-of-threads (implemented differently on various OSs)
*/
class thd_scheduler
{
public:
  void *data;                  /* scheduler-specific data structure */

  thd_scheduler()
  : data(NULL)
  { }

  ~thd_scheduler() { }
};

/* Needed to get access to scheduler variables */
void* thd_get_scheduler_data(THD *thd);
void thd_set_scheduler_data(THD *thd, void *data);
PSI_thread* thd_get_psi(THD *thd);
void thd_set_psi(THD *thd, PSI_thread *psi);


/**
  the struct aggregates two paramenters that identify an event
  uniquely in scope of communication of a particular master and slave couple.
  I.e there can not be 2 events from the same staying connected master which
  have the same coordinates.
  @note
  Such identifier is not yet unique generally as the event originating master
  is resetable. Also the crashed master can be replaced with some other.
*/
typedef struct rpl_event_coordinates
{
  char * file_name; // binlog file name (directories stripped)
  my_off_t  pos;       // event's position in the binlog file
} LOG_POS_COORD;


class MY_LOCALE;

/**
  Query_cache_tls -- query cache thread local data.
*/

struct Query_cache_block;

struct Query_cache_tls
{
  /*
    'first_query_block' should be accessed only via query cache
    functions and methods to maintain proper locking.
  */
  Query_cache_block *first_query_block;
  void set_first_query_block(Query_cache_block *first_query_block_arg)
  {
    first_query_block= first_query_block_arg;
  }

  Query_cache_tls() :first_query_block(NULL) {}
};

class Query_result;
class Time_zone;

#define THD_SENTRY_MAGIC 0xfeedd1ff
#define THD_SENTRY_GONE  0xdeadbeef

#define THD_CHECK_SENTRY(thd) DBUG_ASSERT(thd->dbug_sentry == THD_SENTRY_MAGIC)

typedef ulonglong sql_mode_t;

typedef struct system_variables
{
  /*
    How dynamically allocated system variables are handled:

    The global_system_variables and max_system_variables are "authoritative"
    They both should have the same 'version' and 'size'.
    When attempting to access a dynamic variable, if the session version
    is out of date, then the session version is updated and realloced if
    neccessary and bytes copied from global to make up for missing data.
  */
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint dynamic_variables_head;    /* largest valid variable offset */
  uint dynamic_variables_size;    /* how many bytes are in use */
  LIST *dynamic_variables_allocs; /* memory hunks for PLUGIN_VAR_MEMALLOC */

  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  my_bool end_markers_in_json;
  /* A bitmap for switching optimizations on/off */
  ulonglong optimizer_switch;
  ulonglong optimizer_trace; ///< bitmap to tune optimizer tracing
  ulonglong optimizer_trace_features; ///< bitmap to select features to trace
  long      optimizer_trace_offset;
  long      optimizer_trace_limit;
  ulong     optimizer_trace_max_mem_size;
  sql_mode_t sql_mode; ///< which non-standard SQL behaviour should be enabled
  ulonglong option_bits; ///< OPTION_xxx constants, e.g. OPTION_PROFILING
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  uint  eq_range_index_dive_limit;
  ulong join_buff_size;
  ulong lock_wait_timeout;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_points_in_geometry;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong multi_range_count;
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong myisam_stats_method;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulonglong parser_max_mem_size;
  ulong range_optimizer_max_mem_size;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong div_precincrement;
  ulong sortbuff_size;
  ulong max_sp_recursion_depth;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong group_concat_max_len;

  ulong binlog_format; ///< binlog format for this thd (see enum_binlog_format)
  ulong rbr_exec_mode_options;
  my_bool binlog_direct_non_trans_update;
  ulong binlog_row_image;
  my_bool sql_log_bin;
  ulong transaction_write_set_extraction;
  ulong completion_type;
  ulong query_cache_type;
  ulong tx_isolation;
  ulong transaction_isolation;
  ulong updatable_views_with_limit;
  uint max_user_connections;
  ulong my_aes_mode;

  /**
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  my_thread_id pseudo_thread_id;
  /**
    Default transaction access mode. READ ONLY (true) or READ WRITE (false).
  */
  my_bool tx_read_only;
  my_bool transaction_read_only;
  my_bool low_priority_updates;
  my_bool new_mode;
  my_bool query_cache_wlock_invalidate;
  my_bool keep_files_on_create;

  my_bool old_alter_table;
  uint old_passwords;
  my_bool big_tables;

  plugin_ref table_plugin;
  plugin_ref temp_table_plugin;

  /* Only charset part of these variables is sensible */
  const CHARSET_INFO *character_set_filesystem;
  const CHARSET_INFO *character_set_client;
  const CHARSET_INFO *character_set_results;

  /* Both charset and collation parts of these variables are important */
  const CHARSET_INFO  *collation_server;
  const CHARSET_INFO  *collation_database;
  const CHARSET_INFO  *collation_connection;

  /* Error messages */
  MY_LOCALE *lc_messages;
  /* Locale Support */
  MY_LOCALE *lc_time_names;

  Time_zone *time_zone;
  /*
    TIMESTAMP fields are by default created with DEFAULT clauses
    implicitly without users request. This flag when set, disables
    implicit default values and expect users to provide explicit
    default clause. i.e., when set columns are defined as NULL,
    instead of NOT NULL by default.
  */
  my_bool explicit_defaults_for_timestamp;

  my_bool sysdate_is_now;
  my_bool binlog_rows_query_log_events;

  double long_query_time_double;

  my_bool pseudo_slave_mode;

  Gtid_specification gtid_next;
  Gtid_set_or_null gtid_next_list;
  ulong session_track_gtids;

  ulong max_execution_time;

  char *track_sysvars_ptr;
  my_bool session_track_schema;
  my_bool session_track_state_change;
  ulong   session_track_transaction_info;
  /**
    Used for the verbosity of SHOW CREATE TABLE. Currently used for displaying
    the row format in the output even if the table uses default row format.
  */
  my_bool show_create_table_verbosity;
  /**
    Compatibility option to mark the pre MySQL-5.6.4 temporals columns using
    the old format using comments for SHOW CREATE TABLE and in I_S.COLUMNS
    'COLUMN_TYPE' field.
  */
  my_bool show_old_temporals;
} SV;


/**
  Per thread status variables.
  Must be long/ulong up to last_system_status_var so that
  add_to_status/add_diff_to_status can work.
*/

typedef struct system_status_var
{
  /* IMPORTANT! See first_system_status_var definition below. */
  ulonglong created_tmp_disk_tables;
  ulonglong created_tmp_tables;
  ulonglong ha_commit_count;
  ulonglong ha_delete_count;
  ulonglong ha_read_first_count;
  ulonglong ha_read_last_count;
  ulonglong ha_read_key_count;
  ulonglong ha_read_next_count;
  ulonglong ha_read_prev_count;
  ulonglong ha_read_rnd_count;
  ulonglong ha_read_rnd_next_count;
  /*
    This number doesn't include calls to the default implementation and
    calls made by range access. The intent is to count only calls made by
    BatchedKeyAccess.
  */
  ulonglong ha_multi_range_read_init_count;
  ulonglong ha_rollback_count;
  ulonglong ha_update_count;
  ulonglong ha_write_count;
  ulonglong ha_prepare_count;
  ulonglong ha_discover_count;
  ulonglong ha_savepoint_count;
  ulonglong ha_savepoint_rollback_count;
  ulonglong ha_external_lock_count;
  ulonglong opened_tables;
  ulonglong opened_shares;
  ulonglong table_open_cache_hits;
  ulonglong table_open_cache_misses;
  ulonglong table_open_cache_overflows;
  ulonglong select_full_join_count;
  ulonglong select_full_range_join_count;
  ulonglong select_range_count;
  ulonglong select_range_check_count;
  ulonglong select_scan_count;
  ulonglong long_query_count;
  ulonglong filesort_merge_passes;
  ulonglong filesort_range_count;
  ulonglong filesort_rows;
  ulonglong filesort_scan_count;
  /* Prepared statements and binary protocol. */
  ulonglong com_stmt_prepare;
  ulonglong com_stmt_reprepare;
  ulonglong com_stmt_execute;
  ulonglong com_stmt_send_long_data;
  ulonglong com_stmt_fetch;
  ulonglong com_stmt_reset;
  ulonglong com_stmt_close;

  ulonglong bytes_received;
  ulonglong bytes_sent;

  ulonglong max_execution_time_exceeded;
  ulonglong max_execution_time_set;
  ulonglong max_execution_time_set_failed;

  /* Number of statements sent from the client. */
  ulonglong questions;

  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];

  /*
    IMPORTANT! See last_system_status_var definition below. Variables after
    'last_system_status_var' cannot be handled automatically by add_to_status()
    and add_diff_to_status().
  */
  double last_query_cost;
  ulonglong last_query_partial_plans;

} STATUS_VAR;

/*
  This must reference the LAST ulonglong variable in system_status_var that is
  used as a global counter. It marks the end of a contiguous block of counters
  that can be iteratively totaled. See add_to_status().
*/
#define LAST_STATUS_VAR questions

/*
  This must reference the FIRST ulonglong variable in system_status_var that is
  used as a global counter. It marks the start of a contiguous block of counters
  that can be iteratively totaled.
*/
#define FIRST_STATUS_VAR created_tmp_disk_tables

/* Number of contiguous global status variables. */
const int COUNT_GLOBAL_STATUS_VARS= ((offsetof(STATUS_VAR, LAST_STATUS_VAR) -
                                      offsetof(STATUS_VAR, FIRST_STATUS_VAR)) /
                                      sizeof(ulonglong)) + 1;

/**
  Get collation by name, send error to client on failure.
  @param name     Collation name
  @param name_cs  Character set of the name string
  @return
  @retval         NULL on error
  @retval         Pointter to CHARSET_INFO with the given name on success
*/
inline CHARSET_INFO *
mysqld_collation_get_by_name(const char *name,
                             CHARSET_INFO *name_cs= system_charset_info)
{
  CHARSET_INFO *cs;
  MY_CHARSET_LOADER loader;
  my_charset_loader_init_mysys(&loader);
  if (!(cs= my_collation_get_by_name(&loader, name, MYF(0))))
  {
    ErrConvString err(name, name_cs);
    my_error(ER_UNKNOWN_COLLATION, MYF(0), err.ptr());
    if (loader.error[0])
      push_warning_printf(current_thd,
                          Sql_condition::SL_WARNING,
                          ER_UNKNOWN_COLLATION, "%s", loader.error);
  }
  return cs;
}


#ifdef MYSQL_SERVER

/* The following macro is to make init of Query_arena simpler */
#ifndef DBUG_OFF
#define INIT_ARENA_DBUG_INFO is_backup_arena= 0; is_reprepared= FALSE;
#else
#define INIT_ARENA_DBUG_INFO
#endif

class Query_arena
{
public:
  /*
    List of items created in the parser for this query. Every item puts
    itself to the list on creation (see Item::Item() for details))
  */
  Item *free_list;
  MEM_ROOT *mem_root;                   // Pointer to current memroot
#ifndef DBUG_OFF
  bool is_backup_arena; /* True if this arena is used for backup. */
  bool is_reprepared;
#endif
  /*
    The states relfects three diffrent life cycles for three
    different types of statements:
    Prepared statement: STMT_INITIALIZED -> STMT_PREPARED -> STMT_EXECUTED.
    Stored procedure:   STMT_INITIALIZED_FOR_SP -> STMT_EXECUTED.
    Other statements:   STMT_CONVENTIONAL_EXECUTION never changes.
  */
  enum enum_state
  {
    STMT_INITIALIZED= 0, STMT_INITIALIZED_FOR_SP= 1, STMT_PREPARED= 2,
    STMT_CONVENTIONAL_EXECUTION= 3, STMT_EXECUTED= 4, STMT_ERROR= -1
  };

  /*
    State and state changes in SP:
    1) When state is STMT_INITIALIZED_FOR_SP, objects in the item tree are
       created on the statement memroot. This is enforced through
       ps_arena_holder checking the state.
    2) After the first execute (call p1()), this state should change to
       STMT_EXECUTED. Objects will be created on the execution memroot and will
       be destroyed at the end of each execution.
    3) In case an ER_NEED_REPREPARE error occurs, state should be changed to
       STMT_INITIALIZED_FOR_SP and objects will again be created on the
       statement memroot. At the end of this execution, state should change to
       STMT_EXECUTED.
  */
  enum_state state;

  Query_arena(MEM_ROOT *mem_root_arg, enum enum_state state_arg) :
    free_list(0), mem_root(mem_root_arg), state(state_arg)
  { INIT_ARENA_DBUG_INFO; }
  /*
    This constructor is used only when Query_arena is created as
    backup storage for another instance of Query_arena.
  */
  Query_arena() { INIT_ARENA_DBUG_INFO; }

  virtual ~Query_arena() {};

  inline bool is_stmt_prepare() const { return state == STMT_INITIALIZED; }
  inline bool is_stmt_prepare_or_first_sp_execute() const
  { return (int)state < (int)STMT_PREPARED; }
  inline bool is_stmt_prepare_or_first_stmt_execute() const
  { return (int)state <= (int)STMT_PREPARED; }
  inline bool is_conventional() const
  { return state == STMT_CONVENTIONAL_EXECUTION; }

  inline void* alloc(size_t size) { return alloc_root(mem_root,size); }
  inline void* mem_calloc(size_t size)
  {
    void *ptr;
    if ((ptr=alloc_root(mem_root,size)))
      memset(ptr, 0, size);
    return ptr;
  }
  inline char *mem_strdup(const char *str)
  { return strdup_root(mem_root,str); }
  inline char *strmake(const char *str, size_t size)
  { return strmake_root(mem_root,str,size); }
  inline void *memdup(const void *str, size_t size)
  { return memdup_root(mem_root,str,size); }

  void set_query_arena(Query_arena *set);

  void free_items();
  /* Close the active state associated with execution of this statement */
  virtual void cleanup_stmt();
};

class Prepared_statement;

/**
  Container for all prepared statements created/used in a connection.

  Prepared statements in Prepared_statement_map have unique id
  (guaranteed by id assignment in Prepared_statement::Prepared_statement).

  Non-empty statement names are unique too: attempt to insert a new statement
  with duplicate name causes older statement to be deleted.

  Prepared statements are auto-deleted when they are removed from the map
  and when the map is deleted.
*/

class Prepared_statement_map
{
public:
  Prepared_statement_map();

  /**
    Insert a new statement to the thread-local prepared statement map.

    If there was an old statement with the same name, replace it with the
    new one. Otherwise, check if max_prepared_stmt_count is not reached yet,
    increase prepared_stmt_count, and insert the new statement. It's okay
    to delete an old statement and fail to insert the new one.

    All named prepared statements are also present in names_hash.
    Prepared statement names in names_hash are unique.
    The statement is added only if prepared_stmt_count < max_prepard_stmt_count
    m_last_found_statement always points to a valid statement or is 0

    @retval 0  success
    @retval 1  error: out of resources or max_prepared_stmt_count limit has been
                      reached. An error is sent to the client, the statement
                      is deleted.
  */
  int insert(THD *thd, Prepared_statement *statement);

  /** Find prepared statement by name. */
  Prepared_statement *find_by_name(const LEX_CSTRING &name);

  /** Find prepared statement by ID. */
  Prepared_statement *find(ulong id);

  /** Erase all prepared statements (calls Prepared_statement destructor). */
  void erase(Prepared_statement *statement);

  void claim_memory_ownership();

  void reset();

  ~Prepared_statement_map();
private:
  HASH st_hash;
  HASH names_hash;
  Prepared_statement *m_last_found_statement;
};


/**
  A registry for item tree transformations performed during
  query optimization. We register only those changes which require
  a rollback to re-execute a prepared statement or stored procedure
  yet another time.
*/

class Item_change_record: public ilink<Item_change_record>
{
private:
  // not used
  Item_change_record() {}
public:
  Item_change_record(Item **place, Item *new_value)
    : place(place), old_value(*place), new_value(new_value)
  {}
  Item **place;
  Item *old_value;
  Item *new_value;
};

typedef I_List<Item_change_record> Item_change_list;


/**
  Type of locked tables mode.
  See comment for THD::locked_tables_mode for complete description.
  While adding new enum values add them to the getter method for this enum
  declared below and defined in binlog.cc as well.
*/

enum enum_locked_tables_mode
{
  LTM_NONE= 0,
  LTM_LOCK_TABLES,
  LTM_PRELOCKED,
  LTM_PRELOCKED_UNDER_LOCK_TABLES
};

#ifndef DBUG_OFF
/**
  Getter for the enum enum_locked_tables_mode
  @param locked_tables_mode enum for types of locked tables mode

  @return The string represantation of that enum value
*/
const char * get_locked_tables_mode_name(enum_locked_tables_mode locked_tables_mode);
#endif

/**
  Class that holds information about tables which were opened and locked
  by the thread. It is also used to save/restore this information in
  push_open_tables_state()/pop_open_tables_state().
*/

class Open_tables_state
{
private:
  /**
    A stack of Reprepare_observer-instances. The top most instance is the
    currently active one. This stack is used during execution of prepared
    statements and stored programs in order to detect metadata changes.
    The locking subsystem reports a metadata change if the top-most item is not
    NULL.

    When Open_tables_state part of THD is reset to open a system or
    INFORMATION_SCHEMA table, NULL is temporarily pushed to avoid spurious
    ER_NEED_REPREPARE errors -- system and INFORMATION_SCHEMA tables are not
    subject to metadata version tracking.

    A stack is used here for the convenience -- in some cases we need to
    temporarily override/disable current Reprepare_observer-instance.

    NOTE: This is not a list of observers, only the top-most element will be
    notified in case of a metadata change.

    @sa check_and_update_table_version()
  */
  Prealloced_array<Reprepare_observer *, 4> m_reprepare_observers;

public:
  Reprepare_observer *get_reprepare_observer() const
  {
    return
      m_reprepare_observers.size() > 0 ?
      m_reprepare_observers.back() :
      NULL;
  }

  void push_reprepare_observer(Reprepare_observer *o)
  { m_reprepare_observers.push_back(o); }

  Reprepare_observer *pop_reprepare_observer()
  {
    Reprepare_observer *retval= m_reprepare_observers.back();
    m_reprepare_observers.pop_back();
    return retval;
  }

  void reset_reprepare_observers()
  { m_reprepare_observers.clear(); }

public:
  /**
    List of regular tables in use by this thread. Contains temporary and
    base tables that were opened with @see open_tables().
  */
  TABLE *open_tables;
  /**
    List of temporary tables used by this thread. Contains user-level
    temporary tables, created with CREATE TEMPORARY TABLE, and
    internal temporary tables, created, e.g., to resolve a SELECT,
    or for an intermediate table used in ALTER.
    XXX Why are internal temporary tables added to this list?
  */
  TABLE *temporary_tables;
  TABLE *derived_tables;
  /*
    During a MySQL session, one can lock tables in two modes: automatic
    or manual. In automatic mode all necessary tables are locked just before
    statement execution, and all acquired locks are stored in 'lock'
    member. Unlocking takes place automatically as well, when the
    statement ends.
    Manual mode comes into play when a user issues a 'LOCK TABLES'
    statement. In this mode the user can only use the locked tables.
    Trying to use any other tables will give an error.
    The locked tables are also stored in this member, however,
    thd->locked_tables_mode is turned on.  Manual locking is described in
    the 'LOCK_TABLES' chapter of the MySQL manual.
    See also lock_tables() for details.
  */
  MYSQL_LOCK *lock;

  /*
    CREATE-SELECT keeps an extra lock for the table being
    created. This field is used to keep the extra lock available for
    lower level routines, which would otherwise miss that lock.
   */
  MYSQL_LOCK *extra_lock;

  /*
    Enum enum_locked_tables_mode and locked_tables_mode member are
    used to indicate whether the so-called "locked tables mode" is on,
    and what kind of mode is active.

    Locked tables mode is used when it's necessary to open and
    lock many tables at once, for usage across multiple
    (sub-)statements.
    This may be necessary either for queries that use stored functions
    and triggers, in which case the statements inside functions and
    triggers may be executed many times, or for implementation of
    LOCK TABLES, in which case the opened tables are reused by all
    subsequent statements until a call to UNLOCK TABLES.

    The kind of locked tables mode employed for stored functions and
    triggers is also called "prelocked mode".
    In this mode, first open_tables() call to open the tables used
    in a statement analyses all functions used by the statement
    and adds all indirectly used tables to the list of tables to
    open and lock.
    It also marks the parse tree of the statement as requiring
    prelocking. After that, lock_tables() locks the entire list
    of tables and changes THD::locked_tables_modeto LTM_PRELOCKED.
    All statements executed inside functions or triggers
    use the prelocked tables, instead of opening their own ones.
    Prelocked mode is turned off automatically once close_thread_tables()
    of the main statement is called.
  */
  enum enum_locked_tables_mode locked_tables_mode;

  enum enum_flags {
    BACKUPS_AVAIL = (1U << 0)     /* There are backups available */
  };

  /*
    Flags with information about the open tables state.
  */
  uint state_flags;
  /**
     This constructor initializes Open_tables_state instance which can only
     be used as backup storage. To prepare Open_tables_state instance for
     operations which open/lock/close tables (e.g. open_table()) one has to
     call init_open_tables_state().
  */
  Open_tables_state()
    : m_reprepare_observers(PSI_INSTRUMENT_ME), state_flags(0U) { }

  void set_open_tables_state(Open_tables_state *state);

  void reset_open_tables_state();
};


/**
  Storage for backup of Open_tables_state. Must
  be used only to open system tables (TABLE_CATEGORY_SYSTEM
  and TABLE_CATEGORY_LOG).
*/

class Open_tables_backup: public Open_tables_state
{
public:
  /**
    When we backup the open tables state to open a system
    table or tables, we want to save state of metadata
    locks which were acquired before the backup. It is used
    to release metadata locks on system tables after they are
    no longer used.
  */
  MDL_savepoint mdl_system_tables_svp;
};

/**
  @class Sub_statement_state
  @brief Used to save context when executing a function or trigger
*/

/* Defines used for Sub_statement_state::in_sub_stmt */

#define SUB_STMT_TRIGGER 1
#define SUB_STMT_FUNCTION 2


class Sub_statement_state
{
public:
  ulonglong option_bits;
  ulonglong first_successful_insert_id_in_prev_stmt;
  ulonglong first_successful_insert_id_in_cur_stmt, insert_id_for_cur_row;
  Discrete_interval auto_inc_interval_for_cur_row;
  Discrete_intervals_list auto_inc_intervals_forced;
  ulonglong current_found_rows;
  ulonglong previous_found_rows;
  ha_rows    cuted_fields, sent_row_count, examined_row_count;
  ulong client_capabilities;
  uint in_sub_stmt;
  bool enable_slow_log;
  bool last_insert_id_used;
  SAVEPOINT *savepoints;
  enum enum_check_fields count_cuted_fields;
};


inline char const *
show_system_thread(enum_thread_type thread)
{
#define RETURN_NAME_AS_STRING(NAME) case (NAME): return #NAME
  switch (thread) {
    static char buf[64];
    RETURN_NAME_AS_STRING(NON_SYSTEM_THREAD);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_SLAVE_IO);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_SLAVE_SQL);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_NDBCLUSTER_BINLOG);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_EVENT_SCHEDULER);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_EVENT_WORKER);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_INFO_REPOSITORY);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_SLAVE_WORKER);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_COMPRESS_GTID_TABLE);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_BACKGROUND);
  default:
    sprintf(buf, "<UNKNOWN SYSTEM THREAD: %d>", thread);
    return buf;
  }
#undef RETURN_NAME_AS_STRING
}

/**
  This class represents the interface for internal error handlers.
  Internal error handlers are exception handlers used by the server
  implementation.
*/
class Internal_error_handler
{
protected:
  Internal_error_handler() :
    m_prev_internal_handler(NULL)
  {}

  virtual ~Internal_error_handler() {}

public:
  /**
    Handle a sql condition.
    This method can be implemented by a subclass to achieve any of the
    following:
    - mask a warning/error internally, prevent exposing it to the user,
    - mask a warning/error and throw another one instead.
    When this method returns true, the sql condition is considered
    'handled', and will not be propagated to upper layers.
    It is the responsability of the code installing an internal handler
    to then check for trapped conditions, and implement logic to recover
    from the anticipated conditions trapped during runtime.

    This mechanism is similar to C++ try/throw/catch:
    - 'try' correspond to <code>THD::push_internal_handler()</code>,
    - 'throw' correspond to <code>my_error()</code>,
    which invokes <code>my_message_sql()</code>,
    - 'catch' correspond to checking how/if an internal handler was invoked,
    before removing it from the exception stack with
    <code>THD::pop_internal_handler()</code>.

    @param thd the calling thread
    @param cond the condition raised.
    @return true if the condition is handled
  */
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg) = 0;

private:
  Internal_error_handler *m_prev_internal_handler;
  friend class THD;
};


/**
  Implements the trivial error handler which cancels all error states
  and prevents an SQLSTATE to be set.
*/

class Dummy_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    /* Ignore error */
    return true;
  }
};


/**
  This class is an internal error handler implementation for
  DROP TABLE statements. The thing is that there may be warnings during
  execution of these statements, which should not be exposed to the user.
  This class is intended to silence such warnings.
*/

class Drop_table_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);
};


/**
  Internal error handler to process an error from MDL_context::upgrade_lock()
  and mysql_lock_tables(). Used by implementations of HANDLER READ and
  LOCK TABLES LOCAL.
*/

class MDL_deadlock_and_lock_abort_error_handler: public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char *sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_LOCK_ABORTED || sql_errno == ER_LOCK_DEADLOCK)
      m_need_reopen= true;

    return m_need_reopen;
  }

  bool need_reopen() const { return m_need_reopen; };
  void init() { m_need_reopen= false; };
private:
  bool m_need_reopen;
};


/**
  Tables that were locked with LOCK TABLES statement.

  Encapsulates a list of TABLE_LIST instances for tables
  locked by LOCK TABLES statement, memory root for metadata locks,
  and, generally, the context of LOCK TABLES statement.

  In LOCK TABLES mode, the locked tables are kept open between
  statements.
  Therefore, we can't allocate metadata locks on execution memory
  root -- as well as tables, the locks need to stay around till
  UNLOCK TABLES is called.
  The locks are allocated in the memory root encapsulated in this
  class.

  Some SQL commands, like FLUSH TABLE or ALTER TABLE, demand that
  the tables they operate on are closed, at least temporarily.
  This class encapsulates a list of TABLE_LIST instances, one
  for each base table from LOCK TABLES list,
  which helps conveniently close the TABLEs when it's necessary
  and later reopen them.

  Implemented in sql_base.cc
*/

class Locked_tables_list
{
private:
  MEM_ROOT m_locked_tables_root;
  TABLE_LIST *m_locked_tables;
  TABLE_LIST **m_locked_tables_last;
  /** An auxiliary array used only in reopen_tables(). */
  TABLE **m_reopen_array;
  /**
    Count the number of tables in m_locked_tables list. We can't
    rely on thd->lock->table_count because it excludes
    non-transactional temporary tables. We need to know
    an exact number of TABLE objects.
  */
  size_t m_locked_tables_count;
public:
  Locked_tables_list()
    :m_locked_tables(NULL),
    m_locked_tables_last(&m_locked_tables),
    m_reopen_array(NULL),
    m_locked_tables_count(0)
  {
    init_sql_alloc(key_memory_locked_table_list,
                   &m_locked_tables_root, MEM_ROOT_BLOCK_SIZE, 0);
  }
  void unlock_locked_tables(THD *thd);
  ~Locked_tables_list()
  {
    unlock_locked_tables(0);
  }
  bool init_locked_tables(THD *thd);
  TABLE_LIST *locked_tables() { return m_locked_tables; }
  void unlink_from_list(THD *thd, TABLE_LIST *table_list,
                        bool remove_from_locked_tables);
  void unlink_all_closed_tables(THD *thd,
                                MYSQL_LOCK *lock,
                                size_t reopen_count);
  bool reopen_tables(THD *thd);
};


/**
  Storage engine specific thread local data.
*/

struct Ha_data
{
  /**
    Storage engine specific thread local data.
    Lifetime: one user connection.
  */
  void *ha_ptr;
  /**
    A memorizer to engine specific "native" transaction object to provide
    storage engine detach-re-attach facility.
    The server level transaction object can dissociate from storage engine
    transactions. The released "native" transaction reference
    can be hold in the member until it is reconciled later.
    Lifetime: Depends on caller of @c hton::replace_native_transaction_in_thd.
    For instance in the case of slave server applier handling XA transaction
    it is from XA START to XA PREPARE.
  */
  void *ha_ptr_backup;
  /**
    0: Life time: one statement within a transaction. If @@autocommit is
    on, also represents the entire transaction.
    @sa trans_register_ha()

    1: Life time: one transaction within a connection.
    If the storage engine does not participate in a transaction,
    this should not be used.
    @sa trans_register_ha()
  */
  Ha_trx_info ha_info[2];

  /**
    NULL: engine is not bound to this thread
    non-NULL: engine is bound to this thread, engine shutdown forbidden
  */
  plugin_ref lock;

  Ha_data()
  :ha_ptr(NULL), ha_ptr_backup(NULL),
    lock(NULL)
  { }
};

/**
  An instance of the global read lock in a connection.
  Implemented in lock.cc.
*/

class Global_read_lock
{
public:
  enum enum_grl_state
  {
    GRL_NONE,
    GRL_ACQUIRED,
    GRL_ACQUIRED_AND_BLOCKS_COMMIT
  };

  Global_read_lock()
    : m_state(GRL_NONE),
      m_mdl_global_shared_lock(NULL),
      m_mdl_blocks_commits_lock(NULL)
  {}

  bool lock_global_read_lock(THD *thd);
  void unlock_global_read_lock(THD *thd);

  /**
    Used by innodb memcached server to check if any connections
    have global read lock
  */
  static bool global_read_lock_active()
  {
    return my_atomic_load32(&m_active_requests) ? true : false;
  }

  /**
    Check if this connection can acquire protection against GRL and
    emit error if otherwise.
  */
  bool can_acquire_protection() const
  {
    if (m_state)
    {
      my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
      return TRUE;
    }
    return FALSE;
  }
  bool make_global_read_lock_block_commit(THD *thd);
  bool is_acquired() const { return m_state != GRL_NONE; }
  void set_explicit_lock_duration(THD *thd);
private:
  volatile static int32 m_active_requests;
  enum_grl_state m_state;
  /**
    In order to acquire the global read lock, the connection must
    acquire shared metadata lock in GLOBAL namespace, to prohibit
    all DDL.
  */
  MDL_ticket *m_mdl_global_shared_lock;
  /**
    Also in order to acquire the global read lock, the connection
    must acquire a shared metadata lock in COMMIT namespace, to
    prohibit commits.
  */
  MDL_ticket *m_mdl_blocks_commits_lock;
};

extern "C" void my_message_sql(uint error, const char *str, myf MyFlags);


/**
  Convert microseconds since epoch to timeval.
  @param      micro_time  Microseconds.
  @param[out] tm          A timeval variable to write to.
*/
static inline void
my_micro_time_to_timeval(ulonglong micro_time, struct timeval *tm)
{
  tm->tv_sec=  (long) (micro_time / 1000000);
  tm->tv_usec= (long) (micro_time % 1000000);
}

class Modification_plan;

/**
  @class THD
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public MDL_context_owner,
           public Query_arena,
           public Open_tables_state
{
private:
  inline bool is_stmt_prepare() const
  { DBUG_ASSERT(0); return Query_arena::is_stmt_prepare(); }

  inline bool is_stmt_prepare_or_first_sp_execute() const
  { DBUG_ASSERT(0); return Query_arena::is_stmt_prepare_or_first_sp_execute(); }

  inline bool is_stmt_prepare_or_first_stmt_execute() const
  { DBUG_ASSERT(0); return Query_arena::is_stmt_prepare_or_first_stmt_execute(); }

  inline bool is_conventional() const
  { DBUG_ASSERT(0); return Query_arena::is_conventional(); }

public:
  MDL_context mdl_context;

  /*
    MARK_COLUMNS_NONE:  Means mark_used_colums is not set and no indicator to
                        handler of fields used is set
    MARK_COLUMNS_READ:  Means a bit in read set is set to inform handler
                        that the field is to be read. Update covering_keys
                        and merge_keys too.
    MARK_COLUMNS_WRITE: Means a bit is set in write set to inform handler
                        that it needs to update this field in write_row
                        and update_row. If field list contains duplicates,
                        then thd->dup_field is set to point to the last
                        found duplicate.
    MARK_COLUMNS_TEMP:  Mark bit in read set, but ignore key sets.
                        Used by filesort().
  */
  enum enum_mark_columns mark_used_columns;
  /**
    Used by Item::check_column_privileges() to tell which privileges
    to check for.
    Set to ~0ULL before starting to resolve a statement.
    Set to desired privilege mask before calling a resolver function that will
    call Item::check_column_privileges().
    After use, restore previous value as current value.
  */
  ulong want_privilege;

  LEX *lex;                                     // parse tree descriptor

  /*
   True if @@SESSION.GTID_EXECUTED was read once and the deprecation warning
   was issued.
   This flag needs to be removed once @@SESSION.GTID_EXECUTED is deprecated.
  */
  bool gtid_executed_warning_issued;

private:
  /**
    The query associated with this statement.
  */
  LEX_CSTRING m_query_string;
  String m_normalized_query;

  /**
    Currently selected catalog.
  */

  LEX_CSTRING m_catalog;
  /**
    Name of the current (default) database.

    If there is the current (default) database, "db" contains its name. If
    there is no current (default) database, "db" is NULL and "db_length" is
    0. In other words, "db", "db_length" must either be NULL, or contain a
    valid database name.

    @note this attribute is set and alloced by the slave SQL thread (for
    the THD of that thread); that thread is (and must remain, for now) the
    only responsible for freeing this member.
  */
  LEX_CSTRING m_db;

public:

  /**
    In some cases, we may want to modify the query (i.e. replace
    passwords with their hashes before logging the statement etc.).

    In case the query was rewritten, the original query will live in
    m_query_string, while the rewritten query lives in rewritten_query.
    If rewritten_query is empty, m_query_string should be logged.
    If rewritten_query is non-empty, the rewritten query it contains
    should be used in logs (general log, slow query log, binary log).

    Currently, password obfuscation is the only rewriting we do; more
    may follow at a later date, both pre- and post parsing of the query.
    Rewriting of binloggable statements must preserve all pertinent
    information.
  */
  String      rewritten_query;

  /* Used to execute base64 coded binlog events in MySQL server */
  Relay_log_info* rli_fake;
  /* Slave applier execution context */
  Relay_log_info* rli_slave;

  /**
    The function checks whether the thread is processing queries from binlog,
    as automatically generated by mysqlbinlog.

    @return true  when the thread is a binlog applier
  */
  bool is_binlog_applier() { return rli_fake && variables.pseudo_slave_mode; }

  /**
    When the thread is a binlog or slave applier it detaches the engine
    ha_data associated with it and memorizes the fact of that.
  */
  void rpl_detach_engine_ha_data();

  /**
    @return true   when the current binlog (rli_fake) or slave (rli_slave)
                   applier thread has detached the engine ha_data,
                   see @c rpl_detach_engine_ha_data.
    @note The detached transaction applier resets a memo
          mark at once with this check.
  */
  bool rpl_unflag_detached_engine_ha_data();

  void reset_for_next_command();
  /*
    Constant for THD::where initialization in the beginning of every query.

    It's needed because we do not save/restore THD::where normally during
    primary (non subselect) query execution.
  */
  static const char * const DEFAULT_WHERE;

#ifdef EMBEDDED_LIBRARY
  struct st_mysql  *mysql;
  unsigned long	 client_stmt_id;
  unsigned long  client_param_count;
  struct st_mysql_bind *client_params;
  char *extra_data;
  ulong extra_length;
  struct st_mysql_data *cur_data;
  struct st_mysql_data *first_data;
  struct st_mysql_data **data_tail;
  void clear_data_list();
  struct st_mysql_data *alloc_new_dataset();
  /*
    In embedded server it points to the statement that is processed
    in the current query. We store some results directly in statement
    fields then.
  */
  struct st_mysql_stmt *current_stmt;
#endif
  Query_cache_tls query_cache_tls;
  /** Aditional network instrumentation for the server only. */
  NET_SERVER m_net_server_extension;
  /**
    Hash for user variables.
    User variables are per session,
    but can also be monitored outside of the session,
    so a lock is needed to prevent race conditions.
    Protected by @c LOCK_thd_data.
  */
  HASH    user_vars;			// hash for user variables
  String  convert_buffer;               // buffer for charset conversions
  struct  rand_struct rand;		// used for authentication
  struct  system_variables variables;	// Changeable local variables
  struct  system_status_var status_var; // Per thread statistic vars
  struct  system_status_var *initial_status_var; /* used by show status */
  // has status_var already been added to global_status_var?
  bool status_var_aggregated;

  /**
    Current query cost.
    @sa system_status_var::last_query_cost
  */
  double m_current_query_cost;
  /**
    Current query partial plans.
    @sa system_status_var::last_query_partial_plans
  */
  ulonglong m_current_query_partial_plans;

  /**
    Clear the query costs attributes for the current query.
  */
  void clear_current_query_costs()
  {
    m_current_query_cost= 0.0;
    m_current_query_partial_plans= 0;
  }

  /**
    Save the current query costs attributes in
    the thread session status.
    Use this method only after the query execution is completed,
    so that
      @code SHOW SESSION STATUS like 'last_query_%' @endcode
      @code SELECT * from performance_schema.session_status
      WHERE VARIABLE_NAME like 'last_query_%' @endcode
    actually reports the previous query, not itself.
  */
  void save_current_query_costs()
  {
    status_var.last_query_cost= m_current_query_cost;
    status_var.last_query_partial_plans= m_current_query_partial_plans;
  }

  THR_LOCK_INFO lock_info;              // Locking info of this thread
  /**
    Protects THD data accessed from other threads.
    The attributes protected are:
    - thd->is_killable (used by KILL statement and shutdown).
    - thd->user_vars (user variables, inspected by monitoring)
    Is locked when THD is deleted.
  */
  mysql_mutex_t LOCK_thd_data;

  /**
    Protects THD::m_query_string. No other mutexes should be locked
    while having this mutex locked.
  */
  mysql_mutex_t LOCK_thd_query;

  /**
    Protects THD::variables while being updated. This should be taken inside
    of LOCK_thd_data and outside of LOCK_global_system_variables.
  */
  mysql_mutex_t LOCK_thd_sysvar;

  /**
    Protects query plan (SELECT/UPDATE/DELETE's) from being freed/changed
    while another thread explains it. Following structures are protected by
    this mutex:
      THD::Query_plan
      Modification_plan
      SELECT_LEX::join
      JOIN::plan_state
      Tree of SELECT_LEX_UNIT after THD::Query_plan was set till
        THD::Query_plan cleanup
      JOIN_TAB::select->quick
    Code that changes objects above should take this mutex.
    Explain code takes this mutex to block changes to named structures to
    avoid crashes in following functions:
      explain_single_table_modification
      explain_query
      mysql_explain_other
    When doing EXPLAIN CONNECTION:
      all explain code assumes that this mutex is already taken.
    When doing ordinary EXPLAIN:
      the mutex does need to be taken (no need to protect reading my own data,
      moreover EXPLAIN CONNECTION can't run on an ordinary EXPLAIN).
  */
private:
  mysql_mutex_t LOCK_query_plan;

public:
  /// Locks the query plan of this THD
  void lock_query_plan() { mysql_mutex_lock(&LOCK_query_plan); }
  void unlock_query_plan() { mysql_mutex_unlock(&LOCK_query_plan); }

  /** All prepared statements of this connection. */
  Prepared_statement_map stmt_map;
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  const char *thread_stack;

  /**
    @note
    Some members of THD (currently 'Statement::db',
    'catalog' and 'query')  are set and alloced by the slave SQL thread
    (for the THD of that thread); that thread is (and must remain, for now)
    the only responsible for freeing these 3 members. If you add members
    here, and you add code to set them in replication, don't forget to
    free_them_and_set_them_to_0 in replication properly. For details see
    the 'err:' label of the handle_slave_sql() in sql/slave.cc.

    @see handle_slave_sql
  */

  Security_context m_main_security_ctx;
  Security_context *m_security_ctx;

  Security_context* security_context() const { return m_security_ctx; }
  void set_security_context(Security_context *sctx) { m_security_ctx= sctx; }

  /*
    Points to info-string that we show in SHOW PROCESSLIST
    You are supposed to update thd->proc_info only if you have coded
    a time-consuming piece that MySQL can get stuck in for a long time.

    Set it using the  thd_proc_info(THD *thread, const char *message)
    macro/function.

    This member is accessed and assigned without any synchronization.
    Therefore, it may point only to constant (statically
    allocated) strings, which memory won't go away over time.
  */
  const char *proc_info;

  Protocol_text   protocol_text;    // Normal protocol
  Protocol_binary protocol_binary;  // Binary protocol

  Protocol *get_protocol()
  {
    return m_protocol;
  }

  SSL_handle get_ssl() const
  {
#ifndef DBUG_OFF
    if (current_thd != this)
    {
      /*
        When inspecting this thread from monitoring,
        the monitoring thread MUST lock LOCK_thd_data,
        to be allowed to safely inspect SSL status variables.
      */
      mysql_mutex_assert_owner(&LOCK_thd_data);
    }
#endif
    return m_SSL;
  }

  /**
    Asserts that the protocol is of type text or binary and then
    returns the m_protocol casted to Protocol_classic. This method
    is needed to prevent misuse of pluggable protocols by legacy code
  */
  Protocol_classic *get_protocol_classic() const
  {
    DBUG_ASSERT(m_protocol->type() == Protocol::PROTOCOL_TEXT ||
                m_protocol->type() == Protocol::PROTOCOL_BINARY);

    return (Protocol_classic *) m_protocol;
  }

  void set_protocol(Protocol * protocol)
  {
    m_protocol= protocol;
  }

private:
  Protocol *m_protocol;           // Current protocol
  /**
    SSL data attached to this connection.
    This is an opaque pointer,
    When building with SSL, this pointer is non NULL
    only if the connection is using SSL.
    When building without SSL, this pointer is always NULL.
    The SSL data can be inspected to read per thread
    status variables,
    and this can be inspected while the thread is running.
  */
  SSL_handle m_SSL;

public:
  /**
     Query plan for EXPLAINable commands, should be locked with
     LOCK_query_plan before using.
  */
  class Query_plan
  {
  private:
    THD *const thd;
    /// Original sql_command;
    enum_sql_command sql_command;
    /// LEX of topmost statement
    LEX *lex;
    /// Query plan for UPDATE/DELETE/INSERT/REPLACE
    const Modification_plan *modification_plan;
    /// True if query is run in prepared statement
    bool is_ps;

    explicit Query_plan(const Query_plan&);     ///< not defined
    Query_plan& operator=(const Query_plan&);   ///< not defined

  public:
    /// Asserts that current_thd has locked this plan, if it does not own it.
    void assert_plan_is_locked_if_other() const
#ifdef DBUG_OFF
    {}
#else
    ;
#endif

    explicit Query_plan(THD *thd_arg)
      : thd(thd_arg),
        sql_command(SQLCOM_END),
        lex(NULL),
        modification_plan(NULL),
        is_ps(false)
    {}

    /**
      Set query plan.

      @note This function takes THD::LOCK_query_plan mutex.
    */
    void set_query_plan(enum_sql_command sql_cmd, LEX *lex_arg, bool ps);

    /*
      The 4 getters below expect THD::LOCK_query_plan to be already taken
      if called from another thread.
    */
    enum_sql_command get_command() const
    {
      assert_plan_is_locked_if_other();
      return sql_command;
    }
    LEX *get_lex() const
    {
      assert_plan_is_locked_if_other();
      return lex;
    }
    Modification_plan const *get_modification_plan() const
    {
      assert_plan_is_locked_if_other();
      return modification_plan;
    }
    bool is_ps_query() const
    {
      assert_plan_is_locked_if_other();
      return is_ps;
    }

    void set_modification_plan(Modification_plan *plan_arg);

  } query_plan;

  const LEX_CSTRING &catalog() const
  { return m_catalog; }

  void set_catalog(const LEX_CSTRING &catalog)
  { m_catalog= catalog; }

private:
  unsigned int m_current_stage_key;

public:
  void enter_stage(const PSI_stage_info *stage,
                   PSI_stage_info *old_stage,
                   const char *calling_func,
                   const char *calling_file,
                   const unsigned int calling_line);

  const char *get_proc_info() const
  { return proc_info; }

  /*
    Used in error messages to tell user in what part of MySQL we found an
    error. E. g. when where= "having clause", if fix_fields() fails, user
    will know that the error was in having clause.
  */
  const char *where;

  ulong max_client_packet_length;

  HASH		handler_tables_hash;
  /*
    A thread can hold named user-level locks. This variable
    contains granted tickets if a lock is present. See item_func.cc and
    chapter 'Miscellaneous functions', for functions GET_LOCK, RELEASE_LOCK.
  */
  HASH ull_hash;
#ifndef DBUG_OFF
  uint dbug_sentry; // watch out for memory corruption
#endif
  bool is_killable;
  /**
    Mutex protecting access to current_mutex and current_cond.
  */
  mysql_mutex_t LOCK_current_cond;
  /**
    The mutex used with current_cond.
    @see current_cond
  */
  mysql_mutex_t * volatile current_mutex;
  /**
    Pointer to the condition variable the thread owning this THD
    is currently waiting for. If the thread is not waiting, the
    value is NULL. Set by THD::enter_cond().

    If this thread is killed (shutdown or KILL stmt), another
    thread will broadcast on this condition variable so that the
    thread can be unstuck.
  */
  mysql_cond_t * volatile current_cond;
  /**
    Condition variable used for waiting by the THR_LOCK.c subsystem.
  */
  mysql_cond_t COND_thr_lock;

private:
  /**
    Type of current query: COM_STMT_PREPARE, COM_QUERY, etc.
    Set from first byte of the packet in do_command()
  */
  enum enum_server_command m_command;

public:
  uint32     unmasked_server_id;
  uint32     server_id;
  uint32     file_id;			// for LOAD DATA INFILE
  /* remote (peer) port */
  uint16 peer_port;
  struct timeval start_time;
  struct timeval user_time;
  // track down slow my_thread_create
  ulonglong  thr_create_utime;
  ulonglong  start_utime, utime_after_lock;

  /**
    Type of lock to be used for all DML statements, except INSERT, in cases
    when lock is not specified explicitly.  Set to TL_WRITE or
    TL_WRITE_LOW_PRIORITY depending on whether low_priority_updates option is
    off or on.
  */
  thr_lock_type update_lock_default;
  /**
    Type of lock to be used for INSERT statement if lock is not specified
    explicitly. Set to TL_WRITE_CONCURRENT_INSERT or TL_WRITE_LOW_PRIORITY
    depending on whether low_priority_updates option is off or on.
  */
  thr_lock_type insert_lock_default;

  /* <> 0 if we are inside of trigger or stored function. */
  uint in_sub_stmt;

  /**
    Used by fill_status() to avoid acquiring LOCK_status mutex twice
    when this function is called recursively (e.g. queries
    that contains SELECT on I_S.GLOBAL_STATUS with subquery on the
    same I_S table).
    Incremented each time fill_status() function is entered and
    decremented each time before it returns from the function.
  */
  uint fill_status_recursion_level;
  uint fill_variables_recursion_level;

  /* container for handler's private per-connection data */
  Ha_data ha_data[MAX_HA];

  /*
    Position of first event in Binlog
    *after* last event written by this
    thread.
  */
  rpl_event_coordinates binlog_next_event_pos;
  void set_next_event_pos(const char* _filename, ulonglong _pos);
  void clear_next_event_pos();

  /*
     Ptr to row event extra data to be written to Binlog /
     received from Binlog.

   */
  uchar* binlog_row_event_extra_data;
  static bool binlog_row_event_extra_data_eq(const uchar* a,
                                             const uchar* b);

#ifndef MYSQL_CLIENT
  int binlog_setup_trx_data();

  /*
    Public interface to write RBR events to the binlog
  */
  int binlog_write_table_map(TABLE *table, bool is_transactional,
                             bool binlog_rows_query);
  int binlog_write_row(TABLE* table, bool is_transactional,
                       const uchar *new_data,
                       const uchar* extra_row_info);
  int binlog_delete_row(TABLE* table, bool is_transactional,
                        const uchar *old_data,
                        const uchar* extra_row_info);
  int binlog_update_row(TABLE* table, bool is_transactional,
                        const uchar *old_data, const uchar *new_data,
                        const uchar* extra_row_info);
  void binlog_prepare_row_images(TABLE* table);

  void set_server_id(uint32 sid) { server_id = sid; }

  /*
    Member functions to handle pending event for row-level logging.
  */
  template <class RowsEventT> Rows_log_event*
    binlog_prepare_pending_rows_event(TABLE* table, uint32 serv_id,
                                      size_t needed,
                                      bool is_transactional,
				      RowsEventT* hint,
                                      const uchar* extra_row_info);
  Rows_log_event* binlog_get_pending_rows_event(bool is_transactional) const;
  inline int binlog_flush_pending_rows_event(bool stmt_end)
  {
    return (binlog_flush_pending_rows_event(stmt_end, FALSE) ||
            binlog_flush_pending_rows_event(stmt_end, TRUE));
  }
  int binlog_flush_pending_rows_event(bool stmt_end, bool is_transactional);

  /**
    Determine the binlog format of the current statement.

    @retval 0 if the current statement will be logged in statement
    format.
    @retval nonzero if the current statement will be logged in row
    format.
   */
  int is_current_stmt_binlog_format_row() const {
    DBUG_ASSERT(current_stmt_binlog_format == BINLOG_FORMAT_STMT ||
                current_stmt_binlog_format == BINLOG_FORMAT_ROW);
    return current_stmt_binlog_format == BINLOG_FORMAT_ROW;
  }

  bool is_current_stmt_binlog_disabled() const;

  /**
    Determine if binloging is enabled in row format and write set extraction is
    enabled for this session
    @retval true  if is enable
    @retval false otherwise
  */
  bool is_current_stmt_binlog_row_enabled_with_write_set_extraction() const;

  /** Tells whether the given optimizer_switch flag is on */
  inline bool optimizer_switch_flag(ulonglong flag) const
  {
    return (variables.optimizer_switch & flag);
  }

  enum binlog_filter_state
  {
    BINLOG_FILTER_UNKNOWN,
    BINLOG_FILTER_CLEAR,
    BINLOG_FILTER_SET
  };

  inline void reset_binlog_local_stmt_filter()
  {
    m_binlog_filter_state= BINLOG_FILTER_UNKNOWN;
  }

  inline void clear_binlog_local_stmt_filter()
  {
    DBUG_ASSERT(m_binlog_filter_state == BINLOG_FILTER_UNKNOWN);
    m_binlog_filter_state= BINLOG_FILTER_CLEAR;
  }

  inline void set_binlog_local_stmt_filter()
  {
    DBUG_ASSERT(m_binlog_filter_state == BINLOG_FILTER_UNKNOWN);
    m_binlog_filter_state= BINLOG_FILTER_SET;
  }

  inline binlog_filter_state get_binlog_local_stmt_filter()
  {
    return m_binlog_filter_state;
  }

  /** Holds active timer object */
  struct st_thd_timer_info *timer;
  /**
    After resetting(cancelling) timer, current timer object is cached
    with timer_cache timer to reuse.
  */
  struct st_thd_timer_info *timer_cache;

private:
  /*
    Indicates that the command which is under execution should ignore the
    'read_only' and 'super_read_only' options.
  */
  bool skip_readonly_check;
  /**
    Indicate if the current statement should be discarded
    instead of written to the binlog.
    This is used to discard special statements, such as
    DML or DDL that affects only 'local' (non replicated)
    tables, such as performance_schema.*
  */
  binlog_filter_state m_binlog_filter_state;

  /**
    Indicates the format in which the current statement will be
    logged.  This can only be set from @c decide_logging_format().
  */
  enum_binlog_format current_stmt_binlog_format;

  /**
    Bit field for the state of binlog warnings.

    The first Lex::BINLOG_STMT_UNSAFE_COUNT bits list all types of
    unsafeness that the current statement has.

    This must be a member of THD and not of LEX, because warnings are
    detected and issued in different places (@c
    decide_logging_format() and @c binlog_query(), respectively).
    Between these calls, the THD->lex object may change; e.g., if a
    stored routine is invoked.  Only THD persists between the calls.
  */
  uint32 binlog_unsafe_warning_flags;

  /*
    Number of outstanding table maps, i.e., table maps in the
    transaction cache.
  */
  uint binlog_table_maps;
  /*
    MTS: db names listing to be updated by the query databases
  */
  List<char> *binlog_accessed_db_names;

  /**
    The binary log position of the transaction.

    The file and position are zero if the current transaction has not
    been written to the binary log.

    @see set_trans_pos
    @see get_trans_pos

    @todo Similar information is kept in the patch for BUG#11762277
    and by the master/slave heartbeat implementation.  We should merge
    these positions instead of maintaining three different ones.
   */
  /**@{*/
  const char *m_trans_log_file;
  char *m_trans_fixed_log_file;
  my_off_t m_trans_end_pos;
  /**@}*/
  // NOTE: Ideally those two should be in Protocol,
  // but currently its design doesn't allow that.
  NET     net;                          // client connection descriptor
  String  packet;                       // dynamic buffer for network I/O
public:
  void set_skip_readonly_check()
  {
    skip_readonly_check= true;
  }

  bool is_cmd_skip_readonly()
  {
    return skip_readonly_check;
  }

  void reset_skip_readonly_check()
  {
    if (skip_readonly_check)
      skip_readonly_check= false;
  }

  void issue_unsafe_warnings();

  uint get_binlog_table_maps() const {
    return binlog_table_maps;
  }
  void clear_binlog_table_maps() {
    binlog_table_maps= 0;
  }

  /*
    MTS: accessor to binlog_accessed_db_names list
  */
  List<char> * get_binlog_accessed_db_names()
  {
    return binlog_accessed_db_names;
  }

  /*
     MTS: resetter of binlog_accessed_db_names list normally
     at the end of the query execution
  */
  void clear_binlog_accessed_db_names() { binlog_accessed_db_names= NULL; }

  /* MTS: method inserts a new unique name into binlog_updated_dbs */
  void add_to_binlog_accessed_dbs(const char *db);

#endif /* MYSQL_CLIENT */

private:
  std::auto_ptr<Transaction_ctx> m_transaction;

  /** An utility struct for @c Attachable_trx */
  struct Transaction_state
  {
    void backup(THD *thd);
    void restore(THD *thd);

    /// SQL-command.
    enum_sql_command m_sql_command;

    Query_tables_list m_query_tables_list;

    /// Open-tables state.
    Open_tables_backup m_open_tables_state;

    /// SQL_MODE.
    sql_mode_t m_sql_mode;

    /// Transaction isolation level.
    enum_tx_isolation m_tx_isolation;

    /// Ha_data array.
    Ha_data m_ha_data[MAX_HA];

    /// Transaction_ctx instance.
    Transaction_ctx *m_trx;

    /// Transaction read-only state.
    my_bool m_tx_read_only;

    /// THD options.
    ulonglong m_thd_option_bits;

    /// Current transaction instrumentation.
    PSI_transaction_locker *m_transaction_psi;

    /// Server status flags.
    uint m_server_status;
  };

  /**
    Class representing read-only attachable transaction, encapsulates
    knowledge how to backup state of current transaction, start
    read-only attachable transaction in SE, finalize it and then restore
    state of original transaction back. Also serves as a base class for
    read-write attachable transaction implementation.
  */
  class Attachable_trx
  {
  public:
    Attachable_trx(THD *thd);
    virtual ~Attachable_trx();
    virtual bool is_read_only() const { return true; }
  protected:
    /// THD instance.
    THD *m_thd;

    /// Transaction state data.
    Transaction_state m_trx_state;

  private:
    Attachable_trx(const Attachable_trx &);
    Attachable_trx &operator =(const Attachable_trx &);
  };

  /*
    Forward declaration of a read-write attachable transaction class.
    Its exact definition is located in the gtid module that proves its
    safe usage. Any potential customer to the class must beware of a danger
    of screwing the global transaction state through ha_commit_{stmt,trans}.
  */
  class Attachable_trx_rw;

  Attachable_trx *m_attachable_trx;

public:
  Transaction_ctx *get_transaction()
  { return m_transaction.get(); }

  const Transaction_ctx *get_transaction() const
  { return m_transaction.get(); }

  /**
    Changes the Transaction_ctx instance within THD-object. The previous
    Transaction_ctx instance is destroyed.

    @note this is a THD-internal operation which MUST NOT be used outside.

    @param transaction_ctx new Transaction_ctx instance to be associated with
    the THD-object.
  */
  void set_transaction(Transaction_ctx *transaction_ctx);

  Global_read_lock global_read_lock;
  Field      *dup_field;

  Vio* active_vio;

  /*
    This is to track items changed during execution of a prepared
    statement/stored procedure. It's created by
    register_item_tree_change() in memory root of THD, and freed in
    rollback_item_tree_changes(). For conventional execution it's always
    empty.
  */
  Item_change_list change_list;

  /*
    A permanent memory area of the statement. For conventional
    execution, the parsed tree and execution runtime reside in the same
    memory root. In this case stmt_arena points to THD. In case of
    a prepared statement or a stored procedure statement, thd->mem_root
    conventionally points to runtime memory, and thd->stmt_arena
    points to the memory of the PS/SP, where the parsed tree of the
    statement resides. Whenever you need to perform a permanent
    transformation of a parsed tree, you should allocate new memory in
    stmt_arena, to allow correct re-execution of PS/SP.
    Note: in the parser, stmt_arena == thd, even for PS/SP.
  */
  Query_arena *stmt_arena;

  /*
    map for tables that will be updated for a multi-table update query
    statement, for other query statements, this will be zero.
  */
  table_map table_map_for_update;

  /* Tells if LAST_INSERT_ID(#) was called for the current statement */
  bool arg_of_last_insert_id_function;
  /*
    ALL OVER THIS FILE, "insert_id" means "*automatically generated* value for
    insertion into an auto_increment column".
  */
  /*
    This is the first autogenerated insert id which was *successfully*
    inserted by the previous statement (exactly, if the previous statement
    didn't successfully insert an autogenerated insert id, then it's the one
    of the statement before, etc).
    It can also be set by SET LAST_INSERT_ID=# or SELECT LAST_INSERT_ID(#).
    It is returned by LAST_INSERT_ID().
  */
  ulonglong  first_successful_insert_id_in_prev_stmt;
  /*
    Variant of the above, used for storing in statement-based binlog. The
    difference is that the one above can change as the execution of a stored
    function progresses, while the one below is set once and then does not
    change (which is the value which statement-based binlog needs).
  */
  ulonglong  first_successful_insert_id_in_prev_stmt_for_binlog;
  /*
    This is the first autogenerated insert id which was *successfully*
    inserted by the current statement. It is maintained only to set
    first_successful_insert_id_in_prev_stmt when statement ends.
  */
  ulonglong  first_successful_insert_id_in_cur_stmt;
  /*
    We follow this logic:
    - when stmt starts, first_successful_insert_id_in_prev_stmt contains the
    first insert id successfully inserted by the previous stmt.
    - as stmt makes progress, handler::insert_id_for_cur_row changes;
    every time get_auto_increment() is called,
    auto_inc_intervals_in_cur_stmt_for_binlog is augmented with the
    reserved interval (if statement-based binlogging).
    - at first successful insertion of an autogenerated value,
    first_successful_insert_id_in_cur_stmt is set to
    handler::insert_id_for_cur_row.
    - when stmt goes to binlog,
    auto_inc_intervals_in_cur_stmt_for_binlog is binlogged if
    non-empty.
    - when stmt ends, first_successful_insert_id_in_prev_stmt is set to
    first_successful_insert_id_in_cur_stmt.
  */
  /*
    stmt_depends_on_first_successful_insert_id_in_prev_stmt is set when
    LAST_INSERT_ID() is used by a statement.
    If it is set, first_successful_insert_id_in_prev_stmt_for_binlog will be
    stored in the statement-based binlog.
    This variable is CUMULATIVE along the execution of a stored function or
    trigger: if one substatement sets it to 1 it will stay 1 until the
    function/trigger ends, thus making sure that
    first_successful_insert_id_in_prev_stmt_for_binlog does not change anymore
    and is propagated to the caller for binlogging.
  */
  bool       stmt_depends_on_first_successful_insert_id_in_prev_stmt;
  /*
    List of auto_increment intervals reserved by the thread so far, for
    storage in the statement-based binlog.
    Note that its minimum is not first_successful_insert_id_in_cur_stmt:
    assuming a table with an autoinc column, and this happens:
    INSERT INTO ... VALUES(3);
    SET INSERT_ID=3; INSERT IGNORE ... VALUES (NULL);
    then the latter INSERT will insert no rows
    (first_successful_insert_id_in_cur_stmt == 0), but storing "INSERT_ID=3"
    in the binlog is still needed; the list's minimum will contain 3.
    This variable is cumulative: if several statements are written to binlog
    as one (stored functions or triggers are used) this list is the
    concatenation of all intervals reserved by all statements.
  */
  Discrete_intervals_list auto_inc_intervals_in_cur_stmt_for_binlog;
  /* Used by replication and SET INSERT_ID */
  Discrete_intervals_list auto_inc_intervals_forced;
  /*
    There is BUG#19630 where statement-based replication of stored
    functions/triggers with two auto_increment columns breaks.
    We however ensure that it works when there is 0 or 1 auto_increment
    column; our rules are
    a) on master, while executing a top statement involving substatements,
    first top- or sub- statement to generate auto_increment values wins the
    exclusive right to see its values be written to binlog (the write
    will be done by the statement or its caller), and the losers won't see
    their values be written to binlog.
    b) on slave, while replicating a top statement involving substatements,
    first top- or sub- statement to need to read auto_increment values from
    the master's binlog wins the exclusive right to read them (so the losers
    won't read their values from binlog but instead generate on their own).
    a) implies that we mustn't backup/restore
    auto_inc_intervals_in_cur_stmt_for_binlog.
    b) implies that we mustn't backup/restore auto_inc_intervals_forced.

    If there are more than 1 auto_increment columns, then intervals for
    different columns may mix into the
    auto_inc_intervals_in_cur_stmt_for_binlog list, which is logically wrong,
    but there is no point in preventing this mixing by preventing intervals
    from the secondly inserted column to come into the list, as such
    prevention would be wrong too.
    What will happen in the case of
    INSERT INTO t1 (auto_inc) VALUES(NULL);
    where t1 has a trigger which inserts into an auto_inc column of t2, is
    that in binlog we'll store the interval of t1 and the interval of t2 (when
    we store intervals, soon), then in slave, t1 will use both intervals, t2
    will use none; if t1 inserts the same number of rows as on master,
    normally the 2nd interval will not be used by t1, which is fine. t2's
    values will be wrong if t2's internal auto_increment counter is different
    from what it was on master (which is likely). In 5.1, in mixed binlogging
    mode, row-based binlogging is used for such cases where two
    auto_increment columns are inserted.
  */
  inline void record_first_successful_insert_id_in_cur_stmt(ulonglong id_arg)
  {
    if (first_successful_insert_id_in_cur_stmt == 0)
      first_successful_insert_id_in_cur_stmt= id_arg;
  }
  inline ulonglong read_first_successful_insert_id_in_prev_stmt(void)
  {
    if (!stmt_depends_on_first_successful_insert_id_in_prev_stmt)
    {
      /* It's the first time we read it */
      first_successful_insert_id_in_prev_stmt_for_binlog=
        first_successful_insert_id_in_prev_stmt;
      stmt_depends_on_first_successful_insert_id_in_prev_stmt= 1;
    }
    return first_successful_insert_id_in_prev_stmt;
  }
  inline void reset_first_successful_insert_id()
  {
    arg_of_last_insert_id_function= FALSE;
    first_successful_insert_id_in_prev_stmt= 0;
    first_successful_insert_id_in_cur_stmt= 0;
    first_successful_insert_id_in_prev_stmt_for_binlog= 0;
    stmt_depends_on_first_successful_insert_id_in_prev_stmt= FALSE;
  }

  /*
    Used by Intvar_log_event::do_apply_event() and by "SET INSERT_ID=#"
    (mysqlbinlog). We'll soon add a variant which can take many intervals in
    argument.
  */
  inline void force_one_auto_inc_interval(ulonglong next_id)
  {
    auto_inc_intervals_forced.empty(); // in case of multiple SET INSERT_ID
    auto_inc_intervals_forced.append(next_id, ULLONG_MAX, 0);
  }

  /**
    Stores the result of the FOUND_ROWS() function.  Set at query end, stable
    throughout the query.
  */
  ulonglong  previous_found_rows;
  /**
    Dynamic, collected and set also in subqueries. Not stable throughout query.
    previous_found_rows is a snapshot of this take at query end making it
    stable throughout the next query, see update_previous_found_rows.
  */
  ulonglong  current_found_rows;

  /*
    Indicate if the gtid_executed table is being operated implicitly
    within current transaction. This happens because we are inserting
    a GTID specified through SET GTID_NEXT by user client or
    slave SQL thread/workers.
  */
  bool is_operating_gtid_table_implicitly;
  /*
    Indicate that a sub-statement is being operated implicitly
    within current transaction.
    As we don't want that this implicit sub-statement to consume the
    GTID of the actual transaction, we set it true at the beginning of
    the sub-statement and set it false again after "committing" the
    sub-statement.
    When it is true, the applier will not save the transaction owned
    gtid into mysql.gtid_executed table before transaction prepare, as
    it does when binlog is disabled, or binlog is enabled and
    log_slave_updates is disabled.
    Rpl_info_table::do_flush_info() uses this flag.
  */
  bool is_operating_substatement_implicitly;

private:
  /**
    Stores the result of ROW_COUNT() function.

    ROW_COUNT() function is a MySQL extention, but we try to keep it
    similar to ROW_COUNT member of the GET DIAGNOSTICS stack of the SQL
    standard (see SQL99, part 2, search for ROW_COUNT). It's value is
    implementation defined for anything except INSERT, DELETE, UPDATE.

    ROW_COUNT is assigned according to the following rules:

      - In my_ok():
        - for DML statements: to the number of affected rows;
        - for DDL statements: to 0.

      - In my_eof(): to -1 to indicate that there was a result set.

        We derive this semantics from the JDBC specification, where int
        java.sql.Statement.getUpdateCount() is defined to (sic) "return the
        current result as an update count; if the result is a ResultSet
        object or there are no more results, -1 is returned".

      - In my_error(): to -1 to be compatible with the MySQL C API and
        MySQL ODBC driver.

      - For SIGNAL statements: to 0 per WL#2110 specification (see also
        sql_signal.cc comment). Zero is used since that's the "default"
        value of ROW_COUNT in the Diagnostics Area.
  */

  longlong m_row_count_func;    /* For the ROW_COUNT() function */

public:
  inline longlong get_row_count_func() const
  {
    return m_row_count_func;
  }

  inline void set_row_count_func(longlong row_count_func)
  {
    m_row_count_func= row_count_func;
  }

  ha_rows    cuted_fields;

private:
  /**
    Number of rows we actually sent to the client, including "synthetic"
    rows in ROLLUP etc.
  */
  ha_rows m_sent_row_count;

  /**
    Number of rows read and/or evaluated for a statement. Used for
    slow log reporting.

    An examined row is defined as a row that is read and/or evaluated
    according to a statement condition, including in
    create_sort_index(). Rows may be counted more than once, e.g., a
    statement including ORDER BY could possibly evaluate the row in
    filesort() before reading it for e.g. update.
  */
  ha_rows m_examined_row_count;

private:
  USER_CONN *m_user_connect;

public:
  void set_user_connect(USER_CONN *uc);
  const USER_CONN* get_user_connect()
  { return m_user_connect; }

  void increment_user_connections_counter();
  void decrement_user_connections_counter();

  void increment_con_per_hour_counter();

  void increment_updates_counter();

  void increment_questions_counter();

  void time_out_user_resource_limits();

public:
  ha_rows get_sent_row_count() const
  { return m_sent_row_count; }

  ha_rows get_examined_row_count() const
  { return m_examined_row_count; }

  void set_sent_row_count(ha_rows count);
  void set_examined_row_count(ha_rows count);

  void inc_sent_row_count(ha_rows count);
  void inc_examined_row_count(ha_rows count);

  void inc_status_created_tmp_disk_tables();
  void inc_status_created_tmp_files();
  void inc_status_created_tmp_tables();
  void inc_status_select_full_join();
  void inc_status_select_full_range_join();
  void inc_status_select_range();
  void inc_status_select_range_check();
  void inc_status_select_scan();
  void inc_status_sort_merge_passes();
  void inc_status_sort_range();
  void inc_status_sort_rows(ha_rows count);
  void inc_status_sort_scan();
  void set_status_no_index_used();
  void set_status_no_good_index_used();

  const CHARSET_INFO *db_charset;
#if defined(ENABLED_PROFILING)
  PROFILING  profiling;
#endif

  /** Current stage progress instrumentation. */
  PSI_stage_progress *m_stage_progress_psi;
  /** Current statement digest. */
  sql_digest_state *m_digest;
  /** Current statement digest token array. */
  unsigned char *m_token_array;
  /** Top level statement digest. */
  sql_digest_state m_digest_state;

  /** Current statement instrumentation. */
  PSI_statement_locker *m_statement_psi;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  /** Current statement instrumentation state. */
  PSI_statement_locker_state m_statement_state;
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

  /** Current transaction instrumentation. */
  PSI_transaction_locker *m_transaction_psi;
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  /** Current transaction instrumentation state. */
  PSI_transaction_locker_state m_transaction_state;
#endif /* HAVE_PSI_TRANSACTION_INTERFACE */

  /** Idle instrumentation. */
  PSI_idle_locker *m_idle_psi;
#ifdef HAVE_PSI_IDLE_INTERFACE
  /** Idle instrumentation state. */
  PSI_idle_locker_state m_idle_state;
#endif /* HAVE_PSI_IDLE_INTERFACE */
  /** True if the server code is IDLE for this connection. */
  bool m_server_idle;

  /*
    Id of current query. Statement can be reused to execute several queries
    query_id is global in context of the whole MySQL server.
    ID is automatically generated from mutex-protected counter.
    It's used in handler code for various purposes: to check which columns
    from table are necessary for this select, to check if it's necessary to
    update auto-updatable fields (like auto_increment and timestamp).
  */
  query_id_t query_id;
  ulong      col_access;

  /* Statement id is thread-wide. This counter is used to generate ids */
  ulong      statement_id_counter;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  my_thread_t  real_id;                           /* For debugging */
  /**
    This counter is 32 bit because of the client protocol.

    @note It is not meant to be used for my_thread_self(), see @real_id for this.

    @note Set to reserved_thread_id on initialization. This is a magic
    value that is only to be used for temporary THDs not present in
    the global THD list.
  */
private:
  my_thread_id  m_thread_id;
public:
  /**
    Assign a value to m_thread_id by calling
    Global_THD_manager::get_new_thread_id().
  */
  void set_new_thread_id();
  my_thread_id thread_id() const { return m_thread_id; }
  uint	     tmp_table;
  uint	     server_status,open_options;
  enum enum_thread_type system_thread;
  /*
    Current or next transaction isolation level.
    When a connection is established, the value is taken from
    @@session.tx_isolation (default transaction isolation for
    the session), which is in turn taken from @@global.tx_isolation
    (the global value).
    If there is no transaction started, this variable
    holds the value of the next transaction's isolation level.
    When a transaction starts, the value stored in this variable
    becomes "actual".
    At transaction commit or rollback, we assign this variable
    again from @@session.tx_isolation.
    The only statement that can otherwise change the value
    of this variable is SET TRANSACTION ISOLATION LEVEL.
    Its purpose is to effect the isolation level of the next
    transaction in this session. When this statement is executed,
    the value in this variable is changed. However, since
    this statement is only allowed when there is no active
    transaction, this assignment (naturally) only affects the
    upcoming transaction.
    At the end of the current active transaction the value is
    be reset again from @@session.tx_isolation, as described
    above.
  */
  enum_tx_isolation tx_isolation;
  /*
    Current or next transaction access mode.
    See comment above regarding tx_isolation.
  */
  bool              tx_read_only;
  /*
    Transaction cannot be rolled back must be given priority.
    When two transactions conflict inside InnoDB, the one with
    greater priority wins.
  */
  int tx_priority;
  /*
    All transactions executed by this thread will have high
    priority mode, independent of tx_priority value.
  */
  int thd_tx_priority;

  enum_check_fields count_cuted_fields;

  // For user variables replication
  Prealloced_array<BINLOG_USER_VAR_EVENT*, 2> user_var_events;
  MEM_ROOT      *user_var_events_alloc; /* Allocate above array elements here */

  /**
    Used by MYSQL_BIN_LOG to maintain the commit queue for binary log
    group commit.
  */
  THD *next_to_commit;

  /**
    The member is served for marking a query that CREATEs or ALTERs
    a table declared with a TIMESTAMP column as dependent on
    @@session.explicit_defaults_for_timestamp.
    Is set to true by parser, unset at the end of the query.
    Possible marking in checked by binary logger.
  */
  bool binlog_need_explicit_defaults_ts;

  /**
     Functions to set and get transaction position.

     These functions are used to set the transaction position for the
     transaction written when committing this transaction.
   */
  /**@{*/
  void set_trans_pos(const char *file, my_off_t pos)
  {
    DBUG_ENTER("THD::set_trans_pos");
    DBUG_ASSERT(((file == 0) && (pos == 0)) || ((file != 0) && (pos != 0)));
    if (file)
    {
      DBUG_PRINT("enter", ("file: %s, pos: %llu", file, pos));
      // Only the file name should be used, not the full path
      m_trans_log_file= file + dirname_length(file);
      if (!m_trans_fixed_log_file)
        m_trans_fixed_log_file= (char*) alloc_root(&main_mem_root, FN_REFLEN+1);
      DBUG_ASSERT(strlen(m_trans_log_file) <= FN_REFLEN);
      strcpy(m_trans_fixed_log_file, m_trans_log_file);
    }
    else
    {
      m_trans_log_file= NULL;
      m_trans_fixed_log_file= NULL;
    }

    m_trans_end_pos= pos;
    DBUG_PRINT("return", ("m_trans_log_file: %s, m_trans_fixed_log_file: %s, "
                          "m_trans_end_pos: %llu", m_trans_log_file,
                          m_trans_fixed_log_file, m_trans_end_pos));
    DBUG_VOID_RETURN;
  }

  void get_trans_pos(const char **file_var, my_off_t *pos_var) const
  {
    DBUG_ENTER("THD::get_trans_pos");
    if (file_var)
      *file_var = m_trans_log_file;
    if (pos_var)
      *pos_var= m_trans_end_pos;
    DBUG_PRINT("return", ("file: %s, pos: %llu",
                          file_var ? *file_var : "<none>",
                          pos_var ? *pos_var : 0));
    DBUG_VOID_RETURN;
  }

  void get_trans_fixed_pos(const char **file_var, my_off_t *pos_var) const
  {
    DBUG_ENTER("THD::get_trans_fixed_pos");
    if (file_var)
      *file_var = m_trans_fixed_log_file;
    if (pos_var)
      *pos_var= m_trans_end_pos;
    DBUG_PRINT("return", ("file: %s, pos: %llu",
                          file_var ? *file_var : "<none>",
                          pos_var ? *pos_var : 0));
    DBUG_VOID_RETURN;
  }

  my_off_t get_trans_pos() { return m_trans_end_pos; }
  /**@}*/


  /*
    Error code from committing or rolling back the transaction.
  */
  enum Commit_error
  {
    CE_NONE= 0,
    CE_FLUSH_ERROR,
    CE_SYNC_ERROR,
    CE_COMMIT_ERROR,
    CE_ERROR_COUNT
  } commit_error;

  /*
    Define durability properties that engines may check to
    improve performance.
  */
  enum durability_properties durability_property;

  /*
    If checking this in conjunction with a wait condition, please
    include a check after enter_cond() if you want to avoid a race
    condition. For details see the implementation of awake(),
    especially the "broadcast" part.
  */
  enum killed_state
  {
    NOT_KILLED=0,
    KILL_BAD_DATA=1,
    KILL_CONNECTION=ER_SERVER_SHUTDOWN,
    KILL_QUERY=ER_QUERY_INTERRUPTED,
    KILL_TIMEOUT=ER_QUERY_TIMEOUT,
    KILLED_NO_VALUE      /* means neither of the states */
  };
  killed_state volatile killed;

  /* scramble - random string sent to client on handshake */
  char	     scramble[SCRAMBLE_LENGTH+1];

  /// @todo: slave_thread is completely redundant, we should use 'system_thread' instead /sven
  bool       slave_thread;
  bool	     no_errors;
  uchar      password;
  /**
    Set to TRUE if execution of the current compound statement
    can not continue. In particular, disables activation of
    CONTINUE or EXIT handlers of stored routines.
    Reset in the end of processing of the current user request, in
    @see mysql_reset_thd_for_next_command().
  */
  bool is_fatal_error;
  /**
    Set by a storage engine to request the entire
    transaction (that possibly spans multiple engines) to
    rollback. Reset in ha_rollback.
  */
  bool       transaction_rollback_request;
  /**
    TRUE if we are in a sub-statement and the current error can
    not be safely recovered until we left the sub-statement mode.
    In particular, disables activation of CONTINUE and EXIT
    handlers inside sub-statements. E.g. if it is a deadlock
    error and requires a transaction-wide rollback, this flag is
    raised (traditionally, MySQL first has to close all the reads
    via @see handler::ha_index_or_rnd_end() and only then perform
    the rollback).
    Reset to FALSE when we leave the sub-statement mode.
  */
  bool       is_fatal_sub_stmt_error;
  bool	     query_start_usec_used;
  bool       rand_used, time_zone_used;
  /* for IS NULL => = last_insert_id() fix in remove_eq_conds() */
  bool       substitute_null_with_insert_id;
  bool	     in_lock_tables;
  /**
    True if a slave error. Causes the slave to stop. Not the same
    as the statement execution error (is_error()), since
    a statement may be expected to return an error, e.g. because
    it returned an error on master, and this is OK on the slave.
  */
  bool       is_slave_error;
  bool       bootstrap;

  /**  is set if some thread specific value(s) used in a statement. */
  bool       thread_specific_used;
  /**
    is set if a statement accesses a temporary table created through
    CREATE TEMPORARY TABLE.
  */
  bool	     charset_is_system_charset, charset_is_collation_connection;
  bool       charset_is_character_set_filesystem;
  bool       enable_slow_log;   /* enable slow log for current statement */
  bool 	     got_warning;       /* Set on call to push_warning() */
  /* set during loop of derived table processing */
  bool       derived_tables_processing;
  bool       tablespace_op;     /* This is true in DISCARD/IMPORT TABLESPACE */

  /** Current SP-runtime context. */
  sp_rcontext *sp_runtime_ctx;
  sp_cache   *sp_proc_cache;
  sp_cache   *sp_func_cache;

  /** number of name_const() substitutions, see sp_head.cc:subst_spvars() */
  uint       query_name_consts;

  /*
    If we do a purge of binary logs, log index info of the threads
    that are currently reading it needs to be adjusted. To do that
    each thread that is using LOG_INFO needs to adjust the pointer to it
  */
  LOG_INFO*  current_linfo;
  NET*       slave_net;			// network connection from slave -> m.
  /* Used by the sys_var class to store temporary values */
  union
  {
    my_bool   my_bool_value;
    long      long_value;
    ulong     ulong_value;
    ulonglong ulonglong_value;
    double    double_value;
  } sys_var_tmp;

  struct {
    /*
      If true, mysql_bin_log::write(Log_event) call will not write events to
      binlog, and maintain 2 below variables instead (use
      mysql_bin_log.start_union_events to turn this on)
    */
    bool do_union;
    /*
      If TRUE, at least one mysql_bin_log::write(Log_event) call has been
      made after last mysql_bin_log.start_union_events() call.
    */
    bool unioned_events;
    /*
      If TRUE, at least one mysql_bin_log::write(Log_event e), where
      e.cache_stmt == TRUE call has been made after last
      mysql_bin_log.start_union_events() call.
    */
    bool unioned_events_trans;

    /*
      'queries' (actually SP statements) that run under inside this binlog
      union have thd->query_id >= first_query_id.
    */
    query_id_t first_query_id;
  } binlog_evt_union;

  /**
    Internal parser state.
    Note that since the parser is not re-entrant, we keep only one parser
    state here. This member is valid only when executing code during parsing.
  */
  Parser_state *m_parser_state;

  Locked_tables_list locked_tables_list;

  partition_info *work_part_info;

#ifndef EMBEDDED_LIBRARY
  /**
    Array of active audit plugins which have been used by this THD.
    This list is later iterated to invoke release_thd() on those
    plugins.
  */
  Plugin_array audit_class_plugins;
  /**
    Array of bits indicating which audit classes have already been
    added to the list of audit plugins which are currently in use.
  */
  Prealloced_array<unsigned long, 11> audit_class_mask;
#endif

#if defined(ENABLED_DEBUG_SYNC)
  /* Debug Sync facility. See debug_sync.cc. */
  struct st_debug_sync_control *debug_sync_control;
#endif /* defined(ENABLED_DEBUG_SYNC) */

  // We don't want to load/unload plugins for unit tests.
  bool m_enable_plugins;

  THD(bool enable_plugins= true);

  /*
    The THD dtor is effectively split in two:
      THD::release_resources() and ~THD().

    We want to minimize the time we hold LOCK_thd_list,
    so when destroying a global thread, do:

    thd->release_resources()
    Global_THD_manager::get_instance()->remove_thd();
    delete thd;
   */
  ~THD();

  void release_resources();
  bool release_resources_done() const { return m_release_resources_done; }

private:
  bool m_release_resources_done;
  bool cleanup_done;
  void cleanup(void);

public:
  void init(void);
  /*
    Initialize memory roots necessary for query processing and (!)
    pre-allocate memory for it. We can't do that in THD constructor because
    there are use cases (acl_init, watcher threads,
    killing mysqld) where it's vital to not allocate excessive and not used
    memory. Note, that we still don't return error from init_for_queries():
    if preallocation fails, we should notice that at the first call to
    alloc_root.
  */
  void init_for_queries(Relay_log_info *rli= NULL);
  void cleanup_connection(void);
  void cleanup_after_query();
  bool store_globals();
  void restore_globals();

  inline void set_active_vio(Vio* vio)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    active_vio = vio;
    mysql_mutex_unlock(&LOCK_thd_data);
  }

  inline void set_ssl(Vio* vio)
  {
#ifdef HAVE_OPENSSL
    mysql_mutex_lock(&LOCK_thd_data);
    m_SSL = (SSL*) vio->ssl_arg;
    mysql_mutex_unlock(&LOCK_thd_data);
#else
    m_SSL = NULL;
#endif
  }

  inline void clear_active_vio()
  {
    mysql_mutex_lock(&LOCK_thd_data);
    active_vio = 0;
    m_SSL = NULL;
    mysql_mutex_unlock(&LOCK_thd_data);
  }

  enum_vio_type get_vio_type();

  void shutdown_active_vio();
  void awake(THD::killed_state state_to_set);

  /** Disconnect the associated communication endpoint. */
  void disconnect(bool server_shutdown= false);

#ifndef MYSQL_CLIENT
  enum enum_binlog_query_type {
    /* The query can be logged in row format or in statement format. */
    ROW_QUERY_TYPE,

    /* The query has to be logged in statement format. */
    STMT_QUERY_TYPE,

    QUERY_TYPE_COUNT
  };

  int binlog_query(enum_binlog_query_type qtype,
                   const char *query, size_t query_len, bool is_trans,
                   bool direct, bool suppress_use,
                   int errcode);
#endif

  // Begin implementation of MDL_context_owner interface.

  void enter_cond(mysql_cond_t *cond, mysql_mutex_t* mutex,
                  const PSI_stage_info *stage, PSI_stage_info *old_stage,
                  const char *src_function, const char *src_file,
                  int src_line)
  {
    DBUG_ENTER("THD::enter_cond");
    mysql_mutex_assert_owner(mutex);
    /*
      Sic: We don't lock LOCK_current_cond here.
      If we did, we could end up in deadlock with THD::awake()
      which locks current_mutex while LOCK_current_cond is locked.
    */
    current_mutex= mutex;
    current_cond= cond;
    enter_stage(stage, old_stage, src_function, src_file, src_line);
    DBUG_VOID_RETURN;
  }

  void exit_cond(const PSI_stage_info *stage,
                 const char *src_function, const char *src_file,
                 int src_line)
  {
    DBUG_ENTER("THD::exit_cond");
    /*
      current_mutex must be unlocked _before_ LOCK_current_cond is
      locked (if that would not be the case, you'll get a deadlock if someone
      does a THD::awake() on you).
    */
    mysql_mutex_assert_not_owner(current_mutex);
    mysql_mutex_lock(&LOCK_current_cond);
    current_mutex= NULL;
    current_cond= NULL;
    mysql_mutex_unlock(&LOCK_current_cond);
    enter_stage(stage, NULL, src_function, src_file, src_line);
    DBUG_VOID_RETURN;
  }

  virtual int is_killed() { return killed; }
  virtual THD* get_thd() { return this; }

  /**
    A callback to the server internals that is used to address
    special cases of the locking protocol.
    Invoked when acquiring an exclusive lock, for each thread that
    has a conflicting shared metadata lock.

    This function aborts waiting of the thread on a data lock, to make
    it notice the pending exclusive lock and back off.

    @note This function does not wait for the thread to give away its
          locks. Waiting is done outside for all threads at once.

    @param ctx_in_use           The MDL context owner (thread) to wake up.
    @param needs_thr_lock_abort Indicates that to wake up thread
                                this call needs to abort its waiting
                                on table-level lock.
   */
  virtual void notify_shared_lock(MDL_context_owner *ctx_in_use,
                                  bool needs_thr_lock_abort);

  virtual bool notify_hton_pre_acquire_exclusive(const MDL_key *mdl_key,
                                                 bool *victimized)
  {
    return ha_notify_exclusive_mdl(this, mdl_key, HA_NOTIFY_PRE_EVENT,
                                   victimized);
  }

  virtual void notify_hton_post_release_exclusive(const MDL_key *mdl_key)
  {
    bool unused_arg;
    ha_notify_exclusive_mdl(this, mdl_key, HA_NOTIFY_POST_EVENT, &unused_arg);
  }

  /**
    Provide thread specific random seed for MDL_context's PRNG.

    Note that even if two connections will request seed during handling of
    statements which were started at exactly the same time, and thus will
    get the same values in PRNG at the start, they will naturally diverge
    soon, since calls to PRNG in MDL subsystem are affected by many factors
    making process quite random. OTOH the fact that we use time as a seed
    gives more randomness and thus better coverage in tests as opposed to
    using thread_id for the same purpose.
  */
  virtual uint get_rand_seed() { return (uint)start_utime; }

  // End implementation of MDL_context_owner interface.

  inline bool is_strict_mode() const
  {
    return MY_TEST(variables.sql_mode & (MODE_STRICT_TRANS_TABLES |
                                         MODE_STRICT_ALL_TABLES));
  }
  inline Time_zone *time_zone()
  {
    time_zone_used= 1;
    return variables.time_zone;
  }
  inline time_t query_start()
  {
    return start_time.tv_sec;
  }
  inline long query_start_usec()
  {
    query_start_usec_used= 1;
    return start_time.tv_usec;
  }
  inline timeval query_start_timeval()
  {
    query_start_usec_used= true;
    return start_time;
  }
  timeval query_start_timeval_trunc(uint decimals);
  inline void set_time()
  {
    start_utime= utime_after_lock= my_micro_time();
    if (user_time.tv_sec || user_time.tv_usec)
    {
      start_time= user_time;
    }
    else
      my_micro_time_to_timeval(start_utime, &start_time);

#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_start_time)(start_time.tv_sec);
#endif
  }
  inline void set_current_time()
  {
    my_micro_time_to_timeval(my_micro_time(), &start_time);
#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_start_time)(start_time.tv_sec);
#endif
  }
  inline void set_time(const struct timeval *t)
  {
    start_time= user_time= *t;
    start_utime= utime_after_lock= my_micro_time();
#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_start_time)(start_time.tv_sec);
#endif
  }
  /*TODO: this will be obsolete when we have support for 64 bit my_time_t */
  inline bool	is_valid_time()
  {
    return (IS_TIME_T_VALID_FOR_TIMESTAMP(start_time.tv_sec));
  }
  void set_time_after_lock()
  {
    utime_after_lock= my_micro_time();
    MYSQL_SET_STATEMENT_LOCK_TIME(m_statement_psi, (utime_after_lock - start_utime));
  }
  ulonglong current_utime()  { return my_micro_time(); }
  /**
   Update server status after execution of a top level statement.

   Currently only checks if a query was slow, and assigns
   the status accordingly.
   Evaluate the current time, and if it exceeds the long-query-time
   setting, mark the query as slow.
  */
  void update_server_status()
  {
    ulonglong end_utime_of_query= current_utime();
    if (end_utime_of_query > utime_after_lock + variables.long_query_time)
      server_status|= SERVER_QUERY_WAS_SLOW;
  }
  inline ulonglong found_rows(void)
  {
    return previous_found_rows;
  }

  /*
    Call when it is clear that the query is ended and we have collected the
    right value for current_found_rows. Calling this method makes a snapshot of
    that value and makes it ready and stable for subsequent FOUND_ROWS() call
    in the next statement.
  */
  inline void update_previous_found_rows()
  {
    previous_found_rows= current_found_rows;
  }

  /**
    Returns TRUE if session is in a multi-statement transaction mode.

    OPTION_NOT_AUTOCOMMIT: When autocommit is off, a multi-statement
    transaction is implicitly started on the first statement after a
    previous transaction has been ended.

    OPTION_BEGIN: Regardless of the autocommit status, a multi-statement
    transaction can be explicitly started with the statements "START
    TRANSACTION", "BEGIN [WORK]", "[COMMIT | ROLLBACK] AND CHAIN", etc.

    Note: this doesn't tell you whether a transaction is active.
    A session can be in multi-statement transaction mode, and yet
    have no active transaction, e.g., in case of:
    set @@autocommit=0;
    set @a= 3;                                     <-- these statements don't
    set transaction isolation level serializable;  <-- start an active
    flush tables;                                  <-- transaction

    I.e. for the above scenario this function returns TRUE, even
    though no active transaction has begun.
    @sa in_active_multi_stmt_transaction()
  */
  inline bool in_multi_stmt_transaction_mode() const
  {
    return variables.option_bits & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN);
  }
  /**
    TRUE if the session is in a multi-statement transaction mode
    (@sa in_multi_stmt_transaction_mode()) *and* there is an
    active transaction, i.e. there is an explicit start of a
    transaction with BEGIN statement, or implicit with a
    statement that uses a transactional engine.

    For example, these scenarios don't start an active transaction
    (even though the server is in multi-statement transaction mode):

    set @@autocommit=0;
    select * from nontrans_table;
    set @var=TRUE;
    flush tables;

    Note, that even for a statement that starts a multi-statement
    transaction (i.e. select * from trans_table), this
    flag won't be set until we open the statement's tables
    and the engines register themselves for the transaction
    (see trans_register_ha()),
    hence this method is reliable to use only after
    open_tables() has completed.

    Why do we need a flag?
    ----------------------
    We need to maintain a (at first glance redundant)
    session flag, rather than looking at thd->transaction.all.ha_list
    because of explicit start of a transaction with BEGIN.

    I.e. in case of
    BEGIN;
    select * from nontrans_t1; <-- in_active_multi_stmt_transaction() is true
  */
  inline bool in_active_multi_stmt_transaction() const
  {
    return server_status & SERVER_STATUS_IN_TRANS;
  }
  inline bool fill_derived_tables()
  {
    return !stmt_arena->is_stmt_prepare() && !lex->only_view_structure();
  }
  inline bool fill_information_schema_tables()
  {
    return !stmt_arena->is_stmt_prepare();
  }

  LEX_CSTRING *make_lex_string(LEX_CSTRING *lex_str,
                              const char *str, size_t length,
                              bool allocate_lex_string);
  LEX_STRING *make_lex_string(LEX_STRING *lex_str,
                              const char* str, size_t length,
                              bool allocate_lex_string);

  bool convert_string(LEX_STRING *to, const CHARSET_INFO *to_cs,
		      const char *from, size_t from_length,
		      const CHARSET_INFO *from_cs);

  bool convert_string(LEX_CSTRING *to, const CHARSET_INFO *to_cs,
		      const char *from, size_t from_length,
		      const CHARSET_INFO *from_cs)
  {
    LEX_STRING tmp;
    if (convert_string(&tmp, to_cs, from, from_length, from_cs))
      return true;
    to->str= tmp.str;
    to->length= tmp.length;
    return false;
  }

  bool convert_string(String *s, const CHARSET_INFO *from_cs,
                      const CHARSET_INFO *to_cs);

  void add_changed_table(TABLE *table);
  void add_changed_table(const char *key, long key_length);
  int send_explain_fields(Query_result *result);

  /**
    Clear the current error, if any.
    We do not clear is_fatal_error or is_fatal_sub_stmt_error since we
    assume this is never called if the fatal error is set.
    @todo: To silence an error, one should use Internal_error_handler
    mechanism. In future this function will be removed.
  */
  inline void clear_error()
  {
    DBUG_ENTER("clear_error");
    if (get_stmt_da()->is_error())
      get_stmt_da()->reset_diagnostics_area();
    is_slave_error= false;
    DBUG_VOID_RETURN;
  }

  inline bool is_classic_protocol()
  {
    DBUG_ENTER("THD::is_classic_protocol");
    DBUG_PRINT("info", ("type=%d", get_protocol()->type()));
    switch (get_protocol()->type())
    {
    case Protocol::PROTOCOL_BINARY:
    case Protocol::PROTOCOL_TEXT:
      DBUG_RETURN(true);
    default:
      break;
    }
    DBUG_RETURN(false);
  }

#ifndef EMBEDDED_LIBRARY
  /** Return FALSE if connection to client is broken. */
  virtual bool is_connected()
  {
    /*
      All system threads (e.g., the slave IO thread) are connected but
      not using vio. So this function always returns true for all
      system threads.
    */
    if (system_thread)
      return true;

    if (is_classic_protocol())
      return get_protocol()->connection_alive() &&
             vio_is_connected(get_protocol_classic()->get_vio());
    else
      return get_protocol()->connection_alive();
  }
#else
  virtual bool is_connected() { return true; }
#endif
  /**
    Mark the current error as fatal. Warning: this does not
    set any error, it sets a property of the error, so must be
    followed or prefixed with my_error().
  */
  inline void fatal_error()
  {
    DBUG_ASSERT(get_stmt_da()->is_error() || killed);
    is_fatal_error= 1;
    DBUG_PRINT("error",("Fatal error set"));
  }
  /**
    TRUE if there is an error in the error stack.

    Please use this method instead of direct access to
    net.report_error.

    If TRUE, the current (sub)-statement should be aborted.
    The main difference between this member and is_fatal_error
    is that a fatal error can not be handled by a stored
    procedure continue handler, whereas a normal error can.

    To raise this flag, use my_error().
  */
  inline bool is_error() const { return get_stmt_da()->is_error(); }

  /// Returns first Diagnostics Area for the current statement.
  Diagnostics_area *get_stmt_da()
  { return m_stmt_da; }

  /// Returns first Diagnostics Area for the current statement.
  const Diagnostics_area *get_stmt_da() const
  { return m_stmt_da; }

  /// Returns the second Diagnostics Area for the current statement.
  const Diagnostics_area *get_stacked_da() const
  { return get_stmt_da()->stacked_da(); }

  /**
    Returns thread-local Diagnostics Area for parsing.
    We need to have a clean DA in case errors or warnings are thrown
    during parsing, but we can't just reset the main DA in case we
    have a diagnostic statement on our hand that needs the old DA
    to answer questions about the previous execution.
    Keeping a static per-thread DA for parsing is less costly than
    allocating a temporary one for each statement we parse.
  */
  Diagnostics_area *get_parser_da()
  { return &m_parser_da; }


  /**
    Returns thread-local Diagnostics Area to be used by query rewrite plugins.
    Query rewrite plugins use their own diagnostics area. The reason is that
    they are invoked right before and right after parsing, and we don't want
    conditions raised by plugins in either statement nor parser DA until we
    know which type of statement we have parsed.

    @note The diagnostics area is instantiated the first time it is asked for.
  */
  Diagnostics_area *get_query_rewrite_plugin_da()
  {
    return m_query_rewrite_plugin_da_ptr;
  }

  /**
    Push the given Diagnostics Area on top of the stack, making
    it the new first Diagnostics Area. Conditions in the new second
    Diagnostics Area will be copied to the new first Diagnostics Area.

    @param da   Diagnostics Area to be come the top of
                the Diagnostics Area stack.
    @param copy_conditions
                Copy the conditions from the new second Diagnostics Area
                to the new first Diagnostics Area, as per SQL standard.
  */
  void push_diagnostics_area(Diagnostics_area *da, bool copy_conditions= true)
  {
    get_stmt_da()->push_diagnostics_area(this, da, copy_conditions);
    m_stmt_da= da;
  }

  /// Pop the top DA off the Diagnostics Area stack.
  void pop_diagnostics_area()
  {
    m_stmt_da= get_stmt_da()->pop_diagnostics_area();
  }

public:
  const CHARSET_INFO *charset() const
  { return variables.character_set_client; }
  void update_charset();

  void change_item_tree(Item **place, Item *new_value)
  {
    /* TODO: check for OOM condition here */
    if (!stmt_arena->is_conventional())
    {
      DBUG_PRINT("info",
                 ("change_item_tree place %p old_value %p new_value %p",
                  place, *place, new_value));
      if (new_value)
        new_value->set_runtime_created(); /* Note the change of item tree */
      nocheck_register_item_tree_change(place, new_value);
    }
    *place= new_value;
  }

  /**
    Remember that place was updated with new_value so it can be restored
    by rollback_item_tree_changes().

    @param[in] place the location that will change, and whose old value
               we need to remember for restoration
    @param[in] the new value about to be inserted into *place, remember
               for associative lookup, see replace_rollback_place()
  */
  void nocheck_register_item_tree_change(Item **place, Item *new_value);

  /**
    Find and update change record of an underlying item based on the new
    value for a place.

    If we have already saved a position to rollback for new_value,
    forget that rollback position and register the new place instead,
    typically because a transformation has made the old place irrelevant.
    If not, a no-op.

    @param new_place  The new location in which we have presumably saved
                      the new value, but which need to be rolled back to
                      the old value.
                      This location must also contain the new value.
  */
  void replace_rollback_place(Item **new_place);

  /**
    Restore locations set by calls to nocheck_register_item_tree_change().  The
    value to be restored depends on whether replace_rollback_place()
    has been called. If not, we restore the original value. If it has been
    called, we restore the one supplied by the latest call to
    replace_rollback_place()
  */
  void rollback_item_tree_changes();

  /*
    Cleanup statement parse state (parse tree, lex) and execution
    state after execution of a non-prepared SQL statement.
  */
  void end_statement();
  inline int killed_errno() const
  {
    killed_state killed_val; /* to cache the volatile 'killed' */
    return (killed_val= killed) != KILL_BAD_DATA ? killed_val : 0;
  }
  inline void send_kill_message() const
  {
    int err= killed_errno();
    if (err && !get_stmt_da()->is_set())
    {
      if ((err == KILL_CONNECTION) && !abort_loop)
        err = KILL_QUERY;
      /*
        KILL is fatal because:
        - if a condition handler was allowed to trap and ignore a KILL, one
        could create routines which the DBA could not kill
        - INSERT/UPDATE IGNORE should fail: if KILL arrives during
        JOIN::optimize(), statement cannot possibly run as its caller expected
        => "OK" would be misleading the caller.
      */
      my_message(err, ER(err), MYF(ME_FATALERROR));
    }
  }
  void set_status_var_init();
  void reset_n_backup_open_tables_state(Open_tables_backup *backup);
  void restore_backup_open_tables_state(Open_tables_backup *backup);
  void reset_sub_statement_state(Sub_statement_state *backup, uint new_state);
  void restore_sub_statement_state(Sub_statement_state *backup);
  void set_n_backup_active_arena(Query_arena *set, Query_arena *backup);
  void restore_active_arena(Query_arena *set, Query_arena *backup);

public:
  /**
    Start a read-only attachable transaction.
    There must be no active attachable transactions (in other words, there can
    be only one active attachable transaction at a time).
  */
  void begin_attachable_ro_transaction();

  /**
    Start a read-write attachable transaction.
    All the read-only class' requirements apply.
    Additional requirements are documented along the class
    declaration.
  */
  void begin_attachable_rw_transaction();

  /**
    End an active attachable transaction. Applies to both the read-only
    and the read-write versions.
    Note, that the read-write attachable transaction won't be terminated
    inside this method.
    To invoke the function there must be active attachable transaction.
  */
  void end_attachable_transaction();

  /**
    @return true if there is an active attachable transaction.
  */
  bool is_attachable_ro_transaction_active() const
  { return m_attachable_trx != NULL && m_attachable_trx->is_read_only(); }

  /**
    @return true if there is an active rw attachable transaction.
  */
  bool is_attachable_rw_transaction_active() const;

public:
  /*
    @todo Make these methods private or remove them completely.  Only
    decide_logging_format should call them. /Sven
  */
  inline void set_current_stmt_binlog_format_row_if_mixed()
  {
    DBUG_ENTER("set_current_stmt_binlog_format_row_if_mixed");
    /*
      This should only be called from decide_logging_format.

      @todo Once we have ensured this, uncomment the following
      statement, remove the big comment below that, and remove the
      in_sub_stmt==0 condition from the following 'if'.
    */
    /* DBUG_ASSERT(in_sub_stmt == 0); */
    /*
      If in a stored/function trigger, the caller should already have done the
      change. We test in_sub_stmt to prevent introducing bugs where people
      wouldn't ensure that, and would switch to row-based mode in the middle
      of executing a stored function/trigger (which is too late, see also
      reset_current_stmt_binlog_format_row()); this condition will make their
      tests fail and so force them to propagate the
      lex->binlog_row_based_if_mixed upwards to the caller.
    */
    if ((variables.binlog_format == BINLOG_FORMAT_MIXED) &&
        (in_sub_stmt == 0))
      set_current_stmt_binlog_format_row();

    DBUG_VOID_RETURN;
  }
  inline void set_current_stmt_binlog_format_row()
  {
    DBUG_ENTER("set_current_stmt_binlog_format_row");
    current_stmt_binlog_format= BINLOG_FORMAT_ROW;
    DBUG_VOID_RETURN;
  }
  inline void clear_current_stmt_binlog_format_row()
  {
    DBUG_ENTER("clear_current_stmt_binlog_format_row");
    current_stmt_binlog_format= BINLOG_FORMAT_STMT;
    DBUG_VOID_RETURN;
  }
  inline void reset_current_stmt_binlog_format_row()
  {
    DBUG_ENTER("reset_current_stmt_binlog_format_row");
    /*
      If there are temporary tables, don't reset back to
      statement-based. Indeed it could be that:
      CREATE TEMPORARY TABLE t SELECT UUID(); # row-based
      # and row-based does not store updates to temp tables
      # in the binlog.
      INSERT INTO u SELECT * FROM t; # stmt-based
      and then the INSERT will fail as data inserted into t was not logged.
      So we continue with row-based until the temp table is dropped.
      If we are in a stored function or trigger, we mustn't reset in the
      middle of its execution (as the binary logging way of a stored function
      or trigger is decided when it starts executing, depending for example on
      the caller (for a stored function: if caller is SELECT or
      INSERT/UPDATE/DELETE...).
    */
    DBUG_PRINT("debug",
               ("temporary_tables: %s, in_sub_stmt: %s, system_thread: %s",
                YESNO(temporary_tables), YESNO(in_sub_stmt),
                show_system_thread(system_thread)));
    if (in_sub_stmt == 0)
    {
      if (variables.binlog_format == BINLOG_FORMAT_ROW)
        set_current_stmt_binlog_format_row();
      else if (temporary_tables == NULL)
        clear_current_stmt_binlog_format_row();
    }
    DBUG_VOID_RETURN;
  }

#ifdef HAVE_REPLICATION
  /**
    Copies variables.gtid_next to
    ((Slave_worker *)rli_slave)->currently_executing_gtid,
    if this is a slave thread.
  */
  void set_currently_executing_gtid_for_slave_thread();
#endif

  /// Return the value of @@gtid_next_list: either a Gtid_set or NULL.
  Gtid_set *get_gtid_next_list()
  {
    return variables.gtid_next_list.is_non_null ?
      variables.gtid_next_list.gtid_set : NULL;
  }

  /// Return the value of @@gtid_next_list: either a Gtid_set or NULL.
  const Gtid_set *get_gtid_next_list_const() const
  {
    return const_cast<THD *>(this)->get_gtid_next_list();
  }

  /**
    Return true if the statement/transaction cache is currently empty,
    false otherwise.

    @param is_transactional if true, check the transaction cache.
    If false, check the statement cache.
  */
  bool is_binlog_cache_empty(bool is_transactional);

  /**
    The GTID of the currently owned transaction.

    ==== Modes of ownership ====

    The following modes of ownership are possible:

    - owned_gtid.sidno==0: the thread does not own any transaction.

    - owned_gtid.sidno==THD::OWNED_SIDNO_ANONYMOUS(==-2): the thread
      owns an anonymous transaction

    - owned_gtid.sidno>0 and owned_gtid.gno>0: the thread owns a GTID
      transaction.

    - (owned_gtid.sidno==THD::OWNED_SIDNO_GTID_SET(==-1): this is
      currently not used.  It was reserved for the case where multiple
      GTIDs are owned (using gtid_next_list).  This was one idea to
      make GTIDs work with NDB: due to the epoch concept, multiple
      transactions can be combined into one in NDB, and therefore a
      single transaction on a slave can have multiple GTIDs.)

    ==== Life cycle of ownership ====

    Generally, transaction ownership starts when the transaction is
    assigned its GTID and ends when the transaction commits or rolls
    back.  On a master (GTID_NEXT=AUTOMATIC), the GTID is assigned
    just before binlog flush; on a slave (GTID_NEXT=UUID:NUMBER or
    GTID_NEXT=ANONYMOUS) it is assigned before starting the
    transaction.

    A new client always starts with owned_gtid.sidno=0.

    Ownership can be acquired in the following ways:

    A1. If GTID_NEXT = 'AUTOMATIC' and GTID_MODE = OFF/OFF_PERMISSIVE:
        The thread acquires anonymous ownership in
        gtid_state->generate_automatic_gtid called from
        MYSQL_BIN_LOG::write_gtid.

    A2. If GTID_NEXT = 'AUTOMATIC' and GTID_MODE = ON/ON_PERMISSIVE:
        The thread generates the GTID and acquires ownership in
        gtid_state->generate_automatic_gtid called from
        MYSQL_BIN_LOG::write_gtid.

    A3. If GTID_NEXT = 'UUID:NUMBER': The thread acquires ownership in
        the following ways:

        - In a client, the SET GTID_NEXT statement acquires ownership.

        - The slave's analogy to a clients SET GTID_NEXT statement is
          Gtid_log_event::do_apply_event.  So the slave acquires
          ownership in this function.

        Note: if the GTID UUID:NUMBER is already included in
        GTID_EXECUTED, then the transaction must be skipped (the GTID
        auto-skip feature).  Thus, ownership is *not* acquired in this
        case and owned_gtid.sidno==0.

    A4. If GTID_NEXT = 'ANONYMOUS':

        - In a client, the SET GTID_NEXT statement acquires ownership.

        - In a slave thread, Gtid_log_event::do_apply_event acquires
          ownership.

        - Contrary to the case of GTID_NEXT='UUID:NUMBER', it is
          allowed to execute two transactions in sequence without
          changing GTID_NEXT (cf. R1 and R2 below).  Both transactions
          should be executed as anonymous transactions.  But ownership
          is released when the first transaction commits.  Therefore,
          when GTID_NEXT='ANONYMOUS', we also acquire anonymous
          ownership when starting to execute a statement, in
          gtid_reacquire_ownership_if_anonymous called from
          gtid_pre_statement_checks (usually called from
          mysql_execute_command).

    A5. Slave applier threads start in a special mode, having
        GTID_NEXT='NOT_YET_DETERMINED'.  This mode cannot be set in a
        regular client.  When GTID_NEXT=NOT_YET_DETERMINED, the slave
        thread is postponing the decision of the value of GTID_NEXT
        until it has more information.  There are three cases:

        - If the first transaction of the relay log has a
          Gtid_log_event, then it will set GTID_NEXT=GTID:NUMBER and
          acquire GTID ownership in Gtid_log_event::do_apply_event.

        - If the first transaction of the relay log has a
          Anonymous_gtid_log_event, then it will set
          GTID_NEXT=ANONYMOUS and acquire anonymous ownership in
          Gtid_log_event::do_apply_event.

        - If the relay log was received from a pre-5.7.6 master with
          GTID_MODE=OFF (or a pre-5.6 master), then there are neither
          Gtid_log_events nor Anonymous_log_events in the relay log.
          In this case, the slave sets GTID_NEXT=ANONYMOUS and
          acquires anonymous ownership when executing a
          Query_log_event (Query_log_event::do_apply_event calls
          mysql_parse which calls gtid_pre_statement_checks which
          calls gtid_reacquire_ownership_if_anonymous).

    Ownership is released in the following ways:

    R1. A thread that holds GTID ownership releases ownership at
        transaction commit or rollback.  If GTID_NEXT=AUTOMATIC, all
        is fine. If GTID_NEXT=UUID:NUMBER, the UUID:NUMBER cannot be
        used for another transaction, since only one transaction can
        have any given GTID.  To avoid the user mistake of forgetting
        to set back GTID_NEXT, on commit we set
        thd->variables.gtid_next.type=UNDEFINED_GROUP.  Then, any
        statement that user tries to execute other than SET GTID_NEXT
        will generate an error.

    R2. A thread that holds anonymous ownership releases ownership at
        transaction commit or rollback.  In this case there is no harm
        in leaving GTID_NEXT='ANONYMOUS', so
        thd->variables.gtid_next.type will remain ANONYMOUS_GROUP and
        not UNDEFINED_GROUP.

    There are statements that generate multiple transactions in the
    binary log. This includes the following:

    M1. DROP TABLE that is used with multiple tables, and the tables
        belong to more than one of the following groups: non-temporary
        table, temporary transactional table, temporary
        non-transactional table.  DROP TABLE is split into one
        transaction for each of these groups of tables.

    M2. DROP DATABASE that fails e.g. because rmdir fails. Then a
        single DROP TABLE is generated, which lists all tables that
        were dropped before the failure happened. But if the list of
        tables is big, and grows over a limit, the statement will be
        split into multiple statements.

    M3. CREATE TABLE ... SELECT that is logged in row format.  Then
        the server generates a single CREATE statement, followed by a
        BEGIN ... row events ... COMMIT transaction.

    M4. A statement that updates both transactional and
        non-transactional tables in the same statement, and is logged
        in row format.  Then it generates one transaction for the
        non-transactional row updates, followed by one transaction for
        the transactional row updates.

    M5. CALL is executed as multiple transactions and logged as
        multiple transactions.

    The general rules for multi-transaction statements are:

    - If GTID_NEXT=AUTOMATIC and GTID_MODE=ON or ON_PERMISSIVE, one
      GTID should be generated for each transaction within the
      statement. Therefore, ownership must be released after each
      commit so that a new GTID can be generated by the next
      transaction. Typically mysql_bin_log.commit() is called to
      achieve this. (Note that some of these statements are currently
      disallowed when GTID_MODE=ON.)

    - If GTID_NEXT=AUTOMATIC and GTID_MODE=OFF or OFF_PERMISSIVE, one
      Anonymous_gtid_log_event should be generated for each
      transaction within the statement. Similar to the case above, we
      call mysql_bin_log.commit() and release ownership between
      transactions within the statement.

      This works for all the special cases M1-M5 except M4.  When a
      statement writes both non-transactional and transactional
      updates to the binary log, both the transaction cache and the
      statement cache are flushed within the same call to
      flush_thread_caches(THD) from within the binary log group commit
      code.  At that point we cannot use mysql_bin_log.commit().
      Instead we release ownership using direct calls to
      gtid_state->release_anonymous_ownership() and
      thd->clear_owned_gtids() from binlog_cache_mngr::flush.

    - If GTID_NEXT=ANONYMOUS, anonymous ownership must be *preserved*
      between transactions within the statement, to prevent that a
      concurrent SET GTID_MODE=ON makes it impossible to log the
      statement. To avoid that ownership is released if
      mysql_bin_log.commit() is called, we set
      thd->is_commit_in_middle_of_statement before calling
      mysql_bin_log.commit.  Note that we must set this flag only if
      GTID_NEXT=ANONYMOUS, not if the transaction is anonymous when
      GTID_NEXT=AUTOMATIC and GTID_MODE=OFF.

      This works for all the special cases M1-M5 except M4.  When a
      statement writes non-transactional updates in the middle of a
      transaction, but keeps some transactional updates in the
      transaction cache, then it is not easy to know at the time of
      calling mysql_bin_log.commit() whether anonymous ownership needs
      to be preserved or not.  Instead, we directly check if the
      transaction cache is nonempty before releasing anonymous
      ownership inside Gtid_state::update_gtids_impl.

    - If GTID_NEXT='UUID:NUMBER', it is impossible to log a
      multi-transaction statement, since each GTID can only be used by
      one transaction. Therefore, an error must be generated in this
      case.  Errors are generated in different ways for the different
      statement types:

      - DROP TABLE: we can detect the situation before it happens,
        since the table type is known once the tables are opened. So
        we generate an error before even executing the statement.

      - DROP DATABASE: we can't detect the situation until it is too
        late; the tables have already been dropped and we cannot log
        anything meaningful.  So we don't log at all.

      - CREATE TABLE ... SELECT: this is not allowed when
        enforce_gtid_consistency is ON; the statement will be
        forbidden in is_ddl_gtid_compatible.

      - Statements that update both transactional and
        non-transactional tables are disallowed when GTID_MODE=ON, so
        this normally does not happen. However, it can happen if the
        slave uses a different engine type than the master, so that a
        statement that updates InnoDB+InnoDB on master updates
        InnoDB+MyISAM on slave.  In this case the statement will be
        forbidden in is_dml_gtid_compatible and will not be allowed to
        execute.

      - CALL: the second statement will generate an error because
        GTID_NEXT is 'undefined'.  Note that this situation can only
        happen if user does it on purpose: A CALL on master is logged
        as multiple statements, so a slave never executes CALL with
        GTID_NEXT='UUID:NUMBER'.

    Finally, ownership release is suppressed in one more corner case:

    C1. Administration statements including OPTIMIZE TABLE, REPAIR
        TABLE, or ANALYZE TABLE are written to the binary log even if
        they fail.  This means that the thread first calls
        trans_rollack, and then writes the statement to the binlog.
        Rollback normally releases ownership.  But ownership must be
        kept until writing the binlog.  The solution is that these
        statements set thd->skip_gtid_rollback=true before calling
        trans_rollback, and Gtid_state::update_on_rollback does not
        release ownership if the flag is set.

    @todo It would probably be better to encapsulate this more, maybe
    use Gtid_specification instead of Gtid.
  */
  Gtid owned_gtid;
  static const int OWNED_SIDNO_GTID_SET= -1;
  static const int OWNED_SIDNO_ANONYMOUS= -2;

  /**
    For convenience, this contains the SID component of the GTID
    stored in owned_gtid.
  */
  rpl_sid owned_sid;

#ifdef HAVE_GTID_NEXT_LIST
  /**
    If this thread owns a set of GTIDs (i.e., GTID_NEXT_LIST != NULL),
    then this member variable contains the subset of those GTIDs that
    are owned by this thread.
  */
  Gtid_set owned_gtid_set;
#endif

  /*
   Replication related context.

   @todo: move more parts of replication related fields in THD to inside this
          class.
  */
  Rpl_thd_context rpl_thd_ctx;

  void clear_owned_gtids()
  {
    if (owned_gtid.sidno == OWNED_SIDNO_GTID_SET)
    {
#ifdef HAVE_GTID_NEXT_LIST
      owned_gtid_set.clear();
#else
      DBUG_ASSERT(0);
#endif
    }
    owned_gtid.clear();
    owned_sid.clear();
    owned_gtid.dbug_print(NULL, "set owned_gtid in clear_owned_gtids");
  }

  /*
    There are some statements (like OPTIMIZE TABLE, ANALYZE TABLE and
    REPAIR TABLE) that might call trans_rollback_stmt() and also will be
    sucessfully executed and will have to go to the binary log.
    For these statements, the skip_gtid_rollback flag must be set to avoid
    problems when the statement is executed with a GTID_NEXT set to GTID_GROUP
    (like the SQL thread do when applying events from other server).
    When this flag is set, a call to gtid_rollback() will do nothing.
  */
  bool skip_gtid_rollback;
  /*
    There are some statements (like DROP DATABASE that fails on rmdir
    and gets rewritten to multiple DROP TABLE statements) that may
    call trans_commit_stmt() before it has written all statements to
    the binlog.  When using GTID_NEXT = ANONYMOUS, such statements
    should not release ownership of the anonymous transaction until
    all statements have been written to the binlog.  To prevent that
    update_gtid_impl releases ownership, such statements must set this
    flag.
  */
  bool is_commit_in_middle_of_statement;
  /*
    True while the transaction is executing, if one of
    is_ddl_gtid_consistent or is_dml_gtid_consistent returned false.
  */
  bool has_gtid_consistency_violation;

  const LEX_CSTRING &db() const
  { return m_db; }

  /**
    Set the current database; use deep copy of C-string.

    @param new_db     a pointer to the new database name.
    @param new_db_len length of the new database name.

    Initialize the current database from a NULL-terminated string with
    length. If we run out of memory, we free the current database and
    return TRUE.  This way the user will notice the error as there will be
    no current database selected (in addition to the error message set by
    malloc).

    @note This operation just sets {db, db_length}. Switching the current
    database usually involves other actions, like switching other database
    attributes including security context. In the future, this operation
    will be made private and more convenient interface will be provided.

    @return Operation status
      @retval false Success
      @retval true  Out-of-memory error
  */
  bool set_db(const LEX_CSTRING &new_db)
  {
    bool result;
    /*
      Acquiring mutex LOCK_thd_data as we either free the memory allocated
      for the database and reallocating the memory for the new db or memcpy
      the new_db to the db.
    */
    mysql_mutex_lock(&LOCK_thd_data);
    /* Do not reallocate memory if current chunk is big enough. */
    if (m_db.str && new_db.str && m_db.length >= new_db.length)
      memcpy(const_cast<char*>(m_db.str), new_db.str, new_db.length+1);
    else
    {
      my_free(const_cast<char*>(m_db.str));
      m_db= NULL_CSTR;
      if (new_db.str)
        m_db.str= my_strndup(key_memory_THD_db,
                             new_db.str, new_db.length,
                             MYF(MY_WME | ME_FATALERROR));
    }
    m_db.length= m_db.str ? new_db.length : 0;
    mysql_mutex_unlock(&LOCK_thd_data);
    result= new_db.str && !m_db.str;
#ifdef HAVE_PSI_THREAD_INTERFACE
    if (result)
      PSI_THREAD_CALL(set_thread_db)(new_db.str,
                                     static_cast<int>(new_db.length));
#endif
    return result;
  }

  /**
    Set the current database; use shallow copy of C-string.

    @param new_db     a pointer to the new database name.
    @param new_db_len length of the new database name.

    @note This operation just sets {db, db_length}. Switching the current
    database usually involves other actions, like switching other database
    attributes including security context. In the future, this operation
    will be made private and more convenient interface will be provided.
  */
  void reset_db(const LEX_CSTRING &new_db)
  {
    m_db.str= new_db.str;
    m_db.length= new_db.length;
#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_db)(new_db.str,
                                   static_cast<int>(new_db.length));
#endif
  }
  /*
    Copy the current database to the argument. Use the current arena to
    allocate memory for a deep copy: current database may be freed after
    a statement is parsed but before it's executed.
  */
  bool copy_db_to(char **p_db, size_t *p_db_length)
  {
    if (m_db.str == NULL)
    {
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
      return TRUE;
    }
    *p_db= strmake(m_db.str, m_db.length);
    *p_db_length= m_db.length;
    return false;
  }
  thd_scheduler scheduler;

public:
  /**
    Save the performance schema thread instrumentation
    associated with this user session.
    @param psi Performance schema thread instrumentation
  */
  void set_psi(PSI_thread *psi);
  /**
    Read the performance schema thread instrumentation
    associated with this user session.
    This method is safe to use from a different thread.
  */
  PSI_thread* get_psi();

private:
  /**
    Performance schema thread instrumentation for this session.
    This member is maintained using atomic operations,
    do not access it directly.
    @sa set_psi
    @sa get_psi
  */
  PSI_thread* m_psi;

public:
  inline Internal_error_handler *get_internal_handler()
  { return m_internal_handler; }

  /**
    Add an internal error handler to the thread execution context.
    @param handler the exception handler to add
  */
  void push_internal_handler(Internal_error_handler *handler);

  /**
    Handle a sql condition.
    @param sql_errno the condition error number
    @param sqlstate the condition sqlstate
    @param level the condition level
    @param msg the condition message text
    @return true if the condition is handled
  */
  bool handle_condition(uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level *level,
                        const char* msg);

  /**
    Remove the error handler last pushed.
  */
  Internal_error_handler *pop_internal_handler();

  Opt_trace_context opt_trace; ///< optimizer trace of current statement
  /**
    Raise an exception condition.
    @param code the MYSQL_ERRNO error code of the error
  */
  void raise_error(uint code);

  /**
    Raise an exception condition, with a formatted message.
    @param code the MYSQL_ERRNO error code of the error
  */
  void raise_error_printf(uint code, ...);

  /**
    Raise a completion condition (warning).
    @param code the MYSQL_ERRNO error code of the warning
  */
  void raise_warning(uint code);

  /**
    Raise a completion condition (warning), with a formatted message.
    @param code the MYSQL_ERRNO error code of the warning
  */
  void raise_warning_printf(uint code, ...);

  /**
    Raise a completion condition (note), with a fixed message.
    @param code the MYSQL_ERRNO error code of the note
  */
  void raise_note(uint code);

  /**
    Raise an completion condition (note), with a formatted message.
    @param code the MYSQL_ERRNO error code of the note
  */
  void raise_note_printf(uint code, ...);

private:
  /*
    Only the implementation of the SIGNAL and RESIGNAL statements
    is permitted to raise SQL conditions in a generic way,
    or to raise them by bypassing handlers (RESIGNAL).
    To raise a SQL condition, the code should use the public
    raise_error() or raise_warning() methods provided by class THD.
  */
  friend class Sql_cmd_common_signal;
  friend class Sql_cmd_signal;
  friend class Sql_cmd_resignal;
  friend void push_warning(THD*, Sql_condition::enum_severity_level, uint,
                           const char*);
  friend void my_message_sql(uint, const char *, myf);

  /**
    Raise a generic SQL condition.
    @param sql_errno the condition error number
    @param sqlstate the condition SQLSTATE
    @param level the condition level
    @param msg the condition message text
    @param use_condition_handler Invoke the handle_condition.
    @return The condition raised, or NULL
  */
  Sql_condition*
  raise_condition(uint sql_errno,
                  const char* sqlstate,
                  Sql_condition::enum_severity_level level,
                  const char* msg,
                  bool use_condition_handler= true);

public:
  void set_command(enum enum_server_command command);

  inline enum enum_server_command get_command() const
  { return m_command; }

  /**
    For safe and protected access to the query string, the following
    rules should be followed:
    1: Only the owner (current_thd) can set the query string.
       This will be protected by LOCK_thd_query.
    2: The owner (current_thd) can read the query string without
       locking LOCK_thd_query.
    3: Other threads must lock LOCK_thd_query before reading
       the query string.

    This means that write-write conflicts are avoided by LOCK_thd_query.
    Read(by owner or other thread)-write(other thread) are disallowed.
    Read(other thread)-write(by owner) conflicts are avoided by LOCK_thd_query.
    Read(by owner)-write(by owner) won't happen as THD=thread.
  */
  const LEX_CSTRING &query() const
  {
#ifndef DBUG_OFF
    if (current_thd != this)
      mysql_mutex_assert_owner(&LOCK_thd_query);
#endif
    return m_query_string;
  }

  /**
    The current query in normalized form. The format is intended to be
    identical to the digest text of performance_schema, but not limited in
    size. In this case the parse tree is traversed as opposed to a (limited)
    token buffer. The string is allocated by this Statement and will be
    available until the next call to this function or this object is deleted.

    @note We have no protection against out-of-memory errors as this function
    relies on the Item::print() interface which does not propagate errors.

    @return The current query in normalized form.
  */
  const String normalized_query()
  {
    m_normalized_query.mem_free();
    lex->unit->print(&m_normalized_query, QT_NORMALIZED_FORMAT);
    return m_normalized_query;
  }

  /**
    Assign a new value to thd->m_query_string.
    Protected with the LOCK_thd_query mutex.
  */
  void set_query(const char *query_arg, size_t query_length_arg)
  {
    LEX_CSTRING tmp= { query_arg, query_length_arg };
    set_query(tmp);
  }
  void set_query(const LEX_CSTRING& query_arg);
  void reset_query() { set_query(LEX_CSTRING()); }

  /**
    Assign a new value to thd->query_id.
    Protected with the LOCK_thd_data mutex.
  */
  void set_query_id(query_id_t new_query_id)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    query_id= new_query_id;
    mysql_mutex_unlock(&LOCK_thd_data);
  }

  /**
    Assign a new value to open_tables.
    Protected with the LOCK_thd_data mutex.
  */
  void set_open_tables(TABLE *open_tables_arg)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    open_tables= open_tables_arg;
    mysql_mutex_unlock(&LOCK_thd_data);
  }

  /**
    Assign a new value to is_killable
    Protected with the LOCK_thd_data mutex.
  */
  void set_is_killable(bool is_killable_arg)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    is_killable= is_killable_arg;
    mysql_mutex_unlock(&LOCK_thd_data);
  }

  void enter_locked_tables_mode(enum_locked_tables_mode mode_arg)
  {
    DBUG_ASSERT(locked_tables_mode == LTM_NONE);

    if (mode_arg == LTM_LOCK_TABLES)
    {
      /*
        When entering LOCK TABLES mode we should set explicit duration
        for all metadata locks acquired so far in order to avoid releasing
        them till UNLOCK TABLES statement.
        We don't do this when entering prelocked mode since sub-statements
        don't release metadata locks and restoring status-quo after leaving
        prelocking mode gets complicated.
      */
      mdl_context.set_explicit_duration_for_all_locks();
    }

    locked_tables_mode= mode_arg;
  }
  void leave_locked_tables_mode();
  int decide_logging_format(TABLE_LIST *tables);
  /**
    is_dml_gtid_compatible() and is_ddl_gtid_compatible() check if the
    statement that is about to be processed will safely get a
    GTID. Currently, the following cases may lead to errors
    (e.g. duplicated GTIDs) and as such are forbidden:

     1. DML statements that mix non-transactional updates with
        transactional updates.

     2. Transactions that use non-transactional tables after
        having used transactional tables.

     3. CREATE...SELECT statement;

     4. CREATE TEMPORARY TABLE or DROP TEMPORARY TABLE within a
        transaction

    The first two conditions have to be checked in
    decide_logging_format, because that's where we know if the table
    is transactional or not.  These are implemented in
    is_dml_gtid_compatible().

    The third and fourth conditions have to be checked in
    mysql_execute_command because (1) that prevents implicit commit
    from being executed if the statement fails; (2) DROP TEMPORARY
    TABLE does not invoke decide_logging_format.  These are
    implemented in is_ddl_gtid_compatible().

    In the cases where GTID violations generate errors,
    is_ddl_gtid_compatible() needs to be called before the implicit
    pre-commit, so that there is no implicit commit if the statement
    fails.

    In the cases where GTID violations do not generate errors,
    is_ddl_gtid_compatible() needs to be called after the implicit
    pre-commit, because in these cases the function will increase the
    global counter automatic_gtid_violating_transaction_count or
    anonymous_gtid_violating_transaction_count.  If there is an
    ongoing transaction, the implicit commit will commit the
    transaction, which will call update_gtids_impl, which should
    decrease the counters depending on whether the *old* was violating
    GTID-consistency or not.  Thus, we should increase the counters
    only after the old transaction is committed.

    @param some_transactional_table true if the statement updates some
    transactional table; false otherwise.

    @param some_non_transactional_table true if the statement updates
    some non-transactional table; false otherwise.

    @param non_transactional_tables_are_tmp true if all updated
    non-transactional tables are temporary.

    @retval true if the statement is compatible;
    @retval false if the statement is not compatible.
  */
  bool
  is_dml_gtid_compatible(bool some_transactional_table,
                         bool some_non_transactional_table,
                         bool non_transactional_tables_are_tmp);
  bool is_ddl_gtid_compatible();
  void binlog_invoker() { m_binlog_invoker= TRUE; }
  bool need_binlog_invoker() { return m_binlog_invoker; }
  void get_definer(LEX_USER *definer);
  void set_invoker(const LEX_STRING *user, const LEX_STRING *host)
  {
    m_invoker_user.str= user->str;
    m_invoker_user.length= user->length;
    m_invoker_host.str= host->str;
    m_invoker_host.length= host->length;
  }
  LEX_CSTRING get_invoker_user() const { return m_invoker_user; }
  LEX_CSTRING get_invoker_host() const { return m_invoker_host; }
  bool has_invoker() { return m_invoker_user.str != NULL; }

  void mark_transaction_to_rollback(bool all);

private:

  /** The current internal error handler for this thread, or NULL. */
  Internal_error_handler *m_internal_handler;

  /**
    The lex to hold the parsed tree of conventional (non-prepared) queries.
    Whereas for prepared and stored procedure statements we use an own lex
    instance for each new query, for conventional statements we reuse
    the same lex. (@see mysql_parse for details).
  */
  LEX main_lex;
  /**
    This memory root is used for two purposes:
    - for conventional queries, to allocate structures stored in main_lex
    during parsing, and allocate runtime data (execution plan, etc.)
    during execution.
    - for prepared queries, only to allocate runtime data. The parsed
    tree itself is reused between executions and thus is stored elsewhere.
  */
  MEM_ROOT main_mem_root;
  Diagnostics_area main_da;
  Diagnostics_area m_parser_da;              /**< cf. get_parser_da() */
  Diagnostics_area m_query_rewrite_plugin_da;
  Diagnostics_area *m_query_rewrite_plugin_da_ptr;

  Diagnostics_area *m_stmt_da;

  /**
    It will be set TURE if CURRENT_USER() is called in account management
    statements or default definer is set in CREATE/ALTER SP, SF, Event,
    TRIGGER or VIEW statements.

    Current user will be binlogged into Query_log_event if current_user_used
    is TRUE; It will be stored into m_invoker_host and m_invoker_user by SQL
    thread.
   */
  bool m_binlog_invoker;

  /**
    It points to the invoker in the Query_log_event.
    SQL thread use it as the default definer in CREATE/ALTER SP, SF, Event,
    TRIGGER or VIEW statements or current user in account management
    statements if it is not NULL.
   */
  LEX_CSTRING m_invoker_user;
  LEX_CSTRING m_invoker_host;
  friend class Protocol_classic;

private:
  /**
    Optimizer cost model for server operations.
  */
  Cost_model_server m_cost_model;

public:
  /**
    Initialize the optimizer cost model.

    This function should be called each time a new query is started.
  */
  void init_cost_model() { m_cost_model.init(); }

  /**
    Retrieve the optimizer cost model for this connection.
  */
  const Cost_model_server* cost_model() const { return &m_cost_model; }

  Session_tracker session_tracker;
  Session_sysvar_resource_manager session_sysvar_res_mgr;

  void parse_error_at(const YYLTYPE &location, const char *s= NULL);

  /**
    Send name and type of result to client.

    Sum fields has table name empty and field_name.

    @param list         List of items to send to client
    @param flag         Bit mask with the following functions:
                          - 1 send number of rows
                          - 2 send default values
                          - 4 don't write eof packet

    @retval
      false ok
    @retval
      true Error  (Note that in this case the error is not sent to the client)
  */

  bool send_result_metadata(List<Item> *list, uint flags);

  /**
    Send one result set row.

    @param row_items a collection of column values for that row

    @return Error status.
    @retval true  Error.
    @retval false Success.
  */

  bool send_result_set_row(List<Item> *row_items);


  /*
    Send the status of the current statement execution over network.

    In MySQL, there are two types of SQL statements: those that return
    a result set and those that return status information only.

    If a statement returns a result set, it consists of 3 parts:
    - result set meta-data
    - variable number of result set rows (can be 0)
    - followed and terminated by EOF or ERROR packet

    Once the  client has seen the meta-data information, it always
    expects an EOF or ERROR to terminate the result set. If ERROR is
    received, the result set rows are normally discarded (this is up
    to the client implementation, libmysql at least does discard them).
    EOF, on the contrary, means "successfully evaluated the entire
    result set". Since we don't know how many rows belong to a result
    set until it's evaluated, EOF/ERROR is the indicator of the end
    of the row stream. Note, that we can not buffer result set rows
    on the server -- there may be an arbitrary number of rows. But
    we do buffer the last packet (EOF/ERROR) in the Diagnostics_area and
    delay sending it till the very end of execution (here), to be able to
    change EOF to an ERROR if commit failed or some other error occurred
    during the last cleanup steps taken after execution.

    A statement that does not return a result set doesn't send result
    set meta-data either. Instead it returns one of:
    - OK packet
    - ERROR packet.
    Similarly to the EOF/ERROR of the previous statement type, OK/ERROR
    packet is "buffered" in the Diagnostics Area and sent to the client
    in the end of statement.

    @note This method defines a template, but delegates actual
    sending of data to virtual Protocol::send_{ok,eof,error}. This
    allows for implementation of protocols that "intercept" ok/eof/error
    messages, and store them in memory, etc, instead of sending to
    the client.

    @pre  The Diagnostics Area is assigned or disabled. It can not be empty
          -- we assume that every SQL statement or COM_* command
          generates OK, ERROR, or EOF status.

    @post The status information is encoded to protocol format and sent to the
          client.

    @return We conventionally return void, since the only type of error
            that can happen here is a NET (transport) error, and that one
            will become visible when we attempt to read from the NET the
            next command.
            Diagnostics_area::is_sent is set for debugging purposes only.
  */

  void send_statement_status();

  /**
    This is only used by master dump threads.
    When the master receives a new connection from a slave with a
    UUID (for slave versions >= 5.6)/server_id(for slave versions < 5.6)
    that is already connected, it will set this flag TRUE
    before killing the old slave connection.
  */
  bool duplicate_slave_id;

  /**
    Claim all the memory used by the THD object.
    This method is to keep memory instrumentation statistics
    updated, when an object is transfered across threads.
  */
  void claim_memory_ownership();

  bool is_a_srv_session() const { return is_a_srv_session_thd; }
  void mark_as_srv_session() { is_a_srv_session_thd= true; }
private:
  /**
    Variable to mark if the object is part of a Srv_session object, which
    aggregates THD.
  */
  bool is_a_srv_session_thd;
};

/**
  A simple holder for the Prepared Statement Query_arena instance in THD.
  The class utilizes RAII technique to not forget to restore the THD arena.
*/
class Prepared_stmt_arena_holder
{
public:
  /**
    Constructs a new object, activates the persistent arena if requested and if
    a prepared statement or a stored procedure statement is being executed.

    @param thd                    Thread context.
    @param activate_now_if_needed Attempt to activate the persistent arena in
                                  the constructor or not.
  */
  Prepared_stmt_arena_holder(THD *thd, bool activate_now_if_needed= true)
   :m_thd(thd),
    m_arena(NULL)
  {
    if (activate_now_if_needed &&
        !m_thd->stmt_arena->is_conventional() &&
        m_thd->mem_root != m_thd->stmt_arena->mem_root)
    {
      m_thd->set_n_backup_active_arena(m_thd->stmt_arena, &m_backup);
      m_arena= m_thd->stmt_arena;
    }
  }

  /**
    Deactivate the persistent arena (restore the previous arena) if it has
    been activated.
  */
  ~Prepared_stmt_arena_holder()
  {
    if (is_activated())
      m_thd->restore_active_arena(m_arena, &m_backup);
  }

  bool is_activated() const
  { return m_arena != NULL; }

private:
  /// The thread context to work with.
  THD *const m_thd;

  /// The arena set by this holder (by activate()).
  Query_arena *m_arena;

  /// The arena state to be restored.
  Query_arena m_backup;
};


/** A short cut for thd->get_stmt_da()->set_ok_status(). */

inline void
my_ok(THD *thd, ulonglong affected_rows= 0, ulonglong id= 0,
        const char *message= NULL)
{
  thd->set_row_count_func(affected_rows);
  thd->get_stmt_da()->set_ok_status(affected_rows, id, message);
}


/** A short cut for thd->get_stmt_da()->set_eof_status(). */

inline void
my_eof(THD *thd)
{
  thd->set_row_count_func(-1);
  thd->get_stmt_da()->set_eof_status(thd);
  if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
  {
    ((Transaction_state_tracker *)
     thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER))
      ->add_trx_state(thd, TX_RESULT_SET);
  }
}

#define tmp_disable_binlog(A)       \
  {ulonglong tmp_disable_binlog__save_options= (A)->variables.option_bits; \
  (A)->variables.option_bits&= ~OPTION_BIN_LOG

#define reenable_binlog(A)   (A)->variables.option_bits= tmp_disable_binlog__save_options;}

LEX_STRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_STRING *lex_str, const char* str, size_t length,
                     bool allocate_lex_string);
LEX_CSTRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_CSTRING *lex_str, const char* str, size_t length,
                     bool allocate_lex_string);

inline LEX_STRING *lex_string_copy(MEM_ROOT *root, LEX_STRING *dst,
                                   const char *src, size_t src_len)
{
  return make_lex_string_root(root, dst, src, src_len, false);
}

inline LEX_STRING *lex_string_copy(MEM_ROOT *root, LEX_STRING *dst,
                                   const LEX_STRING &src)
{
  return make_lex_string_root(root, dst, src.str, src.length, false);
}

inline LEX_STRING *lex_string_copy(MEM_ROOT *root, LEX_STRING *dst,
                                   const char *src)
{
  return make_lex_string_root(root, dst, src, strlen(src), false);
}

/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class sql_exchange :public Sql_alloc
{
public:
  Field_separators field;
  Line_separators line;
  enum enum_filetype filetype; /* load XML, Added by Arnold & Erik */
  const char *file_name;
  bool dumpfile;
  ulong skip_lines;
  const CHARSET_INFO *cs;
  sql_exchange(const char *name, bool dumpfile_flag,
               enum_filetype filetype_arg= FILETYPE_CSV);
  bool escaped_given(void);
};

/*
  This is used to get result from a query
*/

class JOIN;

class Query_result :public Sql_alloc {
protected:
  THD *thd;
  SELECT_LEX_UNIT *unit;
public:
  /**
    Number of records estimated in this result.
    Valid only for materialized derived tables/views.
  */
  ha_rows estimated_rowcount;
  Query_result()
    :thd(current_thd), unit(NULL), estimated_rowcount(0)
  { }
  virtual ~Query_result() {};
  /**
    Change wrapped Query_result.

    Replace the wrapped query result object with new_result and call
    prepare() and prepare2() on new_result.

    This base class implementation doesn't wrap other Query_results.

    @param new_result The new query result object to wrap around

    @retval false Success
    @retval true  Error
  */
  virtual bool change_query_result(Query_result *new_result)
  {
    return false;
  }
  /// @return true if an interceptor object is needed for EXPLAIN
  virtual bool need_explain_interceptor() const { return false; }

  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u)
  {
    unit= u;
    return 0;
  }
  virtual int prepare2(void) { return 0; }
  /*
    Because of peculiarities of prepared statements protocol
    we need to know number of columns in the result set (if
    there is a result set) apart from sending columns metadata.
  */
  virtual uint field_count(List<Item> &fields) const
  { return fields.elements; }
  virtual bool send_result_set_metadata(List<Item> &list, uint flags)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual bool initialize_tables (JOIN *join=0) { return 0; }
  virtual void send_error(uint errcode,const char *err)
  { my_message(errcode, err, MYF(0)); }
  virtual bool send_eof()=0;
  /**
    Check if this query returns a result set and therefore is allowed in
    cursors and set an error message if it is not the case.

    @retval FALSE     success
    @retval TRUE      error, an error message is set
  */
  virtual bool check_simple_select() const
  {
    my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
    return TRUE;
  }
  virtual void abort_result_set() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup()
  {
    /* do nothing */
  }
  void set_thd(THD *thd_arg) { thd= thd_arg; }

  /**
    If we execute EXPLAIN SELECT ... LIMIT (or any other EXPLAIN query)
    we have to ignore offset value sending EXPLAIN output rows since
    offset value belongs to the underlying query, not to the whole EXPLAIN.
  */
  void reset_offset_limit_cnt() { unit->offset_limit_cnt= 0; }

#ifdef EMBEDDED_LIBRARY
  virtual void begin_dataset() {}
#else
  void begin_dataset() {}
#endif
};


/*
  Base class for Query_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class Query_result_interceptor: public Query_result
{
public:
  Query_result_interceptor() {}  /* Remove gcc warning */
  uint field_count(List<Item> &fields) const { return 0; }
  bool send_result_set_metadata(List<Item> &fields, uint flag) { return FALSE; }
};


class Query_result_send :public Query_result {
  /**
    True if we have sent result set metadata to the client.
    In this case the client always expects us to end the result
    set with an eof or error packet
  */
  bool is_result_set_started;
public:
  Query_result_send() :is_result_set_started(false) {}
  bool send_result_set_metadata(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const { return FALSE; }
  void abort_result_set();
  /**
    Cleanup an instance of this class for re-use
    at next execution of a prepared statement/
    stored procedure statement.
  */
  virtual void cleanup()
  {
    is_result_set_started= false;
  }
};


class Query_result_to_file :public Query_result_interceptor {
protected:
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];

public:
  Query_result_to_file(sql_exchange *ex) :exchange(ex), file(-1),row_count(0L)
  { path[0]=0; }
  ~Query_result_to_file();
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void cleanup();
};


#define ESCAPE_CHARS "ntrb0ZN" // keep synchronous with READ_INFO::unescape


/*
 List of all possible characters of a numeric value text representation.
*/
#define NUMERIC_CHARS ".0123456789e+-"


class Query_result_export :public Query_result_to_file {
  size_t field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  int field_term_char; // first char of FIELDS TERMINATED BY or MAX_INT
  /*
    The is_ambiguous_field_sep field is true if a value of the field_sep_char
    field is one of the 'n', 't', 'r' etc characters
    (see the READ_INFO::unescape method and the ESCAPE_CHARS constant value).
  */
  bool is_ambiguous_field_sep;
  /*
     The is_ambiguous_field_term is true if field_sep_char contains the first
     char of the FIELDS TERMINATED BY (ENCLOSED BY is empty), and items can
     contain this character.
  */
  bool is_ambiguous_field_term;
  /*
    The is_unsafe_field_sep field is true if a value of the field_sep_char
    field is one of the '0'..'9', '+', '-', '.' and 'e' characters
    (see the NUMERIC_CHARS constant value).
  */
  bool is_unsafe_field_sep;
  bool fixed_row_size;
  const CHARSET_INFO *write_cs; // output charset
public:
  Query_result_export(sql_exchange *ex) : Query_result_to_file(ex) {}
  ~Query_result_export()
  {
    thd->set_sent_row_count(row_count);
  }
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class Query_result_dump :public Query_result_to_file {
public:
  Query_result_dump(sql_exchange *ex) : Query_result_to_file(ex) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};

typedef Mem_root_array<Item*, true> Func_ptr_array;

/**
  Object containing parameters used when creating and using temporary
  tables. Temporary tables created with the help of this object are
  used only internally by the query execution engine.
*/
class Temp_table_param :public Sql_alloc
{
public:
  List<Item> copy_funcs;
  Copy_field *copy_field, *copy_field_end;
  uchar	    *group_buff;
  Func_ptr_array *items_to_copy;             /* Fields in tmp table */
  MI_COLUMNDEF *recinfo,*start_recinfo;

  /**
    After temporary table creation, points to an index on the table
    created depending on the purpose of the table - grouping,
    duplicate elimination, etc. There is at most one such index.
  */
  KEY *keyinfo;
  ha_rows end_write_records;
  /**
    Number of normal fields in the query, including those referred to
    from aggregate functions. Hence, "SELECT `field1`,
    SUM(`field2`) from t1" sets this counter to 2.

    @see count_field_types
  */
  uint	field_count;
  /**
    Number of fields in the query that have functions. Includes both
    aggregate functions (e.g., SUM) and non-aggregates (e.g., RAND).
    Also counts functions referred to from aggregate functions, i.e.,
    "SELECT SUM(RAND())" sets this counter to 2.

    @see count_field_types
  */
  uint  func_count;
  /**
    Number of fields in the query that have aggregate functions. Note
    that the optimizer may choose to optimize away these fields by
    replacing them with constants, in which case sum_func_count will
    need to be updated.

    @see opt_sum_query, count_field_types
  */
  uint  sum_func_count;
  uint  hidden_field_count;
  uint	group_parts,group_length,group_null_parts;
  uint	quick_group;
  /**
    Number of outer_sum_funcs i.e the number of set functions that are
    aggregated in a query block outer to this subquery.

    @see count_field_types
  */
  uint  outer_sum_func_count;
  /**
    Enabled when we have atleast one outer_sum_func. Needed when used
    along with distinct.

    @see create_tmp_table
  */
  bool  using_outer_summary_function;
  CHARSET_INFO *table_charset;
  bool schema_table;
  /*
    True if GROUP BY and its aggregate functions are already computed
    by a table access method (e.g. by loose index scan). In this case
    query execution should not perform aggregation and should treat
    aggregate functions as normal functions.
  */
  bool precomputed_group_by;
  bool force_copy_fields;
  /**
    TRUE <=> don't actually create table handler when creating the result
    table. This allows range optimizer to add indexes later.
    Used for materialized derived tables/views.
    @see TABLE_LIST::update_derived_keys.
  */
  bool skip_create_table;
  /*
    If TRUE, create_tmp_field called from create_tmp_table will convert
    all BIT fields to 64-bit longs. This is a workaround the limitation
    that MEMORY tables cannot index BIT columns.
  */
  bool bit_fields_as_long;

  /*
    Generally, pk of internal temp table can be used as unique key to eliminate
    the duplication of records. But because Innodb doesn't support disable PK
    (cluster key)when doing operations mixed UNION ALL and UNION, the PK can't
    be used as the unique key in such a case.
  */
  bool can_use_pk_for_unique;

  Temp_table_param()
    :copy_field(NULL), copy_field_end(NULL),
     group_buff(NULL),
     items_to_copy(NULL),
     recinfo(NULL), start_recinfo(NULL),
     keyinfo(NULL),
     end_write_records(0),
     field_count(0), func_count(0), sum_func_count(0), hidden_field_count(0),
     group_parts(0), group_length(0), group_null_parts(0),
     quick_group(1),
     outer_sum_func_count(0),
     using_outer_summary_function(false),
     table_charset(NULL),
     schema_table(false), precomputed_group_by(false), force_copy_fields(false),
     skip_create_table(false), bit_fields_as_long(false), can_use_pk_for_unique(true)
  {}
  ~Temp_table_param()
  {
    cleanup();
  }

  void cleanup(void)
  {
    delete [] copy_field;
    copy_field= NULL;
    copy_field_end= NULL;
  }
};

/* Base subselect interface class */
class Query_result_subquery :public Query_result_interceptor
{
protected:
  Item_subselect *item;
public:
  Query_result_subquery(Item_subselect *item_arg)
    : item(item_arg) { }
  bool send_data(List<Item> &items)=0;
  bool send_eof() { return 0; };
};


/* Structure for db & table in sql_yacc */

class Table_ident :public Sql_alloc
{
public:
  LEX_CSTRING db;
  LEX_CSTRING table;
  SELECT_LEX_UNIT *sel;
  inline Table_ident(THD *thd, const LEX_CSTRING &db_arg,
                     const LEX_CSTRING &table_arg,
		     bool force)
    :table(table_arg), sel(NULL)
  {
    if (!force && thd->get_protocol()->has_client_capability(CLIENT_NO_SCHEMA))
      db= NULL_CSTR;
    else
      db= db_arg;
  }
  inline Table_ident(const LEX_CSTRING &db_arg, const LEX_CSTRING &table_arg)
    :db(db_arg), table(table_arg), sel(NULL)
  {}
  inline Table_ident(const LEX_CSTRING &table_arg)
    :table(table_arg), sel(NULL)
  {
    db= NULL_CSTR;
  }
  /*
    This constructor is used only for the case when we create a derived
    table. A derived table has no name and doesn't belong to any database.
    Later, if there was an alias specified for the table, it will be set
    by add_table_to_list.
  */
  inline Table_ident(SELECT_LEX_UNIT *s) : sel(s)
  {
    /* We must have a table name here as this is used with add_table_to_list */
    db= EMPTY_CSTR;                    /* a subject to casedn_str */
    table.str= internal_table_name;
    table.length= 1;
  }
  // True if we can tell from syntax that this is an unnamed derived table.
  bool is_derived_table() const { return MY_TEST(sel); }
  inline void change_db(const char *db_name)
  {
    db.str= db_name;
    db.length= strlen(db_name);
  }
};

// this is needed for user_vars hash
class user_var_entry
{
  static const size_t extra_size= sizeof(double);
  char *m_ptr;          // Value
  size_t m_length;      // Value length
  Item_result m_type;   // Value type
  THD *m_owner;

  void reset_value()
  { m_ptr= NULL; m_length= 0; }
  void set_value(char *value, size_t length)
  { m_ptr= value; m_length= length; }

  /**
    Position inside a user_var_entry where small values are stored:
    double values, longlong values and string values with length
    up to extra_size (should be 8 bytes on all platforms).
    String values with length longer than 8 are stored in a separate
    memory buffer, which is allocated when needed using the method realloc().
  */
  char *internal_buffer_ptr() const
  { return (char *) this + ALIGN_SIZE(sizeof(user_var_entry)); }

  /**
    Position inside a user_var_entry where a null-terminates array
    of characters representing the variable name is stored.
  */
  char *name_ptr() const
  { return internal_buffer_ptr() + extra_size; }

  /**
    Initialize m_ptr to the internal buffer (if the value is small enough),
    or allocate a separate buffer.
    @param length - length of the value to be stored.
  */
  bool mem_realloc(size_t length);

  /**
    Check if m_ptr point to an external buffer previously alloced by realloc().
    @retval true  - an external buffer is alloced.
    @retval false - m_ptr is null, or points to the internal buffer.
  */
  bool alloced()
  { return m_ptr && m_ptr != internal_buffer_ptr(); }

  /**
    Free the external value buffer, if it's allocated.
  */
  void free_value()
  {
    if (alloced())
      my_free(m_ptr);
  }

  /**
    Copy the array of characters from the given name into the internal
    name buffer and initialize entry_name to point to it.
  */
  void copy_name(const Simple_cstring &name)
  {
    name.strcpy(name_ptr());
    entry_name= Name_string(name_ptr(), name.length());
  }

  /**
    Initialize all members
    @param name - Name of the user_var_entry instance.
    @cs         - charset information of the user_var_entry instance.
  */
  void init(THD *thd, const Simple_cstring &name, const CHARSET_INFO *cs)
  {
    DBUG_ASSERT(thd != NULL);
    m_owner= thd;
    copy_name(name);
    reset_value();
    update_query_id= 0;
    collation.set(cs, DERIVATION_IMPLICIT, 0);
    unsigned_flag= 0;
    /*
      If we are here, we were called from a SET or a query which sets a
      variable. Imagine it is this:
      INSERT INTO t SELECT @a:=10, @a:=@a+1.
      Then when we have a Item_func_get_user_var (because of the @a+1) so we
      think we have to write the value of @a to the binlog. But before that,
      we have a Item_func_set_user_var to create @a (@a:=10), in this we mark
      the variable as "already logged" (line below) so that it won't be logged
      by Item_func_get_user_var (because that's not necessary).
    */
    used_query_id= thd->query_id;
    m_type= STRING_RESULT;
  }

  /**
    Store a value of the given type into a user_var_entry instance.
    @param from    Value
    @param length  Size of the value
    @param type    type
    @return
    @retval        false on success
    @retval        true on memory allocation error
  */
  bool store(const void *from, size_t length, Item_result type);

  /**
    Assert the user variable is locked.
    This is debug code only.
    The thread LOCK_thd_data mutex protects:
    - the thd->user_vars hash itself
    - the values in the user variable itself.
    The protection is required for monitoring,
    as a different thread can inspect this session
    user variables, on a live session.
  */
  inline void assert_locked() const
  {
    mysql_mutex_assert_owner(&m_owner->LOCK_thd_data);
  }


  /**
    Currently selected catalog.
  */
  LEX_CSTRING m_catalog;
public:
  user_var_entry() {}                         /* Remove gcc warning */

  Simple_cstring entry_name;  // Variable name
  DTCollation collation;      // Collation with attributes
  query_id_t update_query_id, used_query_id;
  bool unsigned_flag;         // true if unsigned, false if signed

  /**
    Store a value of the given type and attributes (collation, sign)
    into a user_var_entry instance.
    @param from         Value
    @param length       Size of the value
    @param type         type
    @param cs           Character set and collation of the value
    @param dv           Collationd erivation of the value
    @param unsigned_arg Signess of the value
    @return
    @retval        false on success
    @retval        true on memory allocation error
  */
  bool store(const void *from, size_t length, Item_result type,
             const CHARSET_INFO *cs, Derivation dv, bool unsigned_arg);
  /**
    Set type of to the given value.
    @param type  Data type.
  */
  void set_type(Item_result type)
  {
    assert_locked();
    m_type= type;
  }
  /**
    Set value to NULL
    @param type  Data type.
  */

  void set_null_value(Item_result type)
  {
    assert_locked();
    free_value();
    reset_value();
    m_type= type;
  }

  /**
    Allocate and initialize a user variable instance.
    @param namec  Name of the variable.
    @param cs     Charset of the variable.
    @return
    @retval  Address of the allocated and initialized user_var_entry instance.
    @retval  NULL on allocation error.
  */
  static user_var_entry *create(THD *thd, const Name_string &name, const CHARSET_INFO *cs)
  {
    if (check_column_name(name.ptr()))
    {
      my_error(ER_ILLEGAL_USER_VAR, MYF(0), name.ptr());
      return NULL;
    }

    user_var_entry *entry;
    size_t size= ALIGN_SIZE(sizeof(user_var_entry)) +
                 (name.length() + 1) + extra_size;
    if (!(entry= (user_var_entry*) my_malloc(key_memory_user_var_entry,
                                             size, MYF(MY_WME |
                                                       ME_FATALERROR))))
      return NULL;
    entry->init(thd, name, cs);
    return entry;
  }

  /**
    Free all memory used by a user_var_entry instance
    previously created by create().
  */
  void destroy()
  {
    assert_locked();
    free_value();  // Free the external value buffer
    my_free(this); // Free the instance itself
  }

  void lock();
  void unlock();

  /* Routines to access the value and its type */
  const char *ptr() const { return m_ptr; }
  size_t length() const { return m_length; }
  Item_result type() const { return m_type; }
  /* Item-alike routines to access the value */
  double val_real(my_bool *null_value) const;
  longlong val_int(my_bool *null_value) const;
  String *val_str(my_bool *null_value, String *str, uint decimals) const;
  my_decimal *val_decimal(my_bool *null_value, my_decimal *result) const;
};


class Query_dumpvar :public Query_result_interceptor {
  ha_rows row_count;
public:
  List<PT_select_var> var_list;
  Query_dumpvar()  { var_list.empty(); row_count= 0;}
  ~Query_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const;
  void cleanup()
  {
    row_count= 0;
  }
};

/* Bits in sql_command_flags */

#define CF_CHANGES_DATA           (1U << 0)
/* The 2nd bit is unused -- it used to be CF_HAS_ROW_COUNT. */
#define CF_STATUS_COMMAND         (1U << 2)
#define CF_SHOW_TABLE_COMMAND     (1U << 3)
#define CF_WRITE_LOGS_COMMAND     (1U << 4)
/**
  Must be set for SQL statements that may contain
  Item expressions and/or use joins and tables.
  Indicates that the parse tree of such statement may
  contain rule-based optimizations that depend on metadata
  (i.e. number of columns in a table), and consequently
  that the statement must be re-prepared whenever
  referenced metadata changes. Must not be set for
  statements that themselves change metadata, e.g. RENAME,
  ALTER and other DDL, since otherwise will trigger constant
  reprepare. Consequently, complex item expressions and
  joins are currently prohibited in these statements.
*/
#define CF_REEXECUTION_FRAGILE    (1U << 5)
/**
  Implicitly commit before the SQL statement is executed.

  Statements marked with this flag will cause any active
  transaction to end (commit) before proceeding with the
  command execution.

  This flag should be set for statements that probably can't
  be rolled back or that do not expect any previously metadata
  locked tables.
*/
#define CF_IMPLICIT_COMMIT_BEGIN  (1U << 6)
/**
  Implicitly commit after the SQL statement.

  Statements marked with this flag are automatically committed
  at the end of the statement.

  This flag should be set for statements that will implicitly
  open and take metadata locks on system tables that should not
  be carried for the whole duration of a active transaction.
*/
#define CF_IMPLICIT_COMMIT_END    (1U << 7)
/**
  CF_IMPLICIT_COMMIT_BEGIN and CF_IMPLICIT_COMMIT_END are used
  to ensure that the active transaction is implicitly committed
  before and after every DDL statement and any statement that
  modifies our currently non-transactional system tables.
*/
#define CF_AUTO_COMMIT_TRANS  (CF_IMPLICIT_COMMIT_BEGIN | CF_IMPLICIT_COMMIT_END)

/**
  Diagnostic statement.
  Diagnostic statements:
  - SHOW WARNING
  - SHOW ERROR
  - GET DIAGNOSTICS (WL#2111)
  do not modify the Diagnostics Area during execution.
*/
#define CF_DIAGNOSTIC_STMT        (1U << 8)

/**
  Identifies statements that may generate row events
  and that may end up in the binary log.
*/
#define CF_CAN_GENERATE_ROW_EVENTS (1U << 9)

/**
  Identifies statements which may deal with temporary tables and for which
  temporary tables should be pre-opened to simplify privilege checks.
*/
#define CF_PREOPEN_TMP_TABLES   (1U << 10)

/**
  Identifies statements for which open handlers should be closed in the
  beginning of the statement.
*/
#define CF_HA_CLOSE             (1U << 11)

/**
  Identifies statements that can be explained with EXPLAIN.
*/
#define CF_CAN_BE_EXPLAINED       (1U << 12)

/** Identifies statements which may generate an optimizer trace */
#define CF_OPTIMIZER_TRACE        (1U << 14)

/**
   Identifies statements that should always be disallowed in
   read only transactions.
*/
#define CF_DISALLOW_IN_RO_TRANS   (1U << 15)

/**
  Identifies statements and commands that can be used with Protocol Plugin
*/
#define CF_ALLOW_PROTOCOL_PLUGIN (1U << 16)

/* Bits in server_command_flags */

/**
  Skip the increase of the global query id counter. Commonly set for
  commands that are stateless (won't cause any change on the server
  internal states). This is made obsolete as query id is incremented
  for ping and statistics commands as well because of race condition
  (Bug#58785).
*/
#define CF_SKIP_QUERY_ID        (1U << 0)

/**
  Skip the increase of the number of statements that clients have
  sent to the server. Commonly used for commands that will cause
  a statement to be executed but the statement might have not been
  sent by the user (ie: stored procedure).
*/
#define CF_SKIP_QUESTIONS       (1U << 1)

/*
  1U << 16 is reserved for Protocol Plugin statements and commands
*/

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var);


void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var, bool reset_from_var);

/* Inline functions */

inline bool add_item_to_list(THD *thd, Item *item)
{
  return thd->lex->select_lex->add_item_to_list(thd, item);
}

inline void add_order_to_list(THD *thd, ORDER *order)
{
  thd->lex->select_lex->add_order_to_list(order);
}

inline void add_group_to_list(THD *thd, ORDER *order)
{
  thd->lex->select_lex->add_group_to_list(order);
}


/**
  @param THD         thread context
  @param hton        pointer to handlerton
  @return address of the placeholder of handlerton's specific transaction
          object (data)
*/

inline void **thd_ha_data_backup(const THD *thd, const struct handlerton *hton)
{
  return (void **) &thd->ha_data[hton->slot].ha_ptr_backup;
}

/**
  The function re-attaches the engine ha_data (which was previously detached by
  detach_ha_data_from_thd) to THD.
  This is typically done to replication applier executing
  one of XA-PREPARE, XA-COMMIT ONE PHASE or rollback.

  @param thd         thread context
  @param hton        pointer to handlerton
*/

inline void reattach_engine_ha_data_to_thd(THD *thd, const struct handlerton *hton)
{
  if (hton->replace_native_transaction_in_thd)
  {
    /* restore the saved original engine transaction's link with thd */
    void **trx_backup= thd_ha_data_backup(thd, hton);

    hton->
      replace_native_transaction_in_thd(thd, *trx_backup, NULL);
    *trx_backup= NULL;
  }
}

/*************************************************************************/

#endif /* MYSQL_SERVER */

#endif /* SQL_CLASS_INCLUDED */
