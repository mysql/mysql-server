/* Copyright 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

#include "unireg.h"                    // REQUIRED: for other includes
#include "table.h"                              /* open_table_mode */
#include "sql_trigger.h"                        /* trg_event_type */
#include "sql_class.h"                          /* enum_mark_columns */
#include "mysqld.h"                             /* key_map */

class Item_ident;
struct Name_resolution_context;
class Open_table_context;
class Open_tables_state;
class Prelocking_strategy;
struct TABLE_LIST;
class THD;
struct handlerton;
struct TABLE;

typedef class st_select_lex SELECT_LEX;

typedef struct st_lock_param_type ALTER_PARTITION_PARAM_TYPE;

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

enum enum_open_table_action {OT_NO_ACTION= 0, OT_BACK_OFF_AND_RETRY,
                             OT_DISCOVER, OT_REPAIR};

enum find_item_error_report_type {REPORT_ALL_ERRORS, REPORT_EXCEPT_NOT_FOUND,
				  IGNORE_ERRORS, REPORT_EXCEPT_NON_UNIQUE,
                                  IGNORE_EXCEPT_NON_UNIQUE};

enum enum_tdc_remove_table_type {TDC_RT_REMOVE_ALL, TDC_RT_REMOVE_NOT_OWN,
                                 TDC_RT_REMOVE_UNUSED};

/* bits for last argument to remove_table_from_cache() */
#define RTFC_NO_FLAG                0x0000
#define RTFC_OWNED_BY_THD_FLAG      0x0001
#define RTFC_WAIT_OTHER_THREAD_FLAG 0x0002
#define RTFC_CHECK_KILLED_FLAG      0x0004

bool check_dup(const char *db, const char *name, TABLE_LIST *tables);
bool table_cache_init(void);
void table_cache_free(void);
bool table_def_init(void);
void table_def_free(void);
void table_def_start_shutdown(void);
void assign_new_table_id(TABLE_SHARE *share);
uint cached_open_tables(void);
uint cached_table_definitions(void);
uint create_table_def_key(THD *thd, char *key, TABLE_LIST *table_list,
                          bool tmp_table);
TABLE_SHARE *get_table_share(THD *thd, TABLE_LIST *table_list, char *key,
                             uint key_length, uint db_flags, int *error,
                             my_hash_value_type hash_value);
void release_table_share(TABLE_SHARE *share);
TABLE_SHARE *get_cached_table_share(const char *db, const char *table_name);

TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type update,
                   uint lock_flags);
bool open_table(THD *thd, TABLE_LIST *table_list, MEM_ROOT *mem_root,
                Open_table_context *ot_ctx, uint flags);
bool name_lock_locked_table(THD *thd, TABLE_LIST *tables);
bool reopen_name_locked_table(THD* thd, TABLE_LIST* table_list, bool link_in);
TABLE *table_cache_insert_placeholder(THD *thd, const char *key,
                                      uint key_length);
bool lock_table_name_if_not_cached(THD *thd, const char *db,
                                   const char *table_name, TABLE **table);
void detach_merge_children(TABLE *table, bool clear_refs);
bool fix_merge_after_open(TABLE_LIST *old_child_list, TABLE_LIST **old_last,
                          TABLE_LIST *new_child_list, TABLE_LIST **new_last);
bool reopen_table(TABLE *table);
bool reopen_tables(THD *thd,bool get_locks,bool in_refresh);
void close_data_files_and_morph_locks(THD *thd, const char *db,
                                      const char *table_name);
void close_handle_and_leave_table_as_lock(TABLE *table);
bool open_new_frm(THD *thd, TABLE_SHARE *share, const char *alias,
                  uint db_stat, uint prgflag,
                  uint ha_open_flags, TABLE *outparam, TABLE_LIST *table_desc,
                  MEM_ROOT *mem_root);
bool wait_for_tables(THD *thd);
bool table_is_used(TABLE *table, bool wait_for_name_lock);
TABLE *drop_locked_tables(THD *thd,const char *db, const char *table_name);
void abort_locked_tables(THD *thd,const char *db, const char *table_name);

bool get_key_map_from_key_list(key_map *map, TABLE *table,
                               List<String> *index_list);
TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list);
TABLE *find_locked_table(TABLE *list, const char *db, const char *table_name);
TABLE *find_write_locked_table(TABLE *list, const char *db,
                               const char *table_name);
thr_lock_type read_lock_type_for_table(THD *thd, TABLE *table);

my_bool mysql_rm_tmp_tables(void);
bool rm_temporary_table(handlerton *base, char *path);
void close_tables_for_reopen(THD *thd, TABLE_LIST **tables,
                             MDL_ticket *start_of_statement_svp);
TABLE_LIST *find_table_in_list(TABLE_LIST *table,
                               TABLE_LIST *TABLE_LIST::*link,
                               const char *db_name,
                               const char *table_name);
TABLE *find_temporary_table(THD *thd, const char *db, const char *table_name);
TABLE *find_temporary_table(THD *thd, TABLE_LIST *table_list);
void close_thread_tables(THD *thd);
bool fill_record_n_invoke_before_triggers(THD *thd, List<Item> &fields,
                                          List<Item> &values,
                                          bool ignore_errors,
                                          Table_triggers_list *triggers,
                                          enum trg_event_type event);
bool fill_record_n_invoke_before_triggers(THD *thd, Field **field,
                                          List<Item> &values,
                                          bool ignore_errors,
                                          Table_triggers_list *triggers,
                                          enum trg_event_type event);
bool insert_fields(THD *thd, Name_resolution_context *context,
		   const char *db_name, const char *table_name,
                   List_iterator<Item> *it, bool any_privileges);
int setup_wild(THD *thd, TABLE_LIST *tables, List<Item> &fields,
	       List<Item> *sum_func_list, uint wild_num);
bool setup_fields(THD *thd, Item** ref_pointer_array,
                  List<Item> &item, enum_mark_columns mark_used_columns,
                  List<Item> *sum_func_list, bool allow_sum_func);
bool fill_record(THD *thd, Field **field, List<Item> &values,
                 bool ignore_errors);

Field *
find_field_in_tables(THD *thd, Item_ident *item,
                     TABLE_LIST *first_table, TABLE_LIST *last_table,
                     Item **ref, find_item_error_report_type report_error,
                     bool check_privileges, bool register_tree_change);
Field *
find_field_in_table_ref(THD *thd, TABLE_LIST *table_list,
                        const char *name, uint length,
                        const char *item_name, const char *db_name,
                        const char *table_name, Item **ref,
                        bool check_privileges, bool allow_rowid,
                        uint *cached_field_index_ptr,
                        bool register_tree_change, TABLE_LIST **actual_table);
Field *
find_field_in_table(THD *thd, TABLE *table, const char *name, uint length,
                    bool allow_rowid, uint *cached_field_index_ptr);
Field *
find_field_in_table_sef(TABLE *table, const char *name);
Item ** find_item_in_list(Item *item, List<Item> &items, uint *counter,
                          find_item_error_report_type report_error,
                          enum_resolution_type *resolution);
bool setup_tables(THD *thd, Name_resolution_context *context,
                  List<TABLE_LIST> *from_clause, TABLE_LIST *tables,
                  TABLE_LIST **leaves, bool select_insert);
bool setup_tables_and_check_access(THD *thd,
                                   Name_resolution_context *context,
                                   List<TABLE_LIST> *from_clause,
                                   TABLE_LIST *tables,
                                   TABLE_LIST **leaves,
                                   bool select_insert,
                                   ulong want_access_first,
                                   ulong want_access);
bool wait_while_table_is_used(THD *thd, TABLE *table,
                              enum ha_extra_function function);
void unlink_open_table(THD *thd, TABLE *find, bool unlock);

void drop_open_table(THD *thd, TABLE *table, const char *db_name,
                     const char *table_name);
void close_all_tables_for_name(THD *thd, TABLE_SHARE *share,
                               bool remove_from_locked_tables);
void update_non_unique_table_error(TABLE_LIST *update,
                                   const char *operation,
                                   TABLE_LIST *duplicate);
int setup_conds(THD *thd, TABLE_LIST *tables, TABLE_LIST *leaves,
		COND **conds);
int setup_ftfuncs(SELECT_LEX* select);
int init_ftfuncs(THD *thd, SELECT_LEX* select, bool no_order);
void wait_for_condition(THD *thd, mysql_mutex_t *mutex,
                        mysql_cond_t *cond);
bool open_tables(THD *thd, TABLE_LIST **tables, uint *counter, uint flags,
                 Prelocking_strategy *prelocking_strategy);
/* open_and_lock_tables with optional derived handling */
bool open_and_lock_tables(THD *thd, TABLE_LIST *tables,
                          bool derived, uint flags,
                          Prelocking_strategy *prelocking_strategy);
int open_and_lock_tables_derived(THD *thd, TABLE_LIST *tables, bool derived);
/* simple open_and_lock_tables without derived handling for single table */
TABLE *open_n_lock_single_table(THD *thd, TABLE_LIST *table_l,
                                thr_lock_type lock_type, uint flags);
bool open_normal_and_derived_tables(THD *thd, TABLE_LIST *tables, uint flags);
bool lock_tables(THD *thd, TABLE_LIST *tables, uint counter, uint flags);
int abort_and_upgrade_lock(ALTER_PARTITION_PARAM_TYPE *lpt);
int decide_logging_format(THD *thd, TABLE_LIST *tables);
void free_io_cache(TABLE *entry);
void intern_close_table(TABLE *entry);
bool close_thread_table(THD *thd, TABLE **table_ptr);
void close_temporary_tables(THD *thd);
TABLE_LIST *unique_table(THD *thd, TABLE_LIST *table, TABLE_LIST *table_list,
                         bool check_alias);
int drop_temporary_table(THD *thd, TABLE_LIST *table_list);
void close_temporary_table(THD *thd, TABLE *table, bool free_share,
                           bool delete_table);
void close_temporary(TABLE *table, bool free_share, bool delete_table);
bool rename_temporary_table(THD* thd, TABLE *table, const char *new_db,
			    const char *table_name);
void mysql_wait_completed_table(ALTER_PARTITION_PARAM_TYPE *lpt, TABLE *my_table);
void remove_db_from_cache(const char *db);
void flush_tables();
bool is_equal(const LEX_STRING *a, const LEX_STRING *b);

/* Functions to work with system tables. */
bool open_system_tables_for_read(THD *thd, TABLE_LIST *table_list,
                                 Open_tables_backup *backup);
void close_system_tables(THD *thd, Open_tables_backup *backup);
TABLE *open_system_table_for_update(THD *thd, TABLE_LIST *one_table);
TABLE *open_log_table(THD *thd, TABLE_LIST *one_table, Open_tables_backup *backup);
void close_log_table(THD *thd, Open_tables_backup *backup);

TABLE *open_performance_schema_table(THD *thd, TABLE_LIST *one_table,
                                     Open_tables_state *backup);
void close_performance_schema_table(THD *thd, Open_tables_state *backup);

bool close_cached_tables(THD *thd, TABLE_LIST *tables, bool have_lock,
                         bool wait_for_refresh);
bool close_cached_connection_tables(THD *thd, bool wait_for_refresh,
                                    LEX_STRING *connect_string,
                                    bool have_lock = FALSE);
void close_all_tables_for_name(THD *thd, TABLE_SHARE *share,
                               bool remove_from_locked_tables);
OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *db, const char *wild);
bool remove_table_from_cache(THD *thd, const char *db, const char *table,
                             uint flags);
void tdc_remove_table(THD *thd, enum_tdc_remove_table_type remove_type,
                      const char *db, const char *table_name);
bool tdc_open_view(THD *thd, TABLE_LIST *table_list, const char *alias,
                   char *cache_key, uint cache_key_length,
                   MEM_ROOT *mem_root, uint flags);
TABLE *find_table_for_mdl_upgrade(TABLE *list, const char *db,
                                  const char *table_name,
                                  bool no_error);
void mark_tmp_table_for_reuse(TABLE *table);

extern uint  table_cache_count;
extern TABLE *unused_tables;
extern Item **not_found_item;
extern Field *not_found_field;
extern Field *view_ref_found;
extern HASH open_cache;
extern HASH table_def_cache;

/**
  clean/setup table fields and map.

  @param table        TABLE structure pointer (which should be setup)
  @param table_list   TABLE_LIST structure pointer (owner of TABLE)
  @param tablenr     table number
*/


inline void setup_table_map(TABLE *table, TABLE_LIST *table_list, uint tablenr)
{
  table->used_fields= 0;
  table->const_table= 0;
  table->null_row= 0;
  table->status= STATUS_NO_RECORD;
  table->maybe_null= table_list->outer_join;
  TABLE_LIST *embedding= table_list->embedding;
  while (!table->maybe_null && embedding)
  {
    table->maybe_null= embedding->outer_join;
    embedding= embedding->embedding;
  }
  table->tablenr= tablenr;
  table->map= (table_map) 1 << tablenr;
  table->force_index= table_list->force_index;
  table->force_index_order= table->force_index_group= 0;
  table->covering_keys= table->s->keys_for_keyread;
  table->merge_keys.clear_all();
}

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

inline bool setup_fields_with_no_wrap(THD *thd, Item **ref_pointer_array,
                                      List<Item> &item,
                                      enum_mark_columns mark_used_columns,
                                      List<Item> *sum_func_list,
                                      bool allow_sum_func)
{
  bool res;
  thd->lex->select_lex.no_wrap_view_item= TRUE;
  res= setup_fields(thd, ref_pointer_array, item, mark_used_columns,
                    sum_func_list, allow_sum_func);
  thd->lex->select_lex.no_wrap_view_item= FALSE;
  return res;
}

inline bool
open_tables(THD *thd, TABLE_LIST **tables, uint *counter, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;

  return open_tables(thd, tables, counter, flags, &prelocking_strategy);
}


/* open_and_lock_tables with derived handling */
inline bool open_and_lock_tables(THD *thd, TABLE_LIST *tables,
                                 bool derived, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;

  return open_and_lock_tables(thd, tables, derived, flags,
                              &prelocking_strategy);
}

#endif /* SQL_BASE_INCLUDED */
