/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#ifdef MYSQL_SERVER
#include "unireg.h"                    // REQUIRED: for other includes
#endif
#include "sql_const.h"
#include <mysql/plugin_audit.h>
#include "log.h"
#include "rpl_tblmap.h"
#include "mdl.h"
#include "sql_locale.h"                         /* my_locale_st */
#include "sql_profile.h"                   /* PROFILING */
#include "scheduler.h"                     /* thd_scheduler */
#include "protocol.h"             /* Protocol_text, Protocol_binary */
#include "violite.h"              /* vio_is_connected */
#include "thr_lock.h"             /* thr_lock_type, THR_LOCK_DATA,
                                     THR_LOCK_INFO */
#include "opt_trace_context.h"    /* Opt_trace_context */
#include "rpl_gtid.h"

#include <mysql/psi/mysql_stage.h>
#include <mysql/psi/mysql_statement.h>
#include <mysql/psi/mysql_idle.h>
#include <mysql_com_server.h>
#include "sql_data_change.h"

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

/**
  The meat of thd_proc_info(THD*, char*), a macro that packs the last
  three calling-info parameters.
*/
extern "C"
const char *set_thd_proc_info(void *thd_arg, const char *info,
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
                        
#define THD_STAGE_INFO(thd, stage) \
  (thd)->enter_stage(& stage, NULL, __func__, __FILE__, __LINE__)

class Reprepare_observer;
class Relay_log_info;

class Query_log_event;
class Load_log_event;
class sp_rcontext;
class sp_cache;
class Parser_state;
class Rows_log_event;
class Sroutine_hash_entry;
class User_level_lock;
class user_var_entry;

enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };

enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };
enum enum_rbr_exec_mode { RBR_EXEC_MODE_STRICT,
                          RBR_EXEC_MODE_IDEMPOTENT,
                          RBR_EXEC_MODE_LAST_BIT };
enum enum_slave_type_conversions { SLAVE_TYPE_CONVERSIONS_ALL_LOSSY,
                                   SLAVE_TYPE_CONVERSIONS_ALL_NON_LOSSY};
enum enum_slave_rows_search_algorithms { SLAVE_ROWS_TABLE_SCAN = (1U << 0),
                                         SLAVE_ROWS_INDEX_SCAN = (1U << 1),
                                         SLAVE_ROWS_HASH_SCAN  = (1U << 2)};

enum enum_mark_columns
{ MARK_COLUMNS_NONE, MARK_COLUMNS_READ, MARK_COLUMNS_WRITE};
enum enum_filetype { FILETYPE_CSV, FILETYPE_XML };

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
#define MODE_NO_ZERO_IN_DATE            (MODE_STRICT_ALL_TABLES*2)
#define MODE_NO_ZERO_DATE               (MODE_NO_ZERO_IN_DATE*2)
#define MODE_INVALID_DATES              (MODE_NO_ZERO_DATE*2)
#define MODE_ERROR_FOR_DIVISION_BY_ZERO (MODE_INVALID_DATES*2)
#define MODE_TRADITIONAL                (MODE_ERROR_FOR_DIVISION_BY_ZERO*2)
#define MODE_NO_AUTO_CREATE_USER        (MODE_TRADITIONAL*2)
#define MODE_HIGH_NOT_PRECEDENCE        (MODE_NO_AUTO_CREATE_USER*2)
#define MODE_NO_ENGINE_SUBSTITUTION     (MODE_HIGH_NOT_PRECEDENCE*2)
#define MODE_PAD_CHAR_TO_FULL_LENGTH    (ULL(1) << 31)

extern char internal_table_name[2];
extern char empty_c_string[1];
extern LEX_STRING EMPTY_STR;
extern LEX_STRING NULL_STR;
extern MYSQL_PLUGIN_IMPORT const char **errmesg;

extern bool volatile shutdown_in_progress;

extern "C" LEX_STRING * thd_query_string (MYSQL_THD thd);
extern "C" char **thd_query(MYSQL_THD thd);

/**
  @class CSET_STRING
  @brief Character set armed LEX_STRING
*/
class CSET_STRING
{
private:
  LEX_STRING string;
  const CHARSET_INFO *cs;
public:
  CSET_STRING() : cs(&my_charset_bin)
  {
    string.str= NULL;
    string.length= 0;
  }
  CSET_STRING(char *str_arg, size_t length_arg, const CHARSET_INFO *cs_arg) :
  cs(cs_arg)
  {
    DBUG_ASSERT(cs_arg != NULL);
    string.str= str_arg;
    string.length= length_arg;
  }

  inline char *str() const { return string.str; }
  inline size_t length() const { return string.length; }
  const CHARSET_INFO *charset() const { return cs; }

  friend LEX_STRING * thd_query_string (MYSQL_THD thd);
  friend char **thd_query(MYSQL_THD thd);
};


#define TC_LOG_PAGE_SIZE   8192
#define TC_LOG_MIN_SIZE    (3*TC_LOG_PAGE_SIZE)

#define TC_HEURISTIC_RECOVER_COMMIT   1
#define TC_HEURISTIC_RECOVER_ROLLBACK 2
extern ulong tc_heuristic_recover;

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


class Alter_drop :public Sql_alloc {
public:
  enum drop_type {KEY, COLUMN, FOREIGN_KEY };
  const char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,const char *par_name)
    :name(par_name), type(par_type)
  {
    DBUG_ASSERT(par_name != NULL);
  }
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  Alter_drop *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_drop(*this); }
};


class Alter_column :public Sql_alloc {
public:
  const char *name;
  Item *def;
  Alter_column(const char *par_name,Item *literal)
    :name(par_name), def(literal) {}
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  Alter_column *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_column(*this); }
};


class Key :public Sql_alloc {
public:
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE, FULLTEXT, SPATIAL, FOREIGN_KEY};
  enum Keytype type;
  KEY_CREATE_INFO key_create_info;
  List<Key_part_spec> columns;
  LEX_STRING name;
  bool generated;

  Key(enum Keytype type_par, const LEX_STRING &name_arg,
      KEY_CREATE_INFO *key_info_arg,
      bool generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
  Key(enum Keytype type_par, const char *name_arg, size_t name_len_arg,
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
  enum fk_match_opt { FK_MATCH_UNDEF, FK_MATCH_FULL,
		      FK_MATCH_PARTIAL, FK_MATCH_SIMPLE};
  enum fk_option { FK_OPTION_UNDEF, FK_OPTION_RESTRICT, FK_OPTION_CASCADE,
		   FK_OPTION_SET_NULL, FK_OPTION_NO_ACTION, FK_OPTION_DEFAULT};

  LEX_STRING ref_db;
  LEX_STRING ref_table;
  List<Key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  Foreign_key(const LEX_STRING &name_arg, List<Key_part_spec> &cols,
	      const LEX_STRING &ref_db_arg, const LEX_STRING &ref_table_arg,
              List<Key_part_spec> &ref_cols,
	      uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key(FOREIGN_KEY, name_arg, &default_key_create_info, 0, cols),
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
};

typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;


class LEX_COLUMN : public Sql_alloc
{
public:
  String column;
  uint rights;
  LEX_COLUMN (const String& x,const  uint& y ): column (x),rights (y) {}
};

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

#include "sql_lex.h"				/* Must be here */

class select_result;
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
  ulong completion_type;
  ulong query_cache_type;
  ulong tx_isolation;
  ulong updatable_views_with_limit;
  uint max_user_connections;
  /**
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  my_thread_id pseudo_thread_id;
  /**
    Default transaction access mode. READ ONLY (true) or READ WRITE (false).
  */
  my_bool tx_read_only;
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

  Gtid_specification gtid_next;
  Gtid_set_or_null gtid_next_list;

} SV;


/**
  Per thread status variables.
  Must be long/ulong up to last_system_status_var so that
  add_to_status/add_diff_to_status can work.
*/

typedef struct system_status_var
{
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
  /* Prepared statements and binary protocol */
  ulonglong com_stmt_prepare;
  ulonglong com_stmt_reprepare;
  ulonglong com_stmt_execute;
  ulonglong com_stmt_send_long_data;
  ulonglong com_stmt_fetch;
  ulonglong com_stmt_reset;
  ulonglong com_stmt_close;

  ulonglong bytes_received;
  ulonglong bytes_sent;
  /*
    Number of statements sent from the client
  */
  ulonglong questions;

  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];

  /*
    IMPORTANT!
    SEE last_system_status_var DEFINITION BELOW.
    Below 'last_system_status_var' are all variables that cannot be handled
    automatically by add_to_status()/add_diff_to_status().
  */
  double last_query_cost;
  ulonglong last_query_partial_plans;
} STATUS_VAR;

/*
  This is used for 'SHOW STATUS'. It must be updated to the last ulong
  variable in system_status_var which is makes sens to add to the global
  counter
*/

#define last_system_status_var questions

void mark_transaction_to_rollback(THD *thd, bool all);


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

void free_tmp_table(THD *thd, TABLE *entry);


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

  enum_state state;

  /* We build without RTTI, so dynamic_cast can't be used. */
  enum Type
  {
    STATEMENT, PREPARED_STATEMENT, STORED_PROCEDURE
  };

  Query_arena(MEM_ROOT *mem_root_arg, enum enum_state state_arg) :
    free_list(0), mem_root(mem_root_arg), state(state_arg)
  { INIT_ARENA_DBUG_INFO; }
  /*
    This constructor is used only when Query_arena is created as
    backup storage for another instance of Query_arena.
  */
  Query_arena() { INIT_ARENA_DBUG_INFO; }

  virtual Type type() const;
  virtual ~Query_arena() {};

  inline bool is_stmt_prepare() const { return state == STMT_INITIALIZED; }
  inline bool is_stmt_prepare_or_first_sp_execute() const
  { return (int)state < (int)STMT_PREPARED; }
  inline bool is_stmt_prepare_or_first_stmt_execute() const
  { return (int)state <= (int)STMT_PREPARED; }
  inline bool is_conventional() const
  { return state == STMT_CONVENTIONAL_EXECUTION; }

  inline void* alloc(size_t size) { return alloc_root(mem_root,size); }
  inline void* calloc(size_t size)
  {
    void *ptr;
    if ((ptr=alloc_root(mem_root,size)))
      memset(ptr, 0, size);
    return ptr;
  }
  inline char *strdup(const char *str)
  { return strdup_root(mem_root,str); }
  inline char *strmake(const char *str, size_t size)
  { return strmake_root(mem_root,str,size); }
  inline void *memdup(const void *str, size_t size)
  { return memdup_root(mem_root,str,size); }
  inline void *memdup_w_gap(const void *str, size_t size, uint gap)
  {
    void *ptr;
    if ((ptr= alloc_root(mem_root,size+gap)))
      memcpy(ptr,str,size);
    return ptr;
  }

  void set_query_arena(Query_arena *set);

  void free_items();
  /* Close the active state associated with execution of this statement */
  virtual void cleanup_stmt();
};


class Server_side_cursor;

/**
  @class Statement
  @brief State of a single command executed against this connection.

  One connection can contain a lot of simultaneously running statements,
  some of which could be:
   - prepared, that is, contain placeholders,
   - opened as cursors. We maintain 1 to 1 relationship between
     statement and cursor - if user wants to create another cursor for his
     query, we create another statement for it. 
  To perform some action with statement we reset THD part to the state  of
  that statement, do the action, and then save back modified state from THD
  to the statement. It will be changed in near future, and Statement will
  be used explicitly.
*/

class Statement: public Query_arena
{
  Statement(const Statement &rhs);              /* not implemented: */
  Statement &operator=(const Statement &rhs);   /* non-copyable */
public:
  /*
    Uniquely identifies each statement object in thread scope; change during
    statement lifetime. FIXME: must be const
  */
   ulong id;

  /*
    MARK_COLUMNS_NONE:  Means mark_used_colums is not set and no indicator to
                        handler of fields used is set
    MARK_COLUMNS_READ:  Means a bit in read set is set to inform handler
	                that the field is to be read. If field list contains
                        duplicates, then thd->dup_field is set to point
                        to the last found duplicate.
    MARK_COLUMNS_WRITE: Means a bit is set in write set to inform handler
			that it needs to update this field in write_row
                        and update_row.
  */
  enum enum_mark_columns mark_used_columns;

  LEX_STRING name; /* name for named prepared statements */
  LEX *lex;                                     // parse tree descriptor
  /*
    Points to the query associated with this statement. It's const, but
    we need to declare it char * because all table handlers are written
    in C and need to point to it.

    Note that if we set query = NULL, we must at the same time set
    query_length = 0, and protect the whole operation with
    LOCK_thd_data mutex. To avoid crashes in races, if we do not
    know that thd->query cannot change at the moment, we should print
    thd->query like this:
      (1) reserve the LOCK_thd_data mutex;
      (2) print or copy the value of query and query_length
      (3) release LOCK_thd_data mutex.
    This printing is needed at least in SHOW PROCESSLIST and SHOW
    ENGINE INNODB STATUS.
  */
  CSET_STRING query_string;

  /*
    In some cases, we may want to modify the query (i.e. replace
    passwords with their hashes before logging the statement etc.).

    In case the query was rewritten, the original query will live in
    query_string, while the rewritten query lives in rewritten_query.
    If rewritten_query is empty, query_string should be logged.
    If rewritten_query is non-empty, the rewritten query it contains
    should be used in logs (general log, slow query log, binary log).

    Currently, password obfuscation is the only rewriting we do; more
    may follow at a later date, both pre- and post parsing of the query.
    Rewriting of binloggable statements must preserve all pertinent
    information.
  */
  String      rewritten_query;

  inline char *query() const { return query_string.str(); }
  inline uint32 query_length() const { return query_string.length(); }
  const CHARSET_INFO *query_charset() const { return query_string.charset(); }
  void set_query_inner(const CSET_STRING &string_arg)
  {
    query_string= string_arg;
  }
  void set_query_inner(char *query_arg, uint32 query_length_arg,
                       const CHARSET_INFO *cs_arg)
  {
    set_query_inner(CSET_STRING(query_arg, query_length_arg, cs_arg));
  }
  void reset_query_inner()
  {
    set_query_inner(CSET_STRING());
  }
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

  char *db;
  size_t db_length;

public:

  /* This constructor is called for backup statements */
  Statement() {}

  Statement(LEX *lex_arg, MEM_ROOT *mem_root_arg,
            enum enum_state state_arg, ulong id_arg);
  virtual ~Statement();

  /* Assign execution context (note: not all members) of given stmt to self */
  virtual void set_statement(Statement *stmt);
  void set_n_backup_statement(Statement *stmt, Statement *backup);
  void restore_backup_statement(Statement *stmt, Statement *backup);
  /* return class type */
  virtual Type type() const;
};


/**
  Container for all statements created/used in a connection.
  Statements in Statement_map have unique Statement::id (guaranteed by id
  assignment in Statement::Statement)
  Non-empty statement names are unique too: attempt to insert a new statement
  with duplicate name causes older statement to be deleted

  Statements are auto-deleted when they are removed from the map and when the
  map is deleted.
*/

class Statement_map
{
public:
  Statement_map();

  int insert(THD *thd, Statement *statement);

  Statement *find_by_name(LEX_STRING *name)
  {
    Statement *stmt;
    stmt= (Statement*)my_hash_search(&names_hash, (uchar*)name->str,
                                     name->length);
    return stmt;
  }

  Statement *find(ulong id)
  {
    if (last_found_statement == 0 || id != last_found_statement->id)
    {
      Statement *stmt;
      stmt= (Statement *) my_hash_search(&st_hash, (uchar *) &id, sizeof(id));
      if (stmt && stmt->name.str)
        return NULL;
      last_found_statement= stmt;
    }
    return last_found_statement;
  }
  /*
    Close all cursors of this connection that use tables of a storage
    engine that has transaction-specific state and therefore can not
    survive COMMIT or ROLLBACK. Currently all but MyISAM cursors are closed.
    CURRENTLY NOT IMPLEMENTED!
  */
  void close_transient_cursors();
  void erase(Statement *statement);
  /* Erase all statements (calls Statement destructor) */
  void reset();
  ~Statement_map();
private:
  HASH st_hash;
  HASH names_hash;
  Statement *last_found_statement;
};

class Ha_trx_info;

struct THD_TRANS
{
  /* true is not all entries in the ht[] support 2pc */
  bool        no_2pc;
  int         rw_ha_count;
  /* storage engines that registered in this transaction */
  Ha_trx_info *ha_list;

private:
  /* 
    The purpose of this member variable (i.e. flag) is to keep track of
    statements which cannot be rolled back safely(completely).
    For example,

    * statements that modified non-transactional tables. The value
    MODIFIED_NON_TRANS_TABLE is set within mysql_insert, mysql_update,
    mysql_delete, etc if a non-transactional table is modified.

    * 'DROP TEMPORARY TABLE' and 'CREATE TEMPORARY TABLE' statements.
    The former sets the value CREATED_TEMP_TABLE is set and the latter
    the value DROPPED_TEMP_TABLE.
    
    The tracked statements are modified in scope of:

    * transaction, when the variable is a member of THD::transaction.all
    
    * top-level statement or sub-statement, when the variable is a
    member of THD::transaction.stmt

    This member has the following life cycle:

    * stmt.m_unsafe_rollback_flags is used to keep track of top-level statements
    which cannot be rolled back safely. At the end of the statement, the value
    of stmt.m_unsafe_rollback_flags is merged with all.m_unsafe_rollback_flags
    and gets reset.
    
    * all.cannot_safely_rollback is reset at the end of transaction

    * Since we do not have a dedicated context for execution of a sub-statement,
    to keep track of non-transactional changes in a sub-statement, we re-use
    stmt.m_unsafe_rollback_flags. At entrance into a sub-statement, a copy of
    the value of stmt.m_unsafe_rollback_flags (containing the changes of the
    outer statement) is saved on stack.  Then stmt.m_unsafe_rollback_flags is
    reset to 0 and the substatement is executed. Then the new value is merged
    with the saved value.
  */

  unsigned int m_unsafe_rollback_flags;
  /*
    Define the type of statemens which cannot be rolled back safely.
    Each type occupies one bit in m_unsafe_rollback_flags.
  */
  static unsigned int const MODIFIED_NON_TRANS_TABLE= 0x01;
  static unsigned int const CREATED_TEMP_TABLE= 0x02;
  static unsigned int const DROPPED_TEMP_TABLE= 0x04;

public:
#ifndef DBUG_OFF
  void dbug_unsafe_rollback_flags(const char* msg) const
  {
    DBUG_PRINT("debug", ("%s.unsafe_rollback_flags: %s%s%s",
                         msg,
                         FLAGSTR(m_unsafe_rollback_flags, MODIFIED_NON_TRANS_TABLE),
                         FLAGSTR(m_unsafe_rollback_flags, CREATED_TEMP_TABLE),
                         FLAGSTR(m_unsafe_rollback_flags, DROPPED_TEMP_TABLE)));
  }
#endif

  bool cannot_safely_rollback() const
  {
    return m_unsafe_rollback_flags > 0;
  }
  unsigned int get_unsafe_rollback_flags() const
  {
    return m_unsafe_rollback_flags;
  }
  void set_unsafe_rollback_flags(unsigned int flags)
  {
    DBUG_PRINT("debug", ("set_unsafe_rollback_flags: %d", flags));
    m_unsafe_rollback_flags= flags;
  }
  void add_unsafe_rollback_flags(unsigned int flags)
  {
    DBUG_PRINT("debug", ("add_unsafe_rollback_flags: %d", flags));
    m_unsafe_rollback_flags|= flags;
  }
  void reset_unsafe_rollback_flags()
  {
    DBUG_PRINT("debug", ("reset_unsafe_rollback_flags"));
    m_unsafe_rollback_flags= 0;
  }
  void mark_modified_non_trans_table()
  {
    DBUG_PRINT("debug", ("mark_modified_non_trans_table"));
    m_unsafe_rollback_flags|= MODIFIED_NON_TRANS_TABLE;
  }
  bool has_modified_non_trans_table() const
  {
    return m_unsafe_rollback_flags & MODIFIED_NON_TRANS_TABLE;
  }
  void mark_created_temp_table()
  {
    DBUG_PRINT("debug", ("mark_created_temp_table"));
    m_unsafe_rollback_flags|= CREATED_TEMP_TABLE;
  }
  bool has_created_temp_table() const
  {
    return m_unsafe_rollback_flags & CREATED_TEMP_TABLE;
  }
  void mark_dropped_temp_table()
  {
    DBUG_PRINT("debug", ("mark_dropped_temp_table"));
    m_unsafe_rollback_flags|= DROPPED_TEMP_TABLE;
  }
  bool has_dropped_temp_table() const
  {
    return m_unsafe_rollback_flags & DROPPED_TEMP_TABLE;
  }

  void reset()
  {
    no_2pc= FALSE;
    rw_ha_count= 0;
    reset_unsafe_rollback_flags();
  }
  bool is_empty() const { return ha_list == NULL; }
};

/**
  Either statement transaction or normal transaction - related
  thread-specific storage engine data.

  If a storage engine participates in a statement/transaction,
  an instance of this class is present in
  thd->transaction.{stmt|all}.ha_list. The addition to
  {stmt|all}.ha_list is made by trans_register_ha().

  When it's time to commit or rollback, each element of ha_list
  is used to access storage engine's prepare()/commit()/rollback()
  methods, and also to evaluate if a full two phase commit is
  necessary.

  @sa General description of transaction handling in handler.cc.
*/

class Ha_trx_info
{
#ifndef DBUG_OFF
  friend const char *
  ha_list_names(Ha_trx_info *ha_list, char *const buf_arg)
  {
    char *buf = buf_arg;
    while (ha_list)
    {
      buf += sprintf(buf, "%s", ha_legacy_type_name(ha_list->m_ht->db_type));
      ha_list = ha_list->m_next;
      if (ha_list)
        buf += sprintf(buf, ", ");
    }
    if (buf == buf_arg)
      sprintf(buf, "<NONE>");
    return buf_arg;
  }
#endif

public:
  /** Register this storage engine in the given transaction context. */
  void register_ha(THD_TRANS *trans, handlerton *ht_arg)
  {
    DBUG_ENTER("Ha_trx_info::register_ha");
    DBUG_PRINT("enter", ("trans: 0x%llx, ht: 0x%llx (%s)",
                         (ulonglong) trans, (ulonglong) ht_arg,
                         ha_legacy_type_name(ht_arg->db_type)));
    DBUG_ASSERT(m_flags == 0);
    DBUG_ASSERT(m_ht == NULL);
    DBUG_ASSERT(m_next == NULL);

    m_ht= ht_arg;
    m_flags= (int) TRX_READ_ONLY; /* Assume read-only at start. */

    m_next= trans->ha_list;
    trans->ha_list= this;
    DBUG_VOID_RETURN;
  }

  /** Clear, prepare for reuse. */
  void reset()
  {
    DBUG_ENTER("Ha_trx_info::reset");
    m_next= NULL;
    m_ht= NULL;
    m_flags= 0;
    DBUG_VOID_RETURN;
  }

  Ha_trx_info() { reset(); }

  void set_trx_read_write()
  {
    DBUG_ASSERT(is_started());
    m_flags|= (int) TRX_READ_WRITE;
  }
  bool is_trx_read_write() const
  {
    DBUG_ASSERT(is_started());
    return m_flags & (int) TRX_READ_WRITE;
  }
  bool is_started() const { return m_ht != NULL; }
  /** Mark this transaction read-write if the argument is read-write. */
  void coalesce_trx_with(const Ha_trx_info *stmt_trx)
  {
    /*
      Must be called only after the transaction has been started.
      Can be called many times, e.g. when we have many
      read-write statements in a transaction.
    */
    DBUG_ASSERT(is_started());
    if (stmt_trx->is_trx_read_write())
      set_trx_read_write();
  }
  Ha_trx_info *next() const
  {
    DBUG_ASSERT(is_started());
    return m_next;
  }
  handlerton *ht() const
  {
    DBUG_ASSERT(is_started());
    return m_ht;
  }
private:
  enum { TRX_READ_ONLY= 0, TRX_READ_WRITE= 1 };
  /** Auxiliary, used for ha_list management */
  Ha_trx_info *m_next;
  /**
    Although a given Ha_trx_info instance is currently always used
    for the same storage engine, 'ht' is not-NULL only when the
    corresponding storage is a part of a transaction.
  */
  handlerton *m_ht;
  /**
    Transaction flags related to this engine.
    Not-null only if this instance is a part of transaction.
    May assume a combination of enum values above.
  */
  uchar       m_flags;
};

struct st_savepoint {
  struct st_savepoint *prev;
  char                *name;
  uint                 length;
  Ha_trx_info         *ha_list;
  /** State of metadata locks before this savepoint was set. */
  MDL_savepoint        mdl_savepoint;
};

enum xa_states {XA_NOTR=0, XA_ACTIVE, XA_IDLE, XA_PREPARED, XA_ROLLBACK_ONLY};
extern const char *xa_state_names[];

typedef struct st_xid_state {
  /* For now, this is only used to catch duplicated external xids */
  XID  xid;                           // transaction identifier
  enum xa_states xa_state;            // used by external XA only
  bool in_thd;
  /* Error reported by the Resource Manager (RM) to the Transaction Manager. */
  uint rm_error;
} XID_STATE;

extern mysql_mutex_t LOCK_xid_cache;
extern HASH xid_cache;
bool xid_cache_init(void);
void xid_cache_free(void);
XID_STATE *xid_cache_search(XID *xid);
bool xid_cache_insert(XID *xid, enum xa_states xa_state);
bool xid_cache_insert(XID_STATE *xid_state);
void xid_cache_delete(XID_STATE *xid_state);

/**
  @class Security_context
  @brief A set of THD members describing the current authenticated user.
*/

class Security_context {
public:
  Security_context() {}                       /* Remove gcc warning */
  /*
    host - host of the client
    user - user of the client, set to NULL until the user has been read from
    the connection
    priv_user - The user privilege we are using. May be "" for anonymous user.
    ip - client IP
  */
  char   *host, *user, *ip;
  char   priv_user[USERNAME_LENGTH];
  char   proxy_user[USERNAME_LENGTH + MAX_HOSTNAME + 5];
  /* The host privilege we are using */
  char   priv_host[MAX_HOSTNAME];
  /* The external user (if available) */
  char   *external_user;
  /* points to host if host is available, otherwise points to ip */
  const char *host_or_ip;
  ulong master_access;                 /* Global privileges from mysql.user */
  ulong db_access;                     /* Privileges for current db */
  bool password_expired;               /* password expiration flag */

  void init();
  void destroy();
  void skip_grants();
  inline char *priv_host_name()
  {
    return (*priv_host ? priv_host : (char *)"%");
  }
  
  bool set_user(char *user_arg);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  bool
  change_security_context(THD *thd,
                          LEX_STRING *definer_user,
                          LEX_STRING *definer_host,
                          LEX_STRING *db,
                          Security_context **backup);

  void
  restore_security_context(THD *thd, Security_context *backup);
#endif
  bool user_matches(Security_context *);
};

/**
  @class Log_throttle
  @brief Used for rate-limiting a log (slow query log etc.)
*/

class Log_throttle
{
private:
  /**
    We're using our own (empty) security context during summary generation.
    That way, the aggregate value of the suppressed queries isn't printed
    with a specific user's name (i.e. the user who sent a query when or
    after the time-window closes), as that would be misleading.
  */
  Security_context aggregate_sctx;
  /**
    Total of the execution times of queries in this time-window for which
    we suppressed logging. For use in summary printing.
  */
  ulonglong total_exec_time;
  /**
    Total of the lock times of queries in this time-window for which
    we suppressed logging. For use in summary printing.
  */
  ulonglong total_lock_time;

  /**
    When will/did current window end?
  */
  ulonglong window_end;
  /**
    A reference to the threshold ("no more than n log lines per ...").
    References a (system-?) variable in the server.
  */
  ulong *rate;
  /**
    Log no more than rate lines of a given type per window_size
    (e.g. per minute, usually LOG_THROTTLE_WINDOW_SIZE).
  */
  const ulong window_size;
  /**
   There have been this many lines of this type in this window,
   including those that we suppressed. (We don't simply stop
   counting once we reach the threshold as we'll write a summary
   of the suppressed lines later.)
  */
  ulong count;
  /**
    Template for the summary line. Should contain %lu as the only
    conversion specification.
  */
  const char *summary_template;
  /**
    Log_throttle is shared between THDs.
  */
  mysql_mutex_t *LOCK_log_throttle;
  /**
    The routine we call to actually log a line (i.e. our summary).
    The signature miraculously coincides with slow_log_print().
  */
  bool (*log_summary)(THD *, const char *, uint);

  /**
    Lock this object as it's shared between THDs.
  */
  void lock_exclusive() { mysql_mutex_lock(LOCK_log_throttle); }
  /**
    Unlock this object.
  */
  void unlock() { mysql_mutex_unlock(LOCK_log_throttle); }
  /**
    Start a new window.
  */
  void new_window(ulonglong now);
  /**
    Increase count of queries of the type we're handling.
    Returns the new value for the caller to compare against their limit.
  */
  ulong inc_queries() { return ++count; }
  /**
    Check whether we're still in the current window. (If not, the caller
    will want to print a summary (if the logging of any lines was suppressed),
    and start a new window.)
  */
  bool in_window(ulonglong now) const { return (now < window_end); };
  /**
    Prepare a summary of suppressed lines for logging.
    (For now, to slow query log.)
    This function returns the number of queries that were qualified for
    inclusion in the log, but were not printed because of the rate-limiting.
    The summary will contain this count as well as the respective totals for
    lock and execution time.
    This function assumes that the caller already holds the necessary locks.
  */
  ulong prepare_summary(THD *thd);
  /**
    Actually print the prepared summary to log.
  */
  void print_summary(THD *thd, ulong suppressed,
                     ulonglong print_lock_time,
                     ulonglong print_exec_time);

public:
  /**
    We're rate-limiting messages per minute; 60,000,000 microsecs = 60s
    Debugging is less tedious with a window in the region of 5000000
  */
  static const ulong LOG_THROTTLE_WINDOW_SIZE= 60000000;

  /**
    @param threshold     suppress after this many queries ...
    @param window_usecs  ... in this many micro-seconds
    @param logger        call this function to log a single line (our summary)
    @param msg           use this template containing %lu as only non-literal
  */
  Log_throttle(ulong *threshold, mysql_mutex_t *lock, ulong window_usecs,
               bool (*logger)(THD *, const char *, uint),
               const char *msg);

  /**
    Prepare and print a summary of suppressed lines to log.
    (For now, slow query log.)
    The summary states the number of queries that were qualified for
    inclusion in the log, but were not printed because of the rate-limiting,
    and their respective totals for lock and execution time.
    This wrapper for prepare_summary() and print_summary() handles the
    locking/unlocking.

    @param thd                 The THD that tries to log the statement.
    @retval 0                  Logging was not supressed, no summary needed.
    @retval false              Logging was supressed; a summary was printed.
  */
  bool flush(THD *thd);

  /**
    Top-level function.
    @param thd                 The THD that tries to log the statement.
    @param eligible            Is the statement of the type we might suppress?
    @retval true               Logging should be supressed.
    @retval false              Logging should not be supressed.
  */
  bool log(THD *thd, bool eligible);
};

extern Log_throttle log_throttle_qni;


/**
  A registry for item tree transformations performed during
  query optimization. We register only those changes which require
  a rollback to re-execute a prepared statement or stored procedure
  yet another time.
*/

struct Item_change_record: public ilink<Item_change_record>
{
  Item **place;
  Item *old_value;
};

typedef I_List<Item_change_record> Item_change_list;


/**
  Type of locked tables mode.
  See comment for THD::locked_tables_mode for complete description.
*/

enum enum_locked_tables_mode
{
  LTM_NONE= 0,
  LTM_LOCK_TABLES,
  LTM_PRELOCKED,
  LTM_PRELOCKED_UNDER_LOCK_TABLES
};


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
  Dynamic_array<Reprepare_observer *> m_reprepare_observers;

public:
  Reprepare_observer *get_reprepare_observer() const
  {
    return
      m_reprepare_observers.elements() > 0 ?
      *m_reprepare_observers.back() :
      NULL;
  }

  void push_reprepare_observer(Reprepare_observer *o)
  { m_reprepare_observers.append(o); }

  Reprepare_observer *pop_reprepare_observer()
  { return m_reprepare_observers.pop(); }

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
  uint current_tablenr;

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
  Open_tables_state() : state_flags(0U) { }

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
  ulonglong limit_found_rows;
  ha_rows    cuted_fields, sent_row_count, examined_row_count;
  ulong client_capabilities;
  uint in_sub_stmt;
  bool enable_slow_log;
  bool last_insert_id_used;
  SAVEPOINT *savepoints;
  enum enum_check_fields count_cuted_fields;
};


/* Flags for the THD::system_thread variable */
enum enum_thread_type
{
  NON_SYSTEM_THREAD= 0,
  SYSTEM_THREAD_SLAVE_IO= 1,
  SYSTEM_THREAD_SLAVE_SQL= 2,
  SYSTEM_THREAD_NDBCLUSTER_BINLOG= 4,
  SYSTEM_THREAD_EVENT_SCHEDULER= 8,
  SYSTEM_THREAD_EVENT_WORKER= 16,
  SYSTEM_THREAD_INFO_REPOSITORY= 32,
  SYSTEM_THREAD_SLAVE_WORKER= 64
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
                                Sql_condition::enum_severity_level level,
                                const char* msg,
                                Sql_condition ** cond_hdl) = 0;

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
  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level level,
                        const char* msg,
                        Sql_condition ** cond_hdl)
  {
    /* Ignore error */
    return TRUE;
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
  Drop_table_error_handler() {}

public:
  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level level,
                        const char* msg,
                        Sql_condition ** cond_hdl);

private:
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
    init_sql_alloc(&m_locked_tables_root, MEM_ROOT_BLOCK_SIZE, 0);
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
  Ha_data() :ha_ptr(NULL) {}
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


/*
  Convert microseconds since epoch to timeval.
  @param     micro_time  Microseconds.
  @param OUT tm          A timeval variable to write to.
*/
static inline void
my_micro_time_to_timeval(ulonglong micro_time, struct timeval *tm)
{
  tm->tv_sec=  (long) (micro_time / 1000000);
  tm->tv_usec= (long) (micro_time % 1000000);
}

/**
  @class THD
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public MDL_context_owner,
           public Statement,
           public Open_tables_state
{
private:
  inline bool is_stmt_prepare() const
  { DBUG_ASSERT(0); return Statement::is_stmt_prepare(); }

  inline bool is_stmt_prepare_or_first_sp_execute() const
  { DBUG_ASSERT(0); return Statement::is_stmt_prepare_or_first_sp_execute(); }

  inline bool is_stmt_prepare_or_first_stmt_execute() const
  { DBUG_ASSERT(0); return Statement::is_stmt_prepare_or_first_stmt_execute(); }

  inline bool is_conventional() const
  { DBUG_ASSERT(0); return Statement::is_conventional(); }

public:
  MDL_context mdl_context;

  /* Used to execute base64 coded binlog events in MySQL server */
  Relay_log_info* rli_fake;
  /* Slave applier execution context */
  Relay_log_info* rli_slave;

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
#ifdef HAVE_QUERY_CACHE
  Query_cache_tls query_cache_tls;
#endif
  NET	  net;				// client connection descriptor
  /** Aditional network instrumentation for the server only. */
  NET_SERVER m_net_server_extension;
  Protocol *protocol;			// Current protocol
  Protocol_text   protocol_text;	// Normal protocol
  Protocol_binary protocol_binary;	// Binary protocol
  HASH    user_vars;			// hash for user variables
  String  packet;			// dynamic buffer for network I/O
  String  convert_buffer;               // buffer for charset conversions
  struct  rand_struct rand;		// used for authentication
  struct  system_variables variables;	// Changeable local variables
  struct  system_status_var status_var; // Per thread statistic vars
  struct  system_status_var *initial_status_var; /* used by show status */
  THR_LOCK_INFO lock_info;              // Locking info of this thread
  /**
    Protects THD data accessed from other threads:
    - thd->query and thd->query_length (used by SHOW ENGINE
      INNODB STATUS and SHOW PROCESSLIST
    - thd->mysys_var (used by KILL statement and shutdown).
    Is locked when THD is deleted.
  */
  mysql_mutex_t LOCK_thd_data;

  /* all prepared statements and cursors of this connection */
  Statement_map stmt_map;
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  char	  *thread_stack;

  /**
    Currently selected catalog.
  */
  char *catalog;

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

  Security_context main_security_ctx;
  Security_context *security_ctx;

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

  ulong client_capabilities;		/* What the client supports */
  ulong max_client_packet_length;

  HASH		handler_tables_hash;
  /*
    One thread can hold up to one named user-level lock. This variable
    points to a lock object if the lock is present. See item_func.cc and
    chapter 'Miscellaneous functions', for functions GET_LOCK, RELEASE_LOCK. 
  */
  User_level_lock *ull;
#ifndef DBUG_OFF
  uint dbug_sentry; // watch out for memory corruption
#endif
  struct st_my_thread_var *mysys_var;

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
  // track down slow pthread_create
  ulonglong  prior_thr_create_utime, thr_create_utime;
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

  /* container for handler's private per-connection data */
  Ha_data ha_data[MAX_HA];

  /*
    Position of first event in Binlog
    *after* last event written by this
    thread.
  */
  event_coordinates binlog_next_event_pos;
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

private:
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
  my_off_t m_trans_end_pos;
  /**@}*/

public:
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

public:

  struct st_transactions {
    SAVEPOINT *savepoints;
    THD_TRANS all;			// Trans since BEGIN WORK
    THD_TRANS stmt;			// Trans for current statement
    XID_STATE xid_state;
    Rows_log_event *m_pending_rows_event;

    /*
       Tables changed in transaction (that must be invalidated in query cache).
       List contain only transactional tables, that not invalidated in query
       cache (instead of full list of changed in transaction tables).
    */
    CHANGED_TABLE_LIST* changed_tables;
    MEM_ROOT mem_root; // Transaction-life memory allocation pool

    /*
      (Mostly) binlog-specific fields use while flushing the caches
      and committing transactions.
    */
    struct {
      bool enabled:1;                   // see ha_enable_transaction()
      bool pending:1;                   // Is the transaction commit pending?
      bool xid_written:1;               // The session wrote an XID
      bool real_commit:1;               // Is this a "real" commit?
      bool commit_low:1;                // see MYSQL_BIN_LOG::ordered_commit
    } flags;

    void cleanup()
    {
      DBUG_ENTER("THD::st_transaction::cleanup");
      changed_tables= 0;
      savepoints= 0;

      /*
        If rm_error is raised, it means that this piece of a distributed
        transaction has failed and must be rolled back. But the user must
        rollback it explicitly, so don't start a new distributed XA until
        then.
      */
      if (!xid_state.rm_error)
        xid_state.xid.null();
      free_root(&mem_root,MYF(MY_KEEP_PREALLOC));
      DBUG_VOID_RETURN;
    }
    my_bool is_active()
    {
      return (all.ha_list != NULL);
    }
    st_transactions()
    {
      memset(this, 0, sizeof(*this));
      xid_state.xid.null();
      init_sql_alloc(&mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
    }
    void push_unsafe_rollback_warnings(THD *thd)
    {
      if (all.has_modified_non_trans_table())
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_WARNING_NOT_COMPLETE_ROLLBACK,
                     ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));

      if (all.has_created_temp_table())
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE,
                     ER(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE));

      if (all.has_dropped_temp_table())
        push_warning(thd, Sql_condition::SL_WARNING,
                     ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE,
                     ER(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE));
    }
    void merge_unsafe_rollback_flags()
    {
      /*
        Merge stmt.unsafe_rollback_flags to all.unsafe_rollback_flags. If
        the statement cannot be rolled back safely, the transaction including
        this statement definitely cannot rolled back safely.
      */
      all.add_unsafe_rollback_flags(stmt.get_unsafe_rollback_flags());
    }
  } transaction;
  Global_read_lock global_read_lock;
  Field      *dup_field;
#ifndef __WIN__
  sigset_t signals;
#endif
#ifdef SIGNAL_WITH_VIO_CLOSE
  Vio* active_vio;
#endif
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
  /*
    Used by Intvar_log_event::do_apply_event() and by "SET INSERT_ID=#"
    (mysqlbinlog). We'll soon add a variant which can take many intervals in
    argument.
  */
  inline void force_one_auto_inc_interval(ulonglong next_id)
  {
    auto_inc_intervals_forced.empty(); // in case of multiple SET INSERT_ID
    auto_inc_intervals_forced.append(next_id, ULONGLONG_MAX, 0);
  }

  ulonglong  limit_found_rows;

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

  /** Current statement instrumentation. */
  PSI_statement_locker *m_statement_psi;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  /** Current statement instrumentation state. */
  PSI_statement_locker_state m_statement_state;
#endif /* HAVE_PSI_STATEMENT_INTERFACE */
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
  pthread_t  real_id;                           /* For debugging */
  my_thread_id  thread_id;
  uint	     tmp_table;
  uint	     server_status,open_options;
  enum enum_thread_type system_thread;
  uint       select_number;             //number of select (used for EXPLAIN)
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
  enum_check_fields count_cuted_fields;

  DYNAMIC_ARRAY user_var_events;        /* For user variables replication */
  MEM_ROOT      *user_var_events_alloc; /* Allocate above array elements here */

  /**
    Used by MYSQL_BIN_LOG to maintain the commit queue for binary log
    group commit.
  */
  THD *next_to_commit;

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
    }
    else
      m_trans_log_file= NULL;

    m_trans_end_pos= pos;
    DBUG_PRINT("return", ("m_trans_log_file: %s, m_trans_end_pos: %llu",
                          m_trans_log_file, m_trans_end_pos));
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
  /**@}*/


  /*
    Error code from committing or rolling back the transaction.
  */
  int commit_error;

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
    KILLED_NO_VALUE      /* means neither of the states */
  };
  killed_state volatile killed;

  /* scramble - random string sent to client on handshake */
  char	     scramble[SCRAMBLE_LENGTH+1];

  /// @todo: slave_thread is completely redundant, we should use 'system_thread' instead /sven
  bool       slave_thread, one_shot_set;
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
  bool	     query_start_used, query_start_usec_used;
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
  bool	     abort_on_warning;
  bool 	     got_warning;       /* Set on call to push_warning() */
  /* set during loop of derived table processing */
  bool       derived_tables_processing;
  my_bool    tablespace_op;	/* This is TRUE in DISCARD/IMPORT TABLESPACE */

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

#ifdef WITH_PARTITION_STORAGE_ENGINE
  partition_info *work_part_info;
#endif

#ifndef EMBEDDED_LIBRARY
  /**
    Array of active audit plugins which have been used by this THD.
    This list is later iterated to invoke release_thd() on those
    plugins.
  */
  DYNAMIC_ARRAY audit_class_plugins;
  /**
    Array of bits indicating which audit classes have already been
    added to the list of audit plugins which are currently in use.
  */
  unsigned long audit_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
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

    We want to minimize the time we hold LOCK_thread_count,
    so when destroying a global thread, do:

    thd->release_resources()
    mysql_mutex_lock(&LOCK_thread_count);
    remove_global_thread(thd);
    mysql_mutex_unlock(&LOCK_thread_count);
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
  void change_user(void);
  void cleanup_after_query();
  bool store_globals();
  bool restore_globals();
#ifdef SIGNAL_WITH_VIO_CLOSE
  inline void set_active_vio(Vio* vio)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    active_vio = vio;
    mysql_mutex_unlock(&LOCK_thd_data);
  }
  inline void clear_active_vio()
  {
    mysql_mutex_lock(&LOCK_thd_data);
    active_vio = 0;
    mysql_mutex_unlock(&LOCK_thd_data);
  }
  void close_active_vio();
#endif
  void awake(THD::killed_state state_to_set);

  /** Disconnect the associated communication endpoint. */
  void disconnect();

#ifndef MYSQL_CLIENT
  enum enum_binlog_query_type {
    /* The query can be logged in row format or in statement format. */
    ROW_QUERY_TYPE,
    
    /* The query has to be logged in statement format. */
    STMT_QUERY_TYPE,
    
    QUERY_TYPE_COUNT
  };
  
  int binlog_query(enum_binlog_query_type qtype,
                   char const *query, ulong query_len, bool is_trans,
                   bool direct, bool suppress_use,
                   int errcode);
#endif

  // Begin implementation of MDL_context_owner interface.

  inline void
  enter_cond(mysql_cond_t *cond, mysql_mutex_t* mutex,
             const PSI_stage_info *stage, PSI_stage_info *old_stage,
             const char *src_function, const char *src_file,
             int src_line)
  {
    DBUG_ENTER("THD::enter_cond");
    mysql_mutex_assert_owner(mutex);
    DBUG_PRINT("debug", ("thd: 0x%llx, mysys_var: 0x%llx, current_mutex: 0x%llx -> 0x%llx",
                         (ulonglong) this,
                         (ulonglong) mysys_var,
                         (ulonglong) mysys_var->current_mutex,
                         (ulonglong) mutex));
    mysys_var->current_mutex = mutex;
    mysys_var->current_cond = cond;
    enter_stage(stage, old_stage, src_function, src_file, src_line);
    DBUG_VOID_RETURN;
  }
  inline void exit_cond(const PSI_stage_info *stage,
                        const char *src_function, const char *src_file,
                        int src_line)
  {
    DBUG_ENTER("THD::exit_cond");
    /*
      Putting the mutex unlock in thd->exit_cond() ensures that
      mysys_var->current_mutex is always unlocked _before_ mysys_var->mutex is
      locked (if that would not be the case, you'll get a deadlock if someone
      does a THD::awake() on you).
    */
    DBUG_PRINT("debug", ("thd: 0x%llx, mysys_var: 0x%llx, current_mutex: 0x%llx -> 0x%llx",
                         (ulonglong) this,
                         (ulonglong) mysys_var,
                         (ulonglong) mysys_var->current_mutex,
                         0ULL));
    mysql_mutex_unlock(mysys_var->current_mutex);
    mysql_mutex_lock(&mysys_var->mutex);
    mysys_var->current_mutex = 0;
    mysys_var->current_cond = 0;
    enter_stage(stage, NULL, src_function, src_file, src_line);
    mysql_mutex_unlock(&mysys_var->mutex);
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

    @retval  TRUE  if the thread was woken up
    @retval  FALSE otherwise.
   */
  virtual bool notify_shared_lock(MDL_context_owner *ctx_in_use,
                                  bool needs_thr_lock_abort);

  // End implementation of MDL_context_owner interface.

  inline sql_mode_t datetime_flags() const
  {
    return variables.sql_mode &
      (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES);
  }
  inline bool is_strict_mode() const
  {
    return test(variables.sql_mode & (MODE_STRICT_TRANS_TABLES |
                                      MODE_STRICT_ALL_TABLES));
  }
  inline Time_zone *time_zone()
  {
    time_zone_used= 1;
    return variables.time_zone;
  }
  inline time_t query_start()
  {
    query_start_used= 1;
    return start_time.tv_sec;
  }
  inline long query_start_usec()
  {
    query_start_usec_used= 1;
    return start_time.tv_usec;
  }
  inline timeval query_start_timeval()
  {
    query_start_used= query_start_usec_used= true;
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
    return limit_found_rows;
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
  inline void* trans_alloc(unsigned int size)
  {
    return alloc_root(&transaction.mem_root,size);
  }

  LEX_STRING *make_lex_string(LEX_STRING *lex_str,
                              const char* str, uint length,
                              bool allocate_lex_string);

  bool convert_string(LEX_STRING *to, const CHARSET_INFO *to_cs,
		      const char *from, uint from_length,
		      const CHARSET_INFO *from_cs);

  bool convert_string(String *s, const CHARSET_INFO *from_cs,
                      const CHARSET_INFO *to_cs);

  void add_changed_table(TABLE *table);
  void add_changed_table(const char *key, long key_length);
  CHANGED_TABLE_LIST * changed_table_dup(const char *key, long key_length);
  int send_explain_fields(select_result *result);

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
    is_slave_error= 0;
    DBUG_VOID_RETURN;
  }
#ifndef EMBEDDED_LIBRARY
  inline bool vio_ok() const { return net.vio != 0; }
  /** Return FALSE if connection to client is broken. */
  bool is_connected()
  {
    /*
      All system threads (e.g., the slave IO thread) are connected but
      not using vio. So this function always returns true for all
      system threads.
    */
    return system_thread || (vio_ok() ? vio_is_connected(net.vio) : FALSE);
  }
#else
  inline bool vio_ok() const { return TRUE; }
  inline bool is_connected() { return TRUE; }
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
    Push the given Diagnostics Area on top of the stack, making
    it the new first Diagnostics Area. Conditions in the new second
    Diagnostics Area will be copied to the new first Diagnostics Area.

    @param da   Diagnostics Area to be come the top of
                the Diagnostics Area stack.
  */
  void push_diagnostics_area(Diagnostics_area *da)
  {
    get_stmt_da()->push_diagnostics_area(this, da);
    m_stmt_da= da;
  }

  /// Pop the top DA off the Diagnostics Area stack.
  void pop_diagnostics_area()
  {
    m_stmt_da= get_stmt_da()->pop_diagnostics_area();
  }

public:
  inline const CHARSET_INFO *charset()
  { return variables.character_set_client; }
  void update_charset();

  inline Query_arena *activate_stmt_arena_if_needed(Query_arena *backup)
  {
    /*
      Use the persistent arena if we are in a prepared statement or a stored
      procedure statement and we have not already changed to use this arena.
    */
    if (!stmt_arena->is_conventional() && mem_root != stmt_arena->mem_root)
    {
      set_n_backup_active_arena(stmt_arena, backup);
      return stmt_arena;
    }
    return 0;
  }

  void change_item_tree(Item **place, Item *new_value)
  {
    /* TODO: check for OOM condition here */
    if (!stmt_arena->is_conventional())
    {
      DBUG_PRINT("info",
                 ("change_item_tree place %p old_value %p new_value %p",
                  place, *place, new_value));
      nocheck_register_item_tree_change(place, *place, mem_root);
    }
    *place= new_value;
  }

/*
  Find and update change record of an underlying item.

  @param old_ref The old place of moved expression.
  @param new_ref The new place of moved expression.
  @details
  During permanent transformations, e.g. join flattening in simplify_joins,
  a condition could be moved from one place to another, e.g. from on_expr
  to WHERE condition. If the moved condition has replaced some other with
  change_item_tree() function, the change record will restore old value
  to the wrong place during rollback_item_tree_changes. This function goes
  through the list of change records, and replaces Item_change_record::place.
*/
  void change_item_tree_place(Item **old_ref, Item **new_ref);
  void nocheck_register_item_tree_change(Item **place, Item *old_value,
                                         MEM_ROOT *runtime_memroot);
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
      if ((err == KILL_CONNECTION) && !shutdown_in_progress)
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
  /* return TRUE if we will abort query if we make a warning now */
  inline bool really_abort_on_warning()
  {
    return (abort_on_warning &&
            (!transaction.stmt.cannot_safely_rollback() ||
             (variables.sql_mode & MODE_STRICT_ALL_TABLES)));
  }
  void set_status_var_init();
  void reset_n_backup_open_tables_state(Open_tables_backup *backup);
  void restore_backup_open_tables_state(Open_tables_backup *backup);
  void reset_sub_statement_state(Sub_statement_state *backup, uint new_state);
  void restore_sub_statement_state(Sub_statement_state *backup);
  void set_n_backup_active_arena(Query_arena *set, Query_arena *backup);
  void restore_active_arena(Query_arena *set, Query_arena *backup);

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
    Return the statement or transaction group cache for this thread.
    @param is_transactional if true, return the transaction group cache.
    If false, return the statement group cache.
  */
  Group_cache *get_group_cache(bool is_transactional);

  /**
    If this thread owns a single GTID, then owned_gtid is set to that
    group.  If this thread does not own any GTID at all,
    owned_gtid.sidno==0.  If owned_gtid_set contains the set of owned
    gtids, owned_gtid.sidno==-1.
  */
  Gtid owned_gtid;
  /**
    If this thread owns a set of GTIDs (i.e., GTID_NEXT_LIST != NULL),
    then this member variable contains the subset of those GTIDs that
    are owned by this thread.
  */
  Gtid_set owned_gtid_set;

  void clear_owned_gtids()
  {
    if (owned_gtid.sidno == -1)
    {
#ifdef HAVE_NDB_BINLOG
      owned_gtid_set.clear();
#else
      DBUG_ASSERT(0);
#endif
    }
    owned_gtid.sidno= 0;
  }

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
      @retval FALSE Success
      @retval TRUE  Out-of-memory error
  */
  bool set_db(const char *new_db, size_t new_db_len)
  {
    bool result;
    /* Do not reallocate memory if current chunk is big enough. */
    if (db && new_db && db_length >= new_db_len)
      memcpy(db, new_db, new_db_len+1);
    else
    {
      my_free(db);
      if (new_db)
        db= my_strndup(new_db, new_db_len, MYF(MY_WME | ME_FATALERROR));
      else
        db= NULL;
    }
    db_length= db ? new_db_len : 0;
    result= new_db && !db;
#ifdef HAVE_PSI_THREAD_INTERFACE
    if (result)
      PSI_THREAD_CALL(set_thread_db)(new_db, static_cast<int>(new_db_len));
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
  void reset_db(char *new_db, size_t new_db_len)
  {
    db= new_db;
    db_length= new_db_len;
#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_db)(new_db, static_cast<int>(new_db_len));
#endif
  }
  /*
    Copy the current database to the argument. Use the current arena to
    allocate memory for a deep copy: current database may be freed after
    a statement is parsed but before it's executed.
  */
  bool copy_db_to(char **p_db, size_t *p_db_length)
  {
    if (db == NULL)
    {
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
      return TRUE;
    }
    *p_db= strmake(db, db_length);
    *p_db_length= db_length;
    return FALSE;
  }
  thd_scheduler scheduler;

public:
  inline Internal_error_handler *get_internal_handler()
  { return m_internal_handler; }

  /**
    Add an internal error handler to the thread execution context.
    @param handler the exception handler to add
  */
  void push_internal_handler(Internal_error_handler *handler);

private:
  /**
    Handle a sql condition.
    @param sql_errno the condition error number
    @param sqlstate the condition sqlstate
    @param level the condition level
    @param msg the condition message text
    @param[out] cond_hdl the sql condition raised, if any
    @return true if the condition is handled
  */
  bool handle_condition(uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level level,
                        const char* msg,
                        Sql_condition ** cond_hdl);

public:
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
    @return The condition raised, or NULL
  */
  Sql_condition*
  raise_condition(uint sql_errno,
                  const char* sqlstate,
                  Sql_condition::enum_severity_level level,
                  const char* msg);

public:
  /** Overloaded to guard query/query_length fields */
  virtual void set_statement(Statement *stmt);

  void set_command(enum enum_server_command command);

  inline enum enum_server_command get_command() const
  { return m_command; }

  /**
    Assign a new value to thd->query and thd->query_id and mysys_var.
    Protected with LOCK_thd_data mutex.
  */
  void set_query(char *query_arg, uint32 query_length_arg,
                 const CHARSET_INFO *cs_arg)
  {
    set_query(CSET_STRING(query_arg, query_length_arg, cs_arg));
  }
  void set_query(char *query_arg, uint32 query_length_arg) /*Mutex protected*/
  {
    set_query(CSET_STRING(query_arg, query_length_arg, charset()));
  }
  void set_query(const CSET_STRING &str); /* Mutex protected */
  void reset_query()               /* Mutex protected */
  { set_query(CSET_STRING()); }
  void set_query_and_id(char *query_arg, uint32 query_length_arg,
                        const CHARSET_INFO *cs, query_id_t new_query_id);
  void set_query_id(query_id_t new_query_id);
  void set_open_tables(TABLE *open_tables_arg)
  {
    mysql_mutex_lock(&LOCK_thd_data);
    open_tables= open_tables_arg;
    mysql_mutex_unlock(&LOCK_thd_data);
  }
  void set_mysys_var(struct st_my_thread_var *new_mysys_var);
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

     1. Statements that could possibly do DML in a non-transactional
        table;

     2. CREATE...SELECT statement;

     3. CREATE TEMPORARY TABLE or DROP TEMPORARY TABLE within a transaction

    The first condition has to be checked in decide_logging_format,
    because that's where we know if the table is transactional or not.
    The second and third conditions have to be checked in
    mysql_execute_command because (1) that prevents implicit commit
    from being executed if the statement fails; (2) DROP TEMPORARY
    TABLE does not invoke decide_logging_format.

    Later, we can relax the first condition as follows:
     - do not wrap non-transactional updates inside BEGIN ... COMMIT
       when writing them to the binary log.
     - allow non-transactional updates that are made outside of
       transactional context

    Moreover, we can drop the second condition if we fix BUG#11756034.

    @param transactional_table true if the statement updates some
    transactional table; false otherwise.

    @param non_transactional_table true if the statement updates some
    non-transactional table; false otherwise.

    @param non_transactional_tmp_tables true if row binlog format is
    used and all non-transactional tables are temporary.

    @retval true if the statement is compatible;
    @retval false if the statement is not compatible.
  */
  bool
  is_dml_gtid_compatible(bool transactional_table,
                         bool non_transactional_table,
                         bool non_transactional_tmp_tables) const;
  bool is_ddl_gtid_compatible() const;
  void binlog_invoker() { m_binlog_invoker= TRUE; }
  bool need_binlog_invoker() { return m_binlog_invoker; }
  void get_definer(LEX_USER *definer);
  void set_invoker(const LEX_STRING *user, const LEX_STRING *host)
  {
    invoker_user= *user;
    invoker_host= *host;
  }
  LEX_STRING get_invoker_user() { return invoker_user; }
  LEX_STRING get_invoker_host() { return invoker_host; }
  bool has_invoker() { return invoker_user.length > 0; }

#ifndef DBUG_OFF
private:
  int gis_debug; // Storage for "SELECT ST_GIS_DEBUG(param);"
public:
  int get_gis_debug() { return gis_debug; }
  void set_gis_debug(int arg) { gis_debug= arg; }
#endif

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
  Diagnostics_area *m_stmt_da;

  /**
    It will be set TURE if CURRENT_USER() is called in account management
    statements or default definer is set in CREATE/ALTER SP, SF, Event,
    TRIGGER or VIEW statements.

    Current user will be binlogged into Query_log_event if current_user_used
    is TRUE; It will be stored into invoker_host and invoker_user by SQL thread.
   */
  bool m_binlog_invoker;

  /**
    It points to the invoker in the Query_log_event.
    SQL thread use it as the default definer in CREATE/ALTER SP, SF, Event,
    TRIGGER or VIEW statements or current user in account management
    statements if it is not NULL.
   */
  LEX_STRING invoker_user;
  LEX_STRING invoker_host;
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
}

#define tmp_disable_binlog(A)       \
  {ulonglong tmp_disable_binlog__save_options= (A)->variables.option_bits; \
  (A)->variables.option_bits&= ~OPTION_BIN_LOG

#define reenable_binlog(A)   (A)->variables.option_bits= tmp_disable_binlog__save_options;}


LEX_STRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_STRING *lex_str, const char* str, uint length,
                     bool allocate_lex_string);

/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class sql_exchange :public Sql_alloc
{
public:
  enum enum_filetype filetype; /* load XML, Added by Arnold & Erik */
  char *file_name;
  const String *field_term, *enclosed, *line_term, *line_start, *escaped;
  bool opt_enclosed;
  bool dumpfile;
  ulong skip_lines;
  const CHARSET_INFO *cs;
  sql_exchange(char *name, bool dumpfile_flag,
               enum_filetype filetype_arg= FILETYPE_CSV);
  bool escaped_given(void);
};

/*
  This is used to get result from a select
*/

class JOIN;

class select_result :public Sql_alloc {
protected:
  THD *thd;
  SELECT_LEX_UNIT *unit;
public:
  /**
    Number of records estimated in this result.
    Valid only for materialized derived tables/views.
  */
  ha_rows estimated_rowcount;
  select_result();
  virtual ~select_result() {};
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
  virtual void send_error(uint errcode,const char *err);
  virtual bool send_eof()=0;
  /**
    Check if this query returns a result set and therefore is allowed in
    cursors and set an error message if it is not the case.

    @retval FALSE     success
    @retval TRUE      error, an error message is set
  */
  virtual bool check_simple_select() const;
  virtual void abort_result_set() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup();
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
  Base class for select_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class select_result_interceptor: public select_result
{
public:
  select_result_interceptor() {}              /* Remove gcc warning */
  uint field_count(List<Item> &fields) const { return 0; }
  bool send_result_set_metadata(List<Item> &fields, uint flag) { return FALSE; }
};


class select_send :public select_result {
  /**
    True if we have sent result set metadata to the client.
    In this case the client always expects us to end the result
    set with an eof or error packet
  */
  bool is_result_set_started;
public:
  select_send() :is_result_set_started(FALSE) {}
  bool send_result_set_metadata(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const { return FALSE; }
  void abort_result_set();
  virtual void cleanup();
};


class select_to_file :public select_result_interceptor {
protected:
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];

public:
  select_to_file(sql_exchange *ex) :exchange(ex), file(-1),row_count(0L)
  { path[0]=0; }
  ~select_to_file();
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void cleanup();
};


#define ESCAPE_CHARS "ntrb0ZN" // keep synchronous with READ_INFO::unescape


/*
 List of all possible characters of a numeric value text representation.
*/
#define NUMERIC_CHARS ".0123456789e+-"


class select_export :public select_to_file {
  uint field_term_length;
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
  select_export(sql_exchange *ex) :select_to_file(ex) {}
  ~select_export();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class select_dump :public select_to_file {
public:
  select_dump(sql_exchange *ex) :select_to_file(ex) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};

/**
   @todo This class is declared in sql_class.h, but the members are defined in
   sql_insert.cc. It is very confusing that a class is defined in a file with
   a different name than the file where it is declared.
*/
class select_insert :public select_result_interceptor {
public:
  TABLE_LIST *table_list;
  TABLE *table;
private:
  /**
     The columns of the table to be inserted into, *or* the columns of the
     table from which values are selected. For legacy reasons both are
     allowed.
   */
  List<Item> *fields;
public:
  ulonglong autoinc_value_of_last_inserted_row; // autogenerated or not
  COPY_INFO info;
  COPY_INFO update; ///< the UPDATE part of "info"
  bool insert_into_view;

  /**
     Creates a select_insert for routing a result set to an existing
     table.

     @param table_list_par   The table reference for the destination table.
     @param table_par        The destination table. May be NULL.
     @param target_columns   The columns of the table which is the target for
                             insertion. May be NULL, but if not, the same
                             value must be used for target_or_source_columns.
     @param target_or_source_columns The columns of the source table providing
                             data, or columns of the target table. If the
                             target table is known, the columns of that table
                             should be used. If the target table is not known
                             (it may not yet exist), the columns of the source
                             table should be used, and target_columns should
                             be NULL.
     @param update_fields    The columns to be updated in case of duplicate
                             keys. May be NULL.
     @param update_values    The values to be assigned in case of duplicate
                             keys. May be NULL.
     @param duplicate        The policy for handling duplicates.
     @param ignore           How the insert operation is to handle certain
                             errors. See COPY_INFO.

     @todo This constructor takes 8 arguments, 6 of which are used to
     immediately construct a COPY_INFO object. Obviously the constructor
     should take the COPY_INFO object as argument instead. Also, some
     select_insert members initialized here are totally redundant, as they are
     found inside the COPY_INFO.

     Here is the explanation of how we set the manage_defaults parameter of
     info's constructor below.
     @li if target_columns==NULL, the statement is
@verbatim
     CREATE TABLE a_table (possibly some columns1) SELECT columns2
@endverbatim
     which sets all of a_table's columns2 to values returned by SELECT (no
     default needs to be set); a_table's columns1 get set from defaults
     prepared by make_empty_rec() when table is created, not by COPY_INFO. So
     manage_defaults is "false".
     @li otherwise, target_columns!=NULL and so it is INSERT SELECT. If there
     are explicitely listed columns like
@verbatim
     INSERT INTO a_table (columns1) SELECT ...
@verbatim
     then non-listed columns (columns of a_table which are not columns1) may
     need a default set by COPY_INFO so manage_defaults is "true". If no
     column is explicitely listed, all columns will be set to values returned
     by SELECT, so "manage_defaults" is false.
  */
  select_insert(TABLE_LIST *table_list_par,
                TABLE *table_par,
                List<Item> *target_columns,
                List<Item> *target_or_source_columns,
                List<Item> *update_fields,
                List<Item> *update_values,
                enum_duplicates duplic,
                bool ignore)
    :table_list(table_list_par),
     table(table_par),
     fields(target_or_source_columns),
     autoinc_value_of_last_inserted_row(0),
     info(COPY_INFO::INSERT_OPERATION,
          target_columns,
          // manage_defaults
          target_columns != NULL && target_columns->elements != 0,
          duplic,
          ignore),
     update(COPY_INFO::UPDATE_OPERATION,
            update_fields,
            update_values),
     insert_into_view(table_list_par && table_list_par->view != 0)
  {
    DBUG_ASSERT(target_or_source_columns != NULL);
    DBUG_ASSERT(target_columns == target_or_source_columns ||
                target_columns == NULL);
  }


public:
  ~select_insert();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  virtual int prepare2(void);
  bool send_data(List<Item> &items);
  virtual void store_values(List<Item> &values);
  void send_error(uint errcode,const char *err);
  bool send_eof();
  virtual void abort_result_set();
  /* not implemented: select_insert is never re-used in prepared statements */
  void cleanup();
};


/**
   @todo This class inherits a class which is non-abstract. This is not in
   line with good programming practices and the inheritance should be broken
   up. Also, the class is declared in sql_class.h, but defined sql_insert.cc
   which is confusing.
*/
class select_create: public select_insert {
  ORDER *group;
  TABLE_LIST *create_table;
  HA_CREATE_INFO *create_info;
  TABLE_LIST *select_tables;
  Alter_info *alter_info;
  Field **field;
  /* lock data for tmp table */
  MYSQL_LOCK *m_lock;
  /* m_lock or thd->extra_lock */
  MYSQL_LOCK **m_plock;
public:
  select_create (TABLE_LIST *table_arg,
		 HA_CREATE_INFO *create_info_par,
                 Alter_info *alter_info_arg,
		 List<Item> &select_fields,enum_duplicates duplic, bool ignore,
                 TABLE_LIST *select_tables_arg)
    :select_insert (NULL, // table_list_par
                    NULL, // table_par
                    NULL, // target_columns
                    &select_fields,
                    NULL, // update_fields
                    NULL, // update_values
                    duplic,
                    ignore),
     create_table(table_arg),
     create_info(create_info_par),
     select_tables(select_tables_arg),
     alter_info(alter_info_arg),
     m_plock(NULL)
  {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);

  int binlog_show_create_table(TABLE **tables, uint count);
  void store_values(List<Item> &values);
  void send_error(uint errcode,const char *err);
  bool send_eof();
  virtual void abort_result_set();

  // Needed for access from local class MY_HOOKS in prepare(), since thd is proteted.
  const THD *get_thd(void) { return thd; }
  const HA_CREATE_INFO *get_create_info() { return create_info; };
  int prepare2(void);
};

#include <myisam.h>

/* 
  Param to create temporary tables when doing SELECT:s 
  NOTE
    This structure is copied using memcpy as a part of JOIN.
*/

class TMP_TABLE_PARAM :public Sql_alloc
{
public:
  List<Item> copy_funcs;
  Copy_field *copy_field, *copy_field_end;
  uchar	    *group_buff;
  Item	    **items_to_copy;			/* Fields in tmp table */
  MI_COLUMNDEF *recinfo,*start_recinfo;
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
  bool  using_indirect_summary_function;
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

  TMP_TABLE_PARAM()
    :copy_field(0), copy_field_end(0), group_parts(0),
     group_length(0), group_null_parts(0),
     using_indirect_summary_function(0),
     schema_table(0), precomputed_group_by(0), force_copy_fields(0),
     skip_create_table(FALSE), bit_fields_as_long(0)
  {}
  ~TMP_TABLE_PARAM()
  {
    cleanup();
  }
  void init(void);
  inline void cleanup(void)
  {
    if (copy_field)				/* Fix for Intel compiler */
    {
      delete [] copy_field;
      copy_field= NULL;
      copy_field_end= NULL;
    }
  }
};

class select_union :public select_result_interceptor
{
  TMP_TABLE_PARAM tmp_table_param;
public:
  TABLE *table;

  select_union() :table(0) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  bool flush();
  void cleanup();
  bool create_result_table(THD *thd, List<Item> *column_types,
                           bool is_distinct, ulonglong options,
                           const char *alias, bool bit_fields_as_long,
                           bool create_table);
  friend bool mysql_derived_create(THD *thd, LEX *lex, TABLE_LIST *derived);
};

/* Base subselect interface class */
class select_subselect :public select_result_interceptor
{
protected:
  Item_subselect *item;
public:
  select_subselect(Item_subselect *item);
  bool send_data(List<Item> &items)=0;
  bool send_eof() { return 0; };
};

/* Single value subselect interface class */
class select_singlerow_subselect :public select_subselect
{
public:
  select_singlerow_subselect(Item_subselect *item_arg)
    :select_subselect(item_arg)
  {}
  bool send_data(List<Item> &items);
};

/* used in independent ALL/ANY optimisation */
class select_max_min_finder_subselect :public select_subselect
{
  Item_cache *cache;
  bool (select_max_min_finder_subselect::*op)();
  bool fmax;
  /**
    If ignoring NULLs, comparisons will skip NULL values. If not
    ignoring NULLs, the first (if any) NULL value discovered will be
    returned as the maximum/minimum value.
  */
  bool ignore_nulls;
public:
  select_max_min_finder_subselect(Item_subselect *item_arg, bool mx,
                                  bool ignore_nulls)
    :select_subselect(item_arg), cache(0), fmax(mx), ignore_nulls(ignore_nulls)
  {}
  void cleanup();
  bool send_data(List<Item> &items);
private:
  bool cmp_real();
  bool cmp_int();
  bool cmp_decimal();
  bool cmp_str();
};

/* EXISTS subselect interface class */
class select_exists_subselect :public select_subselect
{
public:
  select_exists_subselect(Item_subselect *item_arg)
    :select_subselect(item_arg){}
  bool send_data(List<Item> &items);
};


/* Structs used when sorting */

typedef struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  uint	 length;			/* Length of sort field */
  uint   suffix_length;                 /* Length suffix (0-4) */
  Item_result result_type;		/* Type of item */
  bool reverse;				/* if descending sort */
  bool need_strxnfrm;			/* If we have to use strxnfrm() */
} SORT_FIELD;


typedef struct st_sort_buffer {
  uint index;					/* 0 or 1 */
  uint sort_orders;
  uint change_pos;				/* If sort-fields changed */
  char **buff;
  SORT_FIELD *sortorder;
} SORT_BUFFER;

/* Structure for db & table in sql_yacc */

class Table_ident :public Sql_alloc
{
public:
  LEX_STRING db;
  LEX_STRING table;
  SELECT_LEX_UNIT *sel;
  inline Table_ident(THD *thd, LEX_STRING db_arg, LEX_STRING table_arg,
		     bool force)
    :table(table_arg), sel((SELECT_LEX_UNIT *)0)
  {
    if (!force && (thd->client_capabilities & CLIENT_NO_SCHEMA))
      db.str=0;
    else
      db= db_arg;
  }
  inline Table_ident(LEX_STRING table_arg) 
    :table(table_arg), sel((SELECT_LEX_UNIT *)0)
  {
    db.str=0;
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
    db.str= empty_c_string;                    /* a subject to casedn_str */
    db.length= 0;
    table.str= internal_table_name;
    table.length=1;
  }
  bool is_derived_table() const { return test(sel); }
  inline void change_db(char *db_name)
  {
    db.str= db_name; db.length= (uint) strlen(db_name);
  }
};

// this is needed for user_vars hash
class user_var_entry
{
  static const size_t extra_size= sizeof(double);
  char *m_ptr;          // Value
  ulong m_length;       // Value length
  Item_result m_type;   // Value type

  void reset_value()
  { m_ptr= NULL; m_length= 0; }
  void set_value(char *value, ulong length)
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
  bool realloc(uint length);

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
  */
  void init(const Simple_cstring &name)
  {
    copy_name(name);
    reset_value();
    update_query_id= 0;
    collation.set(NULL, DERIVATION_IMPLICIT, 0);
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
    used_query_id= current_thd->query_id;
    set_type(STRING_RESULT);
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
  bool store(void *from, uint length, Item_result type);

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
  bool store(void *from, uint length, Item_result type,
             const CHARSET_INFO *cs, Derivation dv, bool unsigned_arg);
  /**
    Set type of to the given value.
    @param type  Data type.
  */
  void set_type(Item_result type) { m_type= type; }
  /**
    Set value to NULL
    @param type  Data type.
  */

  void set_null_value(Item_result type)
  {
    free_value();
    reset_value();
    set_type(type);
  }

  /**
    Allocate and initialize a user variable instance.
    @param namec  Name of the variable.
    @return
    @retval  Address of the allocated and initialized user_var_entry instance.
    @retval  NULL on allocation error.
  */
  static user_var_entry *create(const Name_string &name)
  {
    user_var_entry *entry;
    uint size= ALIGN_SIZE(sizeof(user_var_entry)) +
               (name.length() + 1) + extra_size;
    if (!(entry= (user_var_entry*) my_malloc(size, MYF(MY_WME |
                                                       ME_FATALERROR))))
      return NULL;
    entry->init(name);
    return entry;
  }

  /**
    Free all memory used by a user_var_entry instance
    previously created by create().
  */
  void destroy()
  {
    free_value();  // Free the external value buffer
    my_free(this); // Free the instance itself
  }

  /* Routines to access the value and its type */
  const char *ptr() const { return m_ptr; }
  ulong length() const { return m_length; }
  Item_result type() const { return m_type; }
  /* Item-alike routines to access the value */
  double val_real(my_bool *null_value);
  longlong val_int(my_bool *null_value) const;
  String *val_str(my_bool *null_value, String *str, uint decimals);
  my_decimal *val_decimal(my_bool *null_value, my_decimal *result);
};

/*
   Unique -- class for unique (removing of duplicates). 
   Puts all values to the TREE. If the tree becomes too big,
   it's dumped to the file. User can request sorted values, or
   just iterate through them. In the last case tree merging is performed in
   memory simultaneously with iteration, so it should be ~2-3x faster.
 */

class Unique :public Sql_alloc
{
  DYNAMIC_ARRAY file_ptrs;
  ulong max_elements;
  ulonglong max_in_memory_size;
  IO_CACHE file;
  TREE tree;
  uchar *record_pointers;
  bool flush();
  uint size;

public:
  ulong elements;
  Unique(qsort_cmp2 comp_func, void *comp_func_fixed_arg,
	 uint size_arg, ulonglong max_in_memory_size_arg);
  ~Unique();
  ulong elements_in_tree() { return tree.elements_in_tree; }
  inline bool unique_add(void *ptr)
  {
    DBUG_ENTER("unique_add");
    DBUG_PRINT("info", ("tree %u - %lu", tree.elements_in_tree, max_elements));
    if (tree.elements_in_tree > max_elements && flush())
      DBUG_RETURN(1);
    DBUG_RETURN(!tree_insert(&tree, ptr, 0, tree.custom_arg));
  }

  bool get(TABLE *table);
  static double get_use_cost(uint *buffer, uint nkeys, uint key_size, 
                             ulonglong max_in_memory_size);
  inline static int get_cost_calc_buff_size(ulong nkeys, uint key_size, 
                                            ulonglong max_in_memory_size)
  {
    ulonglong max_elems_in_tree=
      (1 + max_in_memory_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+key_size));
    return (int) (sizeof(uint)*(1 + nkeys/max_elems_in_tree));
  }

  void reset();
  bool walk(tree_walk_action action, void *walk_action_arg);

  uint get_size() const { return size; }
  ulonglong get_max_in_memory_size() const { return max_in_memory_size; }

  friend int unique_write_to_file(uchar* key, element_count count, Unique *unique);
  friend int unique_write_to_ptrs(uchar* key, element_count count, Unique *unique);
};


class multi_delete :public select_result_interceptor
{
  TABLE_LIST *delete_tables, *table_being_deleted;
  Unique **tempfiles;
  ha_rows deleted, found;
  uint num_of_tables;
  int error;
  bool do_delete;
  /* True if at least one table we delete from is transactional */
  bool transactional_tables;
  /* True if at least one table we delete from is not transactional */
  bool normal_tables;
  bool delete_while_scanning;
  /*
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

public:
  multi_delete(TABLE_LIST *dt, uint num_of_tables);
  ~multi_delete();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int do_deletes();
  int do_table_deletes(TABLE *table, bool ignore);
  bool send_eof();
  inline ha_rows num_deleted()
  {
    return deleted;
  }
  virtual void abort_result_set();
};


/**
   @todo This class is declared here but implemented in sql_update.cc, which
   is very confusing.
*/
class multi_update :public select_result_interceptor
{
  TABLE_LIST *all_tables; /* query/update command tables */
  TABLE_LIST *leaves;     /* list of leves of join table tree */
  TABLE_LIST *update_tables, *table_being_updated;
  TABLE **tmp_tables, *main_table, *table_to_update;
  TMP_TABLE_PARAM *tmp_table_param;
  ha_rows updated, found;
  List <Item> *fields, *values;
  List <Item> **fields_for_table, **values_for_table;
  uint table_count;
  /*
   List of tables referenced in the CHECK OPTION condition of
   the updated view excluding the updated table. 
  */
  List <TABLE> unupdated_check_opt_tables;
  Copy_field *copy_field;
  enum enum_duplicates handle_duplicates;
  bool do_update, trans_safe;
  /* True if the update operation has made a change in a transactional table */
  bool transactional_tables;
  bool ignore;
  /* 
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

  /**
     Array of update operations, arranged per _updated_ table. For each
     _updated_ table in the multiple table update statement, a COPY_INFO
     pointer is present at the table's position in this array.

     The array is allocated and populated during multi_update::prepare(). The
     position that each table is assigned is also given here and is stored in
     the member TABLE::pos_in_table_list::shared. However, this is a publicly
     available field, so nothing can be trusted about its integrity.

     This member is NULL when the multi_update is created.

     @see multi_update::prepare
  */
  COPY_INFO **update_operations;

public:
  multi_update(TABLE_LIST *ut, TABLE_LIST *leaves_list,
	       List<Item> *fields, List<Item> *values,
	       enum_duplicates handle_duplicates, bool ignore);
  ~multi_update();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_updates();
  bool send_eof();
  inline ha_rows num_found()
  {
    return found;
  }
  inline ha_rows num_updated()
  {
    return updated;
  }
  virtual void abort_result_set();
};

class my_var : public Sql_alloc  {
public:
  LEX_STRING s;
#ifndef DBUG_OFF
  /*
    Routine to which this Item_splocal belongs. Used for checking if correct
    runtime context is used for variable handling.
  */
  sp_head *sp;
#endif
  bool local;
  uint offset;
  enum_field_types type;
  my_var (LEX_STRING& j, bool i, uint o, enum_field_types t)
    :s(j), local(i), offset(o), type(t)
  {}
  ~my_var() {}
};

class select_dumpvar :public select_result_interceptor {
  ha_rows row_count;
public:
  List<my_var> var_list;
  select_dumpvar()  { var_list.empty(); row_count= 0;}
  ~select_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const;
  void cleanup();
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

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var);

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var);
void mark_transaction_to_rollback(THD *thd, bool all);

/* Inline functions */

inline bool add_item_to_list(THD *thd, Item *item)
{
  return thd->lex->current_select->add_item_to_list(thd, item);
}

inline bool add_value_to_list(THD *thd, Item *value)
{
  return thd->lex->value_list.push_back(value);
}

inline bool add_order_to_list(THD *thd, Item *item, bool asc)
{
  return thd->lex->current_select->add_order_to_list(thd, item, asc);
}

inline bool add_group_to_list(THD *thd, Item *item, bool asc)
{
  return thd->lex->current_select->add_group_to_list(thd, item, asc);
}

#endif /* MYSQL_SERVER */

#endif /* SQL_CLASS_INCLUDED */
