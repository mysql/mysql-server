/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/member_actions_handler_configuration.h"
#include "plugin/group_replication/include/plugin.h"
#include "sql/rpl_sys_key_access.h"

void Member_actions_handler_configuration::field_store(
    Field *field, const std::string &value) {
  field->set_notnull();
  field->store(value.c_str(), value.length(), &my_charset_bin);
}

void Member_actions_handler_configuration::field_store(Field *field,
                                                       uint value) {
  field->set_notnull();
  field->store(value, true);
}

Member_actions_handler_configuration::Member_actions_handler_configuration(
    Configuration_propagation *configuration_propagation)
    : m_configuration_propagation(configuration_propagation) {}

Member_actions_handler_configuration::~Member_actions_handler_configuration() {}

std::pair<bool, std::string>
Member_actions_handler_configuration::enable_disable_action(
    const std::string &name, const std::string &event, bool enable) {
  DBUG_TRACE;

  if (event.compare("AFTER_PRIMARY_ELECTION")) {
    return std::make_pair<bool, std::string>(true, "Invalid event name.");
  }

  Rpl_sys_table_access table_op(s_schema_name, s_table_name, s_fields_number);
  if (table_op.open(TL_WRITE)) {
    /* purecov: begin inspected */
    return std::make_pair<bool, std::string>(
        true, "Unable to open configuration persistence.");
    /* purecov: end */
  }

  TABLE *table = table_op.get_table();
  Field **fields = table->field;
  field_store(fields[0], name);
  field_store(fields[1], event);

  Rpl_sys_key_access key_access;
  int error = key_access.init(table);

  if (HA_ERR_KEY_NOT_FOUND == error) {
    return std::make_pair<bool, std::string>(
        true, "The action does not exist for this event.");
  } else if (error) {
    /* purecov: begin inspected */
    return std::make_pair<bool, std::string>(
        true, "Unable to open configuration persistence.");
    /* purecov: end */
  }

  // read table values for log messages
  char buffer[MAX_FIELD_WIDTH];
  String string(buffer, sizeof(buffer), &my_charset_bin);
  table->field[3]->val_str(&string);
  std::string type(string.c_ptr_safe(), string.length());
  uint priority = static_cast<uint>(table->field[4]->val_int());
  table->field[5]->val_str(&string);
  std::string error_handling(string.c_ptr_safe(), string.length());

  // delete row
  error |= table->file->ha_delete_row(table->record[0]);

  // add updated row
  if (!error) {
    field_store(fields[2], enable);
    error |= table->file->ha_write_row(table->record[0]);
  }

  error |= static_cast<int>(key_access.deinit());

  if (!error) {
    error = table_op.increment_version();
  }

  if (!error && !table_op.get_error() &&
      plugin_is_group_replication_running()) {
    std::pair<bool, std::string> error_pair =
        commit_and_propagate_changes(table_op);
    if (error_pair.first) {
      return error_pair;
    }
  }

  error |= static_cast<int>(table_op.close(error));
  if (error) {
    return std::make_pair<bool, std::string>(
        true, "Unable to persist the configuration.");
  }

  if (enable) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_ACTION_ENABLED, name.c_str(),
                 type.c_str(), event.c_str(), priority, error_handling.c_str());
  } else {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_ACTION_DISABLED, name.c_str(),
                 type.c_str(), event.c_str(), priority, error_handling.c_str());
  }

  return std::make_pair<bool, std::string>(false, "");
}

std::pair<bool, std::string>
Member_actions_handler_configuration::commit_and_propagate_changes(
    Rpl_sys_table_access &table_op) {
  DBUG_TRACE;

  protobuf_replication_group_member_actions::ActionList action_list;
  if (get_all_actions_internal(table_op, action_list)) {
    return std::make_pair<bool, std::string>(
        true, "Unable to read the complete configuration.");
  }

  std::string serialized_configuration;
  if (!action_list.SerializeToString(&serialized_configuration)) {
    /* purecov: begin inspected */
    return std::make_pair<bool, std::string>(
        true, "Unable to serialize the configuration.");
    /* purecov: end */
  }

  if (table_op.close(false)) {
    return std::make_pair<bool, std::string>(
        true, "Unable to persist the configuration before propagation.");
  }

  if (m_configuration_propagation->propagate_serialized_configuration(
          serialized_configuration)) {
    return std::make_pair<bool, std::string>(
        true, "Unable to propagate the configuration.");
  }

  return std::make_pair<bool, std::string>(false, "");
}

bool Member_actions_handler_configuration::get_actions_for_event(
    protobuf_replication_group_member_actions::ActionList &action_list,
    const std::string &event) {
  DBUG_TRACE;
  bool error = false;

  Rpl_sys_table_access table_op(s_schema_name, s_table_name, s_fields_number);
  if (table_op.open(TL_READ)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  TABLE *table = table_op.get_table();
  Field **fields = table->field;
  field_store(fields[1], event);

  Rpl_sys_key_access key_access;
  int key_error = key_access.init(table, 1, true, 1, HA_READ_KEY_EXACT);
  if (!key_error) {
    char buffer[MAX_FIELD_WIDTH];
    String string(buffer, sizeof(buffer), &my_charset_bin);

    do {
      protobuf_replication_group_member_actions::Action *action =
          action_list.add_action();
      table->field[0]->val_str(&string);
      action->set_name(string.c_ptr_safe(), string.length());

      table->field[1]->val_str(&string);
      action->set_event(string.c_ptr_safe(), string.length());

      action->set_enabled(table->field[2]->val_int());

      table->field[3]->val_str(&string);
      action->set_type(string.c_ptr_safe(), string.length());

      uint priority = static_cast<uint>(table->field[4]->val_int());
      action->set_priority(priority);

      table->field[5]->val_str(&string);
      action->set_error_handling(string.c_ptr_safe(), string.length());
    } while (!key_access.next());
  } else if (HA_ERR_END_OF_FILE == key_error) {
    /* Table is empty, nothing to read. */
    /* purecov: begin inspected */
    assert(0);
    /* purecov: end */
  } else {
    return true; /* purecov: inspected */
  }

  error |= key_access.deinit();
  error |= table_op.close(error);

  return error;
}

bool Member_actions_handler_configuration::get_all_actions(
    std::string &serialized_configuration, bool set_force_update) {
  DBUG_TRACE;
  bool error = false;

  Rpl_sys_table_access table_op(s_schema_name, s_table_name, s_fields_number);
  if (table_op.open(TL_READ)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  protobuf_replication_group_member_actions::ActionList action_list;
  error = get_all_actions_internal(table_op, action_list);
  error |= table_op.close(error);

  action_list.set_force_update(set_force_update);

  if (!error) {
    error = !action_list.SerializeToString(&serialized_configuration);
  }

  return error;
}

bool Member_actions_handler_configuration::get_all_actions_internal(
    Rpl_sys_table_access &table_op,
    protobuf_replication_group_member_actions::ActionList &action_list) {
  DBUG_TRACE;

  action_list.set_origin(local_member_info->get_uuid());
  action_list.set_version(table_op.get_version());
  action_list.set_force_update(false);

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  int key_error =
      key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT);
  if (!key_error) {
    char buffer[MAX_FIELD_WIDTH];
    String string(buffer, sizeof(buffer), &my_charset_bin);

    do {
      protobuf_replication_group_member_actions::Action *action =
          action_list.add_action();
      table->field[0]->val_str(&string);
      action->set_name(string.c_ptr_safe(), string.length());

      table->field[1]->val_str(&string);
      action->set_event(string.c_ptr_safe(), string.length());

      action->set_enabled(table->field[2]->val_int());

      table->field[3]->val_str(&string);
      action->set_type(string.c_ptr_safe(), string.length());

      uint priority = static_cast<uint>(table->field[4]->val_int());
      action->set_priority(priority);

      table->field[5]->val_str(&string);
      action->set_error_handling(string.c_ptr_safe(), string.length());
    } while (!key_access.next());
  } else if (HA_ERR_END_OF_FILE == key_error) {
    /* Table is already empty, nothing to read. */
    /* purecov: begin inspected */
    assert(0);
    /* purecov: end */
  } else {
    return true; /* purecov: inspected */
  }

  key_access.deinit();

  assert(action_list.version() > 0);
  assert(action_list.action_size() > 0);
  return false;
}

bool Member_actions_handler_configuration::update_all_actions(
    const protobuf_replication_group_member_actions::ActionList &action_list) {
  DBUG_TRACE;
  return update_all_actions_internal(action_list, false, false);
}

bool Member_actions_handler_configuration::replace_all_actions(
    const protobuf_replication_group_member_actions::ActionList &action_list) {
  DBUG_TRACE;
  return update_all_actions_internal(action_list, true, true);
}

bool Member_actions_handler_configuration::update_all_actions_internal(
    const protobuf_replication_group_member_actions::ActionList &action_list,
    bool ignore_version, bool ignore_global_read_lock) {
  DBUG_TRACE;
  assert(action_list.version() > 0);
  assert(action_list.action_size() > 0);
  bool error = false;
  bool mysql_start_failover_channels_if_primary_updated = false;

  Rpl_sys_table_access table_op(s_schema_name, s_table_name, s_fields_number);

  /* Do everything on a single transaction. */
  if (table_op.open(TL_WRITE)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  /*
    `ignore_version = true` means that this server is joining a group,
    on which case the group configuration will override the local one.
    `force_update = true` means that this configuration was sent by the
    elected primary of a group mode change from multi to single-primary,
    as such this configuration must override the local one.
  */
  if (!ignore_version && !action_list.force_update()) {
    /*
      Only update the table if the received data has a greater version
      than the local one.
    */
    longlong local_version = table_op.get_version();
    longlong remote_version = action_list.version();

    if (local_version >= remote_version) {
      /* purecov: begin inspected */
      table_op.close(true);
      return false;
      /* purecov: end */
    }
  }

  /* Update version. */
  if (table_op.update_version(action_list.version())) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  TABLE *table = table_op.get_table();
  /* Delete all rows */
  Rpl_sys_key_access key_access;
  int key_error =
      key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT);
  if (!key_error) {
    do {
      error |= table->file->ha_delete_row(table->record[0]) != 0;
      if (error) {
        return true;
      }
    } while (!error && !key_access.next());
  } else if (HA_ERR_END_OF_FILE == key_error) {
    /* Table is already empty, nothing to delete. */
    /* purecov: begin inspected */
    assert(0);
    /* purecov: end */
  } else {
    return true;
  }

  error |= key_access.deinit();
  if (error) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  /* Write all rows. */
  Field **fields = table->field;
  for (const protobuf_replication_group_member_actions::Action &action :
       action_list.action()) {
    if (action.name().compare("mysql_start_failover_channels_if_primary") ==
        0) {
      mysql_start_failover_channels_if_primary_updated = true;
    }

    field_store(fields[0], action.name());
    field_store(fields[1], action.event());
    field_store(fields[2], action.enabled());
    field_store(fields[3], action.type());
    field_store(fields[4], action.priority());
    field_store(fields[5], action.error_handling());

    error |= table->file->ha_write_row(table->record[0]) != 0;
    if (error) {
      /* purecov: begin inspected */
      return true;
      /* purecov: end */
    }
  }

  /*
    When a 8.0.27+ server joins a 8.0.26 group, the joining server
    will not receive the action "mysql_start_failover_channels_if_primary",
    the group does not know it, as such we need to add its default value.
  */
  if (!mysql_start_failover_channels_if_primary_updated) {
    Field **fields = table->field;
    field_store(fields[0], "mysql_start_failover_channels_if_primary");
    field_store(fields[1], "AFTER_PRIMARY_ELECTION");
    field_store(fields[2], 1);
    field_store(fields[3], "INTERNAL");
    field_store(fields[4], 10);
    field_store(fields[5], "CRITICAL");

    error |= table->file->ha_write_row(table->record[0]) != 0;
    if (error) {
      /* purecov: begin inspected */
      return true;
      /* purecov: end */
    }
  }

  error |= table_op.close(error, ignore_global_read_lock);
  return error;
}

bool Member_actions_handler_configuration::
    reset_to_default_actions_configuration() {
  DBUG_TRACE;

  protobuf_replication_group_member_actions::ActionList action_list;
  action_list.set_version(1);
  action_list.set_force_update(false);

  protobuf_replication_group_member_actions::Action *action1 =
      action_list.add_action();
  action1->set_name("mysql_disable_super_read_only_if_primary");
  action1->set_event("AFTER_PRIMARY_ELECTION");
  action1->set_enabled(1);
  action1->set_type("INTERNAL");
  action1->set_priority(1);
  action1->set_error_handling("IGNORE");

  protobuf_replication_group_member_actions::Action *action2 =
      action_list.add_action();
  action2->set_name("mysql_start_failover_channels_if_primary");
  action2->set_event("AFTER_PRIMARY_ELECTION");
  action2->set_enabled(1);
  action2->set_type("INTERNAL");
  action2->set_priority(10);
  action2->set_error_handling("CRITICAL");

  return replace_all_actions(action_list);
}
