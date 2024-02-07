/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <unordered_map>

#include "sql/command_mapping.h"
#include "sql/sql_parse.h"

namespace {
class Command_maps final {
 public:
  Command_maps() {
    for (unsigned int i = 0; i < static_cast<unsigned int>(COM_END); ++i) {
      server_command_map[Command_names::str_global(
                             static_cast<enum_server_command>(i))
                             .c_str()] = static_cast<enum_server_command>(i);
    }
  }

  enum_server_command get_server_command(const char *server_command) {
    auto it = server_command_map.find(server_command);
    return (it != server_command_map.end() ? it->second : COM_END);
  }

  static const char *sql_commands[static_cast<unsigned int>(SQLCOM_END)];

 private:
  std::unordered_map<const char *, enum_server_command> server_command_map;
};

const char *Command_maps::sql_commands[] = {"select",
                                            "create_table",
                                            "create_index",
                                            "alter_table",
                                            "update",
                                            "insert",
                                            "insert_select",
                                            "delete",
                                            "truncate",
                                            "drop_table",
                                            "drop_index",
                                            "show_databases",
                                            "show_tables",
                                            "show_fields",
                                            "show_keys",
                                            "show_variables",
                                            "show_status",
                                            "show_engine_logs",
                                            "show_engine_status",
                                            "show_engine_mutex",
                                            "show_processlist",
                                            "show_master_stat",
                                            "show_slave_stat",
                                            "show_grants",
                                            "show_create",
                                            "show_charsets",
                                            "show_collations",
                                            "show_create_db",
                                            "show_table_status",
                                            "show_triggers",
                                            "load",
                                            "set_option",
                                            "lock_tables",
                                            "unlock_tables",
                                            "grant",
                                            "change_db",
                                            "create_db",
                                            "drop_db",
                                            "alter_db",
                                            "repair",
                                            "replace",
                                            "replace_select",
                                            "create_function",
                                            "drop_function",
                                            "revoke",
                                            "optimize",
                                            "check",
                                            "assign_to_keycache",
                                            "preload_keys",
                                            "flush",
                                            "kill",
                                            "analyze",
                                            "rollback",
                                            "rollback_to_savepoint",
                                            "commit",
                                            "savepoint",
                                            "release_savepoint",
                                            "slave_start",
                                            "slave_stop",
                                            "start_group_replication",
                                            "stop_group_replication",
                                            "begin",
                                            "change_master",
                                            "change_replication_filter",
                                            "rename_table",
                                            "reset",
                                            "purge",
                                            "purge_before",
                                            "show_binlogs",
                                            "show_open_tables",
                                            "ha_open",
                                            "ha_close",
                                            "ha_read",
                                            "show_slave_hosts",
                                            "delete_multi",
                                            "update_multi",
                                            "show_binlog_events",
                                            "do",
                                            "show_warns",
                                            "empty_query",
                                            "show_errors",
                                            "show_storage_engines",
                                            "show_privileges",
                                            "help",
                                            "create_user",
                                            "drop_user",
                                            "rename_user",
                                            "revoke_all",
                                            "checksum",
                                            "create_procedure",
                                            "create_spfunction",
                                            "call",
                                            "drop_procedure",
                                            "alter_procedure",
                                            "alter_function",
                                            "show_create_proc",
                                            "show_create_func",
                                            "show_status_proc",
                                            "show_status_func",
                                            "prepare",
                                            "execute",
                                            "deallocate_prepare",
                                            "create_view",
                                            "drop_view",
                                            "create_trigger",
                                            "drop_trigger",
                                            "xa_start",
                                            "xa_end",
                                            "xa_prepare",
                                            "xa_commit",
                                            "xa_rollback",
                                            "xa_recover",
                                            "show_proc_code",
                                            "show_func_code",
                                            "alter_tablespace",
                                            "install_plugin",
                                            "uninstall_plugin",
                                            "binlog_base64_event",
                                            "show_plugins",
                                            "create_server",
                                            "drop_server",
                                            "alter_server",
                                            "create_event",
                                            "alter_event",
                                            "drop_event",
                                            "show_create_event",
                                            "show_events",
                                            "show_create_trigger",
                                            "show_profile",
                                            "show_profiles",
                                            "signal",
                                            "resignal",
                                            "show_relaylog_events",
                                            "get_diagnostics",
                                            "alter_user",
                                            "explain_other",
                                            "show_create_user",
                                            "shutdown",
                                            "set_password",
                                            "alter_instance",
                                            "install_component",
                                            "uninstall_component",
                                            "create_role",
                                            "drop_role",
                                            "set_role",
                                            "grant_role",
                                            "revoke_role",
                                            "alter_user_default_role",
                                            "import",
                                            "create_resource_group",
                                            "alter_resource_group",
                                            "drop_resource_group",
                                            "set_resource_group",
                                            "clone",
                                            "lock_instance",
                                            "unlock_instance",
                                            "restart_server",
                                            "create_srs",
                                            "drop_srs"};

Command_maps *g_command_maps{nullptr};
}  // namespace

void init_command_maps() {
  if (!g_command_maps) g_command_maps = new (std::nothrow) Command_maps();
}
void denit_command_maps() {
  if (g_command_maps) delete g_command_maps;
  g_command_maps = nullptr;
}

const char *get_server_command_string(enum_server_command server_command) {
  return Command_names::str_global(server_command).c_str();
}

enum_server_command get_server_command(const char *server_command) {
  return g_command_maps ? g_command_maps->get_server_command(server_command)
                        : COM_END;
}

const char *get_sql_command_string(enum_sql_command sql_command) {
  static_assert(((size_t)(SQLCOM_END - SQLCOM_SELECT)) ==
                (sizeof(Command_maps::sql_commands) / sizeof(char *)));
  return Command_maps::sql_commands[sql_command];
}
