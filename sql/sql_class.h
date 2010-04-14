/* Copyright 2000-2008 MySQL AB, 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Classes in mysql */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "log.h"
#include "rpl_tblmap.h"

/**
  An interface that is used to take an action when
  the locking module notices that a table version has changed
  since the last execution. "Table" here may refer to any kind of
  table -- a base table, a temporary table, a view or an
  information schema table.

  When we open and lock tables for execution of a prepared
  statement, we must verify that they did not change
  since statement prepare. If some table did change, the statement
  parse tree *may* be no longer valid, e.g. in case it contains
  optimizations that depend on table metadata.

  This class provides an interface (a method) that is
  invoked when such a situation takes place.
  The implementation of the method simply reports an error, but
  the exact details depend on the nature of the SQL statement.

  At most 1 instance of this class is active at a time, in which
  case THD::m_reprepare_observer is not NULL.

  @sa check_and_update_table_version() for details of the
  version tracking algorithm 

  @sa Open_tables_state::m_reprepare_observer for the life cycle
  of metadata observers.
*/

class Reprepare_observer
{
public:
  /**
    Check if a change of metadata is OK. In future
    the signature of this method may be extended to accept the old
    and the new versions, but since currently the check is very
    simple, we only need the THD to report an error.
  */
  bool report_error(THD *thd);
  bool is_invalidated() const { return m_invalidated; }
  void reset_reprepare_observer() { m_invalidated= FALSE; }
private:
  bool m_invalidated;
};


class Relay_log_info;

class Query_log_event;
class Load_log_event;
class Slave_log_event;
class sp_rcontext;
class sp_cache;
class Parser_state;
class Rows_log_event;

enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_UPDATE };
enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };
enum enum_slave_exec_mode { SLAVE_EXEC_MODE_STRICT,
                            SLAVE_EXEC_MODE_IDEMPOTENT,
                            SLAVE_EXEC_MODE_LAST_BIT};
enum enum_mark_columns
{ MARK_COLUMNS_NONE, MARK_COLUMNS_READ, MARK_COLUMNS_WRITE};

extern char internal_table_name[2];
extern char empty_c_string[1];
extern MYSQL_PLUGIN_IMPORT const char **errmesg;

extern bool volatile shutdown_in_progress;

#define TC_LOG_PAGE_SIZE   8192
#define TC_LOG_MIN_SIZE    (3*TC_LOG_PAGE_SIZE)

#define TC_HEURISTIC_RECOVER_COMMIT   1
#define TC_HEURISTIC_RECOVER_ROLLBACK 2
extern uint tc_heuristic_recover;

typedef struct st_user_var_events
{
  user_var_entry *user_var_event;
  char *value;
  ulong length;
  Item_result type;
  uint charset_number;
} BINLOG_USER_VAR_EVENT;

#define RP_LOCK_LOG_IS_ALREADY_LOCKED 1
#define RP_FORCE_ROTATE               2

/*
  The COPY_INFO structure is used by INSERT/REPLACE code.
  The schema of the row counting by the INSERT/INSERT ... ON DUPLICATE KEY
  UPDATE code:
    If a row is inserted then the copied variable is incremented.
    If a row is updated by the INSERT ... ON DUPLICATE KEY UPDATE and the
      new data differs from the old one then the copied and the updated
      variables are incremented.
    The touched variable is incremented if a row was touched by the update part
      of the INSERT ... ON DUPLICATE KEY UPDATE no matter whether the row
      was actually changed or not.
*/
typedef struct st_copy_info {
  ha_rows records; /**< Number of processed records */
  ha_rows deleted; /**< Number of deleted records */
  ha_rows updated; /**< Number of updated records */
  ha_rows copied;  /**< Number of copied records */
  ha_rows error_count;
  ha_rows touched; /* Number of touched records */
  enum enum_duplicates handle_duplicates;
  int escape_char, last_errno;
  bool ignore;
  /* for INSERT ... UPDATE */
  List<Item> *update_fields;
  List<Item> *update_values;
  /* for VIEW ... WITH CHECK OPTION */
  TABLE_LIST *view;
} COPY_INFO;


class Key_part_spec :public Sql_alloc {
public:
  const char *field_name;
  uint length;
  Key_part_spec(const char *name,uint len=0) :field_name(name), length(len) {}
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
  enum drop_type {KEY, COLUMN };
  const char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,const char *par_name)
    :name(par_name), type(par_type) {}
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
  const char *name;
  bool generated;

  Key(enum Keytype type_par, const char *name_arg,
      KEY_CREATE_INFO *key_info_arg,
      bool generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
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

  Table_ident *ref_table;
  List<Key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  Foreign_key(const char *name_arg, List<Key_part_spec> &cols,
	      Table_ident *table,   List<Key_part_spec> &ref_cols,
	      uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key(FOREIGN_KEY, name_arg, &default_key_create_info, 0, cols),
    ref_table(table), ref_columns(ref_cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  {}
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

#include "sql_lex.h"				/* Must be here */

class Delayed_insert;
class select_result;
class Time_zone;

#define THD_SENTRY_MAGIC 0xfeedd1ff
#define THD_SENTRY_GONE  0xdeadbeef

#define THD_CHECK_SENTRY(thd) DBUG_ASSERT(thd->dbug_sentry == THD_SENTRY_MAGIC)

struct system_variables
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
  uint dynamic_variables_head;  /* largest valid variable offset */
  uint dynamic_variables_size;  /* how many bytes are in use */
  
  ulonglong myisam_max_extra_sort_file_size;
  ulonglong myisam_max_sort_file_size;
  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  ulong join_buff_size;
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
  /* A bitmap for switching optimizations on/off */
  ulong optimizer_switch;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong query_cache_type;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong div_precincrement;
  ulong sortbuff_size;
  ulong thread_handling;
  ulong tx_isolation;
  ulong completion_type;
  /* Determines which non-standard SQL behaviour should be enabled */
  ulong sql_mode;
  ulong max_sp_recursion_depth;
  /* check of key presence in updatable view */
  ulong updatable_views_with_limit;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;
  ulong group_concat_max_len;
  ulong ndb_autoincrement_prefetch_sz;
  ulong ndb_index_stat_cache_entries;
  ulong ndb_index_stat_update_freq;
  ulong binlog_format; // binlog format for this thd (see enum_binlog_format)
  my_bool binlog_direct_non_trans_update;
  /*
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  my_thread_id pseudo_thread_id;

  my_bool low_priority_updates;
  my_bool new_mode;
  /* 
    compatibility option:
      - index usage hints (USE INDEX without a FOR clause) behave as in 5.0 
  */
  my_bool old_mode;
  my_bool query_cache_wlock_invalidate;
  my_bool engine_condition_pushdown;
  my_bool keep_files_on_create;
  my_bool ndb_force_send;
  my_bool ndb_use_copying_alter_table;
  my_bool ndb_use_exact_count;
  my_bool ndb_use_transactions;
  my_bool ndb_index_stat_enable;

  my_bool old_alter_table;
  my_bool old_passwords;

  plugin_ref table_plugin;

  /* Only charset part of these variables is sensible */
  CHARSET_INFO  *character_set_filesystem;
  CHARSET_INFO  *character_set_client;
  CHARSET_INFO  *character_set_results;

  /* Both charset and collation parts of these variables are important */
  CHARSET_INFO	*collation_server;
  CHARSET_INFO	*collation_database;
  CHARSET_INFO  *collation_connection;

  /* Locale Support */
  MY_LOCALE *lc_time_names;

  Time_zone *time_zone;

  /* DATE, DATETIME and MYSQL_TIME formats */
  DATE_TIME_FORMAT *date_format;
  DATE_TIME_FORMAT *datetime_format;
  DATE_TIME_FORMAT *time_format;
  my_bool sysdate_is_now;
};


/* per thread status variables */

typedef struct system_status_var
{
  ulonglong bytes_received;
  ulonglong bytes_sent;
  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];
  ulong created_tmp_disk_tables;
  ulong created_tmp_tables;
  ulong ha_commit_count;
  ulong ha_delete_count;
  ulong ha_read_first_count;
  ulong ha_read_last_count;
  ulong ha_read_key_count;
  ulong ha_read_next_count;
  ulong ha_read_prev_count;
  ulong ha_read_rnd_count;
  ulong ha_read_rnd_next_count;
  ulong ha_rollback_count;
  ulong ha_update_count;
  ulong ha_write_count;
  ulong ha_prepare_count;
  ulong ha_discover_count;
  ulong ha_savepoint_count;
  ulong ha_savepoint_rollback_count;

  /* KEY_CACHE parts. These are copies of the original */
  ulong key_blocks_changed;
  ulong key_blocks_used;
  ulong key_cache_r_requests;
  ulong key_cache_read;
  ulong key_cache_w_requests;
  ulong key_cache_write;
  /* END OF KEY_CACHE parts */

  ulong net_big_packet_count;
  ulong opened_tables;
  ulong opened_shares;
  ulong select_full_join_count;
  ulong select_full_range_join_count;
  ulong select_range_count;
  ulong select_range_check_count;
  ulong select_scan_count;
  ulong long_query_count;
  ulong filesort_merge_passes;
  ulong filesort_range_count;
  ulong filesort_rows;
  ulong filesort_scan_count;
  /* Prepared statements and binary protocol */
  ulong com_stmt_prepare;
  ulong com_stmt_reprepare;
  ulong com_stmt_execute;
  ulong com_stmt_send_long_data;
  ulong com_stmt_fetch;
  ulong com_stmt_reset;
  ulong com_stmt_close;
  /*
    Number of statements sent from the client
  */
  ulong questions;
  /*
    IMPORTANT!
    SEE last_system_status_var DEFINITION BELOW.
    Below 'last_system_status_var' are all variables which doesn't make any
    sense to add to the /global/ status variable counter.
    Status variables which it does not make sense to add to
    global status variable counter
  */
  double last_query_cost;
} STATUS_VAR;

/*
  This is used for 'SHOW STATUS'. It must be updated to the last ulong
  variable in system_status_var which is makes sens to add to the global
  counter
*/

#define last_system_status_var questions

void mark_transaction_to_rollback(THD *thd, bool all);

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
    Prepared statement: INITIALIZED -> PREPARED -> EXECUTED.
    Stored procedure:   INITIALIZED_FOR_SP -> EXECUTED.
    Other statements:   CONVENTIONAL_EXECUTION never changes.
  */
  enum enum_state
  {
    INITIALIZED= 0, INITIALIZED_FOR_SP= 1, PREPARED= 2,
    CONVENTIONAL_EXECUTION= 3, EXECUTED= 4, ERROR= -1
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

  inline bool is_stmt_prepare() const { return state == INITIALIZED; }
  inline bool is_first_sp_execute() const
  { return state == INITIALIZED_FOR_SP; }
  inline bool is_stmt_prepare_or_first_sp_execute() const
  { return (int)state < (int)PREPARED; }
  inline bool is_stmt_prepare_or_first_stmt_execute() const
  { return (int)state <= (int)PREPARED; }
  inline bool is_first_stmt_execute() const { return state == PREPARED; }
  inline bool is_stmt_execute() const
  { return state == PREPARED || state == EXECUTED; }
  inline bool is_conventional() const
  { return state == CONVENTIONAL_EXECUTION; }

  inline void* alloc(size_t size) { return alloc_root(mem_root,size); }
  inline void* calloc(size_t size)
  {
    void *ptr;
    if ((ptr=alloc_root(mem_root,size)))
      bzero(ptr, size);
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

class Statement: public ilink, public Query_arena
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
  LEX_STRING query_string;
  Server_side_cursor *cursor;

  inline char *query() { return query_string.str; }
  inline uint32 query_length() { return query_string.length; }
  void set_query_inner(char *query_arg, uint32 query_length_arg);

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
    stmt= (Statement*)hash_search(&names_hash, (uchar*)name->str,
                                  name->length);
    return stmt;
  }

  Statement *find(ulong id)
  {
    if (last_found_statement == 0 || id != last_found_statement->id)
    {
      Statement *stmt;
      stmt= (Statement *) hash_search(&st_hash, (uchar *) &id, sizeof(id));
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
  */
  void close_transient_cursors();
  void erase(Statement *statement);
  /* Erase all statements (calls Statement destructor) */
  void reset();
  ~Statement_map();
private:
  HASH st_hash;
  HASH names_hash;
  I_List<Statement> transient_cursor_list;
  Statement *last_found_statement;
};

struct st_savepoint {
  struct st_savepoint *prev;
  char                *name;
  uint                 length;
  Ha_trx_info         *ha_list;
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

extern pthread_mutex_t LOCK_xid_cache;
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
  char   *host, *user, *priv_user, *ip;
  /* The host privilege we are using */
  char   priv_host[MAX_HOSTNAME];
  /* points to host if host is available, otherwise points to ip */
  const char *host_or_ip;
  ulong master_access;                 /* Global privileges from mysql.user */
  ulong db_access;                     /* Privileges for current db */

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
  A registry for item tree transformations performed during
  query optimization. We register only those changes which require
  a rollback to re-execute a prepared statement or stored procedure
  yet another time.
*/

struct Item_change_record;
typedef I_List<Item_change_record> Item_change_list;


/**
  Type of prelocked mode.
  See comment for THD::prelocked_mode for complete description.
*/

enum prelocked_mode_type {NON_PRELOCKED= 0, PRELOCKED= 1,
                          PRELOCKED_UNDER_LOCK_TABLES= 2};


/**
  Class that holds information about tables which were opened and locked
  by the thread. It is also used to save/restore this information in
  push_open_tables_state()/pop_open_tables_state().
*/

class Open_tables_state
{
public:
  /**
    As part of class THD, this member is set during execution
    of a prepared statement. When it is set, it is used
    by the locking subsystem to report a change in table metadata.

    When Open_tables_state part of THD is reset to open
    a system or INFORMATION_SCHEMA table, the member is cleared
    to avoid spurious ER_NEED_REPREPARE errors -- system and
    INFORMATION_SCHEMA tables are not subject to metadata version
    tracking.
    @sa check_and_update_table_version()
  */
  Reprepare_observer *m_reprepare_observer;

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
  /**
    List of tables that were opened with HANDLER OPEN and are
    still in use by this thread.
  */
  TABLE *handler_tables;
  TABLE *derived_tables;
  /*
    During a MySQL session, one can lock tables in two modes: automatic
    or manual. In automatic mode all necessary tables are locked just before
    statement execution, and all acquired locks are stored in 'lock'
    member. Unlocking takes place automatically as well, when the
    statement ends.
    Manual mode comes into play when a user issues a 'LOCK TABLES'
    statement. In this mode the user can only use the locked tables.
    Trying to use any other tables will give an error. The locked tables are
    stored in 'locked_tables' member.  Manual locking is described in
    the 'LOCK_TABLES' chapter of the MySQL manual.
    See also lock_tables() for details.
  */
  MYSQL_LOCK *lock;
  /*
    Tables that were locked with explicit or implicit LOCK TABLES.
    (Implicit LOCK TABLES happens when we are prelocking tables for
     execution of statement which uses stored routines. See description
     THD::prelocked_mode for more info.)
  */
  MYSQL_LOCK *locked_tables;

  /*
    CREATE-SELECT keeps an extra lock for the table being
    created. This field is used to keep the extra lock available for
    lower level routines, which would otherwise miss that lock.
   */
  MYSQL_LOCK *extra_lock;

  /*
    prelocked_mode_type enum and prelocked_mode member are used for
    indicating whenever "prelocked mode" is on, and what type of
    "prelocked mode" is it.

    Prelocked mode is used for execution of queries which explicitly
    or implicitly (via views or triggers) use functions, thus may need
    some additional tables (mentioned in query table list) for their
    execution.

    First open_tables() call for such query will analyse all functions
    used by it and add all additional tables to table its list. It will
    also mark this query as requiring prelocking. After that lock_tables()
    will issue implicit LOCK TABLES for the whole table list and change
    thd::prelocked_mode to non-0. All queries called in functions invoked
    by the main query will use prelocked tables. Non-0 prelocked_mode
    will also surpress mentioned analysys in those queries thus saving
    cycles. Prelocked mode will be turned off once close_thread_tables()
    for the main query will be called.

    Note: Since not all "tables" present in table list are really locked
    thd::prelocked_mode does not imply thd::locked_tables.
  */
  prelocked_mode_type prelocked_mode;
  ulong	version;
  uint current_tablenr;

  enum enum_flags {
    BACKUPS_AVAIL = (1U << 0)     /* There are backups available */
  };

  /*
    Flags with information about the open tables state.
  */
  uint state_flags;

  /*
    This constructor serves for creation of Open_tables_state instances
    which are used as backup storage.
  */
  Open_tables_state() : state_flags(0U) { }

  Open_tables_state(ulong version_arg);

  void set_open_tables_state(Open_tables_state *state)
  {
    *this= *state;
  }

  void reset_open_tables_state()
  {
    open_tables= temporary_tables= handler_tables= derived_tables= 0;
    extra_lock= lock= locked_tables= 0;
    prelocked_mode= NON_PRELOCKED;
    state_flags= 0U;
    m_reprepare_observer= NULL;
  }
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
  ulonglong options;
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
  SYSTEM_THREAD_DELAYED_INSERT= 1,
  SYSTEM_THREAD_SLAVE_IO= 2,
  SYSTEM_THREAD_SLAVE_SQL= 4,
  SYSTEM_THREAD_NDBCLUSTER_BINLOG= 8,
  SYSTEM_THREAD_EVENT_SCHEDULER= 16,
  SYSTEM_THREAD_EVENT_WORKER= 32
};

inline char const *
show_system_thread(enum_thread_type thread)
{
#define RETURN_NAME_AS_STRING(NAME) case (NAME): return #NAME
  switch (thread) {
    static char buf[64];
    RETURN_NAME_AS_STRING(NON_SYSTEM_THREAD);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_DELAYED_INSERT);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_SLAVE_IO);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_SLAVE_SQL);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_NDBCLUSTER_BINLOG);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_EVENT_SCHEDULER);
    RETURN_NAME_AS_STRING(SYSTEM_THREAD_EVENT_WORKER);
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
    Handle an error condition.
    This method can be implemented by a subclass to achieve any of the
    following:
    - mask an error internally, prevent exposing it to the user,
    - mask an error and throw another one instead.
    When this method returns true, the error condition is considered
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

    @param sql_errno the error number
    @param level the error level
    @param thd the calling thread
    @return true if the error is handled
  */
  virtual bool handle_error(uint sql_errno,
                            const char *message,
                            MYSQL_ERROR::enum_warning_level level,
                            THD *thd) = 0;
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
  bool handle_error(uint sql_errno,
                    const char *message,
                    MYSQL_ERROR::enum_warning_level level,
                    THD *thd)
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
  Drop_table_error_handler(Internal_error_handler *err_handler)
    :m_err_handler(err_handler)
  { }

public:
  bool handle_error(uint sql_errno,
                    const char *message,
                    MYSQL_ERROR::enum_warning_level level,
                    THD *thd);

private:
  Internal_error_handler *m_err_handler;
};


/**
  Stores status of the currently executed statement.
  Cleared at the beginning of the statement, and then
  can hold either OK, ERROR, or EOF status.
  Can not be assigned twice per statement.
*/

class Diagnostics_area
{
public:
  enum enum_diagnostics_status
  {
    /** The area is cleared at start of a statement. */
    DA_EMPTY= 0,
    /** Set whenever one calls my_ok(). */
    DA_OK,
    /** Set whenever one calls my_eof(). */
    DA_EOF,
    /** Set whenever one calls my_error() or my_message(). */
    DA_ERROR,
    /** Set in case of a custom response, such as one from COM_STMT_PREPARE. */
    DA_DISABLED
  };
  /** True if status information is sent to the client. */
  bool is_sent;
  /** Set to make set_error_status after set_{ok,eof}_status possible. */
  bool can_overwrite_status;

  void set_ok_status(THD *thd, ha_rows affected_rows_arg,
                     ulonglong last_insert_id_arg,
                     const char *message);
  void set_eof_status(THD *thd);
  void set_error_status(THD *thd, uint sql_errno_arg, const char *message_arg);

  void disable_status();

  void reset_diagnostics_area();

  bool is_set() const { return m_status != DA_EMPTY; }
  bool is_error() const { return m_status == DA_ERROR; }
  bool is_eof() const { return m_status == DA_EOF; }
  bool is_ok() const { return m_status == DA_OK; }
  bool is_disabled() const { return m_status == DA_DISABLED; }
  enum_diagnostics_status status() const { return m_status; }

  const char *message() const
  { DBUG_ASSERT(m_status == DA_ERROR || m_status == DA_OK); return m_message; }

  uint sql_errno() const
  { DBUG_ASSERT(m_status == DA_ERROR); return m_sql_errno; }

  uint server_status() const
  {
    DBUG_ASSERT(m_status == DA_OK || m_status == DA_EOF);
    return m_server_status;
  }

  ha_rows affected_rows() const
  { DBUG_ASSERT(m_status == DA_OK); return m_affected_rows; }

  ulonglong last_insert_id() const
  { DBUG_ASSERT(m_status == DA_OK); return m_last_insert_id; }

  uint total_warn_count() const
  {
    DBUG_ASSERT(m_status == DA_OK || m_status == DA_EOF);
    return m_total_warn_count;
  }

  Diagnostics_area() { reset_diagnostics_area(); }

private:
  /** Message buffer. Can be used by OK or ERROR status. */
  char m_message[MYSQL_ERRMSG_SIZE];
  /**
    SQL error number. One of ER_ codes from share/errmsg.txt.
    Set by set_error_status.
  */
  uint m_sql_errno;

  /**
    Copied from thd->server_status when the diagnostics area is assigned.
    We need this member as some places in the code use the following pattern:
    thd->server_status|= ...
    my_eof(thd);
    thd->server_status&= ~...
    Assigned by OK, EOF or ERROR.
  */
  uint m_server_status;
  /**
    The number of rows affected by the last statement. This is
    semantically close to thd->row_count_func, but has a different
    life cycle. thd->row_count_func stores the value returned by
    function ROW_COUNT() and is cleared only by statements that
    update its value, such as INSERT, UPDATE, DELETE and few others.
    This member is cleared at the beginning of the next statement.

    We could possibly merge the two, but life cycle of thd->row_count_func
    can not be changed.
  */
  ha_rows    m_affected_rows;
  /**
    Similarly to the previous member, this is a replacement of
    thd->first_successful_insert_id_in_prev_stmt, which is used
    to implement LAST_INSERT_ID().
  */
  ulonglong   m_last_insert_id;
  /** The total number of warnings. */
  uint	     m_total_warn_count;
  enum_diagnostics_status m_status;
  /**
    @todo: the following THD members belong here:
    - warn_list, warn_count,
  */
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
  @class THD
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public Statement,
           public Open_tables_state
{
public:
  /* Used to execute base64 coded binlog events in MySQL server */
  Relay_log_info* rli_fake;

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
  NET	  net;				// client connection descriptor
  MEM_ROOT warn_root;			// For warnings and errors
  Protocol *protocol;			// Current protocol
  Protocol_text   protocol_text;	// Normal protocol
  Protocol_binary protocol_binary;	// Binary protocol
  HASH    user_vars;			// hash for user variables
  String  packet;			// dynamic buffer for network I/O
  String  convert_buffer;               // buffer for charset conversions
  struct  sockaddr_in remote;		// client socket address
  struct  rand_struct rand;		// used for authentication
  struct  system_variables variables;	// Changeable local variables
  struct  system_status_var status_var; // Per thread statistic vars
  struct  system_status_var *initial_status_var; /* used by show status */
  THR_LOCK_INFO lock_info;              // Locking info of this thread
  THR_LOCK_OWNER main_lock_id;          // To use for conventional queries
  THR_LOCK_OWNER *lock_id;              // If not main_lock_id, points to
                                        // the lock_id of a cursor.
  /**
    Protects THD data accessed from other threads:
    - thd->query and thd->query_length (used by SHOW ENGINE
      INNODB STATUS and SHOW PROCESSLIST
    - thd->mysys_var (used by KILL statement and shutdown).
    Is locked when THD is deleted.
  */
  pthread_mutex_t LOCK_thd_data;

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

  /*
    Used in error messages to tell user in what part of MySQL we found an
    error. E. g. when where= "having clause", if fix_fields() fails, user
    will know that the error was in having clause.
  */
  const char *where;

  double tmp_double_value;                    /* Used in set_var.cc */
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
  /*
    Type of current query: COM_STMT_PREPARE, COM_QUERY, etc. Set from
    first byte of the packet in do_command()
  */
  enum enum_server_command command;
  uint32     server_id;
  uint32     file_id;			// for LOAD DATA INFILE
  /* remote (peer) port */
  uint16 peer_port;
  time_t     start_time, user_time;
  // track down slow pthread_create
  ulonglong  prior_thr_create_utime, thr_create_utime;
  ulonglong  start_utime, utime_after_lock;
  
  thr_lock_type update_lock_default;
  Delayed_insert *di;

  /* <> 0 if we are inside of trigger or stored function. */
  uint in_sub_stmt;
  /* TRUE when the current top has SQL_LOG_BIN ON */
  bool sql_log_bin_toplevel;

  /* container for handler's private per-connection data */
  Ha_data ha_data[MAX_HA];

#ifndef MYSQL_CLIENT
  int binlog_setup_trx_data();

  /*
    Public interface to write RBR events to the binlog
  */
  void binlog_start_trans_and_stmt();
  void binlog_set_stmt_begin();
  int binlog_write_table_map(TABLE *table, bool is_transactional);
  int binlog_write_row(TABLE* table, bool is_transactional,
                       MY_BITMAP const* cols, size_t colcnt,
                       const uchar *buf);
  int binlog_delete_row(TABLE* table, bool is_transactional,
                        MY_BITMAP const* cols, size_t colcnt,
                        const uchar *buf);
  int binlog_update_row(TABLE* table, bool is_transactional,
                        MY_BITMAP const* cols, size_t colcnt,
                        const uchar *old_data, const uchar *new_data);

  void set_server_id(uint32 sid) { server_id = sid; }

  /*
    Member functions to handle pending event for row-level logging.
  */
  template <class RowsEventT> Rows_log_event*
    binlog_prepare_pending_rows_event(TABLE* table, uint32 serv_id,
                                      MY_BITMAP const* cols,
                                      size_t colcnt,
                                      size_t needed,
                                      bool is_transactional,
				      RowsEventT* hint);
  Rows_log_event* binlog_get_pending_rows_event() const;
  void            binlog_set_pending_rows_event(Rows_log_event* ev);
  int binlog_flush_pending_rows_event(bool stmt_end);
  int binlog_remove_pending_rows_event(bool clear_maps);

private:
  /*
    Number of outstanding table maps, i.e., table maps in the
    transaction cache.
  */
  uint binlog_table_maps;

  enum enum_binlog_flag {
    BINLOG_FLAG_UNSAFE_STMT_PRINTED,
    BINLOG_FLAG_COUNT
  };

  /**
     Flags with per-thread information regarding the status of the
     binary log.
   */
  uint32 binlog_flags;
public:
  uint get_binlog_table_maps() const {
    return binlog_table_maps;
  }
#endif /* MYSQL_CLIENT */

public:

  struct st_transactions {
    SAVEPOINT *savepoints;
    THD_TRANS all;			// Trans since BEGIN WORK
    THD_TRANS stmt;			// Trans for current statement
    bool on;                            // see ha_enable_transaction()
    XID_STATE xid_state;
    Rows_log_event *m_pending_rows_event;

    /*
       Tables changed in transaction (that must be invalidated in query cache).
       List contain only transactional tables, that not invalidated in query
       cache (instead of full list of changed in transaction tables).
    */
    CHANGED_TABLE_LIST* changed_tables;
    MEM_ROOT mem_root; // Transaction-life memory allocation pool
    void cleanup()
    {
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
#ifdef USING_TRANSACTIONS
      free_root(&mem_root,MYF(MY_KEEP_PREALLOC));
#endif
    }
    st_transactions()
    {
#ifdef USING_TRANSACTIONS
      bzero((char*)this, sizeof(*this));
      xid_state.xid.null();
      init_sql_alloc(&mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
#else
      xid_state.xa_state= XA_NOTR;
#endif
    }
  } transaction;
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
  ulonglong  options;           /* Bitmap of states */
  longlong   row_count_func;    /* For the ROW_COUNT() function */
  ha_rows    cuted_fields;

  /*
    number of rows we actually sent to the client, including "synthetic"
    rows in ROLLUP etc.
  */
  ha_rows    sent_row_count;

  /*
    number of rows we read, sent or not, including in create_sort_index()
  */
  ha_rows    examined_row_count;

  /*
    The set of those tables whose fields are referenced in all subqueries
    of the query.
    TODO: possibly this it is incorrect to have used tables in THD because
    with more than one subquery, it is not clear what does the field mean.
  */
  table_map  used_tables;
  USER_CONN *user_connect;
  CHARSET_INFO *db_charset;
  /*
    FIXME: this, and some other variables like 'count_cuted_fields'
    maybe should be statement/cursor local, that is, moved to Statement
    class. With current implementation warnings produced in each prepared
    statement/cursor settle here.
  */
  List	     <MYSQL_ERROR> warn_list;
  uint	     warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_END];
  uint	     total_warn_count;
  Diagnostics_area main_da;
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  PROFILING  profiling;
#endif

  /*
    Id of current query. Statement can be reused to execute several queries
    query_id is global in context of the whole MySQL server.
    ID is automatically generated from mutex-protected counter.
    It's used in handler code for various purposes: to check which columns
    from table are necessary for this select, to check if it's necessary to
    update auto-updatable fields (like auto_increment and timestamp).
  */
  query_id_t query_id, warn_id;
  ulong      col_access;

#ifdef ERROR_INJECT_SUPPORT
  ulong      error_inject_value;
#endif
  /* Statement id is thread-wide. This counter is used to generate ids */
  ulong      statement_id_counter;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  /*
    Row counter, mainly for errors and warnings. Not increased in
    create_sort_index(); may differ from examined_row_count.
  */
  ulong      row_count;
  pthread_t  real_id;                           /* For debugging */
  my_thread_id  thread_id;
  uint	     tmp_table, global_read_lock;
  uint	     server_status,open_options;
  enum enum_thread_type system_thread;
  uint       select_number;             //number of select (used for EXPLAIN)
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  enum_check_fields count_cuted_fields;

  DYNAMIC_ARRAY user_var_events;        /* For user variables replication */
  MEM_ROOT      *user_var_events_alloc; /* Allocate above array elements here */

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

  bool       slave_thread, one_shot_set;
  /* tells if current statement should binlog row-based(1) or stmt-based(0) */
  bool       current_stmt_binlog_row_based;
  bool	     locked, some_tables_deleted;
  bool       last_cuted_field;
  bool	     no_errors, password;
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
  bool	     query_start_used, rand_used, time_zone_used;
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
  bool       bootstrap, cleanup_done;
  
  /**  is set if some thread specific value(s) used in a statement. */
  bool       thread_specific_used;
  bool	     charset_is_system_charset, charset_is_collation_connection;
  bool       charset_is_character_set_filesystem;
  bool       enable_slow_log;   /* enable slow log for current statement */
  bool	     abort_on_warning;
  bool 	     got_warning;       /* Set on call to push_warning() */
  bool	     no_warnings_for_error; /* no warnings on call to my_error() */
  /* set during loop of derived table processing */
  bool       derived_tables_processing;
  my_bool    tablespace_op;	/* This is TRUE in DISCARD/IMPORT TABLESPACE */

  sp_rcontext *spcont;		// SP runtime context
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

#ifdef WITH_PARTITION_STORAGE_ENGINE
  partition_info *work_part_info;
#endif

#if defined(ENABLED_DEBUG_SYNC)
  /* Debug Sync facility. See debug_sync.cc. */
  struct st_debug_sync_control *debug_sync_control;
#endif /* defined(ENABLED_DEBUG_SYNC) */

  THD();
  ~THD();

  void init(void);
  /*
    Initialize memory roots necessary for query processing and (!)
    pre-allocate memory for it. We can't do that in THD constructor because
    there are use cases (acl_init, delayed inserts, watcher threads,
    killing mysqld) where it's vital to not allocate excessive and not used
    memory. Note, that we still don't return error from init_for_queries():
    if preallocation fails, we should notice that at the first call to
    alloc_root. 
  */
  void init_for_queries();
  void change_user(void);
  void cleanup(void);
  void cleanup_after_query();
  bool store_globals();
#ifdef SIGNAL_WITH_VIO_CLOSE
  inline void set_active_vio(Vio* vio)
  {
    pthread_mutex_lock(&LOCK_thd_data);
    active_vio = vio;
    pthread_mutex_unlock(&LOCK_thd_data);
  }
  inline void clear_active_vio()
  {
    pthread_mutex_lock(&LOCK_thd_data);
    active_vio = 0;
    pthread_mutex_unlock(&LOCK_thd_data);
  }
  void close_active_vio();
#endif
  void awake(THD::killed_state state_to_set);

#ifndef MYSQL_CLIENT
  enum enum_binlog_query_type {
    /*
      The query can be logged row-based or statement-based
    */
    ROW_QUERY_TYPE,
    
    /*
      The query has to be logged statement-based
    */
    STMT_QUERY_TYPE,
    
    /*
      The query represents a change to a table in the "mysql"
      database and is currently mapped to ROW_QUERY_TYPE.
    */
    MYSQL_QUERY_TYPE,
    QUERY_TYPE_COUNT
  };
  
  int binlog_query(enum_binlog_query_type qtype,
                   char const *query, ulong query_len,
                   bool is_trans, bool suppress_use,
                   int errcode);
#endif

  /*
    For enter_cond() / exit_cond() to work the mutex must be got before
    enter_cond(); this mutex is then released by exit_cond().
    Usage must be: lock mutex; enter_cond(); your code; exit_cond().
  */
  inline const char* enter_cond(pthread_cond_t *cond, pthread_mutex_t* mutex,
			  const char* msg)
  {
    const char* old_msg = proc_info;
    safe_mutex_assert_owner(mutex);
    mysys_var->current_mutex = mutex;
    mysys_var->current_cond = cond;
    proc_info = msg;
    return old_msg;
  }
  inline void exit_cond(const char* old_msg)
  {
    /*
      Putting the mutex unlock in exit_cond() ensures that
      mysys_var->current_mutex is always unlocked _before_ mysys_var->mutex is
      locked (if that would not be the case, you'll get a deadlock if someone
      does a THD::awake() on you).
    */
    pthread_mutex_unlock(mysys_var->current_mutex);
    pthread_mutex_lock(&mysys_var->mutex);
    mysys_var->current_mutex = 0;
    mysys_var->current_cond = 0;
    proc_info = old_msg;
    pthread_mutex_unlock(&mysys_var->mutex);
  }
  inline time_t query_start() { query_start_used=1; return start_time; }
  inline void set_time()
  {
    if (user_time)
    {
      start_time= user_time;
      start_utime= utime_after_lock= my_micro_time();
    }
    else
      start_utime= utime_after_lock= my_micro_time_and_time(&start_time);
  }
  inline void	set_current_time()    { start_time= my_time(MY_WME); }
  inline void	set_time(time_t t)
  {
    start_time= user_time= t;
    start_utime= utime_after_lock= my_micro_time();
  }
  void set_time_after_lock()  { utime_after_lock= my_micro_time(); }
  ulonglong current_utime()  { return my_micro_time(); }
  inline ulonglong found_rows(void)
  {
    return limit_found_rows;
  }
  inline bool active_transaction()
  {
#ifdef USING_TRANSACTIONS
    return server_status & SERVER_STATUS_IN_TRANS;
#else
    return 0;
#endif
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

  bool convert_string(LEX_STRING *to, CHARSET_INFO *to_cs,
		      const char *from, uint from_length,
		      CHARSET_INFO *from_cs);

  bool convert_string(String *s, CHARSET_INFO *from_cs, CHARSET_INFO *to_cs);

  void add_changed_table(TABLE *table);
  void add_changed_table(const char *key, long key_length);
  CHANGED_TABLE_LIST * changed_table_dup(const char *key, long key_length);
  int send_explain_fields(select_result *result);
#ifndef EMBEDDED_LIBRARY
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
    if (main_da.is_error())
      main_da.reset_diagnostics_area();
    is_slave_error= 0;
    DBUG_VOID_RETURN;
  }
  inline bool vio_ok() const { return net.vio != 0; }
#else
  void clear_error();
  inline bool vio_ok() const { return true; }
#endif
  /**
    Mark the current error as fatal. Warning: this does not
    set any error, it sets a property of the error, so must be
    followed or prefixed with my_error().
  */
  inline void fatal_error()
  {
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
  inline bool is_error() const { return main_da.is_error(); }
  inline CHARSET_INFO *charset() { return variables.character_set_client; }
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
      nocheck_register_item_tree_change(place, *place, mem_root);
    *place= new_value;
  }
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
    if (err)
    {
      if ((err == KILL_CONNECTION) && !shutdown_in_progress)
        err = KILL_QUERY;
      my_message(err, ER(err), MYF(0));
    }
  }
  /* return TRUE if we will abort query if we make a warning now */
  inline bool really_abort_on_warning()
  {
    return (abort_on_warning &&
            (!transaction.stmt.modified_non_trans_table ||
             (variables.sql_mode & MODE_STRICT_ALL_TABLES)));
  }
  void set_status_var_init();
  bool is_context_analysis_only()
    { return stmt_arena->is_stmt_prepare() || lex->view_prepare_mode; }
  void reset_n_backup_open_tables_state(Open_tables_state *backup);
  void restore_backup_open_tables_state(Open_tables_state *backup);
  void reset_sub_statement_state(Sub_statement_state *backup, uint new_state);
  void restore_sub_statement_state(Sub_statement_state *backup);
  void set_n_backup_active_arena(Query_arena *set, Query_arena *backup);
  void restore_active_arena(Query_arena *set, Query_arena *backup);

  inline void set_current_stmt_binlog_row_based_if_mixed()
  {
    /*
      If in a stored/function trigger, the caller should already have done the
      change. We test in_sub_stmt to prevent introducing bugs where people
      wouldn't ensure that, and would switch to row-based mode in the middle
      of executing a stored function/trigger (which is too late, see also
      reset_current_stmt_binlog_row_based()); this condition will make their
      tests fail and so force them to propagate the
      lex->binlog_row_based_if_mixed upwards to the caller.
    */
    if ((variables.binlog_format == BINLOG_FORMAT_MIXED) &&
        (in_sub_stmt == 0))
      current_stmt_binlog_row_based= TRUE;
  }
  inline void set_current_stmt_binlog_row_based()
  {
    current_stmt_binlog_row_based= TRUE;
  }
  inline void clear_current_stmt_binlog_row_based()
  {
    current_stmt_binlog_row_based= FALSE;
  }
  inline void reset_current_stmt_binlog_row_based()
  {
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

      Don't reset binlog format for NDB binlog injector thread.
    */
    DBUG_PRINT("debug",
               ("temporary_tables: %s, in_sub_stmt: %s, system_thread: %s",
                YESNO(temporary_tables), YESNO(in_sub_stmt),
                show_system_thread(system_thread)));
    if ((temporary_tables == NULL) && (in_sub_stmt == 0) &&
        (system_thread != SYSTEM_THREAD_NDBCLUSTER_BINLOG))
    {
      current_stmt_binlog_row_based= 
        test(variables.binlog_format == BINLOG_FORMAT_ROW);
    }
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
    /* Do not reallocate memory if current chunk is big enough. */
    if (db && new_db && db_length >= new_db_len)
      memcpy(db, new_db, new_db_len+1);
    else
    {
      x_free(db);
      db= new_db ? my_strndup(new_db, new_db_len, MYF(MY_WME)) : NULL;
    }
    db_length= db ? new_db_len : 0;
    return new_db && !db;
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

  /**
    Handle an error condition.
    @param sql_errno the error number
    @param level the error level
    @return true if the error is handled
  */
  virtual bool handle_error(uint sql_errno, const char *message,
                            MYSQL_ERROR::enum_warning_level level);

  /**
    Remove the error handler last pushed.
  */
  Internal_error_handler *pop_internal_handler();

  /** Overloaded to guard query/query_length fields */
  virtual void set_statement(Statement *stmt);

  /**
    Assign a new value to thd->query.
    Protected with LOCK_thd_data mutex.
  */
  void set_query(char *query_arg, uint32 query_length_arg);
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
};


/** A short cut for thd->main_da.set_ok_status(). */

inline void
my_ok(THD *thd, ha_rows affected_rows= 0, ulonglong id= 0,
        const char *message= NULL)
{
  thd->main_da.set_ok_status(thd, affected_rows, id, message);
}


/** A short cut for thd->main_da.set_eof_status(). */

inline void
my_eof(THD *thd)
{
  thd->main_da.set_eof_status(thd);
}

#define tmp_disable_binlog(A)       \
  {ulonglong tmp_disable_binlog__save_options= (A)->options; \
  (A)->options&= ~OPTION_BIN_LOG

#define reenable_binlog(A)   (A)->options= tmp_disable_binlog__save_options;}


/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class sql_exchange :public Sql_alloc
{
public:
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  bool dumpfile;
  ulong skip_lines;
  CHARSET_INFO *cs;
  sql_exchange(char *name,bool dumpfile_flag);
  bool escaped_given(void);
};

#include "log_event.h"

/*
  This is used to get result from a select
*/

class JOIN;

class select_result :public Sql_alloc {
protected:
  THD *thd;
  SELECT_LEX_UNIT *unit;
  uint nest_level;
public:
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
  virtual bool send_fields(List<Item> &list, uint flags)=0;
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
  virtual void abort() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup();
  void set_thd(THD *thd_arg) { thd= thd_arg; }
  /**
     The nest level, if supported. 
     @return
     -1 if nest level is undefined, otherwise a positive integer.
   */
  int get_nest_level() { return nest_level; }
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
  bool send_fields(List<Item> &fields, uint flag) { return FALSE; }
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
  bool send_fields(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const { return FALSE; }
  void abort();
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
  CHARSET_INFO *write_cs; // output charset
public:
  select_export(sql_exchange *ex) :select_to_file(ex) {}
  /**
     Creates a select_export to represent INTO OUTFILE <filename> with a
     defined level of subquery nesting.
   */
  select_export(sql_exchange *ex, uint nest_level_arg) :select_to_file(ex) 
  {
    nest_level= nest_level_arg;
  }
  ~select_export();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class select_dump :public select_to_file {
public:
  select_dump(sql_exchange *ex) :select_to_file(ex) {}
  /**
     Creates a select_export to represent INTO DUMPFILE <filename> with a
     defined level of subquery nesting.
   */  
  select_dump(sql_exchange *ex, uint nest_level_arg) : 
    select_to_file(ex) 
  {
    nest_level= nest_level_arg;
  }
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class select_insert :public select_result_interceptor {
 public:
  TABLE_LIST *table_list;
  TABLE *table;
  List<Item> *fields;
  ulonglong autoinc_value_of_last_inserted_row; // autogenerated or not
  COPY_INFO info;
  bool insert_into_view;
  select_insert(TABLE_LIST *table_list_par,
		TABLE *table_par, List<Item> *fields_par,
		List<Item> *update_fields, List<Item> *update_values,
		enum_duplicates duplic, bool ignore);
  ~select_insert();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  virtual int prepare2(void);
  bool send_data(List<Item> &items);
  virtual void store_values(List<Item> &values);
  virtual bool can_rollback_data() { return 0; }
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void abort();
  /* not implemented: select_insert is never re-used in prepared statements */
  void cleanup();
};


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
    :select_insert (NULL, NULL, &select_fields, 0, 0, duplic, ignore),
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
  void abort();
  virtual bool can_rollback_data() { return 1; }

  // Needed for access from local class MY_HOOKS in prepare(), since thd is proteted.
  const THD *get_thd(void) { return thd; }
  const HA_CREATE_INFO *get_create_info() { return create_info; };
  int prepare2(void) { return 0; }
};

#include <myisam.h>

/* 
  Param to create temporary tables when doing SELECT:s 
  NOTE
    This structure is copied using memcpy as a part of JOIN.
*/

class TMP_TABLE_PARAM :public Sql_alloc
{
private:
  /* Prevent use of these (not safe because of lists and copy_field) */
  TMP_TABLE_PARAM(const TMP_TABLE_PARAM &);
  void operator=(TMP_TABLE_PARAM &);

public:
  List<Item> copy_funcs;
  List<Item> save_copy_funcs;
  Copy_field *copy_field, *copy_field_end;
  Copy_field *save_copy_field, *save_copy_field_end;
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
  /* If >0 convert all blob fields to varchar(convert_blob_length) */
  uint  convert_blob_length; 
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

  TMP_TABLE_PARAM()
    :copy_field(0), group_parts(0),
     group_length(0), group_null_parts(0), convert_blob_length(0),
     schema_table(0), precomputed_group_by(0), force_copy_fields(0)
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
      save_copy_field= copy_field= 0;
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

  bool create_result_table(THD *thd, List<Item> *column_types,
                           bool is_distinct, ulonglong options,
                           const char *alias);
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
public:
  select_max_min_finder_subselect(Item_subselect *item_arg, bool mx)
    :select_subselect(item_arg), cache(0), fmax(mx)
  {}
  void cleanup();
  bool send_data(List<Item> &items);
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
 public:
  user_var_entry() {}                         /* Remove gcc warning */
  LEX_STRING name;
  char *value;
  ulong length;
  query_id_t update_query_id, used_query_id;
  Item_result type;
  bool unsigned_flag;

  double val_real(my_bool *null_value);
  longlong val_int(my_bool *null_value) const;
  String *val_str(my_bool *null_value, String *str, uint decimals);
  my_decimal *val_decimal(my_bool *null_value, my_decimal *result);
  DTCollation collation;
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
    register ulonglong max_elems_in_tree=
      (1 + max_in_memory_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+key_size));
    return (int) (sizeof(uint)*(1 + nkeys/max_elems_in_tree));
  }

  void reset();
  bool walk(tree_walk_action action, void *walk_action_arg);

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
  virtual void abort();
};


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
  virtual void abort();
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
  /**
     Creates a select_dumpvar to represent INTO <variable> with a defined 
     level of subquery nesting.
   */
  select_dumpvar(uint nest_level_arg)
  {
    var_list.empty();
    row_count= 0;
    nest_level= nest_level_arg;
  }
  ~select_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const;
  void cleanup();
};

/* Bits in sql_command_flags */

#define CF_CHANGES_DATA		1
#define CF_HAS_ROW_COUNT	2
#define CF_STATUS_COMMAND	4
#define CF_SHOW_TABLE_COMMAND	8
#define CF_WRITE_LOGS_COMMAND  16
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
#define CF_REEXECUTION_FRAGILE 32

/* Functions in sql_class.cc */

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var);

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var);
void mark_transaction_to_rollback(THD *thd, bool all);

#endif /* MYSQL_SERVER */
