/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/member_actions_handler.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/member_actions_handler_configuration.h"

/*
  receive function
*/
DEFINE_BOOL_METHOD(member_actions_handler_receive,
                   (const char *tag, const unsigned char *data,
                    size_t data_length)) {
  return member_actions_handler->receive(tag, data, data_length);
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_message_service_recv)
member_actions_handler_receive, END_SERVICE_IMPLEMENTATION();

/*
  Handler implementation, aka public API.
*/
Member_actions_handler::Member_actions_handler() {
  m_configuration = new Member_actions_handler_configuration(this);
}

Member_actions_handler::~Member_actions_handler() { delete m_configuration; }

bool Member_actions_handler::init() {
  DBUG_TRACE;

  // Create worker thread.
  assert(nullptr == m_mysql_thread);
  m_mysql_thread = new Mysql_thread(
      key_GR_THD_mysql_thread, key_GR_LOCK_mysql_thread_run,
      key_GR_COND_mysql_thread_run, key_GR_LOCK_mysql_thread_dispatcher_run,
      key_GR_COND_mysql_thread_dispatcher_run);
  if (m_mysql_thread->initialize()) {
    return true; /* purecov: inspected */
  }

  // Register listener on recv service.
  my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", get_plugin_registry());
  using group_replication_message_service_recv_t =
      SERVICE_TYPE_NO_CONST(group_replication_message_service_recv);
  if (registrator->register_service(
          m_message_service_listener_name,
          reinterpret_cast<my_h_service>(
              const_cast<group_replication_message_service_recv_t *>(
                  &SERVICE_IMPLEMENTATION(
                      group_replication,
                      group_replication_message_service_recv))))) {
    return true; /* purecov: inspected */
  }

  return false;
}

bool Member_actions_handler::deinit() {
  DBUG_TRACE;

  // Unregister listener on recv service.
  my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", get_plugin_registry());
  bool result = registrator->unregister(m_message_service_listener_name) != 0;

  // Terminate worker thread.
  if (nullptr != m_mysql_thread) {
    result |= m_mysql_thread->terminate();
    delete m_mysql_thread;
    m_mysql_thread = nullptr;
  }

  return result;
}

bool Member_actions_handler::acquire_send_service() {
  DBUG_TRACE;
  DBUG_EXECUTE_IF("gr_message_service_disable_send", return false;);

  if (nullptr != m_group_replication_message_service_send) {
    return true; /* purecov: inspected */
  }

  // Acquire send service.
  my_h_service h_group_replication_message_service_send = nullptr;
  if (get_plugin_registry()->acquire(
          "group_replication_message_service_send",
          &h_group_replication_message_service_send) ||
      nullptr == h_group_replication_message_service_send) {
    /* purecov: begin inspected */
    m_group_replication_message_service_send = nullptr;
    return true;
    /* purecov: end */
  }
  m_group_replication_message_service_send =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(
          group_replication_message_service_send) *>(
          h_group_replication_message_service_send);

  return false;
}

bool Member_actions_handler::release_send_service() {
  DBUG_TRACE;
  bool result = false;

  // Release send service.
  if (nullptr != m_group_replication_message_service_send) {
    my_h_service h_group_replication_message_service_send =
        reinterpret_cast<my_h_service>(
            m_group_replication_message_service_send);
    result |= get_plugin_registry()->release(
                  h_group_replication_message_service_send) != 0;
    m_group_replication_message_service_send = nullptr;
  }

  return result;
}

bool Member_actions_handler::receive(const char *tag, const unsigned char *data,
                                     size_t data_length) {
  DBUG_TRACE;

  if (!strcmp(tag, m_message_tag)) {
    protobuf_replication_group_member_actions::ActionList action_list;
    if (!action_list.ParseFromArray(data, data_length)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_PARSE_ON_RECEIVE);
      return true;
      /* purecov: end */
    }

    /*
      The sender already updated its configuration so skip it.
    */
    if (local_member_info->get_uuid().compare(action_list.origin())) {
      if (m_configuration->update_all_actions(action_list)) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_UPDATE_ACTIONS);
        return true;
      }
    }
  }

  return false;
}

bool Member_actions_handler::reset_to_default_actions_configuration() {
  DBUG_TRACE;
  bool error = m_configuration->reset_to_default_actions_configuration();

  if (!error) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_ACTIONS_RESET);
  }

  return error;
}

bool Member_actions_handler::replace_all_actions(
    const std::vector<std::string>
        &exchanged_members_actions_serialized_configuration) {
  DBUG_TRACE;

  if (exchanged_members_actions_serialized_configuration.size() < 1) {
    /*
      This member joined a group on which all members do not support
      member actions, as such this member needs to reset its actions
      configuration to the default one.
      This will happen on all new members that join the group, after
      the old members leave, all new members will have the default
      configuration.
    */
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_MEMBER_ACTION_DEFAULT_CONFIGURATION);
    if (m_configuration->reset_to_default_actions_configuration()) {
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_MEMBER_ACTION_UNABLE_TO_SET_DEFAULT_CONFIGURATION);
      return true;
    }
    return false;
  }

  /*
    Since we receive the member actions from all non-joining members,
    and actions changes may be being propagated concurrently with
    membership changes, we need to choose the actions with higher version.
  */
  protobuf_replication_group_member_actions::ActionList
      action_list_with_higher_version;
  action_list_with_higher_version.set_version(0);

  for (std::string member_actions_serialized_configuration :
       exchanged_members_actions_serialized_configuration) {
    protobuf_replication_group_member_actions::ActionList action_list;
    if (!action_list.ParseFromString(member_actions_serialized_configuration)) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_PARSE_ON_MEMBER_JOIN);
      continue;
    }

    if (action_list.version() > action_list_with_higher_version.version()) {
      action_list_with_higher_version.CopyFrom(action_list);
    }
  }

  if (action_list_with_higher_version.version() < 1) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MEMBER_ACTION_INVALID_ACTIONS_ON_MEMBER_JOIN);
    return true;
  }

  if (m_configuration->replace_all_actions(action_list_with_higher_version)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MEMBER_ACTION_UPDATE_ACTIONS_ON_MEMBER_JOIN);
    return true;
  }

  return false;
}

bool Member_actions_handler::get_all_actions(
    std::string &serialized_configuration) {
  DBUG_TRACE;

  return m_configuration->get_all_actions(serialized_configuration, false);
}

bool Member_actions_handler::force_my_actions_configuration_on_all_members() {
  DBUG_TRACE;

  std::string serialized_configuration;
  if (m_configuration->get_all_actions(serialized_configuration, true)) {
    return true;
  }

  return propagate_serialized_configuration(serialized_configuration);
}

bool Member_actions_handler::propagate_serialized_configuration(
    const std::string &serialized_configuration) {
  DBUG_TRACE;
  DBUG_EXECUTE_IF("group_replication_force_error_on_member_actions_propagation",
                  return true;);

  if (!plugin_is_group_replication_running()) {
    return false; /* purecov: inspected */
  }
  assert(local_member_info->in_primary_mode() &&
         local_member_info->get_role() ==
             Group_member_info::MEMBER_ROLE_PRIMARY);

  bool error = m_group_replication_message_service_send->send(
      m_message_tag,
      pointer_cast<const unsigned char *>(serialized_configuration.data()),
      serialized_configuration.length());

  return error;
}

std::pair<bool, std::string> Member_actions_handler::enable_action(
    const std::string &name, const std::string &event) {
  DBUG_TRACE;
  return m_configuration->enable_disable_action(name, event, true);
}

std::pair<bool, std::string> Member_actions_handler::disable_action(
    const std::string &name, const std::string &event) {
  DBUG_TRACE;
  return m_configuration->enable_disable_action(name, event, false);
}

void Member_actions_handler::trigger_actions(
    Member_actions::enum_action_event event) {
  DBUG_TRACE;
  assert(local_member_info->in_primary_mode());

  Mysql_thread_task *task =
      new Mysql_thread_task(this, new Member_actions_trigger_parameters(event));
  m_mysql_thread->trigger(task);
  delete task;
}

void Member_actions_handler::run(Mysql_thread_body_parameters *parameters) {
  DBUG_TRACE;
  assert(local_member_info->in_primary_mode());

  // Identify the event.
  Member_actions_trigger_parameters *trigger_parameters =
      dynamic_cast<Member_actions_trigger_parameters *>(parameters);
  assert(Member_actions::AFTER_PRIMARY_ELECTION ==
         trigger_parameters->get_event());
  const std::string event =
      Member_actions::get_event_name(trigger_parameters->get_event());

  // Get the actions for the event.
  protobuf_replication_group_member_actions::ActionList action_list;
  m_configuration->get_actions_for_event(action_list, event);

  // Reorder the actions by priority.
  std::sort(action_list.mutable_action()->begin(),
            action_list.mutable_action()->end(),
            [](const protobuf_replication_group_member_actions::Action &a,
               const protobuf_replication_group_member_actions::Action &b) {
              return a.priority() < b.priority();
            });

  // Run enabled actions.
  for (const protobuf_replication_group_member_actions::Action &action :
       action_list.action()) {
    if (action.enabled() && action.type() == "INTERNAL") {
      LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_MEMBER_ACTION_TRIGGERED,
                   action.name().c_str(), event.c_str(), action.priority());

      int error = run_internal_action(action);

      if (error) {
        DBUG_EXECUTE_IF(
            "group_replication_force_action_error_handling_critical",
            goto error_handling_critical;);

        if (action.error_handling() == "IGNORE") {
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_FAILURE_IGNORE,
                       action.name().c_str(), event.c_str(), action.priority());
        } else {
          assert(action.error_handling() == "CRITICAL");
#if !defined(NDEBUG)
        error_handling_critical:
#endif
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MEMBER_ACTION_FAILURE,
                       action.name().c_str(), event.c_str(), action.priority());
          leave_group_on_failure::mask leave_actions;
          leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
          leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION,
                            true);
          leave_group_on_failure::leave(
              leave_actions, 0, nullptr,
              "Please check previous messages in the error log.");
        }
      }
    }
  }
}

int Member_actions_handler::run_internal_action(
    const protobuf_replication_group_member_actions::Action &action) {
  DBUG_TRACE;
  int error = 0;
  const bool im_the_primary =
      (local_member_info->in_primary_mode() &&
       local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY);

  if (action.name() == "mysql_disable_super_read_only_if_primary") {
    if (im_the_primary) {
      error = disable_server_read_mode();

      DBUG_EXECUTE_IF(
          "group_replication_force_error_on_mysql_disable_super_read_only_if_"
          "primary",
          error = 1;);

      if (error) {
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_DISABLE_READ_ONLY_FAILED);
      }

      return error;
    }
  }

  if (action.name() == "mysql_start_failover_channels_if_primary") {
    if (im_the_primary) {
      return start_failover_channels();
    }
  }

  return error;
}
