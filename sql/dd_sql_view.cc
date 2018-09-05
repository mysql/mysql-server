/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd_sql_view.h"

#include <string.h>
#include <sys/types.h>
#include <set>
#include <vector>

#include "lex_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysqld.h"  // mysqld_server_started
#include "mysqld_error.h"
#include "sql/auth/auth_common.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd.h"                       // dd::get_dictionary
#include "sql/dd/dd_view.h"                  // dd::update_view_status
#include "sql/dd/dictionary.h"               // is_dd_schema_name
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/schema.h"
#include "sql/dd/types/view.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"         // ER_THD
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/handler.h"        // HA_LEX_CREATE_TMP_TABLE
#include "sql/mdl.h"
#include "sql/set_var.h"
#include "sql/sp_head.h"    // sp_name
#include "sql/sql_alter.h"  // Alter_info
#include "sql/sql_base.h"   // open_tables
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"   // LEX
#include "sql/sql_view.h"  // mysql_register_view
#include "sql/system_variables.h"
#include "sql/table.h"  // TABLE_LIST
#include "sql/thd_raii.h"
#include "sql/transaction.h"
#include "thr_lock.h"

namespace dd {
class View_routine;
class View_table;
}  // namespace dd

/**
  RAII class to set the context for View_metadata_updater.
*/

class View_metadata_updater_context {
 public:
  View_metadata_updater_context(THD *thd) : m_thd(thd) {
    // Save sql mode and set sql_mode to 0 in view metadata update context.
    m_saved_sql_mode = thd->variables.sql_mode;
    m_thd->variables.sql_mode = 0;

    // Save current lex and create temporary lex object.
    m_saved_lex = m_thd->lex;
    m_thd->lex = new (m_thd->mem_root) st_lex_local;
    lex_start(m_thd);
    m_thd->lex->sql_command = SQLCOM_SHOW_FIELDS;

    // Backup open tables state.
    m_open_tables_state_backup.set_open_tables_state(m_thd);
    m_thd->reset_open_tables_state();
    m_thd->state_flags |= (Open_tables_state::BACKUPS_AVAIL);
  }

  ~View_metadata_updater_context() {
    // Close all the tables which are opened till now.
    close_thread_tables(m_thd);

    // Restore sql mode.
    m_thd->variables.sql_mode = m_saved_sql_mode;

    // Restore open tables state.
    m_thd->set_open_tables_state(&m_open_tables_state_backup);

    // Restore lex.
    m_thd->lex->unit->cleanup(true);
    lex_end(m_thd->lex);
    delete static_cast<st_lex_local *>(m_thd->lex);
    m_thd->lex = m_saved_lex;

    // While opening views, there is chance of hitting deadlock error. Returning
    // error in this case and resetting transaction_rollback_request here.
    m_thd->transaction_rollback_request = false;
  }

 private:
  // Thread handle
  THD *m_thd;

  // sql mode
  sql_mode_t m_saved_sql_mode;

  // LEX object.
  LEX *m_saved_lex;

  // Open_tables_backup
  Open_tables_backup m_open_tables_state_backup;
};

/**
  A error handler to convert all the errors except deadlock and lock wait
  timeout errors to ER_VIEW_INVALID while updating views metadata.

  Even a warning ER_NO_SUCH_USER generated for non-existing user is handled with
  the error handler.
*/

class View_metadata_updater_error_handler final
    : public Internal_error_handler {
 public:
  virtual bool handle_condition(THD *, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *,
                                const char *) {
    switch (sql_errno) {
      case ER_LOCK_WAIT_TIMEOUT:
      case ER_LOCK_DEADLOCK:
      case ER_STACK_OVERRUN_NEED_MORE:
        break;
      case ER_NO_SUCH_USER:
        m_sql_errno = ER_NO_SUCH_USER;
        break;
      default:
        m_sql_errno = ER_VIEW_INVALID;
        break;
    }
    return is_view_error_handled();
  }

  bool is_view_invalid() const { return m_sql_errno == ER_VIEW_INVALID; }

  bool is_view_error_handled() const {
    /*
      Other errors apart from ER_LOCK_DEADLOCK and ER_LOCK_WAIT_TIMEOUT are
      handled as ER_VIEW_INVALID. Warning ER_NO_SUCH_USER is generated but
      m_sql_errno is not set to ER_VIEW_INVALID.
    */
    return m_sql_errno == ER_NO_SUCH_USER || m_sql_errno == ER_VIEW_INVALID;
  }

 private:
  uint m_sql_errno = 0;
};

Uncommitted_tables_guard::~Uncommitted_tables_guard() {
  for (const TABLE_LIST *table : m_uncommitted_tables) {
    tdc_remove_table(m_thd, TDC_RT_REMOVE_ALL, table->get_db_name(),
                     table->get_table_name(), false);
  }
}

/**
  Prepare TABLE_LIST object for views referencing Base Table/ View/ Stored
  routine "db.tbl_or_sf_name".

  @tparam     T               Type of object (View_table/View_routine) to fetch
                              view names from.
  @param      thd             Current thread.
  @param      db              Database name.
  @param      tbl_or_sf_name  Base table/ View/ Stored function name.
  @param[out] views           TABLE_LIST objects for views.

  @retval     false           Success.
  @retval     true            Failure.

*/

template <typename T>
static bool prepare_view_tables_list(THD *thd, const char *db,
                                     const char *tbl_or_sf_name,
                                     std::vector<TABLE_LIST *> *views) {
  DBUG_ENTER("prepare_view_tables_list");
  std::vector<dd::Object_id> view_ids;
  std::set<dd::Object_id> prepared_view_ids;

  // Fetch all views using db.tbl_or_sf_name (Base table/ View/ Stored function)
  if (thd->dd_client()->fetch_referencing_views_object_id<T>(db, tbl_or_sf_name,
                                                             &view_ids))
    DBUG_RETURN(true);

  for (uint idx = 0; idx < view_ids.size(); idx++) {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    dd::String_type view_name;
    dd::String_type schema_name;
    // Get schema name and view name from the object id of the view.
    {
      dd::View *view = nullptr;
      // We need to use READ_UNCOMMITTED here as the view could be changed
      // by the same statement (e.g. RENAME TABLE).
      if (thd->dd_client()->acquire_uncached_uncommitted(view_ids.at(idx),
                                                         &view))
        DBUG_RETURN(true);
      if (!view) continue;

      dd::Schema *schema = nullptr;
      if (thd->dd_client()->acquire_uncached_uncommitted(view->schema_id(),
                                                         &schema))
        DBUG_RETURN(true);
      if (!schema) continue;
      view_name = view->name();
      schema_name = schema->name();
    }

    // If TABLE_LIST object is already prepared for view name then skip it.
    if (prepared_view_ids.find(view_ids.at(idx)) == prepared_view_ids.end()) {
      // Prepare TABLE_LIST object for the view and push_back
      TABLE_LIST *vw = new (thd->mem_root) TABLE_LIST;
      if (vw == nullptr) DBUG_RETURN(true);

      const char *db_name = strmake_root(thd->mem_root, schema_name.c_str(),
                                         schema_name.length());
      if (db_name == nullptr) DBUG_RETURN(true);

      const char *vw_name =
          strmake_root(thd->mem_root, view_name.c_str(), view_name.length());
      if (vw_name == nullptr) DBUG_RETURN(true);

      vw->init_one_table(db_name, schema_name.length(), vw_name,
                         view_name.length(), vw_name, TL_WRITE, MDL_EXCLUSIVE);

      views->push_back(vw);
      prepared_view_ids.insert(view_ids.at(idx));

      // Fetch all views using schema_name.view_name
      if (thd->dd_client()->fetch_referencing_views_object_id<dd::View_table>(
              schema_name.c_str(), view_name.c_str(), &view_ids))
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}

/**
  Helper method to mark all views state as invalid.

  If operation is drop operation then view referencing it becomes invalid.
  This method is called to mark state of all the referencing views as invalid
  in such case.

  @tparam         T                   Type of object (View_table/View_routine)
                                      to fetch view names from.
  @param          thd                 Current thread.
  @param          db                  Database name.
  @param          tbl_or_sf_name      Base table/ View/ Stored function name.
  @param          views_list          TABLE_LIST objects of the referencing
                                      views.
  @param          commit_dd_changes   Indicates whether changes to DD need
                                      to be committed.

  @retval     false           Success.
  @retval     true            Failure.

*/

template <typename T>
static bool mark_all_views_invalid(THD *thd, const char *db,
                                   const char *tbl_or_sf_name,
                                   std::vector<TABLE_LIST *> *views_list,
                                   bool commit_dd_changes) {
  DBUG_ENTER("mark_all_views_invalid");
  DBUG_ASSERT(!views_list->empty());

  // Acquire lock on all the views.
  MDL_request_list mdl_requests;
  for (auto view : *views_list) {
    MDL_request *schema_request = new (thd->mem_root) MDL_request;
    ;
    MDL_REQUEST_INIT(schema_request, MDL_key::SCHEMA, view->db, "",
                     MDL_INTENTION_EXCLUSIVE, MDL_STATEMENT);
    mdl_requests.push_front(schema_request);
    mdl_requests.push_front(&view->mdl_request);
  }
  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
    DBUG_RETURN(true);

  /*
    In the time gap of listing referencing views and acquiring MDL lock on them
    if any view definition is updated or dropped then it should not be
    considered for state update.
    Hence preparing updated list of view tables after acquiring the lock.
  */
  std::vector<TABLE_LIST *> updated_views_list;
  if (prepare_view_tables_list<T>(thd, db, tbl_or_sf_name, &updated_views_list))
    DBUG_RETURN(true);
  if (updated_views_list.empty()) DBUG_RETURN(false);

  // Update state of the views as invalid.
  for (auto view : *views_list) {
    // Update status of the view if it is listed in the updated_views_list.
    bool update_status = false;
    for (auto vw : updated_views_list) {
      if (!strcmp(view->get_db_name(), vw->get_db_name()) &&
          !strcmp(view->get_table_name(), vw->get_table_name())) {
        update_status = true;
        break;
      }
    }

    // Update Table.options.view_valid as false(invalid).
    if (update_status &&
        dd::update_view_status(thd, view->get_db_name(), view->get_table_name(),
                               false, commit_dd_changes))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

/**
  Helper method to
    - Mark view as invalid if DDL operation leaves the view in
      invalid state.
    - Open all the views from the TABLE_LIST vector and
      recreates the view metadata.

  @param          thd                 Thread handle.
  @param          views               TABLE_LIST objects of the views.
  @param          commit_dd_changes   Indicates whether changes to DD need
                                      to be committed.
  @param[in,out]  uncommitted_tables  Helper class to store list of views
                                      which shares need to be removed from
                                      TDC if we fail to commit changes to
                                      DD. Only used if commit_dd_changes
                                      is false.

  @retval     false            Success.
  @retval     true             Failure.
*/

static bool open_views_and_update_metadata(
    THD *thd, const std::vector<TABLE_LIST *> *views, bool commit_dd_changes,
    Uncommitted_tables_guard *uncommitted_tables) {
  DBUG_ENTER("open_views_and_update_metadata");

  if (!commit_dd_changes) {
    /*
      If we don't plan to commit changes to the data-dictionary in this
      function we need to keep locks on views to be updated until the
      statement end. Because of this we need to acquire them before
      View_metadata_updater_context takes effect.
    */
    for (auto view : *views) {
      MDL_request view_request, schema_request;

      MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, view->db, "",
                       MDL_INTENTION_EXCLUSIVE, MDL_STATEMENT);
      if (thd->mdl_context.acquire_lock(&schema_request,
                                        thd->variables.lock_wait_timeout))
        DBUG_RETURN(true);

      MDL_REQUEST_INIT_BY_KEY(&view_request, &view->mdl_request.key,
                              MDL_EXCLUSIVE, MDL_STATEMENT);
      if (thd->mdl_context.acquire_lock(&view_request,
                                        thd->variables.lock_wait_timeout))
        DBUG_RETURN(true);
    }
  }

  for (auto view : *views) {
    View_metadata_updater_context vw_metadata_update_context(thd);

    View_metadata_updater_error_handler error_handler;
    thd->push_internal_handler(&error_handler);

    DBUG_EXECUTE_IF("enable_stack_overrun_simulation",
                    { DBUG_SET("+d,simulate_stack_overrun"); });

    // This needs to be after View_metadata_updater_context so that
    // objects are released before metadata locks are dropped.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    /*
      Open view.

      Do not open tables which are not already in Table Cache in SE,
      as this might mean that, for example, this table is in the
      process of being ALTERed (by the thread which called our
      function), so its definition which we are going to use for
      opening is not committed/usable with SE.
    */
    uint counter = 0;
    DML_prelocking_strategy prelocking_strategy;
    view->select_lex = thd->lex->select_lex;
    if (open_tables(thd, &view, &counter, MYSQL_OPEN_NO_NEW_TABLE_IN_SE,
                    &prelocking_strategy)) {
      thd->pop_internal_handler();
      /*
        If error is handled by the error handler then update status of the view
        as "invalid." else report an error.

        During server startup, my_message_stderr is set to the
        error_handler_hook until all the server components and network are
        initialized. my_message_stderr does not invoke error handlers pushed.
        Even there will not be any concurrent operations at this stage to hit
        deadlock and lock wait timeout situations. So during server startup,
        view is marked as "invalid" in the error cases.
      */
      if (!mysqld_server_started || error_handler.is_view_invalid()) {
        if (view->mdl_request.ticket != NULL) {
          // Update view status in tables.options.view_valid.
          if (dd::update_view_status(thd, view->get_db_name(),
                                     view->get_table_name(), false,
                                     commit_dd_changes))
            DBUG_RETURN(true);
        }
      } else if (error_handler.is_view_error_handled() == false) {
        // ER_STACK_OVERRUN_NEED_MORE, ER_LOCK_DEADLOCK or
        // ER_LOCK_WAIT_TIMEOUT.
        DBUG_EXECUTE_IF("enable_stack_overrun_simulation",
                        { DBUG_SET("-d,simulate_stack_overrun"); });
        DBUG_RETURN(true);
      }
      continue;
    }
    if (view->is_view() == false) {
      // In between listing views and locking(opening), if view is dropped and
      // created as table then skip it.
      thd->pop_internal_handler();
      continue;
    }

    /* Prepare select to resolve all fields */
    LEX *view_lex = view->view_query();
    LEX *org_lex = thd->lex;
    thd->lex = view_lex;
    view_lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_VIEW;
    if (view_lex->unit->prepare(thd, 0, 0, 0)) {
      thd->lex = org_lex;
      thd->pop_internal_handler();
      // Please refer comments in the view open error handling block above.
      if (!mysqld_server_started || error_handler.is_view_invalid()) {
        // Update view status in tables.options.view_valid.
        if (dd::update_view_status(thd, view->get_db_name(),
                                   view->get_table_name(), false,
                                   commit_dd_changes))
          DBUG_RETURN(true);
      } else if (error_handler.is_view_error_handled() == false) {
        // ER_STACK_OVERRUN_NEED_MORE, ER_LOCK_DEADLOCK or
        // ER_LOCK_WAIT_TIMEOUT.
        DBUG_RETURN(true);
      }
      continue;
    }
    thd->pop_internal_handler();

    /*
      If we are not going commit changes immediately we need to ensure
      that entries for uncommitted views are removed from TDC on error/
      rollback. Add view which we about to update to the helper class
      for TDC invalidation.
    */
    if (!commit_dd_changes) uncommitted_tables->add_table(view);

    // Prepare view query from the Item-tree built from the original query.
    char view_query_buff[4096];
    String view_query(view_query_buff, sizeof(view_query_buff), thd->charset());
    view_query.length(0);

    if (thd->lex->unit->is_mergeable() &&
        view->algorithm != VIEW_ALGORITHM_TEMPTABLE) {
      for (ORDER *order = thd->lex->select_lex->order_list.first; order;
           order = order->next)
        order->used_alias = false;  /// @see Item::print_for_order()
    }
    Sql_mode_parse_guard parse_guard(thd);
    thd->lex->unit->print(&view_query, QT_TO_ARGUMENT_CHARSET);
    if (!thd->make_lex_string(&view->select_stmt, view_query.ptr(),
                              view_query.length(), false))
      DBUG_RETURN(true);

    // Update view metadata in the data-dictionary tables.
    view->updatable_view = is_updatable_view(thd, view);
    dd::View *new_view = nullptr;
    if (thd->dd_client()->acquire_for_modification(view->db, view->table_name,
                                                   &new_view))
      DBUG_RETURN(true);
    DBUG_ASSERT(new_view != nullptr);
    bool res = dd::update_view(thd, new_view, view);

    if (commit_dd_changes) {
      Disable_gtid_state_update_guard disabler(thd);
      if (res) {
        trans_rollback_stmt(thd);
        // Full rollback in case we have THD::transaction_rollback_request.
        trans_rollback(thd);
      } else
        res = trans_commit_stmt(thd) || trans_commit(thd);
    }
    if (res) {
      view_lex->unit->cleanup(true);
      lex_end(view_lex);
      thd->lex = org_lex;
      DBUG_RETURN(true);
    }
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, view->get_db_name(),
                     view->get_table_name(), false);

    view_lex->unit->cleanup(true);
    lex_end(view_lex);
    thd->lex = org_lex;
  }
  DEBUG_SYNC(thd, "after_updating_view_metadata");

  DBUG_RETURN(false);
}

/**
  Helper method to check if view metadata update is required for the DDL
  operation.

  @param   thd              Thread handle.
  @param   db               Database name.
  @param   name             Base table/ View/ Stored routine name.

  @retval  true   if view metadata update is required.
  @retval  false  if view metadata update is NOT required.
*/

static bool is_view_metadata_update_needed(THD *thd, const char *db,
                                           const char *name) {
  DBUG_ENTER("is_view_metadata_update_needed");

  // Update view metadata for only non temporary user tables.
  auto is_non_temp_user_table = [](THD *thd, const char *db, const char *name) {
    LEX_STRING lex_db = {const_cast<char *>(db), strlen(db)};
    LEX_STRING lex_name = {const_cast<char *>(name), strlen(name)};

    if (dd::get_dictionary()->is_dd_schema_name(db) ||
        get_table_category(lex_db, lex_name) != TABLE_CATEGORY_USER ||
        find_temporary_table(thd, db, name))
      return false;

    return true;
  };

  bool retval = false;
  switch (thd->lex->sql_command) {
    case SQLCOM_CREATE_TABLE:
      retval = is_non_temp_user_table(thd, db, name) &&
               !(thd->lex->create_info->options & HA_LEX_CREATE_TMP_TABLE);
      break;
    case SQLCOM_ALTER_TABLE: {
      DBUG_ASSERT(thd->lex->alter_info);

      // Alter operations which affects view column metadata.
      const uint alter_operations =
          (Alter_info::ALTER_ADD_COLUMN | Alter_info::ALTER_DROP_COLUMN |
           Alter_info::ALTER_CHANGE_COLUMN | Alter_info::ALTER_RENAME |
           Alter_info::ALTER_OPTIONS | Alter_info::ALTER_CHANGE_COLUMN_DEFAULT);
      retval = is_non_temp_user_table(thd, db, name) &&
               (thd->lex->alter_info->flags & alter_operations);
      break;
    }
    case SQLCOM_DROP_TABLE:
    case SQLCOM_RENAME_TABLE:
    case SQLCOM_CREATE_VIEW:
    case SQLCOM_DROP_VIEW:
    case SQLCOM_DROP_DB:
      retval = is_non_temp_user_table(thd, db, name);
      break;
    case SQLCOM_CREATE_SPFUNCTION:
    case SQLCOM_DROP_FUNCTION:
    case SQLCOM_INSTALL_PLUGIN:
    case SQLCOM_UNINSTALL_PLUGIN:
      retval = true;
      break;
    default:
      break;
  }

  DBUG_RETURN(retval);
}

/**
  Helper method to update referencing view's metadata.

  @tparam         T                   Type of object (View_table/View_routine)
                                      to fetch referencing view names.
  @param          thd                 Current thread.
  @param          db                  Database name.
  @param          tbl_or_sf_name      Base table/ View/ Stored function name.
  @param          commit_dd_changes   Indicates whether changes to DD need
                                      to be committed.
  @param[in,out]  uncommitted_tables  Helper class to store list of views
                                      which shares need to be removed from
                                      TDC if we fail to commit changes to
                                      DD. Only used if commit_dd_changes
                                      is false.

  @retval     false           Success.
  @retval     true            Failure.

*/

template <typename T>
static bool update_view_metadata(THD *thd, const char *db,
                                 const char *tbl_or_sf_name,
                                 bool commit_dd_changes,
                                 Uncommitted_tables_guard *uncommitted_tables) {
  if (is_view_metadata_update_needed(thd, db, tbl_or_sf_name)) {
    // Prepare list of all views referencing the db.table_name.
    std::vector<TABLE_LIST *> views;
    if (prepare_view_tables_list<T>(thd, db, tbl_or_sf_name, &views))
      return true;
    if (views.empty()) return false;

    DEBUG_SYNC(thd, "after_preparing_view_tables_list");

    bool is_drop_operation = (thd->lex->sql_command == SQLCOM_DROP_TABLE ||
                              thd->lex->sql_command == SQLCOM_DROP_VIEW ||
                              thd->lex->sql_command == SQLCOM_DROP_FUNCTION ||
                              thd->lex->sql_command == SQLCOM_DROP_DB ||
                              thd->lex->sql_command == SQLCOM_UNINSTALL_PLUGIN);

    // If operation is drop operation then view referencing it becomes invalid.
    // Hence mark all view as invalid.
    if (is_drop_operation)
      return mark_all_views_invalid<T>(thd, db, tbl_or_sf_name, &views,
                                       commit_dd_changes);

    /*
       Open views and update views metadata.

       Note that these updates will be done atomically with the main part of
       DDL statement only if main part DDL statement itself is atomic (i.e.
       storage engine involved supports atomic DDL).
       Otherwise, there is a possibility of things going out of sync in fatal
       error or crash scenarios. We will consider handling this case atomically
       as part of WL#9446.
    */
    if (open_views_and_update_metadata(thd, &views, commit_dd_changes,
                                       uncommitted_tables))
      return true;
  }

  return false;
}

static bool update_referencing_views_metadata(
    THD *thd, const char *db, const char *table_name, const char *new_db,
    const char *new_table_name, bool commit_dd_changes,
    Uncommitted_tables_guard *uncommitted_tables) {
  DBUG_ENTER("update_referencing_views_metadata");

  // Update metadata for view's referencing table.
  if (is_view_metadata_update_needed(thd, db, table_name)) {
    // Prepare list of all views referencing the table.
    if (update_view_metadata<dd::View_table>(
            thd, db, table_name, commit_dd_changes, uncommitted_tables))
      DBUG_RETURN(true);

    // Open views and update views metadata.
    if (new_db != nullptr && new_table_name != nullptr &&
        update_view_metadata<dd::View_table>(
            thd, new_db, new_table_name, commit_dd_changes, uncommitted_tables))
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

bool update_referencing_views_metadata(
    THD *thd, const TABLE_LIST *table, const char *new_db,
    const char *new_table_name, bool commit_dd_changes,
    Uncommitted_tables_guard *uncommitted_tables) {
  DBUG_ENTER("update_referencing_views_metadata");
  DBUG_ASSERT(table != nullptr);

  bool error = update_referencing_views_metadata(
      thd, table->get_db_name(), table->get_table_name(), new_db,
      new_table_name, commit_dd_changes, uncommitted_tables);
  DBUG_RETURN(error);
}

bool update_referencing_views_metadata(
    THD *thd, const TABLE_LIST *table, bool commit_dd_changes,
    Uncommitted_tables_guard *uncommitted_tables) {
  return update_referencing_views_metadata(
      thd, table, nullptr, nullptr, commit_dd_changes, uncommitted_tables);
}

bool update_referencing_views_metadata(
    THD *thd, const char *db_name, const char *table_name,
    bool commit_dd_changes, Uncommitted_tables_guard *uncommitted_tables) {
  DBUG_ENTER("update_referencing_views_metadata");
  DBUG_ASSERT(db_name && table_name);

  bool error = update_referencing_views_metadata(
      thd, db_name, table_name, nullptr, nullptr, commit_dd_changes,
      uncommitted_tables);
  DBUG_RETURN(error);
}

bool update_referencing_views_metadata(THD *thd, const sp_name *spname) {
  DBUG_ENTER("update_referencing_views_metadata");
  DBUG_ASSERT(spname);

  /*
    Updates to view metatdata for DDL on stored routines does not include
    any changes to non-atomic SE. Hence transaction is not committed in
    the update_view_metadata().
  */
  Uncommitted_tables_guard uncommitted_tables(thd);
  bool error = update_view_metadata<dd::View_routine>(
      thd, spname->m_db.str, spname->m_name.str, false, &uncommitted_tables);
  DBUG_RETURN(error);
}

void push_view_warning_or_error(THD *thd, const char *db,
                                const char *view_name) {
  // Report error for "SHOW FIELDS/DESCRIBE" operations.
  if (thd->lex->sql_command == SQLCOM_SHOW_FIELDS)
    my_error(ER_VIEW_INVALID, MYF(0), db, view_name);
  else
    // Push invalid view warning.
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_VIEW_INVALID,
                        ER_THD(thd, ER_VIEW_INVALID), db, view_name);
}
