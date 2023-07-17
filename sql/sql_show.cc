/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// SHOW TABLE, SHOW DATABASES, etc.

#include "sql/sql_show.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include "decimal.h"
#include "dig_vec.h"
#include "field_types.h"
#include "keycache.h"  // dflt_key_cache
#include "m_string.h"
#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_command.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_hostname.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_thread_local.h"
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/components/services/log_shared.h"
#include "mysql/my_loglevel.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin.h"  // st_mysql_plugin
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/int2str.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "nulls.h"
#include "scope_guard.h"         // Scope_guard
#include "sql/auth/auth_acls.h"  // DB_ACLS
#include "sql/auth/auth_common.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_schema.h"                // dd::Schema_MDL_locker
#include "sql/dd/dd_table.h"                 // is_encrypted
#include "sql/dd/properties.h"               // dd::Properties
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"               // dd::Column
#include "sql/dd/types/foreign_key.h"          // dd::Foreign_key
#include "sql/dd/types/foreign_key_element.h"  // dd::Foreign_key_element
#include "sql/dd/types/partition.h"
#include "sql/dd/types/partition_index.h"
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"  // dd::Table
#include "sql/debug_sync.h"      // DEBUG_SYNC
#include "sql/derror.h"          // ER_THD
#include "sql/enum_query_type.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/events.h"         // Events
#include "sql/field.h"          // Field
#include "sql/handler.h"
#include "sql/item.h"  // Item_empty_string
#include "sql/key.h"
#include "sql/log.h"  // query_logger
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"              // lower_case_table_names
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/opt_trace.h"           // fill_optimizer_trace_info
#include "sql/partition_info.h"      // partition_info
#include "sql/protocol.h"            // Protocol
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/rpl_source.h"
#include "sql/set_var.h"
#include "sql/sp.h"       // sp_cache_routine
#include "sql/sp_head.h"  // sp_head
#include "sql/sp_rcontext.h"
#include "sql/sql_base.h"  // close_thread_tables
#include "sql/sql_bitmap.h"
#include "sql/sql_check_constraint.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_db.h"  // get_default_db_collation
#include "sql/sql_error.h"
#include "sql/sql_executor.h"  // QEP_TAB
#include "sql/sql_gipk.h"      // table_has_generated_invisible_primary_key
#include "sql/sql_lex.h"       // LEX
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // command_name
#include "sql/sql_partition.h"  // HA_USE_AUTO_PARTITION
#include "sql/sql_plugin.h"     // PLUGIN_IS_DELETED, LOCK_plugin
#include "sql/sql_plugin_ref.h"
#include "sql/sql_profile.h"    // query_profile_statistics_info
#include "sql/sql_table.h"      // primary_key_name
#include "sql/sql_tmp_table.h"  // create_ondisk_from_heap
#include "sql/sql_trigger.h"    // acquire_shared_mdl_for_trigger
#include "sql/strfunc.h"        // lex_string_strmake
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/temp_table_param.h"          // Temp_table_param
#include "sql/thd_raii.h"                  // Prepared_stmt_arena_holder
#include "sql/trigger.h"                   // Trigger
#include "sql/tztime.h"                    // my_tz_SYSTEM
#include "sql_string.h"
#include "string_with_len.h"
#include "strmake.h"
#include "template_utils.h"
#include "thr_lock.h"

/* @see dynamic_privileges_table.cc */
bool iterate_all_dynamic_privileges(THD *thd,
                                    std::function<bool(const char *)> action);
using std::max;
using std::min;

/** Count number of times information_schema.processlist has been used. */
std::atomic_ulong deprecated_use_i_s_processlist_count = 0;

/** Last time information_schema.processlist was used, as usec since epoch. */
std::atomic_ullong deprecated_use_i_s_processlist_last_timestamp = 0;

/**
  @class CSET_STRING
  @brief Character set armed LEX_CSTRING
*/
class CSET_STRING {
 private:
  LEX_CSTRING string;
  const CHARSET_INFO *cs;

 public:
  CSET_STRING() : cs(&my_charset_bin) {
    string.str = nullptr;
    string.length = 0;
  }
  CSET_STRING(const char *str_arg, size_t length_arg,
              const CHARSET_INFO *cs_arg)
      : cs(cs_arg) {
    assert(cs_arg != nullptr);
    string.str = str_arg;
    string.length = length_arg;
  }

  inline const char *str() const { return string.str; }
  inline size_t length() const { return string.length; }
  const CHARSET_INFO *charset() const { return cs; }
};

static const char *grant_names[] = {
    "select",   "insert",  "update", "delete", "create",     "drop",  "reload",
    "shutdown", "process", "file",   "grant",  "references", "index", "alter"};

TYPELIB grant_types = {sizeof(grant_names) / sizeof(char **), "grant_types",
                       grant_names, nullptr};

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info);

static void get_cs_converted_string_value(THD *thd, String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs, bool use_hex);

static void append_algorithm(Table_ref *table, String *buff);

static void view_store_create_info(const THD *thd, Table_ref *table,
                                   String *buff);

bool Sql_cmd_show::check_privileges(THD *thd) {
  // If SHOW command is represented by a plan, ensure user has privileges:
  if (lex->query_tables == nullptr) return false;
  return check_table_access(thd, SELECT_ACL, lex->query_tables, false, UINT_MAX,
                            false);
}

bool Sql_cmd_show::execute(THD *thd) {
  lex = thd->lex;
  if (check_parameters(thd)) {
    return true;
  }
  return Sql_cmd_select::execute(thd);
}

bool Sql_cmd_show_schema_base::set_metadata_lock(THD *thd) {
  LEX_STRING lex_str_db;
  LEX *lex = thd->lex;
  if (lex_string_strmake(thd->mem_root, &lex_str_db, lex->query_block->db,
                         strlen(lex->query_block->db)))
    return true;

  // Acquire IX MDL lock on schema name.
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::SCHEMA, lex_str_db.str, "",
                   MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);
  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout))
    return true;
  return false;
}

bool Sql_cmd_show_schema_base::check_privileges(THD *thd) {
  Table_ref *const tables = thd->lex->query_tables;

  if (check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
    return true;

  const char *dst_db_name = thd->lex->query_block->db;
  assert(dst_db_name != nullptr);

  // Get user's global and db-level privileges.
  Access_bitmask global_db_privs;
  if (check_access(thd, SELECT_ACL, dst_db_name, &global_db_privs, nullptr,
                   false, false))
    return true;

  // Now check, if user has access on global level or to any of database/
  // table/column/routine.
  if (!(global_db_privs & DB_OP_ACLS) && check_grant_db(thd, dst_db_name)) {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             thd->security_context()->priv_user().str,
             thd->security_context()->priv_host().str, dst_db_name);
    return true;
  }
  if (set_metadata_lock(thd)) {
    return true;
  }
  return false;
}

bool Sql_cmd_show_schema_base::check_parameters(THD *thd) {
  // Check that given database exists.
  LEX_STRING lex_str_db;
  if (lex_string_strmake(thd->mem_root, &lex_str_db, lex->query_block->db,
                         strlen(lex->query_block->db)))
    return true;

  bool exists = false;
  if (dd::schema_exists(thd, lex_str_db.str, &exists)) return true;

  if (!exists) {
    my_error(ER_BAD_DB_ERROR, MYF(0), lex->query_block->db);
    return true;
  }
  return false;
}

bool Sql_cmd_show_table_base::check_privileges(THD *thd) {
  Table_ref *const table = thd->lex->query_tables;

  if (check_table_access(thd, SELECT_ACL, table, false, UINT_MAX, false))
    return true;

  Table_ref *dst_table = table->schema_query_block->get_table_list();
  assert(dst_table != nullptr);

  if (m_temporary) return false;

  if (check_access(thd, SELECT_ACL, dst_table->db, &dst_table->grant.privilege,
                   &dst_table->grant.m_internal, false, false))
    return true; /* Access denied */

  /*
    Check_grant will grant access if there is any column privileges on
    all of the tables thanks to the fourth parameter (bool show_table).
  */
  if (check_grant(thd, SELECT_ACL, dst_table, true, UINT_MAX, false))
    return true; /* Access denied */

  return false;
}

bool Sql_cmd_show_binlog_events::check_privileges(THD *thd) {
  return check_global_access(thd, REPL_SLAVE_ACL);
}

bool Sql_cmd_show_binlog_events::execute_inner(THD *thd) {
  return mysql_show_binlog_events(thd);
}

bool Sql_cmd_show_binlogs::check_privileges(THD *thd) {
  return check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL);
}

bool Sql_cmd_show_binlogs::execute_inner(THD *thd) { return show_binlogs(thd); }

bool Sql_cmd_show_create_database::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_database::execute_inner(THD *thd) {
  DBUG_EXECUTE_IF("4x_server_emul", my_error(ER_UNKNOWN_ERROR, MYF(0));
                  return true;);
  if (check_and_convert_db_name(&lex->name, true) != Ident_name_check::OK)
    return true;
  return mysqld_show_create_db(thd, lex->name.str, lex->create_info);
}

bool Sql_cmd_show_create_event::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_event::execute_inner(THD *thd) {
  return Events::show_create_event(thd, lex->spname->m_db,
                                   to_lex_cstring(lex->spname->m_name));
}

bool Sql_cmd_show_create_function::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_function::execute_inner(THD *thd) {
  return sp_show_create_routine(thd, enum_sp_type::FUNCTION, lex->spname);
}

bool Sql_cmd_show_create_procedure::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_procedure::execute_inner(THD *thd) {
  return sp_show_create_routine(thd, enum_sp_type::PROCEDURE, lex->spname);
}

bool Sql_cmd_show_create_table::check_privileges(THD *) {
  // Privilege check for this command is placed in execute_inner() function
  return false;
}

bool Sql_cmd_show_create_table::execute_inner(THD *thd) {
  // Prepare a local LEX object for expansion of table/view
  LEX *old_lex = thd->lex;
  LEX local_lex;

  const Pushed_lex_guard lex_guard(thd, &local_lex);

  LEX *lex = thd->lex;

  lex->only_view = m_is_view;
  lex->sql_command = old_lex->sql_command;

  // Disable constant subquery evaluation as we won't be locking tables.
  lex->context_analysis_only = CONTEXT_ANALYSIS_ONLY_VIEW;

  if (lex->query_block->add_table_to_list(thd, m_table_ident, nullptr, 0) ==
      nullptr)
    return true;
  Table_ref *tbl = lex->query_tables;

  /*
    Access check:
    SHOW CREATE TABLE require any privileges on the table level (ie
    effecting all columns in the table).
    SHOW CREATE VIEW require the SHOW_VIEW and SELECT ACLs on the table level.
    NOTE: SHOW_VIEW ACL is checked when the view is created.
  */
  DBUG_PRINT("debug", ("lex->only_view: %d, table: %s.%s", lex->only_view,
                       tbl->db, tbl->table_name));
  if (lex->only_view) {
    if (check_table_access(thd, SELECT_ACL, tbl, false, 1, false)) {
      DBUG_PRINT("debug", ("check_table_access failed"));
      my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), "SHOW",
               thd->security_context()->priv_user().str,
               thd->security_context()->host_or_ip().str, tbl->alias);
      return true;
    }
    DBUG_PRINT("debug", ("check_table_access succeeded"));

    // Ignore temporary tables if this is "SHOW CREATE VIEW"
    tbl->open_type = OT_BASE_ONLY;
  } else {
    /*
      Temporary tables should be opened for SHOW CREATE TABLE, but not
      for SHOW CREATE VIEW.
    */
    if (open_temporary_tables(thd, tbl)) return true;

    /*
      The fact that check_some_access() returned false does not mean that
      access is granted. We need to check if first_table->grant.privilege
      contains any table-specific privilege.
    */
    DBUG_PRINT("debug",
               ("tbl->grant.privilege: %" PRIx32, tbl->grant.privilege));
    if (check_some_access(thd, TABLE_OP_ACLS, tbl) ||
        (tbl->grant.privilege & TABLE_OP_ACLS) == 0) {
      my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), "SHOW",
               thd->security_context()->priv_user().str,
               thd->security_context()->host_or_ip().str, tbl->alias);
      return true;
    }
  }

  if (mysqld_show_create(thd, tbl)) return true;

  return false;
}

bool Sql_cmd_show_create_trigger::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_trigger::execute_inner(THD *thd) {
  if (lex->spname->m_name.length > NAME_LEN) {
    my_error(ER_TOO_LONG_IDENT, MYF(0), lex->spname->m_name.str);
    return true;
  }

  return show_create_trigger(thd, lex->spname);
}

bool Sql_cmd_show_create_user::check_privileges(THD *) { return false; }

bool Sql_cmd_show_create_user::execute_inner(THD *thd) {
  LEX_USER *show_user = get_current_user(thd, lex->grant_user);
  Security_context *sctx = thd->security_context();
  const bool are_both_users_same =
      !strcmp(sctx->priv_user().str, show_user->user.str) &&
      !my_strcasecmp(system_charset_info, show_user->host.str,
                     sctx->priv_host().str);
  if (are_both_users_same ||
      !check_access(thd, SELECT_ACL, "mysql", nullptr, nullptr, true, false))
    return mysql_show_create_user(thd, show_user, are_both_users_same);
  return false;
}

bool Sql_cmd_show_databases::check_privileges(THD *thd) {
  Table_ref *const table = thd->lex->query_tables;

  if (check_table_access(thd, SELECT_ACL, table, false, UINT_MAX, false))
    return true;

  return (specialflag & SPECIAL_SKIP_SHOW_DB) &&
         check_global_access(thd, SHOW_DB_ACL);
}

bool Sql_cmd_show_engine_logs::check_privileges(THD *thd) {
  return check_access(thd, FILE_ACL, any_db, nullptr, nullptr, false, false);
}

bool Sql_cmd_show_engine_logs::execute_inner(THD *thd) {
  return ha_show_status(thd, lex->create_info->db_type, HA_ENGINE_LOGS);
}

bool Sql_cmd_show_engine_mutex::check_privileges(THD *thd) {
  return check_global_access(thd, PROCESS_ACL);
}

bool Sql_cmd_show_engine_mutex::execute_inner(THD *thd) {
  return ha_show_status(thd, lex->create_info->db_type, HA_ENGINE_MUTEX);
}

bool Sql_cmd_show_engine_status::check_privileges(THD *thd) {
  return check_global_access(thd, PROCESS_ACL);
}

bool Sql_cmd_show_engine_status::execute_inner(THD *thd) {
  return ha_show_status(thd, lex->create_info->db_type, HA_ENGINE_STATUS);
}

bool Sql_cmd_show_events::check_privileges(THD *thd) {
  const char *db = thd->lex->query_block->db;
  assert(db != nullptr);
  /*
    Nobody has EVENT_ACL for I_S and P_S,
    even with a GRANT ALL to *.*,
    because these schemas have additional ACL restrictions:
    see ACL_internal_schema_registry.

    Yet there are no events in I_S and P_S to hide either,
    so this check voluntarily does not enforce ACL for
    SHOW EVENTS in I_S or P_S,
    to return an empty list instead of an access denied error.

    This is more user friendly, in particular for tools.

    EVENT_ACL is not fine grained enough to differentiate:
    - creating / updating / deleting events
    - viewing existing events
  */
  if (!is_infoschema_db(db) && !is_perfschema_db(db) &&
      check_access(thd, EVENT_ACL, db, nullptr, nullptr, false, false))
    return true;

  return Sql_cmd_show_schema_base::check_privileges(thd);
}

bool Sql_cmd_show_grants::check_privileges(THD *) {
  // Checked inside Sql_cmd_show_grants::execute_inner()
  return false;
}

bool Sql_cmd_show_grants::execute_inner(THD *thd) {
  DBUG_TRACE;
  const bool show_mandatory_roles = (for_user == nullptr);
  bool have_using_clause =
      (using_users != nullptr && using_users->elements > 0);

  if (for_user == nullptr || for_user->user.str == nullptr) {
    // SHOW PRIVILEGE FOR CURRENT_USER
    LEX_USER current_user;
    get_default_definer(thd, &current_user);
    if (!have_using_clause) {
      const List_of_auth_id_refs *active_list =
          thd->security_context()->get_active_roles();
      return mysql_show_grants(thd, &current_user, *active_list,
                               show_mandatory_roles, have_using_clause);
    }
  } else if (strcmp(thd->security_context()->priv_user().str,
                    for_user->user.str) != 0) {
    Table_ref table("mysql", "user", nullptr, TL_READ);
    if (!is_granted_table_access(thd, SELECT_ACL, &table)) {
      char command[128];
      get_privilege_desc(command, sizeof(command), SELECT_ACL);
      my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), command,
               thd->security_context()->priv_user().str,
               thd->security_context()->host_or_ip().str, "user");
      return false;
    }
  }
  List_of_auth_id_refs authid_list;
  if (have_using_clause) {
    for (const LEX_USER &user : *using_users) {
      authid_list.emplace_back(user.user, user.host);
    }
  }

  LEX_USER *tmp_user = const_cast<LEX_USER *>(for_user);
  tmp_user = get_current_user(thd, tmp_user);
  return mysql_show_grants(thd, tmp_user, authid_list, show_mandatory_roles,
                           have_using_clause);
}

bool Sql_cmd_show_binary_log_status::check_privileges(THD *thd) {
  return check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL);
}

bool Sql_cmd_show_binary_log_status::execute_inner(THD *thd) {
  return show_binary_log_status(thd);
}

bool Sql_cmd_show_profiles::execute_inner(THD *thd [[maybe_unused]]) {
#if defined(ENABLED_PROFILING)
  thd->profiling->discard_current_query();
  return thd->profiling->show_profiles();
#else
  my_error(ER_FEATURE_DISABLED, MYF(0), "SHOW PROFILES", "enable-profiling");
  return true;
#endif
}

bool Sql_cmd_show_privileges::execute_inner(THD *thd) {
  return mysqld_show_privileges(thd);
}

bool Sql_cmd_show_processlist::check_privileges(THD *thd) {
  if (!thd->security_context()->priv_user().str[0] &&
      check_global_access(thd, PROCESS_ACL))
    return true;
  return Sql_cmd_show::check_privileges(thd);
}

bool Sql_cmd_show_processlist::execute_inner(THD *thd) {
  /*
    If the Performance Schema is configured to support SHOW PROCESSLIST,
    then execute a query on performance_schema.processlist. Otherwise,
    fall back to the legacy method.
  */
  if (use_pfs()) {
    DEBUG_SYNC(thd, "pfs_show_processlist_performance_schema");
    return Sql_cmd_show::execute_inner(thd);
  } else {
    DEBUG_SYNC(thd, "pfs_show_processlist_legacy");
    mysqld_list_processes(thd,
                          thd->security_context()->check_access(PROCESS_ACL)
                              ? NullS
                              : thd->security_context()->priv_user().str,
                          m_verbose, true);
    return false;
  }
}

bool Sql_cmd_show_relaylog_events::check_privileges(THD *thd) {
  return check_global_access(thd, REPL_SLAVE_ACL);
}

bool Sql_cmd_show_relaylog_events::execute_inner(THD *thd) {
  return mysql_show_relaylog_events(thd);
}
bool Sql_cmd_show_routine_code::check_privileges(THD *) {
  // Actual privilege check is within sp_head::show_routine_code
  return false;
}

bool Sql_cmd_show_routine_code::execute_inner(THD *thd) {
#ifndef NDEBUG
  const enum_sp_type sp_type = m_sql_command == SQLCOM_SHOW_PROC_CODE
                                   ? enum_sp_type::PROCEDURE
                                   : enum_sp_type::FUNCTION;
  sp_head *sp;
  if (sp_cache_routine(thd, sp_type, m_routine_name, false, &sp)) {
    return true;
  }
  if (sp == nullptr || sp->show_routine_code(thd)) {
    // Don't distinguish between errors for now */
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
             sp_type == enum_sp_type::PROCEDURE ? "PROCEDURE" : "FUNCTION",
             m_routine_name->m_name.str);
    return true;
  }
  return false;
#else
  if (thd == nullptr) return false;  // To guide an overly eager compiler
  my_error(ER_FEATURE_DISABLED, MYF(0), "SHOW PROCEDURE|FUNCTION CODE",
           "--with-debug");
  return true;
#endif  // ifndef NDEBUG
}

bool Sql_cmd_show_replicas::check_privileges(THD *thd) {
  return check_global_access(thd, REPL_SLAVE_ACL);
}

bool Sql_cmd_show_replicas::execute_inner(THD *thd) {
  return show_replicas(thd);
}

bool Sql_cmd_show_replica_status::check_privileges(THD *thd) {
  return check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL);
}

bool Sql_cmd_show_replica_status::execute_inner(THD *thd) {
  return show_slave_status_cmd(thd);
}

/**
  Try acquire high priority share metadata lock on a table (with
  optional wait for conflicting locks to go away).

  @param thd            Thread context.
  @param table          Table list element for the table
  @param can_deadlock   Indicates that deadlocks are possible due to
                        metadata locks, so to avoid them we should not
                        wait in case if conflicting lock is present.

  @note This is an auxiliary function to be used in cases when we want to
        access table's description by looking up info in TABLE_SHARE without
        going through full-blown table open.
  @note This function assumes that there are no other metadata lock requests
        in the current metadata locking context.

  @retval false  No error, if lock was obtained
  Table_ref::mdl_request::ticket is set to non-NULL value.
  @retval true   Some error occurred (probably thread was killed).
*/

static bool try_acquire_high_prio_shared_mdl_lock(THD *thd, Table_ref *table,
                                                  bool can_deadlock) {
  bool error;
  MDL_REQUEST_INIT(&table->mdl_request, MDL_key::TABLE, table->db,
                   table->table_name, MDL_SHARED_HIGH_PRIO, MDL_TRANSACTION);

  if (can_deadlock) {
    /*
      When .FRM is being open in order to get data for an I_S table,
      we might have some tables not only open but also locked.
      E.g. this happens when a SHOW or I_S statement is run
      under LOCK TABLES or inside a stored function.
      By waiting for the conflicting metadata lock to go away we
      might create a deadlock which won't entirely belong to the
      MDL subsystem and thus won't be detectable by this subsystem's
      deadlock detector. To avoid such situation, when there are
      other locked tables, we prefer not to wait on a conflicting lock.
    */
    error = thd->mdl_context.try_acquire_lock(&table->mdl_request);
  } else {
    error = thd->mdl_context.acquire_lock(&table->mdl_request,
                                          thd->variables.lock_wait_timeout);
  }
  return error;
}

bool Sql_cmd_show_table_base::check_parameters(THD *thd) {
  // No MDL lock required for temporary tables
  if (m_temporary) return false;
  bool can_deadlock = thd->mdl_context.has_locks();
  Table_ref *table = thd->lex->query_tables;
  Table_ref *dst_table = table->schema_query_block->get_table_list();
  if (try_acquire_high_prio_shared_mdl_lock(thd, dst_table, can_deadlock)) {
    /*
      Some error occurred (most probably we have been killed while
      waiting for conflicting locks to go away), let the caller to
      handle the situation.
    */
    return true;
  }

  if (dst_table->mdl_request.ticket == nullptr) {
    /*
      We are in situation when we have encountered conflicting metadata
      lock and deadlocks can occur due to waiting for it to go away.
      So instead of waiting skip this table with an appropriate warning.
    */
    assert(can_deadlock);
    my_error(ER_WARN_I_S_SKIPPED_TABLE, MYF(0), dst_table->db,
             dst_table->table_name);
    return true;
  }

  // Stop if given database does not exist.
  dd::Schema_MDL_locker mdl_handler(thd);
  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *schema = nullptr;
  if (mdl_handler.ensure_locked(dst_table->db) ||
      thd->dd_client()->acquire(dst_table->db, &schema))
    return true;

  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), dst_table->db);
    return true;
  }

  const dd::Abstract_table *at = nullptr;
  if (thd->dd_client()->acquire(dst_table->db, dst_table->table_name, &at))
    return true;
  if (at == nullptr) {
    my_error(ER_NO_SUCH_TABLE, MYF(0), dst_table->db, dst_table->table_name);
    return true;
  }
  return false;
}

bool Sql_cmd_show_status::execute(THD *thd) {
  System_status_var old_status_var = thd->status_var;
  thd->initial_status_var = &old_status_var;

  const bool status = Sql_cmd_show::execute(thd);

  // Don't log SHOW STATUS commands to slow query log
  thd->server_status &=
      ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  // Restore status variables, as we don't want 'show status' to cause changes
  mysql_mutex_lock(&LOCK_status);
  add_diff_to_status(&global_status_var, &thd->status_var, &old_status_var);
  thd->status_var = old_status_var;
  thd->initial_status_var = nullptr;
  mysql_mutex_unlock(&LOCK_status);

  return status;
}

/***************************************************************************
** List all table types supported
***************************************************************************/

static size_t make_version_string(char *buf, size_t buf_length, uint version) {
  return snprintf(buf, buf_length, "%d.%d", version >> 8, version & 0xff);
}

static bool show_plugins(THD *thd, plugin_ref plugin, void *arg) {
  TABLE *table = (TABLE *)arg;
  struct st_mysql_plugin *plug = plugin_decl(plugin);
  struct st_plugin_dl *plugin_dl = plugin_dlib(plugin);
  CHARSET_INFO *cs = system_charset_info;
  char version_buf[20];

  restore_record(table, s->default_values);

  DBUG_EXECUTE_IF("set_uninstall_sync_point", {
    if (strcmp(plugin_name(plugin)->str, "EXAMPLE") == 0)
      DEBUG_SYNC(thd, "before_store_plugin_name");
  });

  mysql_mutex_lock(&LOCK_plugin);
  if (plugin == nullptr || plugin_state(plugin) == PLUGIN_IS_FREED) {
    mysql_mutex_unlock(&LOCK_plugin);
    return false;
  }

  table->field[0]->store(plugin_name(plugin)->str, plugin_name(plugin)->length,
                         cs);

  table->field[1]->store(
      version_buf,
      make_version_string(version_buf, sizeof(version_buf), plug->version), cs);

  switch (plugin_state(plugin)) {
    /* case PLUGIN_IS_FREED: does not happen */
    case PLUGIN_IS_DELETED:
      table->field[2]->store(STRING_WITH_LEN("DELETED"), cs);
      break;
    case PLUGIN_IS_UNINITIALIZED:
    case PLUGIN_IS_WAITING_FOR_UPGRADE:
      table->field[2]->store(STRING_WITH_LEN("INACTIVE"), cs);
      break;
    case PLUGIN_IS_READY:
      table->field[2]->store(STRING_WITH_LEN("ACTIVE"), cs);
      break;
    case PLUGIN_IS_DYING:
      table->field[2]->store(STRING_WITH_LEN("DELETING"), cs);
      break;
    case PLUGIN_IS_DISABLED:
      table->field[2]->store(STRING_WITH_LEN("DISABLED"), cs);
      break;
    default:
      assert(0);
  }

  table->field[3]->store(plugin_type_names[plug->type].str,
                         plugin_type_names[plug->type].length, cs);
  table->field[4]->store(version_buf,
                         make_version_string(version_buf, sizeof(version_buf),
                                             *(uint *)plug->info),
                         cs);

  if (plugin_dl) {
    table->field[5]->store(plugin_dl->dl.str, plugin_dl->dl.length, cs);
    table->field[5]->set_notnull();
    table->field[6]->store(version_buf,
                           make_version_string(version_buf, sizeof(version_buf),
                                               plugin_dl->version),
                           cs);
    table->field[6]->set_notnull();
  } else {
    table->field[5]->set_null();
    table->field[6]->set_null();
  }

  if (plug->author) {
    table->field[7]->store(plug->author, strlen(plug->author), cs);
    table->field[7]->set_notnull();
  } else
    table->field[7]->set_null();

  if (plug->descr) {
    table->field[8]->store(plug->descr, strlen(plug->descr), cs);
    table->field[8]->set_notnull();
  } else
    table->field[8]->set_null();

  switch (plug->license) {
    case PLUGIN_LICENSE_GPL:
      table->field[9]->store(PLUGIN_LICENSE_GPL_STRING,
                             strlen(PLUGIN_LICENSE_GPL_STRING), cs);
      break;
    case PLUGIN_LICENSE_BSD:
      table->field[9]->store(PLUGIN_LICENSE_BSD_STRING,
                             strlen(PLUGIN_LICENSE_BSD_STRING), cs);
      break;
    default:
      table->field[9]->store(PLUGIN_LICENSE_PROPRIETARY_STRING,
                             strlen(PLUGIN_LICENSE_PROPRIETARY_STRING), cs);
      break;
  }
  table->field[9]->set_notnull();

  table->field[10]->store(
      global_plugin_typelib_names[plugin_load_option(plugin)],
      strlen(global_plugin_typelib_names[plugin_load_option(plugin)]), cs);
  mysql_mutex_unlock(&LOCK_plugin);

  return schema_table_store_record(thd, table);
}

static int fill_plugins(THD *thd, Table_ref *tables, Item *) {
  DBUG_TRACE;

  if (plugin_foreach_with_mask(thd, show_plugins, MYSQL_ANY_PLUGIN,
                               ~PLUGIN_IS_FREED, tables->table))
    return 1;

  return 0;
}

/***************************************************************************
 List all privileges supported
***************************************************************************/
struct show_privileges_st {
  const char *privilege;
  const char *context;
  const char *comment;
};

static struct show_privileges_st sys_privileges[] = {
    {"Alter", "Tables", "To alter the table"},
    {"Alter routine", "Functions,Procedures",
     "To alter or drop stored functions/procedures"},
    {"Create", "Databases,Tables,Indexes",
     "To create new databases and tables"},
    {"Create routine", "Databases", "To use CREATE FUNCTION/PROCEDURE"},
    {"Create role", "Server Admin", "To create new roles"},
    {"Create temporary tables", "Databases", "To use CREATE TEMPORARY TABLE"},
    {"Create view", "Tables", "To create new views"},
    {"Create user", "Server Admin", "To create new users"},
    {"Delete", "Tables", "To delete existing rows"},
    {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
    {"Drop role", "Server Admin", "To drop roles"},
    {"Event", "Server Admin", "To create, alter, drop and execute events"},
    {"Execute", "Functions,Procedures", "To execute stored routines"},
    {"File", "File access on server", "To read and write files on the server"},
    {"Grant option", "Databases,Tables,Functions,Procedures",
     "To give to other users those privileges you possess"},
    {"Index", "Tables", "To create or drop indexes"},
    {"Insert", "Tables", "To insert data into tables"},
    {"Lock tables", "Databases",
     "To use LOCK TABLES (together with SELECT privilege)"},
    {"Process", "Server Admin",
     "To view the plain text of currently executing queries"},
    {"Proxy", "Server Admin", "To make proxy user possible"},
    {"References", "Databases,Tables", "To have references on tables"},
    {"Reload", "Server Admin",
     "To reload or refresh tables, logs and privileges"},
    {"Replication client", "Server Admin",
     "To ask where the slave or master servers are"},
    {"Replication slave", "Server Admin",
     "To read binary log events from the master"},
    {"Select", "Tables", "To retrieve rows from table"},
    {"Show databases", "Server Admin",
     "To see all databases with SHOW DATABASES"},
    {"Show view", "Tables", "To see views with SHOW CREATE VIEW"},
    {"Shutdown", "Server Admin", "To shut down the server"},
    {"Super", "Server Admin",
     "To use KILL thread, SET GLOBAL, CHANGE REPLICATION SOURCE, etc."},
    {"Trigger", "Tables", "To use triggers"},
    {"Create tablespace", "Server Admin", "To create/alter/drop tablespaces"},
    {"Update", "Tables", "To update existing rows"},
    {"Usage", "Server Admin", "No privileges - allow connect only"},
    {NullS, NullS, NullS}};

bool mysqld_show_privileges(THD *thd) {
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(new Item_empty_string("Privilege", 10));
  field_list.push_back(new Item_empty_string("Context", 15));
  field_list.push_back(new Item_empty_string("Comment", NAME_CHAR_LEN));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  show_privileges_st *privilege = sys_privileges;
  for (privilege = sys_privileges; privilege->privilege; privilege++) {
    protocol->start_row();
    protocol->store(privilege->privilege, system_charset_info);
    protocol->store(privilege->context, system_charset_info);
    protocol->store(privilege->comment, system_charset_info);
    if (protocol->end_row()) return true;
  }
  if (iterate_all_dynamic_privileges(thd,
                                     /*
                                       For each registered dynamic privilege
                                       send a strz to this lambda function.
                                     */
                                     [&](const char *c) -> bool {
                                       protocol->start_row();
                                       protocol->store(c, system_charset_info);
                                       protocol->store("Server Admin",
                                                       system_charset_info);
                                       protocol->store("", system_charset_info);
                                       if (protocol->end_row()) return true;
                                       return false;
                                     })) {
    return true;
  }
  my_eof(thd);
  return false;
}

/**
   An Internal_error_handler that suppresses errors regarding views'
   underlying tables that occur during privilege checking within SHOW CREATE
   VIEW commands. This happens in the cases when

   - A view's underlying table (e.g. referenced in its SELECT list) does not
     exist or columns of underlying table are altered. There should not be an
     error as no attempt was made to access it per se.

   - Access is denied for some table, column, function or stored procedure
     such as mentioned above. This error gets raised automatically, since we
     can't untangle its access checking from that of the view itself.
 */
class Show_create_error_handler : public Internal_error_handler {
  Table_ref *m_top_view;
  bool m_handling;
  Security_context *m_sctx;

  char m_view_access_denied_message[MYSQL_ERRMSG_SIZE];
  const char *m_view_access_denied_message_ptr;

 public:
  /**
     Creates a new Show_create_error_handler for the particular security
     context and view.

     @param thd Thread context, used for security context information if needed.
     @param top_view The view. We do not verify at this point that top_view is
     in fact a view since, alas, these things do not stay constant.
  */
  explicit Show_create_error_handler(THD *thd, Table_ref *top_view)
      : m_top_view(top_view),
        m_handling(false),
        m_view_access_denied_message_ptr(nullptr) {
    m_sctx = (m_top_view->security_ctx != nullptr) ? m_top_view->security_ctx
                                                   : thd->security_context();
  }

 private:
  /**
     Lazy instantiation of 'view access denied' message. The purpose of the
     Show_create_error_handler is to hide details of underlying tables for
     which we have no privileges behind ER_VIEW_INVALID messages. But this
     obviously does not apply if we lack privileges on the view itself.
     Unfortunately the information about for which table privilege checking
     failed is not available at this point. The only way for us to check is by
     reconstructing the actual error message and see if it's the same.
  */
  // [[maybe_unused]] This applies to CHECK_ERRMSG_FORMAT = ON
  const char *get_view_access_denied_message(THD *thd [[maybe_unused]]) {
    if (!m_view_access_denied_message_ptr) {
      m_view_access_denied_message_ptr = m_view_access_denied_message;
      snprintf(m_view_access_denied_message, MYSQL_ERRMSG_SIZE,
               ER_THD(thd, ER_TABLEACCESS_DENIED_ERROR), "SHOW VIEW",
               m_sctx->priv_user().str, m_sctx->host_or_ip().str,
               m_top_view->get_table_name());
    }
    return m_view_access_denied_message_ptr;
  }

 public:
  bool handle_condition(THD *thd, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *,
                        const char *msg) override {
    /*
       The handler does not handle the errors raised by itself.
       At this point we know if top_view is really a view.
    */
    if (m_handling || !m_top_view->is_view()) return false;

    m_handling = true;

    bool is_handled;

    switch (sql_errno) {
      case ER_TABLEACCESS_DENIED_ERROR:
        if (!strcmp(get_view_access_denied_message(thd), msg)) {
          /* Access to top view is not granted, don't interfere. */
          is_handled = false;
          break;
        }
        [[fallthrough]];
      case ER_COLUMNACCESS_DENIED_ERROR:
      // ER_VIEW_NO_EXPLAIN cannot happen here.
      case ER_PROCACCESS_DENIED_ERROR:
        is_handled = true;
        break;

      case ER_BAD_FIELD_ERROR:
        /*
          Established behavior: warn if column of underlying table is altered.
        */
      case ER_NO_SUCH_TABLE:
        /* Established behavior: warn if underlying tables are missing. */
      case ER_SP_DOES_NOT_EXIST:
        /* Established behavior: warn if underlying functions are missing. */
        push_warning_printf(thd, Sql_condition::SL_WARNING, ER_VIEW_INVALID,
                            ER_THD(thd, ER_VIEW_INVALID),
                            m_top_view->get_db_name(),
                            m_top_view->get_table_name());
        is_handled = true;
        break;
      default:
        is_handled = false;
    }

    m_handling = false;
    return is_handled;
  }
};

bool mysqld_show_create(THD *thd, Table_ref *table_list) {
  Protocol *protocol = thd->get_protocol();
  char buff[2048];
  mem_root_deque<Item *> field_list(thd->mem_root);
  String buffer(buff, sizeof(buff), system_charset_info);
  bool error = true;
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("db: %s  table: %s", table_list->db, table_list->table_name));

  /*
    Metadata locks taken during SHOW CREATE should be released when
    the statmement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  /* We want to preserve the tree for views. */
  thd->lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_VIEW;

  {
    /*
      If there is an error during processing of an underlying view, an
      error message is wanted, but it has to be converted to a warning,
      so that execution can continue.
      This is handled by the Show_create_error_handler class.

      Use open_tables() instead of open_tables_for_query(). If an error occurs,
      this will ensure that tables are not closed on error, but remain open
      for the rest of the processing of the SHOW statement.
    */
    Show_create_error_handler view_error_suppressor(thd, table_list);
    thd->push_internal_handler(&view_error_suppressor);

    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    uint counter;
    bool open_error = open_tables(thd, &table_list, &counter,
                                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL);
    if (!open_error && table_list->is_view_or_derived() &&
        !table_list->derived_query_expression()->is_prepared()) {
      /*
        Prepare result table for view so that we can read the column list.
        Notice that Show_create_error_handler remains active, so that any
        errors due to missing underlying objects are converted to warnings.
      */
      open_error = table_list->resolve_derived(thd, true);

      /*
        If a suppressed error occurred, the query expression may not be
        marked as prepared. Mark it as prepared nevertheless, this is
        good enough for the SHOW CREATE command, as we only need the
        preparation for errors against referenced objects.
      */
      if (!table_list->derived_query_expression()->is_prepared()) {
        table_list->derived_query_expression()->set_prepared();
      }
    }
    thd->pop_internal_handler();
    if (open_error && (thd->killed || thd->is_error())) goto exit;

    /*
      Table_function::print() only works after the table function has been
      resolved. If resolving the view fails, and the view references an
      unresolved table function, raise an error instead of calling print() on
      the unresolved table function.
    */
    if (open_error && table_list->is_view()) {
      for (Table_ref *tl = table_list; tl != nullptr; tl = tl->next_global) {
        if (tl->is_table_function() && tl->table == nullptr) {
          my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                   "SHOW CREATE VIEW on a view that references a non-existent "
                   "table and a table function.");
          goto exit;
        }
      }
    }
  }

  /* TODO: add environment variables show when it become possible */
  if (thd->lex->only_view && !table_list->is_view()) {
    my_error(ER_WRONG_OBJECT, MYF(0), table_list->db, table_list->table_name,
             "VIEW");
    goto exit;
  }

  buffer.length(0);

  if (table_list->is_view()) {
    buffer.set_charset(table_list->view_creation_ctx->get_client_cs());
  }
  if (table_list->is_view())
    view_store_create_info(thd, table_list, &buffer);
  else if (store_create_info(thd, table_list, &buffer, nullptr,
                             false /* show_database */,
                             true /* SHOW CREATE TABLE */))
    goto exit;

  if (table_list->is_view()) {
    field_list.push_back(new Item_empty_string("View", NAME_CHAR_LEN));
    field_list.push_back(new Item_empty_string(
        "Create View", max<uint>(buffer.length(), 1024U)));
    field_list.push_back(
        new Item_empty_string("character_set_client", MY_CS_NAME_SIZE));
    field_list.push_back(
        new Item_empty_string("collation_connection", MY_CS_NAME_SIZE));
  } else {
    field_list.push_back(new Item_empty_string("Table", NAME_CHAR_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string(
        "Create Table", max<size_t>(buffer.length(), 1024U)));
  }

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    goto exit;

  protocol->start_row();
  if (table_list->is_view())
    protocol->store(table_list->table_name, system_charset_info);
  else {
    if (table_list->schema_table)
      protocol->store(table_list->schema_table->table_name,
                      system_charset_info);
    else
      protocol->store(table_list->table->alias, system_charset_info);
  }

  if (table_list->is_view()) {
    protocol->store_string(buffer.ptr(), buffer.length(),
                           table_list->view_creation_ctx->get_client_cs());

    protocol->store(table_list->view_creation_ctx->get_client_cs()->csname,
                    system_charset_info);

    protocol->store(
        table_list->view_creation_ctx->get_connection_cl()->m_coll_name,
        system_charset_info);
  } else
    protocol->store_string(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row()) goto exit;

  error = false;
  my_eof(thd);

exit:
  if (table_list->is_view()) {
    table_list->view_query()->cleanup(true);
    table_list->view_query()->destroy();
  }
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  return error;
}

bool mysqld_show_create_db(THD *thd, char *dbname,
                           HA_CREATE_INFO *create_info) {
  char buff[2048], orig_dbname[NAME_LEN];
  String buffer(buff, sizeof(buff), system_charset_info);
  Security_context *sctx = thd->security_context();
  uint db_access;
  HA_CREATE_INFO create;
  bool schema_read_only{false};
  const uint create_options = create_info ? create_info->options : 0;
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  strcpy(orig_dbname, dbname);
  if (lower_case_table_names && dbname != any_db)
    my_casedn_str(files_charset_info, dbname);
  if (sctx->check_access(DB_OP_ACLS, orig_dbname))
    db_access = DB_OP_ACLS;
  else
    db_access =
        (Security_context::check_db_level_access(
             thd, dbname ? sctx : nullptr, sctx->host().str, sctx->ip().str,
             sctx->priv_user().str, dbname, strlen(dbname)) |
         sctx->master_access(dbname ? dbname : ""));
  if (!(db_access & DB_OP_ACLS) && check_grant_db(thd, dbname, true)) {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), sctx->priv_user().str,
             sctx->host_or_ip().str, dbname);
    query_logger.general_log_print(
        thd, COM_INIT_DB, ER_DEFAULT(ER_DBACCESS_DENIED_ERROR),
        sctx->priv_user().str, sctx->host_or_ip().str, dbname);
    return true;
  }

  bool is_encrypted_schema = false;
  if (is_infoschema_db(dbname)) {
    create.default_table_charset = system_charset_info;
  } else {
    dd::Schema_MDL_locker mdl_handler(thd);
    const dd::cache::Dictionary_client::Auto_releaser releaser(
        thd->dd_client());
    const dd::Schema *schema = nullptr;
    if (mdl_handler.ensure_locked(dbname) ||
        thd->dd_client()->acquire(dbname, &schema))
      return true;

    if (schema == nullptr) {
      my_error(ER_BAD_DB_ERROR, MYF(0), dbname);
      return true;
    }

    if (get_default_db_collation(*schema, &create.default_table_charset)) {
      assert(thd->is_error() || thd->killed);
      return true;
    }

    schema_read_only = schema->read_only();

    if (create.default_table_charset == nullptr)
      create.default_table_charset = thd->collation();

    is_encrypted_schema = schema->default_encryption();
  }
  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(new Item_empty_string("Database", NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database", 1024));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  protocol->start_row();
  protocol->store_string(orig_dbname, strlen(orig_dbname), system_charset_info);
  buffer.length(0);
  buffer.append(STRING_WITH_LEN("CREATE DATABASE "));
  if (create_options & HA_LEX_CREATE_IF_NOT_EXISTS)
    buffer.append(STRING_WITH_LEN("/*!32312 IF NOT EXISTS*/ "));
  append_identifier(thd, &buffer, orig_dbname, strlen(orig_dbname));

  if (create.default_table_charset) {
    buffer.append(STRING_WITH_LEN(" /*!40100"));
    buffer.append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    buffer.append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY) ||
        create.default_table_charset == &my_charset_utf8mb4_0900_ai_ci) {
      buffer.append(STRING_WITH_LEN(" COLLATE "));
      buffer.append(create.default_table_charset->m_coll_name);
    }
    buffer.append(STRING_WITH_LEN(" */"));
  }
  buffer.append(STRING_WITH_LEN(" /*!80016"));
  buffer.append(STRING_WITH_LEN(" DEFAULT ENCRYPTION="));
  if (is_encrypted_schema)
    buffer.append(STRING_WITH_LEN("'Y'"));
  else
    buffer.append(STRING_WITH_LEN("'N'"));
  buffer.append(STRING_WITH_LEN(" */"));

  /*
    The read only option is not supported by the CREATE SCHEMA syntax,
    so we enclose the output in a separate comment without version tag.
  */
  if (schema_read_only) buffer.append(STRING_WITH_LEN(" /* READ ONLY = 1 */"));

  protocol->store_string(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row()) return true;
  my_eof(thd);
  return false;
}

/****************************************************************************
  Return only fields for API mysql_list_fields
  Use "show table wildcard" in mysql instead of this
****************************************************************************/
void mysqld_list_fields(THD *thd, Table_ref *table_list, const char *wild) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table: %s", table_list->table_name));
  if (open_tables_for_query(thd, table_list,
                            MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL))
    return;
  if (table_list->is_view_or_derived()) {
    // Setup materialized result table so that we can read the column list
    if (table_list->resolve_derived(thd, false))
      return; /* purecov: inspected */
    if (table_list->setup_materialized_derived(thd))
      return; /* purecov: inspected */
  }
  TABLE *table = table_list->table;
  mem_root_deque<Item *> field_list(thd->mem_root);
  Field **ptr, *field;
  for (ptr = table->field; (field = *ptr); ptr++) {
    if (!wild || !wild[0] ||
        !wild_case_compare(system_charset_info, field->field_name, wild)) {
      Item *item;
      if (table_list->is_view()) {
        item = new Item_ident_for_show(field, table_list->db,
                                       table_list->table_name);
        (void)item->fix_fields(thd, nullptr);
      } else {
        item = new Item_field(field);
      }
      field_list.push_back(item);
    }
  }
  restore_record(table, s->default_values);  // Get empty record
  table->use_all_columns();
  if (thd->send_result_metadata(field_list, Protocol::SEND_DEFAULTS)) return;
  if (table_list->is_view_or_derived()) {
    close_tmp_table(table);
    free_tmp_table(table);
    table_list->table = nullptr;
  }
  my_eof(thd);
}

/*
  Go through all character combinations and ensure that sql_lex.cc can
  parse it as an identifier.

  SYNOPSIS
  require_quotes()
  name			attribute name
  name_length		length of name

  RETURN
    #	Pointer to conflicting character
    0	No conflicting character
*/

static const char *require_quotes(const char *name, size_t name_length) {
  bool pure_digit = true;
  const char *end = name + name_length;

  // Identifier starting with '$' is deprecated.
  if (name_length && static_cast<uchar>(*name) == '$') return name;

  for (; name < end; name++) {
    const uchar chr = (uchar)*name;
    const uint length = my_mbcharlen(system_charset_info, chr);
    if (length == 0 || (length == 1 && !system_charset_info->ident_map[chr]))
      return name;
    if (length == 1 && (chr < '0' || chr > '9')) pure_digit = false;
  }
  if (pure_digit) return name;
  return nullptr;
}

/**
  Convert and quote the given identifier if needed and append it to the
  target string. If the given identifier is empty, it will be quoted.
  This function always use the backtick as escape char and thus rid itself
  of the THD dependency.

  @param packet                target string
  @param name                  the identifier to be appended
  @param length                length of the appending identifier
*/

void append_identifier(String *packet, const char *name, size_t length) {
  const char *name_end;
  const char quote_char = '`';

  const CHARSET_INFO *cs_info = system_charset_info;
  const char *to_name = name;
  size_t to_length = length;

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  (void)packet->reserve(to_length * 2 + 2);
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end = to_name + to_length; to_name < name_end;
       to_name += to_length) {
    const uchar chr = static_cast<uchar>(*to_name);
    to_length = my_mbcharlen(cs_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!to_length) to_length = 1;
    if (to_length == 1 && chr == static_cast<uchar>(quote_char))
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(to_name, to_length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}

/**
  Convert and quote the given identifier if needed and append it to the
  target string. If the given identifier is empty, it will be quoted.

  @param thd                   thread handler
  @param packet                target string
  @param name                  the identifier to be appended
  @param length                length of the appending identifier
  @param from_cs               Charset information about the input string
  @param to_cs                 Charset information about the target string
*/

void append_identifier(const THD *thd, String *packet, const char *name,
                       size_t length, const CHARSET_INFO *from_cs,
                       const CHARSET_INFO *to_cs) {
  const char *name_end;
  char quote_char;
  int q;

  const CHARSET_INFO *cs_info = system_charset_info;
  const char *to_name = name;
  size_t to_length = length;
  String to_string(name, length, from_cs);

  if (from_cs != nullptr && to_cs != nullptr && from_cs != to_cs) {
    uint dummy_errors;
    StringBuffer<MAX_FIELD_WIDTH> convert_buffer;
    convert_buffer.copy(to_string.ptr(), to_string.length(), from_cs, to_cs,
                        &dummy_errors);
    to_string.copy(convert_buffer);
  }

  if (to_cs != nullptr) {
    to_name = to_string.c_ptr();
    to_length = to_string.length();
    cs_info = to_cs;
  }

  q = thd != nullptr ? get_quote_char_for_identifier(thd, to_name, to_length)
                     : '`';

  if (q == EOF) {
    packet->append(to_name, to_length, packet->charset());
    return;
  }

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  (void)packet->reserve(to_length * 2 + 2);
  quote_char = (char)q;
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end = to_name + to_length; to_name < name_end;
       to_name += to_length) {
    const uchar chr = static_cast<uchar>(*to_name);
    to_length = my_mbcharlen(cs_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!to_length) to_length = 1;
    if (to_length == 1 && chr == static_cast<uchar>(quote_char))
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(to_name, to_length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}

/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()
    thd		Thread handler
    name	name to quote
    length	length of name

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier(const THD *thd, const char *name,
                                  size_t length) {
  if (length && !is_keyword(name, length) && !require_quotes(name, length) &&
      !(thd->variables.option_bits & OPTION_QUOTE_SHOW_CREATE))
    return EOF;
  if (thd->variables.sql_mode & MODE_ANSI_QUOTES) return '"';
  return '`';
}

void append_identifier(const THD *thd, String *packet, const char *name,
                       size_t length) {
  if (thd == nullptr)
    append_identifier(packet, name, length);
  else
    append_identifier(thd, packet, name, length, nullptr, nullptr);
}

/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
                             const char *filename) {
  if (filename && !(thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)) {
    const size_t length = dirname_length(filename);
    packet->append(' ');
    packet->append(dir_type);
    packet->append(STRING_WITH_LEN(" DIRECTORY='"));
#ifdef _WIN32
    /* Convert \ to / to be able to create table on unix */
    char *winfilename = (char *)thd->memdup(filename, length);
    char *pos, *end;
    for (pos = winfilename, end = pos + length; pos < end; pos++) {
      if (*pos == '\\') *pos = '/';
    }
    filename = winfilename;
#endif
    packet->append(filename, length);
    packet->append('\'');
  }
}

/**
  Print "ON UPDATE" clause of a field into a string.

  @param field             The field to generate ON UPDATE clause for.
  @param val               String to write ON UPDATE clause to to
  @param lcase             Whether to print in lower case.
  @return                  false on success, true on error.
*/
static bool print_on_update_clause(Field *field, String *val, bool lcase) {
  assert(val->charset()->mbminlen == 1);
  val->length(0);
  if (field->has_update_default_datetime_value_expression()) {
    if (lcase)
      val->copy(STRING_WITH_LEN("on update "), val->charset());
    else
      val->copy(STRING_WITH_LEN("ON UPDATE "), val->charset());
    val->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
    if (field->decimals() > 0) val->append_parenthesized(field->decimals());
    return true;
  }
  return false;
}

/**
  Print "DEFAULT" clause of a field into a string.

  @param thd               The THD to create the DEFAULT clause for.
  @param field             The field to generate the DEFAULT clause for.
  @param def_value         String to write the DEFAULT clause to to.
  @param quoted            Whether to quote the default value.
                           false for not needed (value will became its
                           own field (with its own charset) in a table);
                           true for quote (as part of a SHOW CREATE statement).
  @return                  true if field has a DEFAULT value, false otherwise.
*/
static bool print_default_clause(THD *thd, Field *field, String *def_value,
                                 bool quoted) {
  const enum enum_field_types field_type = field->type();
  const bool has_default = (!field->is_flag_set(NO_DEFAULT_VALUE_FLAG) &&
                            !(field->auto_flags & Field::NEXT_NUMBER));

  if (field->gcol_info) return false;

  def_value->length(0);
  if (has_default) {
    if (field->has_insert_default_general_value_expression()) {
      def_value->append("(");
      char buffer[128];
      String s(buffer, sizeof(buffer), system_charset_info);
      field->m_default_val_expr->print_expr(thd, &s);
      def_value->append(s);
      def_value->append(")");
    } else if (field->has_insert_default_datetime_value_expression()) {
      /*
        We are using CURRENT_TIMESTAMP instead of NOW because it is the SQL
        standard.
      */
      def_value->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
      if (field->decimals() > 0)
        def_value->append_parenthesized(field->decimals());
      // Not null by default and not a BLOB
    } else if (!field->is_null() && field_type != FIELD_TYPE_BLOB) {
      char tmp[MAX_FIELD_WIDTH];
      String type(tmp, sizeof(tmp), field->charset());
      // Wrap bit values in b'...'
      if (field_type == MYSQL_TYPE_BIT) {
        const longlong dec = field->val_int();
        char *ptr = longlong2str(dec, tmp + 2, 2);
        const uint32 length = (uint32)(ptr - tmp);
        tmp[0] = 'b';
        tmp[1] = '\'';
        tmp[length] = '\'';
        type.length(length + 1);
        quoted = false;
      } else
        field->val_str(&type);

      if (type.length()) {
        /*
          value_only will contain only the default value itself,
          while def_value will contain that, any quotation, escaping,
          character-set designation, and so on.
        */
        String value_only;

        /* Try to convert to system_charset_info (UTF-8). */
        /* Counter-intuitively, we do not receive an error if we can not
           create valid UTF-8 with copy(), so we'll have to talk to the
           lower level functions. */

        const char *well_formed_error_pos;
        const char *cannot_convert_error_pos;
        const char *from_end_pos;
        size_t to_len; /* destination size in bytes (not characters) */
        size_t bytes_copied;

        to_len = type.length() * system_charset_info->mbmaxlen;

        value_only.reserve(to_len);

        bytes_copied = well_formed_copy_nchars(
            /* to    */ system_charset_info, value_only.ptr(), to_len,
            /* from  */ field->charset(), type.ptr(), type.length(),
            /* chars */ type.length(), &well_formed_error_pos,
            &cannot_convert_error_pos, &from_end_pos);

        // If conversion failed, hexify string.
        if ((well_formed_error_pos != nullptr) ||
            (cannot_convert_error_pos != nullptr)) {
          unsigned char *p;
          size_t l = type.length();
          char
              hex[3];  // 2 characters as we have 2 nibbles per byte, plus '\0'.

          /*
            Result length:
            - 2 characters (for 2 nibbles) for each byte we hexify
            - plus "0x"
            - plus '\0'
          */
          def_value->reserve(l * 2 + 3);
          def_value->append("0x");
          p = (unsigned char *)type.ptr();
          while (l--) {
            snprintf(hex, sizeof(hex), "%02X", (int)(*(p++) & 0xff));
            def_value->append(hex);
          }
        }

        /*
          Charset conversation succeeded or wasn't necessary.
        */
        else {
          value_only.length(bytes_copied);

          if (quoted)
            append_unescaped(def_value, value_only.ptr(), value_only.length());
          else
            def_value->append(value_only.ptr(), value_only.length());
        }

      } else if (quoted)  // !type.length()
        def_value->append(STRING_WITH_LEN("''"));
    } else if (field->is_nullable() && quoted && field_type != FIELD_TYPE_BLOB)
      def_value->append(STRING_WITH_LEN("NULL"));  // Null as default
    else
      return false;
  }
  return has_default;
}

/*
  Check if we should print encryption clause. We always show explicit
  ENCRYPTION clause, OR if table's schema has default encryption enabled.

  @param thd              The thread
  @param share            Represents table share metadata.
  @param *print     [out] Point to out param.

  @returns true if ENCRYPTION clause should be printed, else false.
*/
static bool should_print_encryption_clause(THD *thd, TABLE_SHARE *share,
                                           bool *print) {
  // Don't print for temporary
  if (share->tmp_table) {
    *print = false;
    return false;
  }

  // Find schema encryption default.
  dd::Schema_MDL_locker mdl_handler(thd);
  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *schema = nullptr;
  if (mdl_handler.ensure_locked(share->db.str) ||
      thd->dd_client()->acquire(share->db.str, &schema))
    return true;
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), share->db.str);
    return true;
  }

  // Decide if we need to print the clause.
  const bool table_is_encrypted = dd::is_encrypted(share->encrypt_type);
  *print = table_is_encrypted ||
           (schema->default_encryption() != table_is_encrypted);

  return false;
}

/**
  Append definitions of FOREIGN KEY statements to SHOW CREATE TABLE statement
  output for the table.

  @param          thd         Thread context.
  @param          db          Table's database.
  @param          table_obj   dd::Table describing the table.
  @param[in,out]  packet      Pointer to a string where CREATE TABLE statement
                              describing the table is being constructed.
*/
static void print_foreign_key_info(THD *thd, const LEX_CSTRING *db,
                                   const dd::Table *table_obj, String *packet) {
  if (!table_obj) return;

  for (const dd::Foreign_key *fk : table_obj->foreign_keys()) {
    packet->append(STRING_WITH_LEN(",\n  "));
    packet->append(STRING_WITH_LEN("CONSTRAINT "));
    append_identifier(thd, packet, fk->name().c_str(), fk->name().length());
    packet->append(STRING_WITH_LEN(" FOREIGN KEY ("));

    bool first = true;
    for (const dd::Foreign_key_element *fk_el : fk->elements()) {
      if (!first)
        packet->append(STRING_WITH_LEN(", "));
      else
        first = false;
      append_identifier(thd, packet, fk_el->column().name().c_str(),
                        fk_el->column().name().length());
    }

    packet->append(STRING_WITH_LEN(") REFERENCES "));

    if (my_strcasecmp(table_alias_charset, db->str,
                      fk->referenced_table_schema_name().c_str())) {
      append_identifier(thd, packet, fk->referenced_table_schema_name().c_str(),
                        fk->referenced_table_schema_name().length());
      packet->append('.');
    }
    append_identifier(thd, packet, fk->referenced_table_name().c_str(),
                      fk->referenced_table_name().length());
    packet->append(STRING_WITH_LEN(" ("));

    first = true;
    for (const dd::Foreign_key_element *fk_el : fk->elements()) {
      if (!first)
        packet->append(STRING_WITH_LEN(", "));
      else
        first = false;
      append_identifier(thd, packet, fk_el->referenced_column_name().c_str(),
                        fk_el->referenced_column_name().length());
    }
    packet->append(")");

    switch (fk->delete_rule()) {
      case dd::Foreign_key::RULE_NO_ACTION:
        // Don't print clause when default is used.
        break;
      case dd::Foreign_key::RULE_RESTRICT:
        packet->append(STRING_WITH_LEN(" ON DELETE RESTRICT"));
        break;
      case dd::Foreign_key::RULE_CASCADE:
        packet->append(STRING_WITH_LEN(" ON DELETE CASCADE"));
        break;
      case dd::Foreign_key::RULE_SET_NULL:
        packet->append(STRING_WITH_LEN(" ON DELETE SET NULL"));
        break;
      case dd::Foreign_key::RULE_SET_DEFAULT:
        // Future-proofing, we don't support this now.
        packet->append(STRING_WITH_LEN(" ON DELETE SET DEFAULT"));
        break;
      default:
        assert(0);
        break;
    }
    switch (fk->update_rule()) {
      case dd::Foreign_key::RULE_NO_ACTION:
        // Don't print clause when default is used.
        break;
      case dd::Foreign_key::RULE_RESTRICT:
        packet->append(STRING_WITH_LEN(" ON UPDATE RESTRICT"));
        break;
      case dd::Foreign_key::RULE_CASCADE:
        packet->append(STRING_WITH_LEN(" ON UPDATE CASCADE"));
        break;
      case dd::Foreign_key::RULE_SET_NULL:
        packet->append(STRING_WITH_LEN(" ON UPDATE SET NULL"));
        break;
      case dd::Foreign_key::RULE_SET_DEFAULT:
        // Future-proofing, we don't support this now.
        packet->append(STRING_WITH_LEN(" ON UPDATE SET DEFAULT"));
        break;
      default:
        assert(0);
        break;
    }
  }
}

/**
  Build a CREATE TABLE statement for a table.

  @param thd              The thread
  @param table_list       A list containing one table to write statement for.
  @param packet           Pointer to a string where statement will be written.
  @param create_info_arg  Pointer to create information that can be used to
                          tailor the format of the statement.  Can be NULL,
                          in which case only SQL_MODE is considered when
                          building the statement.
  @param show_database    If true, then print the database before the table
                          name. The database name is only printed in the event
                          that it is different from the current database.
                          If false, then do not print the database before
                          the table name.
  @param for_show_create_stmt  If true, then build CREATE TABLE statement for
                               SHOW CREATE TABLE statement. If false, then
                               store_create_info() is invoked to build CREATE
                               TABLE statement while logging event to binlog.


  @returns true if error, false otherwise.
*/

bool store_create_info(THD *thd, Table_ref *table_list, String *packet,
                       HA_CREATE_INFO *create_info_arg, bool show_database,
                       bool for_show_create_stmt) {
  char tmp[MAX_FIELD_WIDTH], buff[128], def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr, *field;
  uint primary_key;
  KEY *key_info;
  TABLE *table = table_list->table;
  handler *file = table->file;
  TABLE_SHARE *share = table->s;
  HA_CREATE_INFO create_info;
  bool show_table_options = false;
  const bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;
  my_bitmap_map *old_map;
  bool error = false;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table: %s", table->s->table_name.str));

  restore_record(table, s->default_values);  // Get empty record

  if (share->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (create_info_arg &&
      (create_info_arg->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (table_list->schema_table)
    alias = table_list->schema_table->table_name;
  else {
    if (lower_case_table_names == 2)
      alias = table->alias;
    else {
      alias = share->table_name.str;
    }
  }

  /*
    Print the database before the table name if told to do that. The
    database name is only printed in the event that it is different
    from the current database.  The main reason for doing this is to
    avoid having to update gazillions of tests and result files, but
    it also saves a few bytes of the binary log.
   */
  const LEX_CSTRING *const db =
      table_list->schema_table ? &INFORMATION_SCHEMA_NAME : &table->s->db;
  if (show_database) {
    if (!thd->db().str || strcmp(db->str, thd->db().str)) {
      append_identifier(thd, packet, db->str, db->length);
      packet->append(STRING_WITH_LEN("."));
    }
  }

  append_identifier(thd, packet, alias, strlen(alias));
  packet->append(STRING_WITH_LEN(" (\n"));
  /*
    We need this to get default values from the table
    We have to restore the read_set if we are called from insert in case
    of row based replication.
  */
  old_map = tmp_use_all_columns(table, table->read_set);
  auto grd = create_scope_guard(
      [&]() { tmp_restore_column_map(table->read_set, old_map); });
  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_obj = nullptr;
  if (share->tmp_table)
    table_obj = table->s->tmp_table_def;
  else {
    if (thd->dd_client()->acquire(dd::String_type(share->db.str),
                                  dd::String_type(share->table_name.str),
                                  &table_obj))
      return true;
    DBUG_EXECUTE_IF("sim_acq_fail_in_store_ci", {
      my_error(ER_DA_UNKNOWN_ERROR_NUMBER, MYF(0), 42);
      return true;
    });
  }

  /*
    When building CREATE TABLE statement for the SHOW CREATE TABLE (i.e.
    for_show_create_stmt = true), skip generated invisible primary key
    if system variable 'show_gipk_in_create_table_and_information_schema' is set
    to OFF.
  */
  const bool skip_gipk =
      (for_show_create_stmt &&
       table_has_generated_invisible_primary_key(table) &&
       !thd->variables.show_gipk_in_create_table_and_information_schema);

  Field **first_field = table->field;
  /*
    Generated invisible primary key column is placed at the first position.
    So skip first column when skip_gipk is set.
  */
  assert(!table_has_generated_invisible_primary_key(table) ||
         is_generated_invisible_primary_key_column_name(
             (*first_field)->field_name));
  if (skip_gipk) first_field++;

  for (ptr = first_field; (field = *ptr); ptr++) {
    // Skip hidden system fields.
    if (field->is_hidden_by_system()) continue;

    if (ptr != first_field) packet->append(STRING_WITH_LEN(",\n"));

    packet->append(STRING_WITH_LEN("  "));
    append_identifier(thd, packet, field->field_name,
                      strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    packet->append(type.ptr(), type.length(), system_charset_info);

    bool column_has_explicit_collation = false;
    /* We may not have a table_obj for schema_tables. */
    if (table_obj)
      column_has_explicit_collation =
          table_obj->get_column(field->field_name)->is_explicit_collation();

    if (field->has_charset()) {
      /*
        For string types dump charset name only if field charset is same as
        table charset or was explicitly assigned.
      */
      if (field->charset() != share->table_charset ||
          column_has_explicit_collation) {
        packet->append(STRING_WITH_LEN(" CHARACTER SET "));
        packet->append(field->charset()->csname);
      }
      /*
        For string types dump collation name only if
        collation is not primary for the given charset
        or was explicitly assigned.
      */
      if (!(field->charset()->state & MY_CS_PRIMARY) ||
          column_has_explicit_collation ||
          (field->charset() == &my_charset_utf8mb4_0900_ai_ci &&
           share->table_charset != &my_charset_utf8mb4_0900_ai_ci)) {
        packet->append(STRING_WITH_LEN(" COLLATE "));
        packet->append(field->charset()->m_coll_name);
      }
    }

    if (field->gcol_info) {
      packet->append(STRING_WITH_LEN(" GENERATED ALWAYS"));
      packet->append(STRING_WITH_LEN(" AS ("));
      char buffer[128];
      String s(buffer, sizeof(buffer), system_charset_info);
      field->gcol_info->print_expr(thd, &s);
      packet->append(s);
      packet->append(STRING_WITH_LEN(")"));
      if (field->stored_in_db)
        packet->append(STRING_WITH_LEN(" STORED"));
      else
        packet->append(STRING_WITH_LEN(" VIRTUAL"));
    }

    if (field->is_flag_set(NOT_NULL_FLAG))
      packet->append(STRING_WITH_LEN(" NOT NULL"));
    else if (field->type() == MYSQL_TYPE_TIMESTAMP) {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(STRING_WITH_LEN(" NULL"));
    }

    if (field->is_flag_set(NOT_SECONDARY_FLAG))
      packet->append(STRING_WITH_LEN(" NOT SECONDARY"));

    if (field->type() == MYSQL_TYPE_GEOMETRY) {
      const Field_geom *field_geom = down_cast<const Field_geom *>(field);
      if (field_geom->get_srid().has_value()) {
        packet->append(STRING_WITH_LEN(" /*!80003 SRID "));
        packet->append_ulonglong(field_geom->get_srid().value());
        packet->append(STRING_WITH_LEN(" */"));
      }
    }
    switch (field->field_storage_type()) {
      case HA_SM_DEFAULT:
        break;
      case HA_SM_DISK:
        packet->append(STRING_WITH_LEN(" /*!50606 STORAGE DISK */"));
        break;
      case HA_SM_MEMORY:
        packet->append(STRING_WITH_LEN(" /*!50606 STORAGE MEMORY */"));
        break;
      default:
        assert(0);
        break;
    }

    switch (field->column_format()) {
      case COLUMN_FORMAT_TYPE_DEFAULT:
        break;
      case COLUMN_FORMAT_TYPE_FIXED:
        packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT FIXED */"));
        break;
      case COLUMN_FORMAT_TYPE_DYNAMIC:
        packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT DYNAMIC */"));
        break;
      default:
        assert(0);
        break;
    }

    if (print_default_clause(thd, field, &def_value, true)) {
      packet->append(STRING_WITH_LEN(" DEFAULT "));
      packet->append(def_value.ptr(), def_value.length(), system_charset_info);
    }

    if (print_on_update_clause(field, &def_value, false)) {
      packet->append(STRING_WITH_LEN(" "));
      packet->append(def_value);
    }

    if (field->auto_flags & Field::NEXT_NUMBER)
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT"));

    // Column visibility attribute
    if (field->is_hidden_by_user())
      packet->append(STRING_WITH_LEN(" /*!80023 INVISIBLE */"));

    if (field->comment.length) {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, field->comment.str, field->comment.length);
    }

    // Storage engine specific json attributes
    if (field->m_engine_attribute.length) {
      packet->append(STRING_WITH_LEN(" /*!80021 ENGINE_ATTRIBUTE "));
      // append escaped JSON
      append_unescaped(packet, field->m_engine_attribute.str,
                       field->m_engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }
    if (field->m_secondary_engine_attribute.length) {
      packet->append(STRING_WITH_LEN(" /*!80021 SECONDARY_ENGINE_ATTRIBUTE "));
      // escape JSON
      append_unescaped(packet, field->m_secondary_engine_attribute.str,
                       field->m_secondary_engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }
  }

  key_info = table->key_info;
  /*
    Primary key is always at the first position in the keys list. Skip printing
    primary key definition when skip_gipk is set.
  */
  assert(!table_has_generated_invisible_primary_key(table) ||
         ((key_info->user_defined_key_parts == 1) &&
          is_generated_invisible_primary_key_column_name(
              key_info->key_part->field->field_name)));
  if (skip_gipk) key_info++;

  /* Allow update_create_info to update row type */
  create_info.row_type = share->row_type;
  file->update_create_info(&create_info);
  primary_key = share->primary_key;

  for (uint i = skip_gipk ? 1 : 0; i < share->keys; i++, key_info++) {
    KEY_PART_INFO *key_part = key_info->key_part;
    bool found_primary = false;
    packet->append(STRING_WITH_LEN(",\n  "));

    if (i == primary_key && !strcmp(key_info->name, primary_key_name)) {
      found_primary = true;
      /*
        No space at end, because a space will be added after where the
        identifier would go, but that is not added for primary key.
      */
      packet->append(STRING_WITH_LEN("PRIMARY KEY"));
    } else if (key_info->flags & HA_NOSAME)
      packet->append(STRING_WITH_LEN("UNIQUE KEY "));
    else if (key_info->flags & HA_FULLTEXT)
      packet->append(STRING_WITH_LEN("FULLTEXT KEY "));
    else if (key_info->flags & HA_SPATIAL)
      packet->append(STRING_WITH_LEN("SPATIAL KEY "));
    else
      packet->append(STRING_WITH_LEN("KEY "));

    if (!found_primary)
      append_identifier(thd, packet, key_info->name, strlen(key_info->name));

    packet->append(STRING_WITH_LEN(" ("));

    for (uint j = 0; j < key_info->user_defined_key_parts; j++, key_part++) {
      if (j) packet->append(',');

      if (key_part->field) {
        // If this fields represents a functional index, print the expression
        // instead of the column name.
        if (key_part->field->is_field_for_functional_index()) {
          assert(key_part->field->gcol_info);

          StringBuffer<STRING_BUFFER_USUAL_SIZE> s;
          s.set_charset(system_charset_info);
          key_part->field->gcol_info->print_expr(thd, &s);
          packet->append("(");
          packet->append(s);
          packet->append(")");
        } else {
          append_identifier(thd, packet, key_part->field->field_name,
                            strlen(key_part->field->field_name));
        }
      }

      if (key_part->field &&
          (key_part->length !=
               table->field[key_part->fieldnr - 1]->key_length() &&
           !(key_info->flags & (HA_FULLTEXT | HA_SPATIAL)))) {
        packet->append_parenthesized((long)key_part->length /
                                     key_part->field->charset()->mbmaxlen);
      }
      if (key_part->key_part_flag & HA_REVERSE_SORT)
        packet->append(STRING_WITH_LEN(" DESC"));
    }
    packet->append(')');
    store_key_options(thd, packet, table, key_info);
    if (key_info->parser) {
      LEX_CSTRING *parser_name = plugin_name(key_info->parser);
      packet->append(STRING_WITH_LEN(" /*!50100 WITH PARSER "));
      append_identifier(thd, packet, parser_name->str, parser_name->length);
      packet->append(STRING_WITH_LEN(" */ "));
    }
  }

  // Append foreign key constraint definitions to the CREATE TABLE statement.
  print_foreign_key_info(thd, db, table_obj, packet);

  /*
    Append check constraints to the CREATE TABLE statement. All check
    constraints are listed in table check constraint form.
  */
  if (table->table_check_constraint_list != nullptr) {
    for (auto &cc : *table->table_check_constraint_list) {
      packet->append(STRING_WITH_LEN(",\n  CONSTRAINT "));
      append_identifier(thd, packet, cc.name().str, cc.name().length);

      packet->append(STRING_WITH_LEN(" CHECK ("));
      packet->append(cc.expr_str().str, cc.expr_str().length,
                     system_charset_info);
      packet->append(STRING_WITH_LEN(")"));

      /*
        If check constraint is not-enforced then it is listed with the comment
        "NOT ENFORCED".
      */
      if (!cc.is_enforced()) {
        packet->append(STRING_WITH_LEN(" /*!80016 NOT ENFORCED */"));
      }
    }
  }

  packet->append(STRING_WITH_LEN("\n)"));

  /**
    Append START TRANSACTION for CREATE SELECT on SE supporting atomic DDL.
    This is done only while binlogging CREATE TABLE AS SELECT.
  */
  if (!thd->lex->query_block->field_list_is_empty() &&
      (create_info_arg->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL)) {
    packet->append(STRING_WITH_LEN(" START TRANSACTION"));
  }

  bool show_tablespace = false;
  if (!foreign_db_mode) {
    show_table_options = true;

    // Show tablespace name only if it is explicitly provided by user.
    if (share->tmp_table) {
      // Innodb allows temporary tables in be in system temporary tablespace.
      show_tablespace = share->tablespace;
    } else if (share->tablespace && table_obj) {
      show_tablespace = table_obj->is_explicit_tablespace();
    }

    /* TABLESPACE and STORAGE */
    if (show_tablespace || share->default_storage_media != HA_SM_DEFAULT) {
      packet->append(STRING_WITH_LEN(" /*!50100"));
      if (show_tablespace) {
        packet->append(STRING_WITH_LEN(" TABLESPACE "));
        append_identifier(thd, packet, share->tablespace,
                          strlen(share->tablespace));
      }

      if (share->default_storage_media == HA_SM_DISK)
        packet->append(STRING_WITH_LEN(" STORAGE DISK"));
      if (share->default_storage_media == HA_SM_MEMORY)
        packet->append(STRING_WITH_LEN(" STORAGE MEMORY"));

      packet->append(STRING_WITH_LEN(" */"));
    }

    /* Get Autoextend_size attribute for file_per_table tablespaces. */

    ulonglong autoextend_size{};

    if (create_info_arg != nullptr) {
      if ((create_info_arg->used_fields & HA_CREATE_USED_AUTOEXTEND_SIZE) !=
          0) {
        autoextend_size =
            create_info_arg->m_implicit_tablespace_autoextend_size;
      }
    } else if (!share->tmp_table && table_obj &&
               table_obj->engine() == "InnoDB") {
      /* Get the AUTOEXTEND_SIZE if the tablespace is an implicit tablespace. */
      dd::get_implicit_tablespace_options(thd, table_obj, &autoextend_size);
    }

    /* Print autoextend_size attribute if it is set to a non-zero value */
    if (autoextend_size > 0) {
      char buf[std::numeric_limits<decltype(autoextend_size)>::digits10 + 2];
      const int len = snprintf(buf, sizeof(buf), "%llu", autoextend_size);
      assert(len < static_cast<int>(sizeof(buf)));
      packet->append(STRING_WITH_LEN(" /*!80023 AUTOEXTEND_SIZE="));
      packet->append(buf, len);
      packet->append(STRING_WITH_LEN(" */"));
    }

    /*
      IF   check_create_info
      THEN add ENGINE only if it was used when creating the table
    */
    if (!create_info_arg ||
        (create_info_arg->used_fields & HA_CREATE_USED_ENGINE)) {
      packet->append(STRING_WITH_LEN(" ENGINE="));
      /*
        TODO: Replace this if with the else branch. Not done yet since
        NDB handlerton says "ndbcluster" and ha_ndbcluster says "NDBCLUSTER".
      */
      if (table->part_info) {
        packet->append(ha_resolve_storage_engine_name(
            table->part_info->default_engine_type));
      } else {
        packet->append(file->table_type());
      }
    }

    /*
      Add AUTO_INCREMENT=... if there is an AUTO_INCREMENT column,
      and NEXT_ID > 1 (the default).  We must not print the clause
      for engines that do not support this as it would break the
      import of dumps, but as of this writing, the test for whether
      AUTO_INCREMENT columns are allowed and whether AUTO_INCREMENT=...
      is supported is identical, !(file->table_flags() & HA_NO_AUTO_INCREMENT))
      Because of that, we do not explicitly test for the feature,
      but may extrapolate its existence from that of an AUTO_INCREMENT column.

      If table has a generated invisible primary key and skip_gipk is set,
      then we should not print the AUTO_INCREMENT as AUTO_INCREMENT column
      (generated invisible primary key column) is skipped with this setting.
    */

    if (create_info.auto_increment_value > 1 && !skip_gipk) {
      char *end;
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT="));
      end = longlong10_to_str(create_info.auto_increment_value, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->table_charset) {
      /*
        IF   check_create_info
        THEN add DEFAULT CHARSET only if it was used when creating the table
      */
      if (!create_info_arg ||
          (create_info_arg->used_fields & HA_CREATE_USED_DEFAULT_CHARSET)) {
        packet->append(STRING_WITH_LEN(" DEFAULT CHARSET="));
        packet->append(share->table_charset->csname);
        if (!(share->table_charset->state & MY_CS_PRIMARY) ||
            share->table_charset == &my_charset_utf8mb4_0900_ai_ci) {
          packet->append(STRING_WITH_LEN(" COLLATE="));
          packet->append(table->s->table_charset->m_coll_name);
        }
      }
    }

    if (share->min_rows) {
      char *end;
      packet->append(STRING_WITH_LEN(" MIN_ROWS="));
      end = longlong10_to_str(share->min_rows, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->max_rows && !table_list->schema_table) {
      char *end;
      packet->append(STRING_WITH_LEN(" MAX_ROWS="));
      end = longlong10_to_str(share->max_rows, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->avg_row_length) {
      char *end;
      packet->append(STRING_WITH_LEN(" AVG_ROW_LENGTH="));
      end = longlong10_to_str(share->avg_row_length, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=1"));
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=0"));
    if (share->db_create_options & HA_OPTION_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=1"));
    if (share->db_create_options & HA_OPTION_NO_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=0"));
    if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_ON)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=1"));
    else if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_OFF)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=0"));
    if (share->stats_sample_pages != 0) {
      char *end;
      packet->append(STRING_WITH_LEN(" STATS_SAMPLE_PAGES="));
      end = longlong10_to_str(share->stats_sample_pages, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compatibility */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      packet->append(STRING_WITH_LEN(" CHECKSUM=1"));
    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      packet->append(STRING_WITH_LEN(" DELAY_KEY_WRITE=1"));

    /*
      If 'show_create_table_verbosity' is enabled, the row format would
      be displayed in the output of SHOW CREATE TABLE even if default
      row format is used. Otherwise only the explicitly mentioned
      row format would be displayed.
    */
    if (thd->variables.show_create_table_verbosity) {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint)share->real_row_type]);
    } else if (create_info.row_type != ROW_TYPE_DEFAULT) {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint)create_info.row_type]);
    }
    if (table->s->key_block_size) {
      char *end;
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end = longlong10_to_str(table->s->key_block_size, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    if (table->s->compress.length) {
      packet->append(STRING_WITH_LEN(" COMPRESSION="));
      append_unescaped(packet, share->compress.str, share->compress.length);
    }
    bool print_encryption = false;
    if (should_print_encryption_clause(thd, share, &print_encryption))
      return true;
    if (print_encryption) {
      /*
        Add versioned comment when there is TABLESPACE clause displayed and
        the table uses general tablespace.
      */
      bool uses_general_tablespace = false;
      if (table_obj)
        uses_general_tablespace =
            show_tablespace && dd::uses_general_tablespace(*table_obj);
      if (uses_general_tablespace) packet->append(STRING_WITH_LEN(" /*!80016"));

      packet->append(STRING_WITH_LEN(" ENCRYPTION="));
      if (share->encrypt_type.length) {
        append_unescaped(packet, share->encrypt_type.str,
                         share->encrypt_type.length);
      } else {
        /*
          We print ENCRYPTION='N' only in case user did not explicitly
          provide ENCRYPTION clause and schema has default_encryption 'Y'.
          In other words, if there is no ENCRYPTION clause supplied, then
          it is always unencrypted table. Server always maintains
          ENCRYPTION clause for encrypted tables, even if user did not
          supply the clause explicitly.
        */
        packet->append(STRING_WITH_LEN("\'N\'"));
      }

      if (uses_general_tablespace) packet->append(STRING_WITH_LEN(" */"));
    }
    table->file->append_create_info(packet);
    if (share->comment.length) {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, share->comment.str, share->comment.length);
    }
    if (share->connect_string.length) {
      packet->append(STRING_WITH_LEN(" CONNECTION="));
      append_unescaped(packet, share->connect_string.str,
                       share->connect_string.length);
    }
    if (share->has_secondary_engine() &&
        !thd->variables.show_create_table_skip_secondary_engine) {
      packet->append(" SECONDARY_ENGINE=");
      packet->append(share->secondary_engine.str,
                     share->secondary_engine.length);
    }

    if (share->engine_attribute.length) {
      packet->append(STRING_WITH_LEN(" /*!80021 ENGINE_ATTRIBUTE="));
      append_unescaped(packet, share->engine_attribute.str,
                       share->engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }
    if (share->secondary_engine_attribute.length) {
      packet->append(STRING_WITH_LEN(" /*!80021 SECONDARY_ENGINE_ATTRIBUTE="));
      // escape JSON
      append_unescaped(packet, share->secondary_engine_attribute.str,
                       share->secondary_engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }
    append_directory(thd, packet, "DATA", create_info.data_file_name);
    append_directory(thd, packet, "INDEX", create_info.index_file_name);
  }
  {
    if (table->part_info &&
        !(table->s->db_type()->partition_flags &&
          (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION) &&
          table->part_info->is_auto_partitioned)) {
      /*
        Partition syntax for CREATE TABLE is at the end of the syntax.
      */
      uint part_syntax_len;
      char *part_syntax;
      String comment_start;
      table->part_info->set_show_version_string(&comment_start);
      if ((part_syntax = generate_partition_syntax(
               table->part_info, &part_syntax_len, false, show_table_options,
               true,  // For proper quoting.
               comment_start.c_ptr()))) {
        packet->append(comment_start);
        if (packet->append(part_syntax, part_syntax_len) ||
            packet->append(STRING_WITH_LEN(" */")))
          error = true;
        my_free(part_syntax);
      }
    }
  }
  return error;
}

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info) {
  const bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;
  char *end, buff[32];

  if (!foreign_db_mode) {
    /*
      Send USING clause only if key algorithm was explicitly specified
      at the table creation time.
    */
    if (key_info->is_algorithm_explicit) {
      if (key_info->algorithm == HA_KEY_ALG_BTREE)
        packet->append(STRING_WITH_LEN(" USING BTREE"));

      if (key_info->algorithm == HA_KEY_ALG_HASH)
        packet->append(STRING_WITH_LEN(" USING HASH"));

      if (key_info->algorithm == HA_KEY_ALG_RTREE) {
        /* We should send USING only in non-default case: non-spatial rtree. */
        assert(!(key_info->flags & HA_SPATIAL));
        packet->append(STRING_WITH_LEN(" USING RTREE"));
      }
    }

    if ((key_info->flags & HA_USES_BLOCK_SIZE) &&
        table->s->key_block_size != key_info->block_size) {
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end = longlong10_to_str(key_info->block_size, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    assert(((key_info->flags & HA_USES_COMMENT) != 0) ==
           (key_info->comment.length > 0));
    if (key_info->flags & HA_USES_COMMENT) {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, key_info->comment.str, key_info->comment.length);
    }

    if (!key_info->is_visible)
      packet->append(STRING_WITH_LEN(" /*!80000 INVISIBLE */"));

    if (key_info->engine_attribute.length > 0) {
      packet->append(STRING_WITH_LEN(" /*!80021 ENGINE_ATTRIBUTE "));
      // escape JSON
      append_unescaped(packet, key_info->engine_attribute.str,
                       key_info->engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }

    if (key_info->secondary_engine_attribute.length > 0) {
      packet->append(STRING_WITH_LEN(" /*!80021 SECONDARY_ENGINE_ATTRIBUTE "));
      // escape JSON
      append_unescaped(packet, key_info->secondary_engine_attribute.str,
                       key_info->secondary_engine_attribute.length);
      packet->append(STRING_WITH_LEN(" */"));
    }
  }
}

void view_store_options(const THD *thd, Table_ref *table, String *buff) {
  append_algorithm(table, buff);
  append_definer(thd, buff, table->definer.user, table->definer.host);
  if (table->view_suid)
    buff->append(STRING_WITH_LEN("SQL SECURITY DEFINER "));
  else
    buff->append(STRING_WITH_LEN("SQL SECURITY INVOKER "));
}

/**
  Append ALGORITHM clause to the given buffer.

  @param table              VIEW definition
  @param [in,out] buff      buffer to hold ALGORITHM clause
*/

static void append_algorithm(Table_ref *table, String *buff) {
  buff->append(STRING_WITH_LEN("ALGORITHM="));
  switch ((int8)table->algorithm) {
    case VIEW_ALGORITHM_UNDEFINED:
      buff->append(STRING_WITH_LEN("UNDEFINED "));
      break;
    case VIEW_ALGORITHM_TEMPTABLE:
      buff->append(STRING_WITH_LEN("TEMPTABLE "));
      break;
    case VIEW_ALGORITHM_MERGE:
      buff->append(STRING_WITH_LEN("MERGE "));
      break;
    default:
      assert(0);  // never should happen
  }
}

/**
  Append DEFINER clause to the given buffer.

  @param thd           thread handle
  @param [in,out] buffer        buffer to hold DEFINER clause
  @param definer_user  user name part of definer
  @param definer_host  host name part of definer
*/

void append_definer(const THD *thd, String *buffer,
                    const LEX_CSTRING &definer_user,
                    const LEX_CSTRING &definer_host) {
  buffer->append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, buffer, definer_user.str, definer_user.length);
  buffer->append('@');
  append_identifier(thd, buffer, definer_host.str, definer_host.length);
  buffer->append(' ');
}

static void view_store_create_info(const THD *thd, Table_ref *table,
                                   String *buff) {
  const bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;

  // Print compact view name if the view belongs to the current database
  const bool compact_view_name =
      thd->db().str != nullptr && (!strcmp(thd->db().str, table->db));

  buff->append(STRING_WITH_LEN("CREATE "));
  if (!foreign_db_mode) {
    view_store_options(thd, table, buff);
  }
  buff->append(STRING_WITH_LEN("VIEW "));
  if (!compact_view_name) {
    append_identifier(thd, buff, table->db, table->db_length);
    buff->append('.');
  }
  append_identifier(thd, buff, table->table_name, table->table_name_length);
  print_derived_column_names(thd, buff, table->derived_column_names());

  buff->append(STRING_WITH_LEN(" AS "));

  /*
    We can't just use table->query, because our SQL_MODE may trigger
    a different syntax, like when ANSI_QUOTES is defined.

    Append the db name only if it is not the same as connection's
    database.
  */
  table->view_query()->unit->print(
      thd, buff,
      enum_query_type(QT_TO_ARGUMENT_CHARSET | QT_NO_DEFAULT_DB |
                      QT_HIDE_ROLLUP_FUNCTIONS));

  if (table->with_check != VIEW_CHECK_NONE) {
    if (table->with_check == VIEW_CHECK_LOCAL)
      buff->append(STRING_WITH_LEN(" WITH LOCAL CHECK OPTION"));
    else
      buff->append(STRING_WITH_LEN(" WITH CASCADED CHECK OPTION"));
  }
}

/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/
class thread_info {
 public:
  thread_info()
      : thread_id(0),
        start_time_in_secs(0),
        command(0),
        user(nullptr),
        host(nullptr),
        db(nullptr),
        proc_info(nullptr),
        state_info(nullptr) {}

  my_thread_id thread_id;
  time_t start_time_in_secs;
  uint command;
  const char *user, *host, *db, *proc_info, *state_info;
  CSET_STRING query_string;
};

// For sorting by thread_id.
class thread_info_compare {
 public:
  bool operator()(const thread_info *p1, const thread_info *p2) {
    return p1->thread_id < p2->thread_id;
  }
};

static const char *thread_state_info(THD *invoking_thd, THD *inspected_thd) {
  DBUG_TRACE;
  if (inspected_thd->get_protocol()->get_rw_status()) {
    if (inspected_thd->get_protocol()->get_rw_status() == 2)
      return "Sending to client";
    if (inspected_thd->get_command() == COM_SLEEP) return "";
    return "Receiving from client";
  } else {
    MUTEX_LOCK(lock, &inspected_thd->LOCK_current_cond);
    const char *proc_info = inspected_thd->proc_info_session(invoking_thd);
    if (proc_info) return proc_info;
    if (inspected_thd->current_cond.load()) return "Waiting on cond";
    return nullptr;
  }
}

/**
  This class implements callback function used by mysqld_list_processes() to
  list all the client process information.
*/
typedef Mem_root_array<thread_info *> Thread_info_array;
class List_process_list : public Do_THD_Impl {
 private:
  /* Username of connected client. */
  const char *m_user;
  Thread_info_array *m_thread_infos;
  /* THD of connected client. */
  THD *m_client_thd;
  size_t m_max_query_length;

 public:
  List_process_list(const char *user_value, Thread_info_array *thread_infos,
                    THD *thd_value, size_t max_query_length)
      : m_user(user_value),
        m_thread_infos(thread_infos),
        m_client_thd(thd_value),
        m_max_query_length(max_query_length) {
    push_deprecated_warn(m_client_thd, "INFORMATION_SCHEMA.PROCESSLIST",
                         "performance_schema.processlist");

    deprecated_use_i_s_processlist_last_timestamp = my_micro_time();
    deprecated_use_i_s_processlist_count++;
  }

  void operator()(THD *inspect_thd) override {
    DBUG_TRACE;

    thread_info *thd_info = nullptr;

    {
      MUTEX_LOCK(grd_secctx, &inspect_thd->LOCK_thd_security_ctx);

      Security_context *inspect_sctx = inspect_thd->security_context();

      const LEX_CSTRING inspect_sctx_user = inspect_sctx->user();
      const LEX_CSTRING inspect_sctx_host = inspect_sctx->host();
      const LEX_CSTRING inspect_sctx_host_or_ip = inspect_sctx->host_or_ip();

      /*
        Since we only access a cached value of connection_alive, which is
        also an atomic, we do not need to lock LOCK_thd_protocol here. We
        may get a value that is slightly outdated, but we will not get a crash
        due to reading invalid memory at least.
      */
      if (!inspect_thd->is_connected(true) ||
          (m_user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                      strcmp(inspect_sctx_user.str, m_user)))) {
        return;
      }

      DBUG_EXECUTE_IF(
          "enable_debug_sync_after_reading_sp_sctx",
          if (inspect_thd->sp_runtime_ctx != nullptr &&
              (strcmp(inspect_thd->sp_runtime_ctx->sp->m_name.str, "proc") ==
               0)) {
            DEBUG_SYNC(m_client_thd, "after_reading_security_context");
          });

      thd_info = new (m_client_thd->mem_root) thread_info;

      /* ID */
      thd_info->thread_id = inspect_thd->thread_id();

      /* USER */
      if (inspect_sctx_user.str)
        thd_info->user = m_client_thd->mem_strdup(inspect_sctx_user.str);
      else if (inspect_thd->system_thread)
        thd_info->user = "system user";
      else
        thd_info->user = "unauthenticated user";

      /* HOST */
      if (inspect_thd->peer_port &&
          (inspect_sctx_host.length || inspect_sctx->ip().length) &&
          m_client_thd->security_context()->host_or_ip().str[0]) {
        char *host =
            static_cast<char *>(m_client_thd->alloc(HOST_AND_PORT_LENGTH + 1));
        if (host)
          snprintf(host, HOST_AND_PORT_LENGTH + 1, "%s:%u",
                   inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
        thd_info->host = host;
      } else
        thd_info->host = m_client_thd->mem_strdup(
            inspect_sctx_host_or_ip.str[0] ? inspect_sctx_host_or_ip.str
            : inspect_sctx_host.length     ? inspect_sctx_host.str
                                           : "");
    }  // We've copied the security context, so release the lock.

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data", {
      if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
          inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
        DEBUG_SYNC(m_client_thd,
                   "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
    });
    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db = inspect_thd->db().str;
    if (db) thd_info->db = m_client_thd->mem_strdup(db);

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION)
      thd_info->proc_info = "Killed";
    thd_info->command = (int)inspect_thd->get_command();  // Used for !killed.

    /* STATE */
    thd_info->state_info = thread_state_info(m_client_thd, inspect_thd);

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str = nullptr;
      size_t query_length = 0;

      /* If a rewritten query exists, use that. */
      if (inspect_thd->rewritten_query().length() > 0) {
        query_length = inspect_thd->rewritten_query().length();
        query_str = inspect_thd->rewritten_query().ptr();
      }
      /*
        Otherwise, use the original query. If the query contains password in
        plain text, we have the query re-written immediately after parsing and
        password string is replaced. However, there is a unsafe window before
        rewrite is done and in such case we should not display the plain text
        password.
      */
      else if (inspect_thd->safe_to_display()) {
        query_length = inspect_thd->query().length;
        query_str = inspect_thd->query().str;
      }

      /* In the stand-alone server, add "PLUGIN" as needed. */
      String buf;
      if (inspect_thd->is_a_srv_session()) {
        buf.append(query_length ? "PLUGIN: " : "PLUGIN");

        if (query_length) buf.append(query_str, query_length);

        query_str = buf.c_ptr();
        query_length = buf.length();
      }
      /* No else. We need fall-through */
      /* If we managed to create query info, set a copy on thd_info. */
      if (query_str) {
        const size_t width = min<size_t>(m_max_query_length, query_length);
        const char *q = m_client_thd->strmake(query_str, width);
        /* Safety: in case strmake failed, we set length to 0. */
        thd_info->query_string =
            CSET_STRING(q, q ? width : 0, inspect_thd->charset());
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    thd_info->start_time_in_secs = inspect_thd->query_start_in_secs();

    m_thread_infos->push_back(thd_info);
  }
};

/**
  List running processes (actually connected sessions).

  @param thd        thread handle.
  @param user       Username of connected client.
  @param verbose    if false, limit output to PROCESS_LIST_WIDTH characters.
  @param has_cursor if true, called from a command object that handles
                    terminatation of sending to client, otherwise terminate
                    explicitly with my_eof() call.
*/

void mysqld_list_processes(THD *thd, const char *user, bool verbose,
                           bool has_cursor) {
  Item *field;
  mem_root_deque<Item *> field_list(thd->mem_root);
  Thread_info_array thread_infos(thd->mem_root);
  size_t max_query_length =
      (verbose ? thd->variables.max_allowed_packet : PROCESS_LIST_WIDTH);
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  field_list.push_back(
      new Item_int(NAME_STRING("Id"), 0, MY_INT64_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("User", USERNAME_CHAR_LENGTH));
  field_list.push_back(new Item_empty_string("Host", HOSTNAME_LENGTH));
  field_list.push_back(field = new Item_empty_string("db", NAME_CHAR_LEN));
  field->set_nullable(true);
  field_list.push_back(new Item_empty_string("Command", 16));
  field_list.push_back(field = new Item_return_int("Time", 7, MYSQL_TYPE_LONG));
  field->unsigned_flag = false;
  field_list.push_back(field = new Item_empty_string("State", 30));
  field->set_nullable(true);
  field_list.push_back(field = new Item_empty_string("Info", max_query_length));
  field->set_nullable(true);
  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return;

  if (!thd->killed) {
    thread_infos.reserve(Global_THD_manager::get_instance()->get_thd_count());
    List_process_list list_process_list(user, &thread_infos, thd,
                                        max_query_length);
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&list_process_list);
  }

  // Return list sorted by thread_id.
  std::sort(thread_infos.begin(), thread_infos.end(), thread_info_compare());

  const time_t now = time(nullptr);
  for (size_t ix = 0; ix < thread_infos.size(); ++ix) {
    thread_info *thd_info = thread_infos.at(ix);
    protocol->start_row();
    protocol->store((ulonglong)thd_info->thread_id);
    protocol->store(thd_info->user, system_charset_info);
    protocol->store(thd_info->host, system_charset_info);
    protocol->store(thd_info->db, system_charset_info);
    if (thd_info->proc_info)
      protocol->store(thd_info->proc_info, system_charset_info);
    else
      protocol->store(Command_names::str_session(thd_info->command).c_str(),
                      system_charset_info);
    if (thd_info->start_time_in_secs)
      protocol->store_long((longlong)(now - thd_info->start_time_in_secs));
    else
      protocol->store_null();
    protocol->store(thd_info->state_info, system_charset_info);
    protocol->store(thd_info->query_string.str(),
                    thd_info->query_string.charset());
    if (protocol->end_row()) break; /* purecov: inspected */
  }
  /*
    "show" commands that are implemented as subclass of Sql_cmd_show_noplan
    usually call my_eof() directly, as they don't have a cursor implementation.
    However, SHOW PROCESSLIST has one implementation using PFS that uses
    a regular join plan, and another that calls this function, so inherits
    from Sql_cmd_show.
    The following code is necessary to prevent my_eof() from being called twice
    when this function is called as part of Sql_cmd execution.
  */
  if (has_cursor)
    thd->lex->unit->query_result()->send_eof(thd);
  else
    my_eof(thd);
}

/**
  This class implements callback function used by fill_schema_processlist()
  to populate all the client process information into I_S table.
*/
class Fill_process_list : public Do_THD_Impl {
 private:
  /* THD of connected client. */
  THD *m_client_thd;
  /* Information of each process is added as records into this table. */
  Table_ref *m_tables;

 public:
  Fill_process_list(THD *thd_value, Table_ref *tables_value)
      : m_client_thd(thd_value), m_tables(tables_value) {
    push_deprecated_warn(m_client_thd, "INFORMATION_SCHEMA.PROCESSLIST",
                         "performance_schema.processlist");

    deprecated_use_i_s_processlist_last_timestamp = my_micro_time();
    deprecated_use_i_s_processlist_count++;
  }

  ~Fill_process_list() override {
    DBUG_EXECUTE_IF("test_fill_proc_with_x_root",
                    { DEBUG_SYNC(m_client_thd, "fill_proc_list_ended"); });
  }

  void operator()(THD *inspect_thd) override {
    DBUG_TRACE;

    TABLE *table;
    const char *val = nullptr;

    {
      MUTEX_LOCK(grd_secctx, &inspect_thd->LOCK_thd_security_ctx);

      Security_context *inspect_sctx = inspect_thd->security_context();

      const LEX_CSTRING inspect_sctx_user = inspect_sctx->user();
      const LEX_CSTRING inspect_sctx_host = inspect_sctx->host();
      const LEX_CSTRING inspect_sctx_host_or_ip = inspect_sctx->host_or_ip();

      const char *client_priv_user =
          m_client_thd->security_context()->priv_user().str;
      const char *user =
          m_client_thd->security_context()->check_access(PROCESS_ACL)
              ? NullS
              : client_priv_user;

      /*
        Since we only access a cached value of connection_alive, which is
        also an atomic, we do not need to lock LOCK_thd_protocol here. We
        may get a value that is slightly outdated, but we will not get a crash
        due to reading invalid memory at least.
      */
      if (!inspect_thd->is_connected(true) ||
          (user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                    strcmp(inspect_sctx_user.str, user)))) {
        return;
      }

      DBUG_EXECUTE_IF(
          "enable_debug_sync_after_reading_sp_sctx",
          if (inspect_thd->sp_runtime_ctx != nullptr &&
              (strcmp(inspect_thd->sp_runtime_ctx->sp->m_name.str, "proc") ==
               0)) {
            DEBUG_SYNC(m_client_thd, "after_reading_security_context");
          });

      DBUG_EXECUTE_IF(
          "test_fill_proc_with_x_root",
          if (0 == strcmp(inspect_sctx_user.str, "x_root")) {
            DEBUG_SYNC(m_client_thd, "fill_proc_list_with_x_root");
          });

      table = m_tables->table;
      restore_record(table, s->default_values);

      /* ID */
      table->field[0]->store((ulonglong)inspect_thd->thread_id(), true);

      /* USER */
      if (inspect_sctx_user.str)
        val = inspect_sctx_user.str;
      else if (inspect_thd->system_thread)
        val = "system user";
      else
        val = "unauthenticated user";
      table->field[1]->store(val, strlen(val), system_charset_info);

      /* HOST */
      if (inspect_thd->peer_port &&
          (inspect_sctx_host.length || inspect_sctx->ip().length) &&
          m_client_thd->security_context()->host_or_ip().str[0]) {
        char host[HOST_AND_PORT_LENGTH + 1];
        snprintf(host, HOST_AND_PORT_LENGTH + 1, "%s:%u",
                 inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
        table->field[2]->store(host, strlen(host), system_charset_info);
      } else
        table->field[2]->store(inspect_sctx_host_or_ip.str,
                               inspect_sctx_host_or_ip.length,
                               system_charset_info);
    }  // We've copied the security context, so release the lock.

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data", {
      if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
          inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
        DEBUG_SYNC(m_client_thd,
                   "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
    });

    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db = inspect_thd->db().str;
    if (db) {
      table->field[3]->store(db, strlen(db), system_charset_info);
      table->field[3]->set_notnull();
    }

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION) {
      val = "Killed";
      table->field[4]->store(val, strlen(val), system_charset_info);
    } else {
      const std::string &cn =
          Command_names::str_session(inspect_thd->get_command());
      table->field[4]->store(cn.c_str(), cn.length(), system_charset_info);
    }

    /* STATE */
    val = thread_state_info(m_client_thd, inspect_thd);
    if (val) {
      table->field[6]->store(val, strlen(val), system_charset_info);
      table->field[6]->set_notnull();
    }

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str = nullptr;
      size_t query_length = 0;

      /* If a rewritten query exists, use that. */
      if (inspect_thd->rewritten_query().length() > 0) {
        query_length = inspect_thd->rewritten_query().length();
        query_str = inspect_thd->rewritten_query().ptr();
      }
      /*
        Otherwise, use the original query. If the query contains password in
        plain text, we have the query re-written immediately after parsing and
        password string is replaced. However, there is a unsafe window before
        rewrite is done and in such case we should not display the plain text
        password.
      */
      else if (inspect_thd->safe_to_display()) {
        query_length = inspect_thd->query().length;
        query_str = inspect_thd->query().str;
      }

      /* In the stand-alone server, add "PLUGIN" as needed. */
      String buf;
      if (inspect_thd->is_a_srv_session()) {
        buf.append(query_length ? "PLUGIN: " : "PLUGIN");

        if (query_length) buf.append(query_str, query_length);

        query_str = buf.c_ptr();
        query_length = buf.length();
      }
      /* No else. We need fall-through */
      /* If we managed to create query info, set a copy on thd_info. */
      if (query_str) {
        const size_t width = min<size_t>(PROCESS_LIST_INFO_WIDTH, query_length);
        table->field[7]->store(query_str, width, inspect_thd->charset());
        table->field[7]->set_notnull();
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    if (inspect_thd->query_start_in_secs())
      table->field[5]->store((longlong)(my_micro_time() / 1000000 -
                                        inspect_thd->query_start_in_secs()),
                             false);
    else
      table->field[5]->store(0, false);

    schema_table_store_record(m_client_thd, table);
  }
};

static int fill_schema_processlist(THD *thd, Table_ref *tables, Item *) {
  DBUG_TRACE;

  Fill_process_list fill_process_list(thd, tables);
  if (!thd->killed) {
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&fill_process_list);
  }
  return 0;
}

/*****************************************************************************
  Status functions
*****************************************************************************/
// TODO: allocator based on my_malloc.
typedef std::vector<SHOW_VAR> Status_var_array;

Status_var_array all_status_vars(0);
bool status_vars_inited = false;
/* Version counter, protected by LOCK_STATUS. */
ulonglong status_var_array_version = 0;

static inline int show_var_cmp(const SHOW_VAR *var1, const SHOW_VAR *var2) {
  return strcmp(var1->name, var2->name);
}

class Show_var_cmp {
 public:
  bool operator()(const SHOW_VAR &var1, const SHOW_VAR &var2) {
    return show_var_cmp(&var1, &var2) < 0;
  }
};

static inline bool is_show_undef(const SHOW_VAR &var) {
  return var.type == SHOW_UNDEF;
}

/*
  Deletes all the SHOW_UNDEF elements from the array.
  Shrinks array capacity to zero if it is completely empty.
*/
static void shrink_var_array(Status_var_array *array) {
  /* remove_if maintains order for the elements that are *not* removed */
  array->erase(std::remove_if(array->begin(), array->end(), is_show_undef),
               array->end());
  if (array->empty()) Status_var_array().swap(*array);
}

/*
  Adds an array of SHOW_VAR entries to the output of SHOW STATUS

  SYNOPSIS
    add_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to add to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    The handling of all_status_vars[] is completely internal, it's allocated
    automatically when something is added to it, and deleted completely when
    the last entry is removed.

    As a special optimization, if add_status_vars() is called before
    init_status_vars(), it assumes "startup mode" - neither concurrent access
    to the array nor SHOW STATUS are possible (thus it skips locks and sort)

    The last entry of the all_status_vars[] should always be {0,0,SHOW_UNDEF}
*/
bool add_status_vars(const SHOW_VAR *list) {
  MUTEX_LOCK(lock, status_vars_inited ? &LOCK_status : nullptr);

  try {
    while (list->name) all_status_vars.push_back(*list++);
  } catch (const std::bad_alloc &) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(Status_var_array::value_type)));
    return true;
  }

  if (status_vars_inited)
    std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());

  status_var_array_version++;
  return false;
}

/*
  Make all_status_vars[] usable for SHOW STATUS

  NOTE
    See add_status_vars(). Before init_status_vars() call, add_status_vars()
    works in a special fast "startup" mode. Thus init_status_vars()
    should be called as late as possible but before enabling multi-threading.
*/
void init_status_vars() {
  status_vars_inited = true;
  std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());
  status_var_array_version++;
}

void reset_status_vars() {
  Status_var_array::iterator ptr = all_status_vars.begin();
  Status_var_array::iterator last = all_status_vars.end();
  for (; ptr < last; ptr++) {
    /* Note that SHOW_LONG_NOFLUSH variables are not reset */
    if (ptr->type == SHOW_LONG || ptr->type == SHOW_SIGNED_LONG)
      *(ulong *)ptr->value = 0;
  }
}

/*
  Current version of the all_status_vars.
*/
ulonglong get_status_vars_version(void) { return (status_var_array_version); }

/*
  catch-all cleanup function, cleans up everything no matter what

  DESCRIPTION
    This function is not strictly required if all add_to_status/
    remove_status_vars are properly paired, but it's a safety measure that
    deletes everything from the all_status_vars[] even if some
    remove_status_vars were forgotten
*/
void free_status_vars() {
  Status_var_array().swap(all_status_vars);
  status_var_array_version++;
}

/**
  Search for a status variable and gets its string value

  Check if the supplied @ref SHOW_VAR contains the
  status variable, diving into @ref SHOW_ARRAY arrays
  and evaluating @ref SHOW_FUNC functions as needed.
  Can be called recursively.
  If a variable is found its string value is produced
  by calling @ref get_one_variable.

  Used in conjunction with @ref get_recursive_status_var

  @brief           Get the value of given status variable

  @param[in]       thd        thread handler
  @param[in]       list       list of SHOW_VAR objects in which function should
                              search
  @param[in]       is_a_list  true if it's a list array that needs to be
  iterated
  @param[in]       name       name of the status variable
  @param[in]       var_type   Variable type
  @param[in,out]   value      buffer in which value of the status variable
                              needs to be filled in
  @param[in,out]   length     filled with buffer length
  @param[out]      charset    charset of the data returned

  @return          status
    @retval        false      if variable is not found in the list
    @retval        true       if variable is found in the list
*/
static bool get_recursive_status_var_inner(THD *thd, SHOW_VAR *list,
                                           bool is_a_list, const char *name,
                                           char *const value,
                                           enum_var_type var_type,
                                           size_t *length,
                                           const CHARSET_INFO **charset) {
  for (; list->name; list++) {
    const int res = strcmp(list->name, name);
    if (res == 0) {
      /*
        if var->type is SHOW_FUNC, call the function.
        Repeat as necessary, if new var is again SHOW_FUNC
       */
      SHOW_VAR tmp;
      for (; list->type == SHOW_FUNC; list = &tmp)
        ((mysql_show_var_func)(list->value))(thd, &tmp, value);

      const char *ret = get_one_variable(
          thd, list, var_type, list->type,
          var_type == OPT_SESSION ? &thd->status_var : &global_status_var,
          charset, value, length);
      if (ret != value && *length != 0) memcpy(value, ret, *length + 1);
      return true;
    } else if (list->type == SHOW_ARRAY && list->name &&
               !strncmp(list->name, name, strlen(list->name))) {
      // the variable name matches an array prefix: dive into it and skip the
      // prefix and the underscore
      if (get_recursive_status_var_inner(thd, (SHOW_VAR *)list->value, true,
                                         name + strlen(list->name) + 1, value,
                                         var_type, length, charset))
        return true;
    }
    if (!is_a_list) break;
  }
  return false;
}

/**
  Get the string value of a status variable. A top level API

  Takes the @ref LOCK_status lock and iterates over the
  registered status variable array @ref all_status_vars
  to evaluate the value of a named status variable by calling
  @ref get_recursive_status_var_inner for each element in
  @ref all_status_vars.

  @brief           Get the value of given status variable

  @param[in]       thd        thread handler
  @param[in]       name       name of the status variable
  @param[in]       var_type   Variable type
  @param[in,out]   value      buffer in which value of the status variable
                              needs to be filled in
  @param[in,out]   length     filled with buffer length
  @param[out]      charset    charset of the data returned

  @return          status
    @retval        false      if variable is not found in the list
    @retval        true       if variable is found in the list
*/
bool get_recursive_status_var(THD *thd, const char *name, char *const value,
                              enum_var_type var_type, size_t *length,
                              const CHARSET_INFO **charset) {
  MUTEX_LOCK(lock, status_vars_inited ? &LOCK_status : nullptr);
  for (SHOW_VAR var : all_status_vars) {
    if (get_recursive_status_var_inner(thd, &var, false, name, value, var_type,
                                       length, charset))
      return true;
  }
  return false;
}

/*
  Removes an array of SHOW_VAR entries from the output of SHOW STATUS

  SYNOPSIS
    remove_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to remove to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    there's lots of room for optimizing this, especially in non-sorted mode,
    but nobody cares - it may be called only in case of failed plugin
    initialization in the mysqld startup.
*/

void remove_status_vars(SHOW_VAR *list) {
  if (status_vars_inited) {
    mysql_mutex_lock(&LOCK_status);
    size_t a = 0, b = all_status_vars.size(), c = (a + b) / 2;

    for (; list->name; list++) {
      int res = 0;
      for (a = 0, b = all_status_vars.size(); b - a > 1; c = (a + b) / 2) {
        res = show_var_cmp(list, &all_status_vars[c]);
        if (res < 0)
          b = c;
        else if (res > 0)
          a = c;
        else
          break;
      }
      if (res == 0) all_status_vars[c].type = SHOW_UNDEF;
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
    mysql_mutex_unlock(&LOCK_status);
  } else {
    uint i;
    for (; list->name; list++) {
      for (i = 0; i < all_status_vars.size(); i++) {
        if (show_var_cmp(list, &all_status_vars[i])) continue;
        all_status_vars[i].type = SHOW_UNDEF;
        break;
      }
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
  }
}

/**
  Returns the value of a system or a status variable.

  @param thd            The handle of the current THD.
  @param variable       Details of the variable.
  @param value_type     Variable type.
  @param show_type      Variable show type.
  @param status_var     Status values or NULL if for system variable.
  @param [out] charset  Character set of the value.
  @param [in,out] buff  Buffer to store the value.
  @param [out] length   Length of the value.
  @param [out] is_null  Is variable value NULL or not.

  @returns              Pointer to the value buffer.
*/

const char *get_one_variable(THD *thd, const SHOW_VAR *variable,
                             enum_var_type value_type, SHOW_TYPE show_type,
                             System_status_var *status_var,
                             const CHARSET_INFO **charset, char *buff,
                             size_t *length, bool *is_null) {
  return get_one_variable_ext(thd, thd, variable, value_type, show_type,
                              status_var, charset, buff, length, is_null);
}

/**
  @brief Returns the value of a system or a status variable.

  @param running_thd     The handle of the current THD.
  @param target_thd      The handle of the remote THD.
  @param variable        Details of the variable.
  @param value_type      Variable type.
  @param show_type       Variable show type.
  @param status_var      Status values or NULL if for system variable.
  @param [out] charset   Character set of the value.
  @param [in,out] buff   Buffer to store the value.
  @param [out] length    Length of the value.
  @param [out] is_null   Is variable value NULL or not. This parameter is set
                         only for variables of string type.

  @returns               Pointer to the value buffer.
*/

const char *get_one_variable_ext(THD *running_thd, THD *target_thd,
                                 const SHOW_VAR *variable,
                                 enum_var_type value_type, SHOW_TYPE show_type,
                                 System_status_var *status_var,
                                 const CHARSET_INFO **charset, char *buff,
                                 size_t *length, bool *is_null) {
  const char *value;
  const CHARSET_INFO *value_charset;

  if (show_type == SHOW_SYS) {
    sys_var *var = ((sys_var *)variable->value);
    show_type = var->show_type();
    value = pointer_cast<const char *>(
        var->value_ptr(running_thd, target_thd, value_type, {}));
    value_charset = var->charset(target_thd);
  } else {
    value = variable->value;
    value_charset = system_charset_info;
  }

  const char *pos = buff;
  const char *end = buff;

  /*
    Note that value may == buff. All SHOW_xxx code below should still work.
  */
  switch (show_type) {
    case SHOW_DOUBLE_STATUS:
      value = (char *)status_var + reinterpret_cast<size_t>(value);
      /* 6 is the default precision for '%f' in sprintf() */
      end = buff +
            my_fcvt(*pointer_cast<const double *>(value), 6, buff, nullptr);
      value_charset = system_charset_info;
      break;

    case SHOW_DOUBLE:
      /* 6 is the default precision for '%f' in sprintf() */
      end = buff +
            my_fcvt(*pointer_cast<const double *>(value), 6, buff, nullptr);
      value_charset = system_charset_info;
      break;

    case SHOW_LONG_STATUS:
      value = (char *)status_var + reinterpret_cast<size_t>(value);
      end = longlong10_to_str(*pointer_cast<const ulong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONG:
      /* the difference lies in refresh_status() */
    case SHOW_LONG_NOFLUSH:
      end = longlong10_to_str(*pointer_cast<const ulong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_SIGNED_LONG:
      end = longlong10_to_str(*pointer_cast<const long *>(value), buff, -10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONGLONG_STATUS:
      value = (char *)status_var + reinterpret_cast<size_t>(value);
      end = longlong10_to_str(*pointer_cast<const longlong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONGLONG:
      end = longlong10_to_str(*pointer_cast<const longlong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_SIGNED_LONGLONG:
      end =
          longlong10_to_str(*pointer_cast<const longlong *>(value), buff, -10);
      value_charset = system_charset_info;
      break;

    case SHOW_HA_ROWS:
      end = longlong10_to_str(
          static_cast<longlong>(*pointer_cast<const ha_rows *>(value)), buff,
          10);
      value_charset = system_charset_info;
      break;

    case SHOW_BOOL:
      end = my_stpcpy(buff, *pointer_cast<const bool *>(value) ? "ON" : "OFF");
      value_charset = system_charset_info;
      break;

    case SHOW_MY_BOOL:
      end = my_stpcpy(buff, *pointer_cast<const bool *>(value) ? "ON" : "OFF");
      value_charset = system_charset_info;
      break;

    case SHOW_INT:
      end = longlong10_to_str(*pointer_cast<const uint32 *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_SIGNED_INT:
      end = longlong10_to_str(*pointer_cast<const int32 *>(value), buff, -10);
      value_charset = system_charset_info;
      break;

    case SHOW_HAVE: {
      SHOW_COMP_OPTION tmp = *pointer_cast<const SHOW_COMP_OPTION *>(value);
      pos = show_comp_option_name[(int)tmp];
      end = strend(pos);
      value_charset = system_charset_info;
      break;
    }

    case SHOW_CHAR: {
      if (!(pos = value)) pos = "";
      end = strend(pos);
      break;
    }

    case SHOW_CHAR_PTR: {
      if (!(pos = *pointer_cast<char *const *>(value))) {
        pos = "";
        if (is_null) *is_null = true;
      } else {
        if (is_null) *is_null = false;
      }
      end = strend(pos);
      break;
    }

    case SHOW_LEX_STRING: {
      const LEX_STRING *ls = pointer_cast<const LEX_STRING *>(value);
      if (!(pos = ls->str))
        end = pos = "";
      else
        end = pos + ls->length;
      break;
    }

    case SHOW_KEY_CACHE_LONG:
      value = (char *)dflt_key_cache + reinterpret_cast<size_t>(value);
      end = longlong10_to_str(*pointer_cast<const ulong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_KEY_CACHE_LONGLONG:
      value = (char *)dflt_key_cache + reinterpret_cast<size_t>(value);
      end = longlong10_to_str(*pointer_cast<const longlong *>(value), buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_UNDEF:
      break; /* Return empty string */

    case SHOW_SYS: /* Cannot happen */

    default:
      assert(0);
      break;
  }

  *length = (size_t)(end - pos);
  /* Some callers do not use the result. */
  if (charset != nullptr) {
    assert(value_charset != nullptr);
    *charset = value_charset;
  }
  return pos;
}

/**
  Collect status for all running threads.
*/
class Add_status : public Do_THD_Impl {
 public:
  Add_status(System_status_var *value) : m_stat_var(value) {}
  void operator()(THD *thd) override {
    if (!thd->status_var_aggregated)
      add_to_status(m_stat_var, &thd->status_var);
  }

 private:
  /* Status of all threads are summed into this. */
  System_status_var *m_stat_var;
};

void calc_sum_of_all_status(System_status_var *to) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_status);
  /* Get global values as base. */
  *to = global_status_var;
  Add_status add_status(to);
  Global_THD_manager::get_instance()->do_for_all_thd_copy(&add_status);
}

/* This is only used internally, but we need it here as a forward reference */
extern ST_SCHEMA_TABLE schema_tables[];

/*
  Store record to I_S table, convert HEAP table
  to MyISAM if necessary

  SYNOPSIS
    schema_table_store_record()
    thd                   thread handler
    table                 Information schema table to be updated

  RETURN
    0	                  success
    1	                  error
*/

bool schema_table_store_record(THD *thd, TABLE *table) {
  int error;
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (create_ondisk_from_heap(thd, table, error, /*insert_last_record=*/true,
                                /*ignore_last_dup=*/false,
                                /*is_duplicate=*/nullptr))
      return true;
  }
  return false;
}

/**
  Store record to I_S table, convert HEAP table to InnoDB table if necessary.

  @param[in]  thd            thread handler
  @param[in]  table          Information schema table to be updated
  @param[in]  make_ondisk    if true, convert heap table to on disk table.
                             default value is true.
  @return 0 on success
  @return error code on failure.
*/
int schema_table_store_record2(THD *thd, TABLE *table, bool make_ondisk) {
  int error;
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (!make_ondisk) return error;

    if (convert_heap_table_to_ondisk(thd, table, error)) return 1;
  }
  return 0;
}

/**
  Convert HEAP table to InnoDB table if necessary

  @param[in] thd     thread handler
  @param[in] table   Information schema table to be converted.
  @param[in] error   the error code returned previously.
  @return false on success, true on error.
*/
bool convert_heap_table_to_ondisk(THD *thd, TABLE *table, int error) {
  return (create_ondisk_from_heap(
      thd, table, error, /*insert_last_record=*/true,
      /*ignore_last_dup=*/false, /*is_duplicate=*/nullptr));
}

/**
  Prepare a Table_ident and add a table_list into Query_block

  @param thd         Thread
  @param sel         Instance of Query_block.
  @param db_name     Database name.
  @param table_name  Table name.

  @returns true on failure.
           false on success.
*/
bool make_table_list(THD *thd, Query_block *sel, const LEX_CSTRING &db_name,
                     const LEX_CSTRING &table_name) {
  Table_ident *table_ident = new (thd->mem_root)
      Table_ident(thd->get_protocol(), db_name, table_name, true);
  if (!sel->add_table_to_list(thd, table_ident, nullptr, 0, TL_READ,
                              MDL_SHARED_READ))
    return true;
  return false;
}

enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table) {
  return (enum enum_schema_tables)(schema_table - &schema_tables[0]);
}

/**
 * Implementation of SHOW INDEX / SHOW COLUMNS for temporary tables.
 *
 * @param thd           Thread handle
 * @param tables        Table to fill with data
 */

static int show_temporary_tables(THD *thd, Table_ref *tables, Item *) {
  TABLE *table = tables->table;
  Query_block *lsel = tables->schema_query_block;
  ST_SCHEMA_TABLE *schema_table = tables->schema_table;

  DBUG_TRACE;

  /*
    This code is now only used for SHOW statements for temporary tables
    and not for any I_S queries.
  */
  assert(thd->lex->sql_command == SQLCOM_SHOW_KEYS ||
         thd->lex->sql_command == SQLCOM_SHOW_FIELDS);
  assert(lsel && lsel->get_table_list());

  /*
    In cases when SELECT from I_S table being filled by this call is
    part of statement which also uses other tables or is being executed
    under LOCK TABLES or is part of transaction which also uses other
    tables waiting for metadata locks which happens below might result
    in deadlocks.
    To avoid them we don't wait if conflicting metadata lock is
    encountered and skip table with emitting an appropriate warning.
  */
  bool can_deadlock = thd->mdl_context.has_locks();

  /*
    We should not introduce deadlocks even if we already have some
    tables open and locked, since we won't lock tables which we will
    open and will ignore pending exclusive metadata locks for these
    tables by using high-priority requests for shared metadata locks.
  */
  Open_tables_backup open_tables_state_backup;
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup, 0);

  /*
    When a view is opened its structures are allocated on a permanent
    statement arena and linked into the LEX tree for the current statement
    (this happens even in cases when view is handled through TEMPTABLE
    algorithm).

    To prevent this process from unnecessary hogging of memory in the permanent
    arena of our I_S query and to avoid damaging its LEX we use temporary
    arena and LEX for table/view opening.

    Use temporary arena instead of statement permanent arena. Also make
    it active arena and save original one for successive restoring.
  */
  Query_arena i_s_arena(thd->mem_root, Query_arena::STMT_REGULAR_EXECUTION);
  Query_arena *old_arena = thd->stmt_arena;
  thd->stmt_arena = &i_s_arena;
  Query_arena backup_arena;
  thd->swap_query_arena(i_s_arena, &backup_arena);

  /* Prepare temporary LEX. */
  LEX temp_lex;
  LEX *lex = &temp_lex;
  LEX *old_lex = thd->lex;
  thd->lex = lex;
  lex_start(thd);

  /* Disable constant subquery evaluation as we won't be locking tables. */
  lex->context_analysis_only = CONTEXT_ANALYSIS_ONLY_VIEW;

  /*
    Some of process_table() functions rely on wildcard being passed from
    old LEX (or at least being initialized).
  */
  lex->wild = old_lex->wild;

  Table_ref *table_list;
  bool result = true;

  /*
    Since make_table_list() might change database and table name passed
    to it we create copies of the db and table name.
    These copies are used for make_table_list() while unaltered values
    are passed to process_table() functions.
  */
  LEX_CSTRING db_name_lex_cstr, table_name_lex_cstr;
  if (lex_string_strmake(thd->mem_root, &db_name_lex_cstr,
                         lsel->get_table_list()->db,
                         lsel->get_table_list()->db_length) ||
      lex_string_strmake(thd->mem_root, &table_name_lex_cstr,
                         lsel->get_table_list()->table_name,
                         lsel->get_table_list()->table_name_length))
    goto end;

  /*
    Create table list element for table to be open. Link it with the
    temporary LEX. The latter is required to correctly open views and
    produce table describing their structure.
  */
  if (make_table_list(thd, lex->query_block, db_name_lex_cstr,
                      table_name_lex_cstr))
    goto end;

  table_list = lex->query_block->get_table_list();
  assert(!table_list->is_view_or_derived());

  /*
    Restore thd->temporary_tables to be able to process
    temporary tables (only for 'show index' & 'show columns').
  */
  thd->temporary_tables = open_tables_state_backup.temporary_tables;

  result = open_temporary_tables(thd, table_list);

  if (!result)
    result = open_tables_for_query(
        thd, table_list,
        MYSQL_OPEN_IGNORE_FLUSH | MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL |
            (can_deadlock ? MYSQL_OPEN_FAIL_ON_MDL_CONFLICT : 0));

  /*
    Restore old value of sql_command back as it is being looked at in
    process_table() function.
  */
  lex->sql_command = old_lex->sql_command;

  if (!result) {
    const LEX_CSTRING orig_db_name{lsel->get_table_list()->db,
                                   lsel->get_table_list()->db_length};

    const LEX_CSTRING orig_table_name{
        lsel->get_table_list()->table_name,
        lsel->get_table_list()->table_name_length};

    result = schema_table->process_table(thd, table_list, table, result,
                                         orig_db_name, orig_table_name);
  }

end:
  lex->cleanup(true);

  /* Restore original LEX value, statement's arena and THD arena values. */
  lex_end(thd->lex);

  // Free items, before restoring backup_arena below.
  assert(i_s_arena.item_list() == nullptr);
  thd->free_items();

  /*
    For safety reset list of open temporary tables before closing
    all tables open within this Open_tables_state.
  */
  thd->temporary_tables = nullptr;
  close_thread_tables(thd);

  thd->lex = old_lex;

  thd->stmt_arena = old_arena;
  thd->swap_query_arena(backup_arena, &i_s_arena);

  thd->restore_backup_open_tables_state(&open_tables_state_backup);

  return result;
}

/* Define fields' indexes for COLUMNS of temporary tables */
#define TMP_TABLE_COLUMNS_COLUMN_NAME 0
#define TMP_TABLE_COLUMNS_COLUMN_TYPE 1
#define TMP_TABLE_COLUMNS_COLLATION_NAME 2
#define TMP_TABLE_COLUMNS_IS_NULLABLE 3
#define TMP_TABLE_COLUMNS_COLUMN_KEY 4
#define TMP_TABLE_COLUMNS_COLUMN_DEFAULT 5
#define TMP_TABLE_COLUMNS_EXTRA 6
#define TMP_TABLE_COLUMNS_PRIVILEGES 7
#define TMP_TABLE_COLUMNS_COLUMN_COMMENT 8
#define TMP_TABLE_COLUMNS_GENERATION_EXPRESSION 9

static int get_schema_tmp_table_columns_record(THD *thd, Table_ref *tables,
                                               TABLE *table, bool res,
                                               LEX_CSTRING db_name,
                                               LEX_CSTRING table_name) {
  DBUG_TRACE;

  assert(thd->lex->sql_command == SQLCOM_SHOW_FIELDS);

  if (res) return res;

  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : nullptr;
  CHARSET_INFO *cs = system_charset_info;
  TABLE *show_table = tables->table;
  Field **ptr = show_table->field;
  Field *field;
  show_table->use_all_columns();  // Required for default
  restore_record(show_table, s->default_values);

  /*
    If a primary key is generated for the table then hide definition of a
    generated invisible primary key when system variable
    show_gipk_in_create_table_and_information_schema is set to OFF.
  */
  if (table_has_generated_invisible_primary_key(show_table) &&
      !thd->variables.show_gipk_in_create_table_and_information_schema) {
    /*
      Generated invisible primary key column is placed at the first position.
      So skip first column.
    */
    assert(is_generated_invisible_primary_key_column_name((*ptr)->field_name));
    ptr++;
  }

  for (; (field = *ptr); ptr++) {
    const uchar *pos;
    char tmp[MAX_FIELD_WIDTH];
    String type(tmp, sizeof(tmp), system_charset_info);

    if (wild && wild[0] &&
        wild_case_compare(system_charset_info, field->field_name, wild))
      continue;

    // Get default row, with all NULL fields set to NULL
    restore_record(table, s->default_values);

    // NAME
    table->field[TMP_TABLE_COLUMNS_COLUMN_NAME]->store(
        field->field_name, strlen(field->field_name), cs);

    // COLUMN_TYPE
    field->sql_type(type);
    table->field[TMP_TABLE_COLUMNS_COLUMN_TYPE]->store(type.ptr(),
                                                       type.length(), cs);

    // COLLATION_NAME
    if (field->has_charset()) {
      table->field[TMP_TABLE_COLUMNS_COLLATION_NAME]->store(
          field->charset()->m_coll_name, strlen(field->charset()->m_coll_name),
          cs);
      table->field[TMP_TABLE_COLUMNS_COLLATION_NAME]->set_notnull();
    }

    // IS_NULLABLE
    pos = pointer_cast<const uchar *>(
        field->is_flag_set(NOT_NULL_FLAG) ? "NO" : "YES");
    table->field[TMP_TABLE_COLUMNS_IS_NULLABLE]->store(
        (const char *)pos, strlen((const char *)pos), cs);

    // COLUMN_KEY
    pos = pointer_cast<const uchar *>(
        field->is_flag_set(PRI_KEY_FLAG)        ? "PRI"
        : field->is_flag_set(UNIQUE_KEY_FLAG)   ? "UNI"
        : field->is_flag_set(MULTIPLE_KEY_FLAG) ? "MUL"
                                                : "");
    table->field[TMP_TABLE_COLUMNS_COLUMN_KEY]->store(
        (const char *)pos, strlen((const char *)pos), cs);

    // COLUMN_DEFAULT
    if (print_default_clause(thd, field, &type, false)) {
      table->field[TMP_TABLE_COLUMNS_COLUMN_DEFAULT]->store(type.ptr(),
                                                            type.length(), cs);
      table->field[TMP_TABLE_COLUMNS_COLUMN_DEFAULT]->set_notnull();
    }

    // EXTRA
    const bool on_update_clause_was_printed =
        print_on_update_clause(field, &type, true);
    if (field->auto_flags & Field::NEXT_NUMBER) {
      if (on_update_clause_was_printed) type.append(STRING_WITH_LEN(" "), cs);
      type.append(STRING_WITH_LEN("auto_increment"), cs);
    } else if (field->gcol_info != nullptr) {
      if (on_update_clause_was_printed) type.append(STRING_WITH_LEN(" "), cs);

      if (field->stored_in_db)
        type.append(STRING_WITH_LEN("STORED GENERATED"), cs);
      else
        type.append(STRING_WITH_LEN("VIRTUAL GENERATED"), cs);
    }

    // Column visibility attribute.
    if (field->is_hidden_by_user()) {
      if (type.length() > 0) type.append(STRING_WITH_LEN(" "), cs);
      type.append(STRING_WITH_LEN("INVISIBLE"), cs);
    }

    if (type.length() > 0)
      table->field[TMP_TABLE_COLUMNS_EXTRA]->store(type.ptr(), type.length(),
                                                   cs);
    else
      table->field[TMP_TABLE_COLUMNS_EXTRA]->store(STRING_WITH_LEN("NULL"), cs);

    // PRIVILEGES
    uint col_access;
    check_access(thd, SELECT_ACL, db_name.str, &tables->grant.privilege,
                 nullptr, false, tables->schema_table != nullptr);
    col_access = get_column_grant(thd, &tables->grant, db_name.str,
                                  table_name.str, field->field_name) &
                 COL_ACLS;
    if (!tables->schema_table && !col_access) continue;
    char *end = tmp;
    for (uint bitnr = 0; col_access; col_access >>= 1, bitnr++) {
      if (col_access & 1) {
        *end++ = ',';
        end = my_stpcpy(end, grant_types.type_names[bitnr]);
      }
    }
    table->field[TMP_TABLE_COLUMNS_PRIVILEGES]->store(
        tmp + 1, end == tmp ? 0 : (uint)(end - tmp - 1), cs);

    // COLUMN_COMMENT
    table->field[TMP_TABLE_COLUMNS_COLUMN_COMMENT]->store(
        field->comment.str, field->comment.length, cs);

    // COLUMN_GENERATION_EXPRESSION
    if (field->gcol_info) {
      char buffer[128];
      String s(buffer, sizeof(buffer), system_charset_info);
      field->gcol_info->print_expr(thd, &s);
      table->field[TMP_TABLE_COLUMNS_GENERATION_EXPRESSION]->store(
          s.ptr(), s.length(), cs);
    } else
      table->field[TMP_TABLE_COLUMNS_GENERATION_EXPRESSION]->set_null();

    if (schema_table_store_record(thd, table)) return 1;
  }

  return 0;
}

static bool iter_schema_engines(THD *thd, plugin_ref plugin, void *ptable) {
  TABLE *table = (TABLE *)ptable;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  CHARSET_INFO *scs = system_charset_info;
  handlerton *default_type = ha_default_handlerton(thd);
  DBUG_TRACE;

  /* Disabled plugins */
  if (plugin_state(plugin) != PLUGIN_IS_READY) {
    struct st_mysql_plugin *plug = plugin_decl(plugin);

    if (!(wild && wild[0] && wild_case_compare(scs, plug->name, wild))) {
      restore_record(table, s->default_values);
      table->field[0]->store(plug->name, strlen(plug->name), scs);
      table->field[1]->store(STRING_WITH_LEN("NO"), scs);
      table->field[2]->store(plug->descr, strlen(plug->descr), scs);
      if (schema_table_store_record(thd, table)) return true;
    }
    return false;
  }

  if (!(hton->flags & HTON_HIDDEN)) {
    LEX_CSTRING *name = plugin_name(plugin);
    if (!(wild && wild[0] && wild_case_compare(scs, name->str, wild))) {
      const char *option_name = show_comp_option_name[(int)hton->state];
      restore_record(table, s->default_values);

      table->field[0]->store(name->str, name->length, scs);
      if (hton->state == SHOW_OPTION_YES && default_type == hton)
        option_name = "DEFAULT";
      table->field[1]->store(option_name, strlen(option_name), scs);
      table->field[2]->store(plugin_decl(plugin)->descr,
                             strlen(plugin_decl(plugin)->descr), scs);

      LEX_CSTRING yes{STRING_WITH_LEN("YES")};
      LEX_CSTRING no{STRING_WITH_LEN("NO")};
      LEX_CSTRING *tmp;

      tmp = (hton->commit != nullptr) ? &yes : &no;
      table->field[3]->store(tmp->str, tmp->length, scs);
      table->field[3]->set_notnull();
      tmp = (hton->prepare != nullptr) ? &yes : &no;
      table->field[4]->store(tmp->str, tmp->length, scs);
      table->field[4]->set_notnull();
      tmp = (hton->savepoint_set != nullptr) ? &yes : &no;
      table->field[5]->store(tmp->str, tmp->length, scs);
      table->field[5]->set_notnull();

      if (schema_table_store_record(thd, table)) return true;
    }
  }
  return false;
}

static int fill_schema_engines(THD *thd, Table_ref *tables, Item *) {
  DBUG_TRACE;
  if (plugin_foreach_with_mask(thd, iter_schema_engines,
                               MYSQL_STORAGE_ENGINE_PLUGIN, ~PLUGIN_IS_FREED,
                               tables->table))
    return 1;
  return 0;
}

/* Define fields' indexes for KEYS of temporary tables */
#define TMP_TABLE_KEYS_TABLE_NAME 0
#define TMP_TABLE_KEYS_IS_NON_UNIQUE 1
#define TMP_TABLE_KEYS_INDEX_SCHEMA 2
#define TMP_TABLE_KEYS_INDEX_NAME 3
#define TMP_TABLE_KEYS_SEQ_IN_INDEX 4
#define TMP_TABLE_KEYS_COLUMN_NAME 5
#define TMP_TABLE_KEYS_COLLATION 6
#define TMP_TABLE_KEYS_CARDINALITY 7
#define TMP_TABLE_KEYS_SUB_PART 8
#define TMP_TABLE_KEYS_PACKED 9
#define TMP_TABLE_KEYS_IS_NULLABLE 10
#define TMP_TABLE_KEYS_INDEX_TYPE 11
#define TMP_TABLE_KEYS_COMMENT 12
#define TMP_TABLE_KEYS_INDEX_COMMENT 13
#define TMP_TABLE_KEYS_IS_VISIBLE 14
#define TMP_TABLE_KEYS_EXPRESSION 15

static int get_schema_tmp_table_keys_record(THD *thd, Table_ref *tables,
                                            TABLE *table, bool res, LEX_CSTRING,
                                            LEX_CSTRING table_name) {
  DBUG_TRACE;

  assert(thd->lex->sql_command == SQLCOM_SHOW_KEYS);

  if (res) return res;

  CHARSET_INFO *cs = system_charset_info;
  TABLE *show_table = tables->table;
  KEY *key_info = show_table->s->key_info;
  if (show_table->file)
    show_table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);

  uint i = 0;
  /*
    If a primary key is generated for the table then hide definition of a
    generated invisible primary key when system variable
    show_gipk_in_create_table_and_information_schema is set to OFF.
  */
  if (table_has_generated_invisible_primary_key(show_table) &&
      !thd->variables.show_gipk_in_create_table_and_information_schema) {
    /*
      Primary key is always at the first position in the keys list. Skip
      printing primary key definition.
    */
    assert((key_info->user_defined_key_parts == 1) &&
           (is_generated_invisible_primary_key_column_name(
               key_info->key_part->field->field_name)));
    i++;
    key_info++;
  }

  for (; i < show_table->s->keys; i++, key_info++) {
    KEY_PART_INFO *key_part = key_info->key_part;
    const char *str;
    for (uint j = 0; j < key_info->user_defined_key_parts; j++, key_part++) {
      restore_record(table, s->default_values);

      // TABLE_NAME
      table->field[TMP_TABLE_KEYS_TABLE_NAME]->store(table_name.str,
                                                     table_name.length, cs);

      // NON_UNIQUE
      table->field[TMP_TABLE_KEYS_IS_NON_UNIQUE]->store(
          (longlong)((key_info->flags & HA_NOSAME) ? 0 : 1), true);

      // INDEX_NAME
      table->field[TMP_TABLE_KEYS_INDEX_NAME]->store(
          key_info->name, strlen(key_info->name), cs);

      // SEQ_IN_INDEX
      table->field[TMP_TABLE_KEYS_SEQ_IN_INDEX]->store((longlong)(j + 1), true);

      // COLUMN_NAME
      str = (key_part->field ? key_part->field->field_name : "?unknown field?");

      if (key_part->field && key_part->field->is_hidden_by_system()) {
        table->field[TMP_TABLE_KEYS_COLUMN_NAME]->set_null();
      } else {
        table->field[TMP_TABLE_KEYS_COLUMN_NAME]->store(str, strlen(str), cs);
        table->field[TMP_TABLE_KEYS_COLUMN_NAME]->set_notnull();
      }

      if (show_table->file) {
        // COLLATION
        if (show_table->file->index_flags(i, j, false) & HA_READ_ORDER) {
          table->field[TMP_TABLE_KEYS_COLLATION]->store(
              ((key_part->key_part_flag & HA_REVERSE_SORT) ? "D" : "A"), 1, cs);
          table->field[TMP_TABLE_KEYS_COLLATION]->set_notnull();
        }

        // CARDINALITY
        KEY *key = show_table->key_info + i;
        if (key->has_records_per_key(j)) {
          const double records =
              (show_table->file->stats.records / key->records_per_key(j));
          table->field[TMP_TABLE_KEYS_CARDINALITY]->store(
              static_cast<longlong>(round(records)), true);
          table->field[TMP_TABLE_KEYS_CARDINALITY]->set_notnull();
        }
      }

      // INDEX_TYPE
      if (key_info->flags & HA_SPATIAL)
        str = "SPATIAL";
      else {
        const ha_key_alg key_alg = key_info->algorithm;
        /* If index algorithm is implicit get SE default. */
        switch (key_alg) {
          case HA_KEY_ALG_SE_SPECIFIC:
            str = "";
            break;
          case HA_KEY_ALG_BTREE:
            str = "BTREE";
            break;
          case HA_KEY_ALG_RTREE:
            str = "RTREE";
            break;
          case HA_KEY_ALG_HASH:
            str = "HASH";
            break;
          case HA_KEY_ALG_FULLTEXT:
            str = "FULLTEXT";
            break;
          default:
            assert(0);
            str = "";
        }
      }
      table->field[TMP_TABLE_KEYS_INDEX_TYPE]->store(str, strlen(str), cs);

      // SUB_PART
      if (!(key_info->flags & HA_FULLTEXT) &&
          (key_part->field &&
           key_part->length !=
               show_table->s->field[key_part->fieldnr - 1]->key_length())) {
        table->field[TMP_TABLE_KEYS_SUB_PART]->store(
            key_part->length / key_part->field->charset()->mbmaxlen, true);
        table->field[TMP_TABLE_KEYS_SUB_PART]->set_notnull();
      }

      // NULLABLE
      const char *pos;
      if (key_part->field && key_part->field->is_flag_set(NOT_NULL_FLAG))
        pos = "";
      else
        pos = "YES";
      table->field[TMP_TABLE_KEYS_IS_NULLABLE]->store(pos, strlen(pos), cs);

      // COMMENT
      if (!show_table->s->keys_in_use.is_set(i) && key_info->is_visible)
        table->field[TMP_TABLE_KEYS_COMMENT]->store(STRING_WITH_LEN("disabled"),
                                                    cs);
      else
        table->field[TMP_TABLE_KEYS_COMMENT]->store("", 0, cs);
      table->field[TMP_TABLE_KEYS_COMMENT]->set_notnull();

      // INDEX_COMMENT
      assert(((key_info->flags & HA_USES_COMMENT) != 0) ==
             (key_info->comment.length > 0));
      if (key_info->flags & HA_USES_COMMENT)
        table->field[TMP_TABLE_KEYS_INDEX_COMMENT]->store(
            key_info->comment.str, key_info->comment.length, cs);

      // is_visible column
      const char *is_visible = key_info->is_visible ? "YES" : "NO";
      table->field[TMP_TABLE_KEYS_IS_VISIBLE]->store(is_visible,
                                                     strlen(is_visible), cs);
      table->field[TMP_TABLE_KEYS_IS_VISIBLE]->set_notnull();

      // Expression for functional key parts
      if (key_part->field != nullptr &&
          key_part->field->is_field_for_functional_index()) {
        Value_generator *gcol = key_part->field->gcol_info;

        table->field[TMP_TABLE_KEYS_EXPRESSION]->store(
            gcol->expr_str.str, gcol->expr_str.length, cs);
        table->field[TMP_TABLE_KEYS_EXPRESSION]->set_notnull();
      } else {
        table->field[TMP_TABLE_KEYS_EXPRESSION]->set_null();
      }

      if (schema_table_store_record(thd, table)) return 1;
    }
  }
  return res;
}

/*
  Convert a string in a given character set to a string which can be
  used for FRM file storage in which case use_hex is true and we store
  the character constants as hex strings in the character set encoding
  their field have. In the case of SHOW CREATE TABLE and the
  PARTITIONS information schema table we instead provide utf8 strings
  to the user and convert to the utf8 character set.

  SYNOPSIS
    get_cs_converted_part_value_from_string()
    item                           Item from which constant comes
    input_str                      String as provided by val_str after
                                   conversion to character set
    output_str                     Out value: The string created
    cs                             Character set string is encoded in
                                   NULL for INT_RESULT's here
    use_hex                        true => hex string created
                                   false => utf8 constant string created

  RETURN VALUES
    true                           Error
    false                          Ok
*/

bool get_cs_converted_part_value_from_string(THD *thd, Item *item,
                                             String *input_str,
                                             String *output_str,
                                             const CHARSET_INFO *cs,
                                             bool use_hex) {
  if (item->result_type() == INT_RESULT) {
    const longlong value = item->val_int();
    output_str->set(value, system_charset_info);
    return false;
  }
  if (!input_str) {
    my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
    return true;
  }
  get_cs_converted_string_value(thd, input_str, output_str, cs, use_hex);
  return false;
}

static int fill_open_tables(THD *thd, Table_ref *tables, Item *) {
  DBUG_TRACE;
  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table = tables->table;
  CHARSET_INFO *cs = system_charset_info;
  OPEN_TABLE_LIST *open_list;
  if (!(open_list = list_open_tables(thd, thd->lex->query_block->db, wild)) &&
      thd->is_fatal_error())
    return 1;

  for (; open_list; open_list = open_list->next) {
    restore_record(table, s->default_values);
    table->field[0]->store(open_list->db, strlen(open_list->db), cs);
    table->field[1]->store(open_list->table, strlen(open_list->table), cs);
    table->field[2]->store((longlong)open_list->in_use, true);
    table->field[3]->store((longlong)open_list->locked, true);
    if (schema_table_store_record(thd, table)) return 1;
  }
  return 0;
}

struct schema_table_ref {
  const char *table_name;
  ST_SCHEMA_TABLE *schema_table;
};

/*
  Find schema_tables element by name

  SYNOPSIS
    find_schema_table_in_plugin()
    plugin              plugin
    table_name          table name

  RETURN
    0	table not found
    1   found the schema table
*/
static bool find_schema_table_in_plugin(THD *, plugin_ref plugin,
                                        void *p_table) {
  auto *p_schema_table = (schema_table_ref *)p_table;
  const char *table_name = p_schema_table->table_name;
  ST_SCHEMA_TABLE *schema_table = plugin_data<ST_SCHEMA_TABLE *>(plugin);
  DBUG_TRACE;

  if (!my_strcasecmp(system_charset_info, schema_table->table_name,
                     table_name)) {
    p_schema_table->schema_table = schema_table;
    return true;
  }

  return false;
}

/*
  Find schema_tables element by name

  SYNOPSIS
    find_schema_table()
    thd                 thread handler
    table_name          table name

  RETURN
    0	table not found
    #   pointer to 'schema_tables' element
*/

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char *table_name) {
  schema_table_ref schema_table_a;
  ST_SCHEMA_TABLE *schema_table = schema_tables;
  DBUG_TRACE;

  for (; schema_table->table_name; schema_table++) {
    if (!my_strcasecmp(system_charset_info, schema_table->table_name,
                       table_name))
      return schema_table;
  }

  schema_table_a.table_name = table_name;
  if (plugin_foreach(thd, find_schema_table_in_plugin,
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &schema_table_a))
    return schema_table_a.schema_table;

  return nullptr;
}

ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx) {
  return &schema_tables[schema_table_idx];
}

/**
  Create information_schema table using schema_table data.

  @note
    For MYSQL_TYPE_DECIMAL fields only, the field_length member has encoded
    into it two numbers, based on modulus of base-10 numbers.  In the ones
    position is the number of decimals.  Tens position is unused.  In the
    hundreds and thousands position is a two-digit decimal number representing
    length.  Encode this value with  (decimals*100)+length  , where
    0<decimals<10 and 0<=length<100 .

  @param thd	       	          thread handler

  @param table_list Used to pass I_S table information(fields info, tables
  parameters etc) and table name.

  @returns  Pointer to created table
  @retval  NULL           Can't create table
*/

static TABLE *create_schema_table(THD *thd, Table_ref *table_list) {
  int field_count = 0;
  Item *item;
  TABLE *table;
  mem_root_deque<Item *> fields(thd->mem_root);
  ST_SCHEMA_TABLE *schema_table = table_list->schema_table;
  ST_FIELD_INFO *fields_info = schema_table->fields_info;
  CHARSET_INFO *cs = system_charset_info;
  DBUG_TRACE;

  for (; fields_info->field_name; fields_info++) {
    switch (fields_info->field_type) {
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_INT24:
        if (!(item = new Item_return_int(
                  fields_info->field_name, fields_info->field_length,
                  fields_info->field_type, fields_info->value))) {
          return nullptr;
        }
        item->unsigned_flag = (fields_info->field_flags & MY_I_S_UNSIGNED);
        break;
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME: {
        const Name_string field_name(fields_info->field_name,
                                     strlen(fields_info->field_name));
        if (!(item =
                  new Item_temporal(fields_info->field_type, field_name, 0, 0)))
          return nullptr;

        if (fields_info->field_type == MYSQL_TYPE_TIMESTAMP ||
            fields_info->field_type == MYSQL_TYPE_DATETIME)
          item->decimals = fields_info->field_length;

        break;
      }
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE: {
        const Name_string field_name(fields_info->field_name,
                                     strlen(fields_info->field_name));
        if ((item = new Item_float(field_name, 0.0, DECIMAL_NOT_SPECIFIED,
                                   fields_info->field_length)) == nullptr)
          return nullptr;
        break;
      }
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        if (!(item = new Item_decimal((longlong)fields_info->value, false))) {
          return nullptr;
        }
        item->unsigned_flag = (fields_info->field_flags & MY_I_S_UNSIGNED);
        item->decimals = fields_info->field_length % 10;
        item->max_length = (fields_info->field_length / 100) % 100;
        if (item->unsigned_flag == 0) item->max_length += 1;
        if (item->decimals > 0) item->max_length += 1;
        item->item_name.copy(fields_info->field_name);
        break;
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VECTOR:
        if (!(item = new Item_blob(fields_info->field_name,
                                   fields_info->field_length))) {
          return nullptr;
        }
        break;
      default:
        /* Don't let unimplemented types pass through. Could be a grave error.
         */
        assert(fields_info->field_type == MYSQL_TYPE_STRING);

        if (!(item =
                  new Item_empty_string("", fields_info->field_length, cs))) {
          return nullptr;
        }
        item->item_name.copy(fields_info->field_name);
        break;
    }
    fields.push_back(item);
    item->set_nullable(fields_info->field_flags & MY_I_S_MAYBE_NULL);
    field_count++;
  }
  Temp_table_param *tmp_table_param = new (thd->mem_root) Temp_table_param;
  if (!tmp_table_param) return nullptr;

  tmp_table_param->table_charset = cs;
  tmp_table_param->func_count = field_count;
  tmp_table_param->schema_table = true;
  Query_block *query_block = thd->lex->current_query_block();
  if (!(table = create_tmp_table(
            thd, tmp_table_param, fields, /*group=*/nullptr, false, false,
            query_block->active_options() | TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR,
            table_list->alias)))
    return nullptr;

  // create_tmp_table() makes read_set and write_set share the same buffer, so
  // they are always identical, and always have all bits set. For information
  // schema tables we need to distinguish between read and write, so break the
  // link between them here.
  my_bitmap_map *bitmaps =
      (my_bitmap_map *)thd->alloc(bitmap_buffer_size(field_count));
  bitmap_init(&table->def_read_set, bitmaps, field_count);
  table->read_set = &table->def_read_set;
  table->read_set_internal = table->def_read_set;
  bitmap_clear_all(table->read_set);
  return table;
}

/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    make_old_format()
    thd			thread handler
    schema_table        pointer to 'schema_tables' element

  RETURN
   1	error
   0	success
*/

static int make_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table) {
  ST_FIELD_INFO *field_info = schema_table->fields_info;
  Name_resolution_context *context = &thd->lex->query_block->context;
  for (; field_info->field_name; field_info++) {
    if (field_info->old_name) {
      Item_field *field =
          new Item_field(context, NullS, NullS, field_info->field_name);
      if (field) {
        field->item_name.copy(field_info->old_name);
        if (add_item_to_list(thd, field)) return 1;
      }
    }
  }
  return 0;
}

static int make_tmp_table_columns_format(THD *thd,
                                         ST_SCHEMA_TABLE *schema_table) {
  int fields_arr[] = {
      TMP_TABLE_COLUMNS_COLUMN_NAME,    TMP_TABLE_COLUMNS_COLUMN_TYPE,
      TMP_TABLE_COLUMNS_COLLATION_NAME, TMP_TABLE_COLUMNS_IS_NULLABLE,
      TMP_TABLE_COLUMNS_COLUMN_KEY,     TMP_TABLE_COLUMNS_COLUMN_DEFAULT,
      TMP_TABLE_COLUMNS_EXTRA,          TMP_TABLE_COLUMNS_PRIVILEGES,
      TMP_TABLE_COLUMNS_COLUMN_COMMENT, -1};
  int *field_num = fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context = &thd->lex->query_block->context;

  for (; *field_num >= 0; field_num++) {
    field_info = &schema_table->fields_info[*field_num];
    if (!thd->lex->verbose && (*field_num == TMP_TABLE_COLUMNS_COLLATION_NAME ||
                               *field_num == TMP_TABLE_COLUMNS_PRIVILEGES ||
                               *field_num == TMP_TABLE_COLUMNS_COLUMN_COMMENT))
      continue;
    Item_field *field =
        new Item_field(context, NullS, NullS, field_info->field_name);
    if (field) {
      field->item_name.copy(field_info->old_name);
      if (add_item_to_list(thd, field)) return 1;
    }
  }
  return 0;
}

/**
  Create information_schema table

  @param thd                thread handler
  @param lex                pointer to LEX
  @param table_list         pointer to table_list

  @return true on error, false otherwise.
*/

bool mysql_schema_table(THD *thd, LEX *lex, Table_ref *table_list) {
  DBUG_TRACE;
  TABLE *table = create_schema_table(thd, table_list);
  if (table == nullptr) return true;

  table->s->tmp_table = SYSTEM_TMP_TABLE;
  table_list->grant.privilege = SELECT_ACL;
  /*
    This test is necessary to make
    case insensitive file systems +
    upper case table names(information schema tables) +
    views
    working correctly
  */
  table->alias_name_used = my_strcasecmp(
      table_alias_charset, table_list->table_name, table_list->alias);
  table_list->table = table;
  table->pos_in_table_list = table_list;
  if (table_list->query_block->first_execution)
    table_list->query_block->add_base_options(OPTION_SCHEMA_TABLE);
  lex->safe_to_cache_query = false;

  if (table_list->schema_table_reformed)  // show command
  {
    Query_block *sel = lex->current_query_block();
    Field_translator *transl;

    const Access_bitmask want_privilege_saved = thd->want_privilege;
    thd->want_privilege = SELECT_ACL;
    const enum enum_mark_columns save_mark_used_columns =
        thd->mark_used_columns;
    thd->mark_used_columns = MARK_COLUMNS_READ;

    if (table_list->field_translation) {
      Field_translator *end = table_list->field_translation_end;
      for (transl = table_list->field_translation; transl < end; transl++) {
        if (!transl->item->fixed &&
            transl->item->fix_fields(thd, &transl->item))
          return true;
      }

      thd->want_privilege = want_privilege_saved;
      thd->mark_used_columns = save_mark_used_columns;

      return false;
    }
    if (!(transl = (Field_translator *)(thd->stmt_arena->alloc(
              sel->fields.size() * sizeof(Field_translator))))) {
      return true;
    }
    Field_translator *org_transl = transl;
    for (Item *item : sel->visible_fields()) {
      transl->item = item;
      transl->name = item->item_name.ptr();
      if (!item->fixed && item->fix_fields(thd, &transl->item)) {
        return true;
      }
      ++transl;
    }

    thd->want_privilege = want_privilege_saved;
    thd->mark_used_columns = save_mark_used_columns;
    table_list->field_translation = org_transl;
    table_list->field_translation_end = transl;
  }

  return false;
}

/**
  Generate select from information_schema table

  @param thd                  thread handler
  @param sel                  pointer to Query_block
  @param schema_table_idx     index of 'schema_tables' element

  @return true on error, false otherwise
*/

bool make_schema_query_block(THD *thd, Query_block *sel,
                             enum enum_schema_tables schema_table_idx) {
  ST_SCHEMA_TABLE *schema_table = get_schema_table(schema_table_idx);
  LEX_STRING db{nullptr, 0}, table{nullptr, 0};
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("mysql_schema_query_block: %s", schema_table->table_name));
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  lex_string_strmake(thd->mem_root, &db, INFORMATION_SCHEMA_NAME.str,
                     INFORMATION_SCHEMA_NAME.length);
  lex_string_strmake(thd->mem_root, &table, schema_table->table_name,
                     strlen(schema_table->table_name));

  if (schema_table->old_format(thd, schema_table)) { /* Handle old syntax */
    return true;
  }

  Table_ref *const schema_tlist = sel->add_table_to_list(
      thd,
      new (thd->mem_root) Table_ident(thd->get_protocol(), to_lex_cstring(db),
                                      to_lex_cstring(table), false),
      nullptr, 0, TL_READ, MDL_SHARED_READ);

  if (schema_tlist == nullptr) {
    return true;
  }

  return sel->add_joined_table(schema_tlist);
}

/**
  Fill INFORMATION_SCHEMA-table, leave correct Diagnostics_area
  state after itself.

  This function is a wrapper around ST_SCHEMA_TABLE::fill_table(), which
  may "partially silence" some errors. The thing is that during
  fill_table() many errors might be emitted. These errors stem from the
  nature of fill_table().

  For example, SELECT ... FROM INFORMATION_SCHEMA.xxx WHERE TABLE_NAME = 'xxx'
  results in a number of 'Table @<db name@>.xxx does not exist' errors,
  because fill_table() tries to open the 'xxx' table in every possible
  database.

  Those errors are cleared (the error status is cleared from
  Diagnostics_area) inside fill_table(), but they remain in the
  Diagnostics_area condition list (the list is not cleared because
  it may contain useful warnings).

  This function is responsible for making sure that Diagnostics_area
  does not contain warnings corresponding to the cleared errors.

  @note: THD::no_warnings_for_error used to be set before calling
  fill_table(), thus those errors didn't go to Diagnostics_area. This is not
  the case now (THD::no_warnings_for_error was eliminated as a hack), so we
  need to take care of those warnings here.

  @param thd            Thread context.
  @param table_list     I_S table.
  @param condition
    Condition, which can be used to do less file manipulations (for
    example, WHERE TABLE_SCHEMA='test' allows to open only directory 'test',
    not other database directories).

  @return Error status.
  @retval true Error.
  @retval false Success.
*/
bool do_fill_information_schema_table(THD *thd, Table_ref *table_list,
                                      Item *condition) {
  /*
    Return if there is already an error reported.

    This situation occurs because there are few functions
    that return success, even after reporting error as
    mentioned in Bug#25642468. The following check would
    be removed by fix for Bug#25642468.
  */
  if (thd->is_error()) return true;

  // NOTE: fill_table() may generate many "useless" warnings, which will be
  // ignored afterwards. On the other hand, there might be "useful"
  // warnings, which should be presented to the user. Diagnostics_area usually
  // stores no more than THD::variables.max_error_count warnings.
  // The problem is that "useless warnings" may occupy all the slots in the
  // Diagnostics_area, so "useful warnings" get rejected. In order to avoid
  // that problem we create a Diagnostics_area instance, which is capable of
  // storing "unlimited" number of warnings.
  Diagnostics_area *da = thd->get_stmt_da();
  Diagnostics_area tmp_da(true);

  // Don't copy existing conditions from the old DA so we don't get them twice
  // when we call copy_non_errors_from_da below.
  thd->push_diagnostics_area(&tmp_da, false);

  const bool res =
      table_list->schema_table->fill_table(thd, table_list, condition);

  thd->pop_diagnostics_area();

  // Pass an error if any.
  if (tmp_da.is_error()) {
    da->set_error_status(tmp_da.mysql_errno(), tmp_da.message_text(),
                         tmp_da.returned_sqlstate());
    da->push_warning(thd, tmp_da.mysql_errno(), tmp_da.returned_sqlstate(),
                     Sql_condition::SL_ERROR, tmp_da.message_text());
  }

  // Pass warnings (if any).
  //
  // Filter out warnings with SL_ERROR level, because they
  // correspond to the errors which were filtered out in fill_table().
  da->copy_non_errors_from_da(thd, &tmp_da);

  return res;
}

struct run_hton_fill_schema_table_args {
  Table_ref *tables;
  Item *cond;
};

ST_FIELD_INFO engines_fields_info[] = {
    {"ENGINE", 64, MYSQL_TYPE_STRING, 0, 0, "Engine", 0},
    {"SUPPORT", 8, MYSQL_TYPE_STRING, 0, 0, "Support", 0},
    {"COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, "Comment", 0},
    {"TRANSACTIONS", 3, MYSQL_TYPE_STRING, 0, 1, "Transactions", 0},
    {"XA", 3, MYSQL_TYPE_STRING, 0, 1, "XA", 0},
    {"SAVEPOINTS", 3, MYSQL_TYPE_STRING, 0, 1, "Savepoints", 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO tmp_table_keys_fields_info[] = {
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", 0},
    {"NON_UNIQUE", 1, MYSQL_TYPE_LONGLONG, 0, 0, "Non_unique", 0},
    {"INDEX_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"INDEX_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Key_name", 0},
    {"SEQ_IN_INDEX", 2, MYSQL_TYPE_LONGLONG, 0, 0, "Seq_in_index", 0},
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Column_name", 0},
    {"COLLATION", 1, MYSQL_TYPE_STRING, 0, 1, "Collation", 0},
    {"CARDINALITY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 1,
     "Cardinality", 0},
    {"SUB_PART", 3, MYSQL_TYPE_LONGLONG, 0, 1, "Sub_part", 0},
    {"PACKED", 10, MYSQL_TYPE_STRING, 0, 1, "Packed", 0},
    {"NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", 0},
    {"INDEX_TYPE", 16, MYSQL_TYPE_STRING, 0, 0, "Index_type", 0},
    {"COMMENT", 16, MYSQL_TYPE_STRING, 0, 1, "Comment", 0},
    {"INDEX_COMMENT", INDEX_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0,
     "Index_comment", 0},
    {"IS_VISIBLE", 4, MYSQL_TYPE_STRING, 0, 1, "Visible", 0},
    {"EXPRESSION", MAX_FIELD_BLOBLENGTH, MYSQL_TYPE_STRING, 0, 1, "Expression",
     0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

/**
  Grantee is of form 'user'@'hostname', so add +1 for '@' and +4 for the
  single quotes.
*/
static const int GRANTEE_MAX_CHAR_LENGTH =
    USERNAME_CHAR_LENGTH + 1 + HOSTNAME_LENGTH + 4;

ST_FIELD_INFO user_privileges_fields_info[] = {
    {"GRANTEE", GRANTEE_MAX_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO schema_privileges_fields_info[] = {
    {"GRANTEE", GRANTEE_MAX_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO table_privileges_fields_info[] = {
    {"GRANTEE", GRANTEE_MAX_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO column_privileges_fields_info[] = {
    {"GRANTEE", GRANTEE_MAX_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO open_tables_fields_info[] = {
    {"Database", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database", 0},
    {"Table", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", 0},
    {"In_use", 1, MYSQL_TYPE_LONGLONG, 0, 0, "In_use", 0},
    {"Name_locked", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Name_locked", 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO processlist_fields_info[] = {
    {"ID", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, "Id", 0},
    {"USER", USERNAME_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, "User", 0},
    {"HOST", HOST_AND_PORT_LENGTH, MYSQL_TYPE_STRING, 0, 0, "Host", 0},
    {"DB", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Db", 0},
    {"COMMAND", 16, MYSQL_TYPE_STRING, 0, 0, "Command", 0},
    {"TIME", 7, MYSQL_TYPE_LONG, 0, 0, "Time", 0},
    {"STATE", 64, MYSQL_TYPE_STRING, 0, 1, "State", 0},
    {"INFO", PROCESS_LIST_INFO_WIDTH, MYSQL_TYPE_STRING, 0, 1, "Info", 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO plugin_fields_info[] = {
    {"PLUGIN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name", 0},
    {"PLUGIN_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PLUGIN_STATUS", 10, MYSQL_TYPE_STRING, 0, 0, "Status", 0},
    {"PLUGIN_TYPE", 80, MYSQL_TYPE_STRING, 0, 0, "Type", 0},
    {"PLUGIN_TYPE_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"PLUGIN_LIBRARY", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Library", 0},
    {"PLUGIN_LIBRARY_VERSION", 20, MYSQL_TYPE_STRING, 0, 1, nullptr, 0},
    {"PLUGIN_AUTHOR", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, nullptr, 0},
    {"PLUGIN_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, nullptr, 0},
    {"PLUGIN_LICENSE", 80, MYSQL_TYPE_STRING, 0, 1, "License", 0},
    {"LOAD_OPTION", 64, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

ST_FIELD_INFO tmp_table_columns_fields_info[] = {
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Field", 0},
    {"COLUMN_TYPE", 65535, MYSQL_TYPE_STRING, 0, 0, "Type", 0},
    {"COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 1, "Collation",
     0},
    {"IS_NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", 0},
    {"COLUMN_KEY", 3, MYSQL_TYPE_STRING, 0, 0, "Key", 0},
    {"COLUMN_DEFAULT", MAX_FIELD_VARCHARLENGTH, MYSQL_TYPE_STRING, 0, 1,
     "Default", 0},
    {"EXTRA", 40, MYSQL_TYPE_STRING, 0, 0, "Extra", 0},
    {"PRIVILEGES", 80, MYSQL_TYPE_STRING, 0, 0, "Privileges", 0},
    {"COLUMN_COMMENT", COLUMN_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0,
     "Comment", 0},
    {"GENERATION_EXPRESSION", GENERATED_COLUMN_EXPRESSION_MAXLEN,
     MYSQL_TYPE_STRING, 0, 0, "Generation expression", 0},
    {nullptr, 0, MYSQL_TYPE_STRING, 0, 0, nullptr, 0}};

/** For creating fields of information_schema.OPTIMIZER_TRACE */
extern ST_FIELD_INFO optimizer_trace_info[];

/*
  Description of ST_FIELD_INFO in table.h

  Make sure that the order of schema_tables and enum_schema_tables are the same.

*/

ST_SCHEMA_TABLE schema_tables[] = {
    {"COLUMN_PRIVILEGES", column_privileges_fields_info,
     fill_schema_column_privileges, nullptr, nullptr, false},
    {"ENGINES", engines_fields_info, fill_schema_engines, make_old_format,
     nullptr, false},
    {"OPEN_TABLES", open_tables_fields_info, fill_open_tables, make_old_format,
     nullptr, true},
    {"OPTIMIZER_TRACE", optimizer_trace_info, fill_optimizer_trace_info,
     nullptr, nullptr, false},
    {"PLUGINS", plugin_fields_info, fill_plugins, make_old_format, nullptr,
     false},
    {"PROCESSLIST", processlist_fields_info, fill_schema_processlist,
     make_old_format, nullptr, false},
    {"PROFILING", query_profile_statistics_info,
     fill_query_profile_statistics_info, make_profile_table_for_show, nullptr,
     false},
    {"SCHEMA_PRIVILEGES", schema_privileges_fields_info,
     fill_schema_schema_privileges, nullptr, nullptr, false},
    {"TABLE_PRIVILEGES", table_privileges_fields_info,
     fill_schema_table_privileges, nullptr, nullptr, false},
    {"USER_PRIVILEGES", user_privileges_fields_info,
     fill_schema_user_privileges, nullptr, nullptr, false},
    {"TMP_TABLE_COLUMNS", tmp_table_columns_fields_info, show_temporary_tables,
     make_tmp_table_columns_format, get_schema_tmp_table_columns_record, true},
    {"TMP_TABLE_KEYS", tmp_table_keys_fields_info, show_temporary_tables,
     make_old_format, get_schema_tmp_table_keys_record, true},
    {nullptr, nullptr, nullptr, nullptr, nullptr, false}};

int initialize_schema_table(st_plugin_int *plugin) {
  ST_SCHEMA_TABLE *schema_table;
  DBUG_TRACE;

  if (!(schema_table = (ST_SCHEMA_TABLE *)my_malloc(key_memory_ST_SCHEMA_TABLE,
                                                    sizeof(ST_SCHEMA_TABLE),
                                                    MYF(MY_WME | MY_ZEROFILL))))
    return 1;
  /* Historical Requirement */
  plugin->data = schema_table;  // shortcut for the future
  if (plugin->plugin->init) {
    schema_table->old_format = make_old_format;

    /* Make the name available to the init() function. */
    schema_table->table_name = plugin->name.str;

    if (plugin->plugin->init(schema_table)) {
      LogErr(ERROR_LEVEL, ER_PLUGIN_INIT_FAILED, plugin->name.str);
      plugin->data = nullptr;
      my_free(schema_table);
      return 1;
    }

    /* Make sure the plugin name is not set inside the init() function. */
    schema_table->table_name = plugin->name.str;
  }
  return 0;
}

int finalize_schema_table(st_plugin_int *plugin) {
  ST_SCHEMA_TABLE *schema_table = (ST_SCHEMA_TABLE *)plugin->data;
  DBUG_TRACE;

  if (schema_table) {
    if (plugin->plugin->deinit) {
      DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
      if (plugin->plugin->deinit(plugin)) {
        DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                               plugin->name.str));
      }
    }
    my_free(schema_table);
  }
  return 0;
}

/**
  Output trigger information (SHOW CREATE TRIGGER) to the client.

  @param thd          Thread context.
  @param trigger      table trigger to dump.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

static bool show_create_trigger_impl(THD *thd, Trigger *trigger) {
  Protocol *p = thd->get_protocol();

  // Construct sql_mode string.

  LEX_STRING sql_mode_str;

  if (sql_mode_string_representation(thd, trigger->get_sql_mode(),
                                     &sql_mode_str))
    return true;

  char create_trg_str_buf[10 * STRING_BUFFER_USUAL_SIZE];
  String create_trg_str(create_trg_str_buf, sizeof(create_trg_str_buf),
                        system_charset_info);
  create_trg_str.length(0);

  /*
    NOTE: SQL statement field must be not less than 1024 in order not to
    confuse old clients.
  */
  Item_empty_string *stmt_fld = new Item_empty_string(
      "SQL Original Statement", max<size_t>(create_trg_str.length(), 1024));

  if (stmt_fld == nullptr) return true;

  if (trigger->create_full_trigger_definition(thd, &create_trg_str))
    return true;

  stmt_fld->set_nullable(true);

  // Send header.

  mem_root_deque<Item *> fields(thd->mem_root);
  fields.push_back(new Item_empty_string("Trigger", NAME_LEN));
  fields.push_back(new Item_empty_string("sql_mode", sql_mode_str.length));
  fields.push_back(stmt_fld);
  fields.push_back(
      new Item_empty_string("character_set_client", MY_CS_NAME_SIZE));
  fields.push_back(
      new Item_empty_string("collation_connection", MY_CS_NAME_SIZE));
  fields.push_back(
      new Item_empty_string("Database Collation", MY_CS_NAME_SIZE));
  fields.push_back(
      new Item_temporal(MYSQL_TYPE_TIMESTAMP,
                        Name_string("Created", sizeof("created") - 1), 0, 0));
  if (thd->send_result_metadata(fields,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  // Resolve trigger client character set.

  const CHARSET_INFO *client_cs;

  if (resolve_charset(trigger->get_client_cs_name().str, nullptr, &client_cs))
    return true;

  // Send data.

  p->start_row();

  if (p->store_string(trigger->get_trigger_name().str,
                      trigger->get_trigger_name().length,
                      system_charset_info) ||
      p->store(sql_mode_str, system_charset_info) ||
      p->store_string(create_trg_str.c_ptr(), create_trg_str.length(),
                      client_cs) ||
      p->store_string(trigger->get_client_cs_name().str,
                      trigger->get_client_cs_name().length,
                      system_charset_info) ||
      p->store_string(trigger->get_connection_cl_name().str,
                      trigger->get_connection_cl_name().length,
                      system_charset_info) ||
      p->store_string(trigger->get_db_cl_name().str,
                      trigger->get_db_cl_name().length, system_charset_info))
    return true;

  bool rc;
  if (!trigger->is_created_timestamp_null()) {
    MYSQL_TIME timestamp;
    my_tz_SYSTEM->gmt_sec_to_TIME(&timestamp, trigger->get_created_timestamp());
    rc = p->store_datetime(timestamp, 2);
  } else
    rc = p->store_null();

  if (rc || p->end_row()) return true;

  my_eof(thd);

  return false;
}

/**
  Read the Data Dictionary to obtain base table name for the specified
  trigger name and construct TABE_LIST object for the base table.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return Table_ref object corresponding to the base table.

  TODO: This function is a copy&paste from add_table_to_list() and
  sp_add_to_query_tables(). The problem is that in order to be compatible
  with Stored Programs (Prepared Statements), we should not touch thd->lex.
  The "source" functions also add created Table_ref object to the
  thd->lex->query_tables.

  The plan to eliminate this copy&paste is to:

    - get rid of sp_add_to_query_tables() and use Lex::add_table_to_list().
      Only add_table_to_list() must be used to add tables from the parser
      into Lex::query_tables list.

    - do not update Lex::query_tables in add_table_to_list().
*/

static Table_ref *get_trigger_table(THD *thd, const sp_name *trg_name) {
  LEX_CSTRING db;
  LEX_STRING tbl_name;

  dd::Schema_MDL_locker mdl_locker(thd);

  dd::cache::Dictionary_client *dd_client = thd->dd_client();
  const dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const dd::Schema *sch_obj = nullptr;
  if (mdl_locker.ensure_locked(trg_name->m_db.str) ||
      dd_client->acquire(trg_name->m_db.str, &sch_obj))
    return nullptr;

  if (sch_obj == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), trg_name->m_db.str);
    return nullptr;
  }

  dd::String_type table_name;
  if (dd_client->get_table_name_by_trigger_name(*sch_obj, trg_name->m_name.str,
                                                &table_name))
    return nullptr;

  if (table_name == "") {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    return nullptr;
  }

  db = trg_name->m_db;
  db.str = thd->strmake(db.str, db.length);

  char lc_table_name[NAME_LEN + 1];
  const char *table_name_ptr = table_name.c_str();
  if (lower_case_table_names == 2) {
    my_stpncpy(lc_table_name, table_name.c_str(), NAME_LEN);
    my_casedn_str(files_charset_info, lc_table_name);
    lc_table_name[NAME_LEN] = '\0';
    table_name_ptr = lc_table_name;
  }

  const size_t table_name_length = strlen(table_name_ptr);

  tbl_name.str = thd->strmake(table_name_ptr, table_name_length);
  tbl_name.length = table_name_length;

  if (db.str == nullptr || tbl_name.str == nullptr) return nullptr;

  /* We need to reset statement table list to be PS/SP friendly. */
  return new (thd->mem_root)
      Table_ref(db.str, db.length, tbl_name.str, tbl_name.length, tbl_name.str,
                TL_IGNORE);
}

/**
  Acquire shared MDL lock for a specified database name/table name.

  @param thd         Thread context.
  @param db_name     Database name.
  @param table_name  Table name.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

static bool acquire_mdl_for_table(THD *thd, const char *db_name,
                                  const char *table_name) {
  MDL_request table_request;
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, db_name, table_name,
                   MDL_SHARED, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&table_request,
                                    thd->variables.lock_wait_timeout))
    return true;

  return false;
}

/**
  Helper to look up a Trigger object in a TABLE_SHARE by trigger name.

  @param share TABLE_SHARE in whose list of Trigger objects the lookup
               is to be performed.
  @param name  Name of trigger to find.

  @return Pointer to Trigger object, or nullptr if no trigger with the
          provided name was found.
*/

static Trigger *find_trigger_in_share(TABLE_SHARE *share,
                                      const LEX_STRING &name) {
  Trigger *t;
  List_iterator_fast<Trigger> it(*(share->triggers));

  while ((t = it++) != nullptr) {
    if (!my_strnncoll(dd::Trigger::name_collation(),
                      pointer_cast<const uchar *>(t->get_trigger_name().str),
                      t->get_trigger_name().length,
                      pointer_cast<const uchar *>(name.str), name.length))
      return t;
  }
  return nullptr;
}

/**
  SHOW CREATE TRIGGER high-level implementation.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

bool show_create_trigger(THD *thd, const sp_name *trg_name) {
  uint num_tables; /* NOTE: unused, only to pass to open_tables(). */
  bool error = true;
  Trigger *trigger;

  /*
    Metadata locks taken during SHOW CREATE TRIGGER should be released when
    the statement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  if (acquire_shared_mdl_for_trigger(thd, trg_name->m_db.str,
                                     trg_name->m_name.str))
    return true;

  Table_ref *lst = get_trigger_table(thd, trg_name);

  if (!lst) return true;

  if (check_table_access(thd, TRIGGER_ACL, lst, false, 1, true)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "TRIGGER");
    return true;
  }

  DEBUG_SYNC(thd, "show_create_trigger_before_table_lock");

  if (acquire_mdl_for_table(thd, trg_name->m_db.str, lst->table_name))
    return true;

  /*
    Open the table by name in order to load Table_trigger_dispatcher object.
  */
  if (open_tables(thd, &lst, &num_tables,
                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL)) {
    my_error(ER_TRG_CANT_OPEN_TABLE, MYF(0), trg_name->m_db.str,
             lst->table_name);

    goto exit;

    /* Perform closing actions and return error status. */
  }

  if (!lst->table->s->triggers) {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    goto exit;
  }

  trigger = find_trigger_in_share(lst->table->s, trg_name->m_name);

  if (!trigger) {
    my_error(ER_TRG_CORRUPTED_FILE, MYF(0), trg_name->m_db.str,
             lst->table_name);

    goto exit;
  }

  error = show_create_trigger_impl(thd, trigger);

  /*
    NOTE: if show_create_trigger_impl() failed, that means we could not
    send data to the client. In this case we simply raise the error
    status and client connection will be closed.
  */

exit:
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE TRIGGER. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  return error;
}

static IS_internal_schema_access is_internal_schema_access;

void initialize_information_schema_acl() {
  ACL_internal_schema_registry::register_schema(INFORMATION_SCHEMA_NAME,
                                                &is_internal_schema_access);
}

/*
  Convert a string in character set in column character set format
  to utf8 character set if possible, the utf8 character set string
  will later possibly be converted to character set used by client.
  Thus we attempt conversion from column character set to both
  utf8 and to character set client.

  Examples of strings that should fail conversion to utf8 are unassigned
  characters as e.g. 0x81 in cp1250 (Windows character set for for countries
  like Czech and Poland). Example of string that should fail conversion to
  character set on client (e.g. if this is latin1) is 0x2020 (daggger) in
  ucs2.

  If the conversion fails we will as a fall back convert the string to
  hex encoded format. The caller of the function can also ask for hex
  encoded format of output string unconditionally.

  SYNOPSIS
    get_cs_converted_string_value()
    thd                             Thread object
    input_str                       Input string in cs character set
    output_str                      Output string to be produced in utf8
    cs                              Character set of input string
    use_hex                         Use hex string unconditionally


  RETURN VALUES
    No return value
*/

static void get_cs_converted_string_value(THD *thd, String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs,
                                          bool use_hex) {
  output_str->length(0);
  if (input_str->length() == 0) {
    output_str->append("''");
    return;
  }
  // Note that since the String charset conversion functions used below, are
  // created to be used as (potentially unsafe) "casts" for user data, they
  // perform conversion from binary to UTF-8 by simply copying bytes. By
  // forcing a hex string when cs is binary, we avoid creating strings that are
  // invalid. Such invalid strings looks strange and will cause asserts when
  // they are stored in TEXT columns in the DD.
  if (!use_hex && cs != &my_charset_bin) {
    String try_val;
    uint try_conv_error = 0;

    try_val.copy(input_str->ptr(), input_str->length(), cs,
                 thd->variables.character_set_client, &try_conv_error);
    if (!try_conv_error) {
      String val;
      uint conv_error = 0;

      val.copy(input_str->ptr(), input_str->length(), cs, system_charset_info,
               &conv_error);
      if (!conv_error) {
        append_unescaped(output_str, val.ptr(), val.length());
        return;
      }
    }
    /* We had a conversion error, use hex encoded string for safety */
  }
  {
    const uchar *ptr;
    size_t i, len;
    char buf[3];

    output_str->append("_");
    output_str->append(cs->csname);
    output_str->append(" ");
    output_str->append("0x");
    len = input_str->length();
    ptr = (uchar *)input_str->ptr();
    for (i = 0; i < len; i++) {
      uint high, low;

      high = (*ptr) >> 4;
      low = (*ptr) & 0x0F;
      buf[0] = dig_vec_upper[high];
      buf[1] = dig_vec_upper[low];
      buf[2] = 0;
      output_str->append((const char *)buf);
      ptr++;
    }
  }
}

/**
  A field's SQL type printout

  @param type     the type to print
  @param is_array whether the field is a typed array
  @param metadata field's metadata, depending on the type
                  could be nothing, length, or length + decimals
  @param str      String to print to
  @param field_cs field's charset. When given [var]char length is printed in
                  characters, otherwise - in bytes
  @param vector_dimensionality The dimensionality of a vector field

*/

void show_sql_type(enum_field_types type, bool is_array, uint metadata,
                   String *str, const CHARSET_INFO *field_cs,
                   unsigned int vector_dimensionality) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("type: %d, metadata: 0x%x", type, metadata));

  switch (type) {
    case MYSQL_TYPE_BOOL:
      str->set_ascii(STRING_WITH_LEN("boolean"));
      break;

    case MYSQL_TYPE_TINY:
      str->set_ascii(STRING_WITH_LEN("tinyint"));
      break;

    case MYSQL_TYPE_SHORT:
      str->set_ascii(STRING_WITH_LEN("smallint"));
      break;

    case MYSQL_TYPE_LONG:
      str->set_ascii(STRING_WITH_LEN("int"));
      break;

    case MYSQL_TYPE_FLOAT:
      str->set_ascii(STRING_WITH_LEN("float"));
      break;

    case MYSQL_TYPE_DOUBLE:
      str->set_ascii(STRING_WITH_LEN("double"));
      break;

    case MYSQL_TYPE_NULL:
      str->set_ascii(STRING_WITH_LEN("null"));
      break;

    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      str->set_ascii(STRING_WITH_LEN("timestamp"));
      break;

    case MYSQL_TYPE_LONGLONG:
      str->set_ascii(STRING_WITH_LEN("bigint"));
      break;

    case MYSQL_TYPE_INT24:
      str->set_ascii(STRING_WITH_LEN("mediumint"));
      break;

    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
      str->set_ascii(STRING_WITH_LEN("date"));
      break;

    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      str->set_ascii(STRING_WITH_LEN("time"));
      break;

    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
      str->set_ascii(STRING_WITH_LEN("datetime"));
      break;

    case MYSQL_TYPE_YEAR:
      str->set_ascii(STRING_WITH_LEN("year"));
      break;

    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR: {
      const CHARSET_INFO *cs = str->charset();
      size_t length;
      if (field_cs)
        length =
            cs->cset->snprintf(cs, str->ptr(), str->alloced_length(),
                               "varchar(%u)", metadata / field_cs->mbmaxlen);
      else
        length = cs->cset->snprintf(cs, str->ptr(), str->alloced_length(),
                                    "varchar(%u(bytes))", metadata);
      str->length(length);
    } break;

    case MYSQL_TYPE_BIT: {
      const CHARSET_INFO *cs = str->charset();
      const int bit_length = 8 * (metadata >> 8) + (metadata & 0xFF);
      const size_t length = cs->cset->snprintf(
          cs, str->ptr(), str->alloced_length(), "bit(%d)", bit_length);
      str->length(length);
    } break;

    case MYSQL_TYPE_DECIMAL: {
      const CHARSET_INFO *cs = str->charset();
      const size_t length = cs->cset->snprintf(
          cs, str->ptr(), str->alloced_length(), "decimal(%d,?)", metadata);
      str->length(length);
    } break;

    case MYSQL_TYPE_NEWDECIMAL: {
      const CHARSET_INFO *cs = str->charset();
      const uint len = (metadata >> 8) & 0xff;
      const uint dec = metadata & 0xff;
      const size_t length = cs->cset->snprintf(
          cs, str->ptr(), str->alloced_length(), "decimal(%d,%d)", len, dec);
      str->length(length);
    } break;

    case MYSQL_TYPE_ENUM:
      str->set_ascii(STRING_WITH_LEN("enum"));
      break;

    case MYSQL_TYPE_SET:
      str->set_ascii(STRING_WITH_LEN("set"));
      break;

    case MYSQL_TYPE_TINY_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("tinyblob"));
      else
        str->set_ascii(STRING_WITH_LEN("tinytext"));
      break;

    case MYSQL_TYPE_MEDIUM_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("mediumblob"));
      else
        str->set_ascii(STRING_WITH_LEN("mediumtext"));
      break;

    case MYSQL_TYPE_LONG_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("longblob"));
      else
        str->set_ascii(STRING_WITH_LEN("longtext"));
      break;

    case MYSQL_TYPE_VECTOR: {
      const CHARSET_INFO *cs = str->charset();
      size_t length = cs->cset->snprintf(cs, str->ptr(), str->alloced_length(),
                                         "vector(%u)", vector_dimensionality);
      str->length(length);
      break;
    }

    case MYSQL_TYPE_BLOB:
      /*
        Field::real_type() lies regarding the actual type of a BLOB, so
        it is necessary to check the pack length to figure out what kind
        of blob it really is.
        Non-'BLOB' is handled above.
       */
      switch (metadata) {
        case 1:
          str->set_ascii(STRING_WITH_LEN("tinyblob"));
          break;

        case 3:
          str->set_ascii(STRING_WITH_LEN("mediumblob"));
          break;

        case 4:
          str->set_ascii(STRING_WITH_LEN("longblob"));
          break;

        default:
        case 2:
          if (!field_cs || field_cs == &my_charset_bin)
            str->set_ascii(STRING_WITH_LEN("blob"));
          else
            str->set_ascii(STRING_WITH_LEN("text"));
          break;
      }
      break;

    case MYSQL_TYPE_STRING: {
      /*
        This is taken from Field_string::unpack.
      */
      const CHARSET_INFO *cs = str->charset();
      const uint bytes =
          (((metadata >> 4) & 0x300) ^ 0x300) + (metadata & 0x00ff);
      size_t length;
      if (field_cs)
        length = cs->cset->snprintf(cs, str->ptr(), str->alloced_length(),
                                    "char(%d)", bytes / field_cs->mbmaxlen);
      else
        length = cs->cset->snprintf(cs, str->ptr(), str->alloced_length(),
                                    "char(%d(bytes))", bytes);
      str->length(length);
    } break;

    case MYSQL_TYPE_GEOMETRY:
      str->set_ascii(STRING_WITH_LEN("geometry"));
      break;

    case MYSQL_TYPE_JSON:
      str->set_ascii(STRING_WITH_LEN("json"));
      break;

    case MYSQL_TYPE_INVALID:
    default:
      str->set_ascii(STRING_WITH_LEN("<unknown type>"));
  }
  if (is_array) str->append(STRING_WITH_LEN(" array"));
}
