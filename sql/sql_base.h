/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_BASE_INCLUDED
#define SQL_BASE_INCLUDED

#include "sql_class.h"                          /* enum_mark_columns */

class COPY_INFO;
class Item_ident;
struct Name_resolution_context;
class Open_table_context;
class Open_tables_state;
class Prelocking_strategy;
struct TABLE_LIST;
class THD;
struct handlerton;
struct TABLE;
class Table_trigger_dispatcher;

typedef class st_select_lex SELECT_LEX;

typedef struct st_lock_param_type ALTER_PARTITION_PARAM_TYPE;

#define TEMP_PREFIX	"MY"

	/* Defines for use with openfrm, openprt and openfrd */

#define READ_ALL		1	/* openfrm: Read all parameters */
#define EXTRA_RECORD		8	/* Reservera plats f|r extra record */
#define DELAYED_OPEN	        4096    /* Open table later */
#define OPEN_VIEW		8196    /* Allow open on view */
#define OPEN_VIEW_NO_PARSE     16384    /* Open frm only if it's a view,
                                           but do not parse view itself */
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to open FRM file only to get necessary data.
*/
#define OPEN_FRM_FILE_ONLY     32768
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to process tables only to get necessary data.
  Views are not processed.
*/
#define OPEN_TABLE_ONLY        OPEN_FRM_FILE_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to process views only to get necessary data.
  Tables are not processed.
*/
#define OPEN_VIEW_ONLY         OPEN_TABLE_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine.
  The flag means that we need to open a view.
*/
#define OPEN_VIEW_FULL         OPEN_VIEW_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine.
  The flag means that I_S table uses optimization algorithm.
*/
#define OPTIMIZE_I_S_TABLE     OPEN_VIEW_FULL*2
/**
  The flag means that we need to process trigger files only.
*/
#define OPEN_TRIGGER_ONLY      OPTIMIZE_I_S_TABLE*2
/**
  This flag is used to instruct tdc_open_view() to check metadata version.
*/
#define CHECK_METADATA_VERSION OPEN_TRIGGER_ONLY*2


/*
  This enumeration type is used only by the function find_item_in_list
  to return the info on how an item has been resolved against a list
  of possibly aliased items.
  The item can be resolved:
   - against an alias name of the list's element (RESOLVED_AGAINST_ALIAS)
   - against non-aliased field name of the list  (RESOLVED_WITH_NO_ALIAS)
   - against an aliased field name of the list   (RESOLVED_BEHIND_ALIAS)
   - ignoring the alias name in cases when SQL requires to ignore aliases
     (e.g. when the resolved field reference contains a table name or
     when the resolved item is an expression)   (RESOLVED_IGNORING_ALIAS)
*/
enum enum_resolution_type {
  NOT_RESOLVED=0,
  RESOLVED_IGNORING_ALIAS,
  RESOLVED_BEHIND_ALIAS,
  RESOLVED_WITH_NO_ALIAS,
  RESOLVED_AGAINST_ALIAS
};

enum find_item_error_report_type {REPORT_ALL_ERRORS, REPORT_EXCEPT_NOT_FOUND,
				  IGNORE_ERRORS, REPORT_EXCEPT_NON_UNIQUE,
                                  IGNORE_EXCEPT_NON_UNIQUE};

enum enum_tdc_remove_table_type {TDC_RT_REMOVE_ALL, TDC_RT_REMOVE_NOT_OWN,
                                 TDC_RT_REMOVE_UNUSED,
                                 TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE};

/* bits for last argument to remove_table_from_cache() */
#define RTFC_NO_FLAG                0x0000
#define RTFC_OWNED_BY_THD_FLAG      0x0001
#define RTFC_WAIT_OTHER_THREAD_FLAG 0x0002
#define RTFC_CHECK_KILLED_FLAG      0x0004

bool check_dup(const char *db, const char *name, TABLE_LIST *tables);
extern mysql_mutex_t LOCK_open;
bool table_cache_init(void);
void table_cache_free(void);
bool table_def_init(void);
void table_def_free(void);
void table_def_start_shutdown(void);
void assign_new_table_id(TABLE_SHARE *share);
uint cached_table_definitions(void);
size_t get_table_def_key(const TABLE_LIST *table_list, const char **key);
TABLE_SHARE *get_table_share(THD *thd, TABLE_LIST *table_list,
                             const char *key, size_t key_length,
                             uint db_flags, int *error,
                             my_hash_value_type hash_value);
void release_table_share(TABLE_SHARE *share);
TABLE_SHARE *get_cached_table_share(THD *thd, const char *db,
                                    const char *table_name);

TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type update,
                   uint lock_flags);

/* mysql_lock_tables() and open_table() flags bits */
#define MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK      0x0001
#define MYSQL_OPEN_IGNORE_FLUSH                 0x0002
/* MYSQL_OPEN_TEMPORARY_ONLY (0x0004) is not used anymore. */
#define MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY      0x0008
#define MYSQL_LOCK_LOG_TABLE                    0x0010
/**
  Do not try to acquire a metadata lock on the table: we
  already have one.
*/
#define MYSQL_OPEN_HAS_MDL_LOCK                 0x0020
/**
  If in locked tables mode, ignore the locked tables and get
  a new instance of the table.
*/
#define MYSQL_OPEN_GET_NEW_TABLE                0x0040
/* 0x0080 used to be MYSQL_OPEN_SKIP_TEMPORARY */
/** Fail instead of waiting when conficting metadata lock is discovered. */
#define MYSQL_OPEN_FAIL_ON_MDL_CONFLICT         0x0100
/** Open tables using MDL_SHARED lock instead of one specified in parser. */
#define MYSQL_OPEN_FORCE_SHARED_MDL             0x0200
/**
  Open tables using MDL_SHARED_HIGH_PRIO lock instead of one specified
  in parser.
*/
#define MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL   0x0400
/**
  When opening or locking the table, use the maximum timeout
  (LONG_TIMEOUT = 1 year) rather than the user-supplied timeout value.
*/
#define MYSQL_LOCK_IGNORE_TIMEOUT               0x0800
/**
  When acquiring "strong" (SNW, SNRW, X) metadata locks on tables to
  be open do not acquire global, tablespace-scope and schema-scope IX locks.
*/
#define MYSQL_OPEN_SKIP_SCOPED_MDL_LOCK         0x1000
/**
  When opening or locking a replication table through an internal
  operation rather than explicitly through an user thread.
*/
#define MYSQL_LOCK_RPL_INFO_TABLE               0x2000
/**
  Only check THD::killed if waits happen (e.g. wait on MDL, wait on
  table flush, wait on thr_lock.c locks) while opening and locking table.
*/
#define MYSQL_OPEN_IGNORE_KILLED                0x4000

/** Please refer to the internals manual. */
#define MYSQL_OPEN_REOPEN  (MYSQL_OPEN_IGNORE_FLUSH |\
                            MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |\
                            MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY |\
                            MYSQL_LOCK_IGNORE_TIMEOUT |\
                            MYSQL_OPEN_IGNORE_KILLED |\
                            MYSQL_OPEN_GET_NEW_TABLE |\
                            MYSQL_OPEN_HAS_MDL_LOCK)

bool open_table(THD *thd, TABLE_LIST *table_list, Open_table_context *ot_ctx);

bool get_key_map_from_key_list(key_map *map, TABLE *table,
                               List<String> *index_list);
TABLE *open_table_uncached(THD *thd, const char *path, const char *db,
			   const char *table_name,
                           bool add_to_temporary_tables_list,
                           bool open_in_engine);
TABLE *find_locked_table(TABLE *list, const char *db, const char *table_name);
thr_lock_type read_lock_type_for_table(THD *thd,
                                       Query_tables_list *prelocking_ctx,
                                       TABLE_LIST *table_list,
                                       bool routine_modifies_data);

my_bool mysql_rm_tmp_tables(void);
bool rm_temporary_table(handlerton *base, const char *path);
void close_tables_for_reopen(THD *thd, TABLE_LIST **tables,
                             const MDL_savepoint &start_of_statement_svp);
TABLE_LIST *find_table_in_list(TABLE_LIST *table,
                               TABLE_LIST *TABLE_LIST::*link,
                               const char *db_name,
                               const char *table_name);
TABLE *find_temporary_table(THD *thd, const char *db, const char *table_name);
TABLE *find_temporary_table(THD *thd, const TABLE_LIST *tl);
TABLE *find_temporary_table(THD *thd, const char *table_key,
                            size_t table_key_length);
void close_thread_tables(THD *thd);
bool fill_record_n_invoke_before_triggers(THD *thd, COPY_INFO *optype_info,
                                          List<Item> &fields,
                                          List<Item> &values,
                                          TABLE *table,
                                          enum enum_trigger_event_type event,
                                          int num_fields);
bool fill_record_n_invoke_before_triggers(THD *thd, Field **field,
                                          List<Item> &values,
                                          TABLE *table,
                                          enum enum_trigger_event_type event,
                                          int num_fields);
bool insert_fields(THD *thd, Name_resolution_context *context,
		   const char *db_name, const char *table_name,
                   List_iterator<Item> *it, bool any_privileges);
bool setup_fields(THD *thd, Ref_ptr_array ref_pointer_array,
                  List<Item> &item, ulong privilege,
                  List<Item> *sum_func_list,
                  bool allow_sum_func, bool column_update);
bool fill_record(THD *thd, TABLE *table, List<Item> &fields, List<Item> &values,
                 MY_BITMAP *bitmap, MY_BITMAP *insert_into_fields_bitmap);
bool fill_record(THD *thd, TABLE *table, Field **field, List<Item> &values,
                 MY_BITMAP *bitmap, MY_BITMAP *insert_into_fields_bitmap);

bool check_record(THD *thd, Field **ptr);

Field *
find_field_in_tables(THD *thd, Item_ident *item,
                     TABLE_LIST *first_table, TABLE_LIST *last_table,
                     Item **ref, find_item_error_report_type report_error,
                     ulong want_privilege, bool register_tree_change);
Field *
find_field_in_table_ref(THD *thd, TABLE_LIST *table_list,
                        const char *name, size_t length,
                        const char *item_name, const char *db_name,
                        const char *table_name, Item **ref,
                        ulong want_privilege, bool allow_rowid,
                        uint *cached_field_index_ptr,
                        bool register_tree_change, TABLE_LIST **actual_table);
Field *
find_field_in_table(THD *thd, TABLE *table, const char *name, size_t length,
                    bool allow_rowid, uint *cached_field_index_ptr);
Field *
find_field_in_table_sef(TABLE *table, const char *name);
Item ** find_item_in_list(Item *item, List<Item> &items, uint *counter,
                          find_item_error_report_type report_error,
                          enum_resolution_type *resolution);
bool setup_natural_join_row_types(THD *thd,
                                  List<TABLE_LIST> *from_clause,
                                  Name_resolution_context *context);
bool wait_while_table_is_used(THD *thd, TABLE *table,
                              enum ha_extra_function function);

void drop_open_table(THD *thd, TABLE *table, const char *db_name,
                     const char *table_name);
void update_non_unique_table_error(TABLE_LIST *update,
                                   const char *operation,
                                   TABLE_LIST *duplicate);
int setup_ftfuncs(SELECT_LEX* select);
bool init_ftfuncs(THD *thd, SELECT_LEX* select);
int run_before_dml_hook(THD *thd);
bool lock_table_names(THD *thd, TABLE_LIST *table_list,
                      TABLE_LIST *table_list_end, ulong lock_wait_timeout,
                      uint flags);
bool open_tables(THD *thd, TABLE_LIST **tables, uint *counter, uint flags,
                 Prelocking_strategy *prelocking_strategy);
/* open_and_lock_tables */
bool open_and_lock_tables(THD *thd, TABLE_LIST *tables, uint flags,
                          Prelocking_strategy *prelocking_strategy);
/* simple open_and_lock_tables for single table */
TABLE *open_n_lock_single_table(THD *thd, TABLE_LIST *table_l,
                                thr_lock_type lock_type, uint flags,
                                Prelocking_strategy *prelocking_strategy);
bool open_tables_for_query(THD *thd, TABLE_LIST *tables, uint flags);
bool lock_tables(THD *thd, TABLE_LIST *tables, uint counter, uint flags);
void free_io_cache(TABLE *entry);
void intern_close_table(TABLE *entry);
void close_thread_table(THD *thd, TABLE **table_ptr);
bool close_temporary_tables(THD *thd);
TABLE_LIST *unique_table(THD *thd, const TABLE_LIST *table,
                         TABLE_LIST *table_list, bool check_alias);
int drop_temporary_table(THD *thd, TABLE_LIST *table_list, bool *is_trans);
void close_temporary_table(THD *thd, TABLE *table, bool free_share,
                           bool delete_table);
void close_temporary(TABLE *table, bool free_share, bool delete_table);
bool rename_temporary_table(THD* thd, TABLE *table, const char *new_db,
			    const char *table_name);
bool open_temporary_tables(THD *thd, TABLE_LIST *tl_list);
bool open_temporary_table(THD *thd, TABLE_LIST *tl);
bool is_equal(const LEX_STRING *a, const LEX_STRING *b);

/* Functions to work with system tables. */
bool open_nontrans_system_tables_for_read(THD *thd, TABLE_LIST *table_list,
                                 Open_tables_backup *backup);
bool open_trans_system_tables_for_read(THD *thd, TABLE_LIST *table_list);
void close_nontrans_system_tables(THD *thd, Open_tables_backup *backup);
void close_trans_system_tables(THD *thd);
void close_mysql_tables(THD *thd);
TABLE *open_system_table_for_update(THD *thd, TABLE_LIST *one_table);
TABLE *open_log_table(THD *thd, TABLE_LIST *one_table, Open_tables_backup *backup);
void close_log_table(THD *thd, Open_tables_backup *backup);

TABLE *open_performance_schema_table(THD *thd, TABLE_LIST *one_table,
                                     Open_tables_state *backup);
void close_performance_schema_table(THD *thd, Open_tables_state *backup);

bool close_cached_tables(THD *thd, TABLE_LIST *tables,
                         bool wait_for_refresh, ulong timeout);
void close_all_tables_for_name(THD *thd, TABLE_SHARE *share,
                               bool remove_from_locked_tables,
                               TABLE *skip_table);
OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *db, const char *wild);
void tdc_remove_table(THD *thd, enum_tdc_remove_table_type remove_type,
                      const char *db, const char *table_name,
                      bool has_lock);
bool tdc_open_view(THD *thd, TABLE_LIST *table_list, const char *alias,
                   const char *cache_key, size_t cache_key_length, uint flags);
void tdc_flush_unused_tables();
TABLE *find_table_for_mdl_upgrade(THD *thd, const char *db,
                                  const char *table_name,
                                  bool no_error);
void mark_tmp_table_for_reuse(TABLE *table);
bool check_if_table_exists(THD *thd, TABLE_LIST *table, bool *exists);

extern Item **not_found_item;
extern Field *not_found_field;
extern Field *view_ref_found;
extern HASH table_def_cache;

/**
  clean/setup table fields and map.

  @param table        TABLE structure pointer (which should be setup)
  @param table_list   TABLE_LIST structure pointer (owner of TABLE)
  @param tableno      table number
*/

inline TABLE_LIST *find_table_in_global_list(TABLE_LIST *table,
                                             const char *db_name,
                                             const char *table_name)
{
  return find_table_in_list(table, &TABLE_LIST::next_global,
                            db_name, table_name);
}

inline TABLE_LIST *find_table_in_local_list(TABLE_LIST *table,
                                            const char *db_name,
                                            const char *table_name)
{
  return find_table_in_list(table, &TABLE_LIST::next_local,
                            db_name, table_name);
}


/**
  An abstract class for a strategy specifying how the prelocking
  algorithm should extend the prelocking set while processing
  already existing elements in the set.
*/

class Prelocking_strategy
{
public:
  virtual ~Prelocking_strategy() { }

  virtual bool handle_routine(THD *thd, Query_tables_list *prelocking_ctx,
                              Sroutine_hash_entry *rt, sp_head *sp,
                              bool *need_prelocking) = 0;
  virtual bool handle_table(THD *thd, Query_tables_list *prelocking_ctx,
                            TABLE_LIST *table_list, bool *need_prelocking) = 0;
  virtual bool handle_view(THD *thd, Query_tables_list *prelocking_ctx,
                           TABLE_LIST *table_list, bool *need_prelocking)= 0;
};


/**
  A Strategy for prelocking algorithm suitable for DML statements.

  Ensures that all tables used by all statement's SF/SP/triggers and
  required for foreign key checks are prelocked and SF/SPs used are
  cached.
*/

class DML_prelocking_strategy : public Prelocking_strategy
{
public:
  virtual bool handle_routine(THD *thd, Query_tables_list *prelocking_ctx,
                              Sroutine_hash_entry *rt, sp_head *sp,
                              bool *need_prelocking);
  virtual bool handle_table(THD *thd, Query_tables_list *prelocking_ctx,
                            TABLE_LIST *table_list, bool *need_prelocking);
  virtual bool handle_view(THD *thd, Query_tables_list *prelocking_ctx,
                           TABLE_LIST *table_list, bool *need_prelocking);
};


/**
  A strategy for prelocking algorithm to be used for LOCK TABLES
  statement.
*/

class Lock_tables_prelocking_strategy : public DML_prelocking_strategy
{
  virtual bool handle_table(THD *thd, Query_tables_list *prelocking_ctx,
                            TABLE_LIST *table_list, bool *need_prelocking);
};


/**
  Strategy for prelocking algorithm to be used for ALTER TABLE statements.

  Unlike DML or LOCK TABLES strategy, it doesn't
  prelock triggers, views or stored routines, since they are not
  used during ALTER.
*/

class Alter_table_prelocking_strategy : public Prelocking_strategy
{
public:
  virtual bool handle_routine(THD *thd, Query_tables_list *prelocking_ctx,
                              Sroutine_hash_entry *rt, sp_head *sp,
                              bool *need_prelocking);
  virtual bool handle_table(THD *thd, Query_tables_list *prelocking_ctx,
                            TABLE_LIST *table_list, bool *need_prelocking);
  virtual bool handle_view(THD *thd, Query_tables_list *prelocking_ctx,
                           TABLE_LIST *table_list, bool *need_prelocking);
};


inline bool
open_tables(THD *thd, TABLE_LIST **tables, uint *counter, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;

  return open_tables(thd, tables, counter, flags, &prelocking_strategy);
}


inline TABLE *open_n_lock_single_table(THD *thd, TABLE_LIST *table_l,
                                       thr_lock_type lock_type, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;

  return open_n_lock_single_table(thd, table_l, lock_type, flags,
                                  &prelocking_strategy);
}


// open_and_lock_tables with default prelocking strategy
inline bool open_and_lock_tables(THD *thd, TABLE_LIST *tables, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;

  return open_and_lock_tables(thd, tables, flags, &prelocking_strategy);
}


/**
  A context of open_tables() function, used to recover
  from a failed open_table() or open_routine() attempt.
*/

class Open_table_context
{
public:
  enum enum_open_table_action
  {
    OT_NO_ACTION= 0,
    OT_BACKOFF_AND_RETRY,
    OT_REOPEN_TABLES,
    OT_DISCOVER,
    OT_REPAIR
  };
  Open_table_context(THD *thd, uint flags);

  bool recover_from_failed_open();
  bool request_backoff_action(enum_open_table_action action_arg,
                              TABLE_LIST *table);

  bool can_recover_from_failed_open() const
  { return m_action != OT_NO_ACTION; }

  /**
    When doing a back-off, we close all tables acquired by this
    statement.  Return an MDL savepoint taken at the beginning of
    the statement, so that we can rollback to it before waiting on
    locks.
  */
  const MDL_savepoint &start_of_statement_svp() const
  {
    return m_start_of_statement_svp;
  }

  inline ulong get_timeout() const
  {
    return m_timeout;
  }

  uint get_flags() const { return m_flags; }

  /**
    Set flag indicating that we have already acquired metadata lock
    protecting this statement against GRL while opening tables.
  */
  void set_has_protection_against_grl()
  {
    m_has_protection_against_grl= TRUE;
  }

  bool has_protection_against_grl() const
  {
    return m_has_protection_against_grl;
  }

  bool can_back_off() const
  {
    return !m_has_locks;
  }

private:
  /* THD for which tables are opened. */
  THD *m_thd;
  /**
    For OT_DISCOVER and OT_REPAIR actions, the table list element for
    the table which definition should be re-discovered or which
    should be repaired.
  */
  TABLE_LIST *m_failed_table;
  MDL_savepoint m_start_of_statement_svp;
  /**
    Lock timeout in seconds. Initialized to LONG_TIMEOUT when opening system
    tables or to the "lock_wait_timeout" system variable for regular tables.
  */
  ulong m_timeout;
  /* open_table() flags. */
  uint m_flags;
  /** Back off action. */
  enum enum_open_table_action m_action;
  /**
    Whether we had any locks when this context was created.
    If we did, they are from the previous statement of a transaction,
    and we can't safely do back-off (and release them).
  */
  bool m_has_locks;
  /**
    Indicates that in the process of opening tables we have acquired
    protection against global read lock.
  */
  bool m_has_protection_against_grl;
};


/**
  Check if a TABLE_LIST instance represents a pre-opened temporary table.
*/

inline bool is_temporary_table(TABLE_LIST *tl)
{
  if (tl->is_view() || tl->schema_table)
    return FALSE;

  if (!tl->table)
    return FALSE;

  /*
    NOTE: 'table->s' might be NULL for specially constructed TABLE
    instances. See SHOW TRIGGERS for example.
  */

  if (!tl->table->s)
    return FALSE;

  return tl->table->s->tmp_table != NO_TMP_TABLE;
}

/**
  A simple holder for Internal_error_handler.
  The class utilizes RAII technique to not forget to pop the handler.

  @tparam Error_handler      Internal_error_handler to instantiate.
  @tparam Error_handler_arg  Type of the error handler ctor argument.
*/
template<typename Error_handler, typename Error_handler_arg>
class Internal_error_handler_holder
{
  THD *m_thd;
  bool m_activate;
  Error_handler m_error_handler;

public:
  Internal_error_handler_holder(THD *thd, bool activate,
                                Error_handler_arg *arg)
    : m_thd(thd), m_activate(activate), m_error_handler(arg)
  {
    if (activate)
      thd->push_internal_handler(&m_error_handler);
  }


  ~Internal_error_handler_holder()
  {
    if (m_activate)
      m_thd->pop_internal_handler();
  }
};

/**
   An Internal_error_handler that suppresses errors regarding views'
   underlying tables that occur during privilege checking. It hides errors which
   show view underlying table information.
   This happens in the cases when

   - A view's underlying table (e.g. referenced in its SELECT list) does not
     exist or columns of underlying table are altered. There should not be an
     error as no attempt was made to access it per se.

   - Access is denied for some table, column, function or stored procedure
     such as mentioned above. This error gets raised automatically, since we
     can't untangle its access checking from that of the view itself.

    There are currently two mechanisms at work that handle errors for views
    based on an Internal_error_handler. This one and another one is
    Show_create_error_handler. The latter handles errors encountered during
    execution of SHOW CREATE VIEW, while this mechanism using this method is
    handles SELECT from views. The two methods should not clash.

*/
class View_error_handler : public Internal_error_handler
{
  TABLE_LIST *m_top_view;

public:
  View_error_handler(TABLE_LIST *top_view) :
  m_top_view(top_view)
  {}
  virtual bool handle_condition(THD *thd, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *level,
                                const char *message);
};

/**
  This internal handler is used to trap ER_NO_SUCH_TABLE.
*/

class No_such_table_error_handler : public Internal_error_handler
{
public:
  No_such_table_error_handler()
    : m_handled_errors(0), m_unhandled_errors(0)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_NO_SUCH_TABLE)
    {
      m_handled_errors++;
      return true;
    }

    m_unhandled_errors++;
    return false;
  }

  /**
    Returns true if one or more ER_NO_SUCH_TABLE errors have been
    trapped and no other errors have been seen. false otherwise.
  */
  bool safely_trapped_errors() const
  {
    /*
      If m_unhandled_errors != 0, something else, unanticipated, happened,
      so the error is not trapped but returned to the caller.
      Multiple ER_NO_SUCH_TABLE can be raised in case of views.
    */
    return ((m_handled_errors > 0) && (m_unhandled_errors == 0));
  }

private:
  int m_handled_errors;
  int m_unhandled_errors;
};


/**
  This internal handler implements downgrade from SL_ERROR to SL_WARNING
  for statements which support IGNORE.
*/

class Ignore_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);
};

/**
  This internal handler implements upgrade from SL_WARNING to SL_ERROR
  for the error codes affected by STRICT mode. Currently STRICT mode does
  not affect SELECT statements.
*/

class Strict_error_handler : public Internal_error_handler
{
public:
  enum enum_set_select_behavior
  {
    DISABLE_SET_SELECT_STRICT_ERROR_HANDLER,
    ENABLE_SET_SELECT_STRICT_ERROR_HANDLER
  };

  Strict_error_handler()
    : m_set_select_behavior(DISABLE_SET_SELECT_STRICT_ERROR_HANDLER)
  {}

  Strict_error_handler(enum_set_select_behavior param)
    : m_set_select_behavior(param)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);

private:
  /*
    For SELECT and SET statement, we do not always give error in STRICT mode.
    For triggers, Strict_error_handler is pushed in the beginning of statement.
    If a SELECT or SET is executed from the Trigger, it should not always give
    error. We use this flag to choose when to give error and when warning.
  */
  enum_set_select_behavior m_set_select_behavior;
};

void update_indexed_column_map(TABLE *table, MY_BITMAP *read_set);

#endif /* SQL_BASE_INCLUDED */
