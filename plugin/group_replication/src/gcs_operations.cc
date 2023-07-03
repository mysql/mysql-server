/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/gcs_operations.h"

#include <stddef.h>
#include <vector>

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/transaction_message.h"

const std::string Gcs_operations::gcs_engine = "xcom";

Gcs_operations::Gcs_operations()
    : auth_provider(),
      native_interface(),
      gcs_mysql_net_provider(nullptr),
      gcs_interface(nullptr),
      injected_view_modification(false),
      leave_coordination_leaving(false),
      leave_coordination_left(false) {
  gcs_operations_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_gcs_operations
#endif
  );
  view_observers_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_gcs_operations_view_change_observers
#endif
  );
}

Gcs_operations::~Gcs_operations() {
  delete gcs_operations_lock;
  delete view_observers_lock;
}

int Gcs_operations::initialize() {
  DBUG_TRACE;
  int error = 0;
  Gcs_interface_runtime_requirements reqs;

  gcs_operations_lock->wrlock();

  leave_coordination_leaving = false;
  leave_coordination_left = false;

  assert(gcs_interface == nullptr);
  if ((gcs_interface = Gcs_interface_factory::get_interface_implementation(
           gcs_engine)) == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_COMMUNICATION_ENG_INIT_FAILED,
                 gcs_engine.c_str());
    error = GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
    goto end;
    /* purecov: end */
  }

  if (gcs_interface->set_logger(&gcs_logger)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_SET_GRP_COMMUNICATION_ENG_LOGGER_FAILED);
    error = GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
    goto end;
    /* purecov: end */
  }

  // Setup resources and pass them to GCS. In the future, refactor to also pass
  // the logger.
  if (gcs_mysql_net_provider ==
      nullptr) {  // finalize might have not been called.
    gcs_mysql_net_provider = std::make_shared<Gcs_mysql_network_provider>(
        &auth_provider, &native_interface);
  }

  reqs.provider = gcs_mysql_net_provider;
  reqs.namespace_manager = &native_interface;
  gcs_interface->setup_runtime_resources(reqs);
end:
  gcs_operations_lock->unlock();
  return error;
}

void Gcs_operations::finalize() {
  DBUG_TRACE;
  gcs_operations_lock->wrlock();

  if (gcs_interface != nullptr) gcs_interface->finalize();

  // Remove the provider since it is owned by the plugin.
  if (gcs_interface != nullptr) {
    Gcs_interface_runtime_requirements reqs;
    reqs.provider = gcs_mysql_net_provider;
    gcs_interface->cleanup_runtime_resources(reqs);
  }

  Gcs_interface_factory::cleanup(gcs_engine);
  gcs_interface = nullptr;

  // Will destroy gcs_mysql_net_provider if it is still alive after finalize
  gcs_mysql_net_provider = nullptr;

  gcs_operations_lock->unlock();
}

enum enum_gcs_error Gcs_operations::configure(
    const Gcs_interface_parameters &parameters) {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;
  gcs_operations_lock->wrlock();

  if (gcs_interface != nullptr) {
    error = gcs_interface->initialize(parameters);

    // This forces GCS to create group interfaces under a RW lock.
    if (gcs_interface->is_initialized()) {
      std::string group_name(get_group_name_var());
      Gcs_group_identifier group_id(group_name);
      gcs_interface->get_communication_session(group_id);
    }
  }

  gcs_operations_lock->unlock();
  return error;
}

enum enum_gcs_error Gcs_operations::reconfigure(
    const Gcs_interface_parameters &parameters) {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;
  gcs_operations_lock->wrlock();

  if (gcs_interface != nullptr) error = gcs_interface->configure(parameters);

  gcs_operations_lock->unlock();
  return error;
}

enum enum_gcs_error Gcs_operations::do_set_debug_options(
    std::string &debug_options) const {
  int64_t res_debug_options;
  enum enum_gcs_error error = GCS_NOK;

  if (!Gcs_debug_options::get_debug_options(debug_options, res_debug_options)) {
    debug_options.clear();
    Gcs_debug_options::force_debug_options(res_debug_options);
    Gcs_debug_options::get_debug_options(res_debug_options, debug_options);
    error = GCS_OK;

    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_DEBUG_OPTIONS,
                 debug_options.c_str());
  } else {
    std::string str_debug_options;
    Gcs_debug_options::get_current_debug_options(str_debug_options);

    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INVALID_DEBUG_OPTIONS,
                 debug_options.c_str());
  }

  return error;
}

enum enum_gcs_error Gcs_operations::set_debug_options(
    std::string &debug_options) const {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;

  gcs_operations_lock->wrlock();

  error = do_set_debug_options(debug_options);

  gcs_operations_lock->unlock();
  return error;
}

enum enum_gcs_error Gcs_operations::join(
    const Gcs_communication_event_listener &communication_event_listener,
    const Gcs_control_event_listener &control_event_listener,
    Plugin_gcs_view_modification_notifier *view_notifier) {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;
  gcs_operations_lock->wrlock();

  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    return GCS_NOK;
    /* purecov: end */
  }

  std::string group_name(get_group_name_var());
  Gcs_group_identifier group_id(group_name);

  Gcs_communication_interface *gcs_communication =
      gcs_interface->get_communication_session(group_id);
  Gcs_control_interface *gcs_control =
      gcs_interface->get_control_session(group_id);

  if (gcs_communication == nullptr || gcs_control == nullptr) {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    return GCS_NOK;
    /* purecov: end */
  }

  gcs_control->add_event_listener(control_event_listener);
  gcs_communication->add_event_listener(communication_event_listener);
  view_observers_lock->wrlock();
  injected_view_modification = false;
  view_change_notifier_list.push_back(view_notifier);
  view_observers_lock->unlock();

  /*
    Fake a GCS join error by not invoking join(), the
    view_change_notifier will error out and return a error on
    START GROUP_REPLICATION command.
  */
  DBUG_EXECUTE_IF("group_replication_inject_gcs_join_error", {
    gcs_operations_lock->unlock();
    return GCS_OK;
  };);

  error = gcs_control->join();

  gcs_operations_lock->unlock();
  return error;
}

bool Gcs_operations::belongs_to_group() {
  DBUG_TRACE;
  bool res = false;
  gcs_operations_lock->rdlock();

  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control =
        gcs_interface->get_control_session(group_id);

    if (gcs_control != nullptr && gcs_control->belongs_to_group()) res = true;
  }

  gcs_operations_lock->unlock();
  return res;
}

Gcs_operations::enum_leave_state Gcs_operations::leave(
    Plugin_gcs_view_modification_notifier *view_notifier) {
  DBUG_TRACE;
  enum_leave_state state = ERROR_WHEN_LEAVING;
  gcs_operations_lock->wrlock();

  if (leave_coordination_left) {
    state = ALREADY_LEFT;
    goto end;
  }

  view_observers_lock->wrlock();
  injected_view_modification = false;
  if (nullptr != view_notifier)
    view_change_notifier_list.push_back(view_notifier);
  view_observers_lock->unlock();

  if (leave_coordination_leaving) {
    state = ALREADY_LEAVING;
    goto end;
  }

  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control =
        gcs_interface->get_control_session(group_id);

    if (gcs_control != nullptr) {
      if (!gcs_control->leave()) {
        state = NOW_LEAVING;
        leave_coordination_leaving = true;
        goto end;
      }
    } else {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_EXIT_GRP_GCS_ERROR);
      goto end;
      /* purecov: end */
    }
  } else {  // This is the "something went wrong branch". We will avoid
            // incoming connections to ensure that we do not end in a deadlock,
            // thus de-registering the incoming connection callback
    if (gcs_mysql_net_provider != nullptr) {
      gcs_mysql_net_provider->stop();
    }
  }

end:
  gcs_operations_lock->unlock();
  return state;
}

void Gcs_operations::notify_of_view_change_end() {
  view_observers_lock->rdlock();
  for (Plugin_gcs_view_modification_notifier *view_notifier :
       view_change_notifier_list) {
    view_notifier->end_view_modification();
  }
  view_observers_lock->unlock();
}

void Gcs_operations::notify_of_view_change_cancellation(int error) {
  view_observers_lock->rdlock();
  for (Plugin_gcs_view_modification_notifier *view_notifier :
       view_change_notifier_list) {
    view_notifier->cancel_view_modification(error);
  }
  view_observers_lock->unlock();
}

bool Gcs_operations::is_injected_view_modification() {
  view_observers_lock->rdlock();
  bool result = injected_view_modification;
  view_observers_lock->unlock();
  return result;
}

void Gcs_operations::remove_view_notifer(
    Plugin_gcs_view_modification_notifier *view_notifier) {
  if (nullptr == view_notifier) return;

  view_observers_lock->wrlock();
  view_change_notifier_list.remove(view_notifier);
  view_observers_lock->unlock();
}

void Gcs_operations::leave_coordination_member_left() {
  DBUG_TRACE;

  leave_coordination_leaving = false;
  leave_coordination_left = true;
}

Gcs_view *Gcs_operations::get_current_view() {
  DBUG_TRACE;
  Gcs_view *view = nullptr;
  gcs_operations_lock->rdlock();

  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control =
        gcs_interface->get_control_session(group_id);

    if (gcs_control != nullptr && gcs_control->belongs_to_group())
      view = gcs_control->get_current_view();
  }

  gcs_operations_lock->unlock();
  return view;
}

int Gcs_operations::get_local_member_identifier(std::string &identifier) {
  DBUG_TRACE;
  int error = 1;
  gcs_operations_lock->rdlock();

  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control =
        gcs_interface->get_control_session(group_id);

    if (gcs_control != nullptr) {
      identifier.assign(
          gcs_control->get_local_member_identifier().get_member_id());
      error = 0;
    }
  }

  gcs_operations_lock->unlock();
  return error;
}

enum enum_gcs_error Gcs_operations::send_message(
    const Plugin_gcs_message &message, bool skip_if_not_initialized) {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;
  gcs_operations_lock->rdlock();

  /*
    Ensure that group communication interfaces are initialized
    and ready to use, since plugin can leave the group on errors
    but continue to be active.
  */
  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    gcs_operations_lock->unlock();
    return skip_if_not_initialized ? GCS_OK : GCS_NOK;
  }

  std::string group_name(get_group_name_var());
  Gcs_group_identifier group_id(group_name);

  Gcs_communication_interface *gcs_communication =
      gcs_interface->get_communication_session(group_id);
  Gcs_control_interface *gcs_control =
      gcs_interface->get_control_session(group_id);

  if (gcs_communication == nullptr || gcs_control == nullptr) {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    return skip_if_not_initialized ? GCS_OK : GCS_NOK;
    /* purecov: end */
  }

  std::vector<uchar> message_data;
  message.encode(&message_data);

  Gcs_member_identifier origin = gcs_control->get_local_member_identifier();
  Gcs_message gcs_message(origin, new Gcs_message_data(0, message_data.size()));
  gcs_message.get_message_data().append_to_payload(&message_data.front(),
                                                   message_data.size());
  error = gcs_communication->send_message(gcs_message);

  gcs_operations_lock->unlock();
  return error;
}

enum enum_gcs_error Gcs_operations::send_transaction_message(
    Transaction_message_interface &message) {
  DBUG_TRACE;
  enum enum_gcs_error error = GCS_NOK;
  gcs_operations_lock->rdlock();

  /*
    Ensure that group communication interfaces are initialized
    and ready to use, since plugin can leave the group on errors
    but continue to be active.
  */
  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    gcs_operations_lock->unlock();
    return GCS_NOK;
  }

  std::string group_name(get_group_name_var());
  Gcs_group_identifier group_id(group_name);

  Gcs_communication_interface *gcs_communication =
      gcs_interface->get_communication_session(group_id);
  Gcs_control_interface *gcs_control =
      gcs_interface->get_control_session(group_id);

  if (gcs_communication == nullptr || gcs_control == nullptr) {
    gcs_operations_lock->unlock();
    return GCS_NOK;
  }

  Gcs_member_identifier origin = gcs_control->get_local_member_identifier();
  Gcs_message_data *gcs_message_data = message.get_message_data_and_reset();
  if (nullptr == gcs_message_data) {
    gcs_operations_lock->unlock();
    return GCS_NOK;
  }
  Gcs_message gcs_message(origin, gcs_message_data);
  error = gcs_communication->send_message(gcs_message);

  gcs_operations_lock->unlock();
  return error;
}

Gcs_operations::enum_force_members_state Gcs_operations::force_members(
    const char *members) {
  DBUG_TRACE;
  enum_force_members_state error = FORCE_MEMBERS_OK;
  gcs_operations_lock->wrlock();

  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_MEMBER_OFFLINE);
    error = FORCE_MEMBERS_ER_MEMBER_NOT_ONLINE;
    goto end;
    /* purecov: end */
  }

  /*
     If we are already leaving the group, maybe because an error happened then
     it makes no sense to force a new membership in this member.
  */
  if (leave_coordination_leaving) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FORCE_MEMBERS_WHEN_LEAVING);
    error = FORCE_MEMBERS_ER_MEMBERS_WHEN_LEAVING;
    goto end;
  }

  if (local_member_info->get_recovery_status() ==
      Group_member_info::MEMBER_ONLINE) {
    std::string group_id_str(get_group_name_var());
    Gcs_group_identifier group_id(group_id_str);
    Gcs_group_management_interface *gcs_management =
        gcs_interface->get_management_session(group_id);

    if (gcs_management == nullptr) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_INTERFACE_ERROR);
      error = FORCE_MEMBERS_ER_INTERNAL_ERROR;
      goto end;
      /* purecov: end */
    }

    Plugin_gcs_view_modification_notifier view_change_notifier;
    view_change_notifier.start_view_modification();

    view_observers_lock->wrlock();
    injected_view_modification = true;
    view_change_notifier_list.push_back(&view_change_notifier);
    view_observers_lock->unlock();

    Gcs_interface_parameters gcs_interface_parameters;
    gcs_interface_parameters.add_parameter("peer_nodes", std::string(members));
    enum_gcs_error result =
        gcs_management->modify_configuration(gcs_interface_parameters);
    if (result != GCS_OK) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FORCE_MEMBER_VALUE_SET_ERROR,
                   members);
      error = FORCE_MEMBERS_ER_VALUE_SET_ERROR;
      view_change_notifier.cancel_view_modification();
      remove_view_notifer(&view_change_notifier);
      goto end;
      /* purecov: end */
    }
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_FORCE_MEMBER_VALUE_SET, members);
    if (view_change_notifier.wait_for_view_modification(
            FORCE_MEMBERS_VIEW_MODIFICATION_TIMEOUT)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FORCE_MEMBER_VALUE_TIME_OUT,
                   members);
      error = FORCE_MEMBERS_ER_TIMEOUT_ON_WAIT_FOR_VIEW;
      /* purecov: end */
    }
    remove_view_notifer(&view_change_notifier);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_MEMBER_OFFLINE);
    error = FORCE_MEMBERS_ER_MEMBER_NOT_ONLINE;
    goto end;
  }

end:
  gcs_operations_lock->unlock();
  return error;
}

Gcs_group_management_interface *Gcs_operations::get_gcs_group_manager() const {
  std::string const group_name(get_group_name_var());
  Gcs_group_identifier const group_id(group_name);
  Gcs_control_interface *gcs_control = nullptr;
  Gcs_group_management_interface *gcs_group_manager = nullptr;
  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    /* purecov: begin inspected */
    goto end;
    /* purecov: end */
  }
  gcs_control = gcs_interface->get_control_session(group_id);
  if (gcs_control == nullptr || !gcs_control->belongs_to_group()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_INTERFACE_ERROR);
    goto end;
    /* purecov: end */
  }
  gcs_group_manager = gcs_interface->get_management_session(group_id);
  if (gcs_group_manager == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_INTERFACE_ERROR);
    goto end;
    /* purecov: end */
  }
end:
  return gcs_group_manager;
}

enum enum_gcs_error Gcs_operations::get_write_concurrency(
    uint32_t &write_concurrency) {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->rdlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->get_write_concurrency(write_concurrency);
  }
  gcs_operations_lock->unlock();
  return result;
}

enum enum_gcs_error Gcs_operations::set_write_concurrency(
    uint32_t new_write_concurrency) {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->wrlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->set_write_concurrency(new_write_concurrency);
  }
  gcs_operations_lock->unlock();
  return result;
}

enum enum_gcs_error Gcs_operations::set_leader(
    Gcs_member_identifier const &leader) {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->wrlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->set_single_leader(leader);
  }

  gcs_operations_lock->unlock();
  return result;
}

enum enum_gcs_error Gcs_operations::set_everyone_leader() {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->wrlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->set_everyone_leader();
  }

  gcs_operations_lock->unlock();
  return result;
}

enum enum_gcs_error Gcs_operations::get_leaders(
    std::vector<Gcs_member_identifier> &preferred_leaders,
    std::vector<Gcs_member_identifier> &actual_leaders) {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->rdlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->get_leaders(preferred_leaders, actual_leaders);
  }

  gcs_operations_lock->unlock();
  return result;
}

uint32_t Gcs_operations::get_minimum_write_concurrency() const {
  DBUG_TRACE;
  uint32_t result = 0;
  gcs_operations_lock->rdlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->get_minimum_write_concurrency();
  }
  gcs_operations_lock->unlock();
  return result;
}

uint32_t Gcs_operations::get_maximum_write_concurrency() const {
  DBUG_TRACE;
  uint32_t result = 0;
  gcs_operations_lock->rdlock();
  Gcs_group_management_interface *gcs_group_manager = get_gcs_group_manager();
  if (gcs_group_manager != nullptr) {
    result = gcs_group_manager->get_maximum_write_concurrency();
  }
  gcs_operations_lock->unlock();
  return result;
}

Gcs_communication_interface *Gcs_operations::get_gcs_communication() const {
  std::string const group_name(get_group_name_var());
  Gcs_group_identifier const group_id(group_name);
  Gcs_control_interface *gcs_control = nullptr;
  Gcs_communication_interface *gcs_communication = nullptr;
  if (gcs_interface == nullptr || !gcs_interface->is_initialized()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_MEMBER_OFFLINE);
    goto end;
    /* purecov: end */
  }
  gcs_control = gcs_interface->get_control_session(group_id);
  if (gcs_control == nullptr || !gcs_control->belongs_to_group()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_INTERFACE_ERROR);
    goto end;
    /* purecov: end */
  }
  gcs_communication = gcs_interface->get_communication_session(group_id);
  if (gcs_communication == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_INTERFACE_ERROR);
    goto end;
    /* purecov: end */
  }
end:
  return gcs_communication;
}

Gcs_protocol_version Gcs_operations::get_protocol_version() {
  DBUG_TRACE;
  Gcs_protocol_version protocol = Gcs_protocol_version::UNKNOWN;
  gcs_operations_lock->rdlock();
  Gcs_communication_interface *gcs_communication = get_gcs_communication();
  if (gcs_communication != nullptr) {
    protocol = gcs_communication->get_protocol_version();
  }
  gcs_operations_lock->unlock();
  return protocol;
}

Gcs_protocol_version Gcs_operations::get_maximum_protocol_version() {
  DBUG_TRACE;
  Gcs_protocol_version protocol = Gcs_protocol_version::UNKNOWN;
  gcs_operations_lock->rdlock();
  Gcs_communication_interface *gcs_communication = get_gcs_communication();
  if (gcs_communication != nullptr) {
    protocol = gcs_communication->get_maximum_supported_protocol_version();
  }
  gcs_operations_lock->unlock();
  return protocol;
}

std::pair<bool, std::future<void>> Gcs_operations::set_protocol_version(
    Gcs_protocol_version gcs_protocol) {
  DBUG_TRACE;
  bool will_change_protocol = false;
  std::future<void> future;

  gcs_operations_lock->wrlock();
  Gcs_communication_interface *gcs_communication = get_gcs_communication();
  if (gcs_communication != nullptr) {
    std::tie(will_change_protocol, future) =
        gcs_communication->set_protocol_version(gcs_protocol);
  }
  gcs_operations_lock->unlock();

  return std::make_pair(will_change_protocol, std::move(future));
}

enum enum_gcs_error Gcs_operations::set_xcom_cache_size(uint64_t new_size) {
  DBUG_TRACE;
  enum enum_gcs_error result = GCS_NOK;
  gcs_operations_lock->wrlock();
  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control =
        gcs_interface->get_control_session(group_id);
    if (gcs_control != nullptr) {
      result = gcs_control->set_xcom_cache_size(new_size);
    }
  }
  gcs_operations_lock->unlock();
  return result;
}

enum_transport_protocol
Gcs_operations::get_current_incoming_connections_protocol() {
  DBUG_TRACE;
  enum_transport_protocol result = INVALID_PROTOCOL;
  gcs_operations_lock->rdlock();
  if (gcs_interface != nullptr && gcs_interface->is_initialized()) {
    std::string group_name(get_group_name_var());
    Gcs_group_identifier group_id(group_name);
    Gcs_communication_interface *gcs_comms =
        gcs_interface->get_communication_session(group_id);

    if (gcs_comms != nullptr) {
      result = gcs_comms->get_incoming_connections_protocol();
    }
  }
  gcs_operations_lock->unlock();
  return result;
}

Gcs_mysql_network_provider *Gcs_operations::get_mysql_network_provider() {
  DBUG_TRACE;
  Gcs_mysql_network_provider *result = nullptr;

  gcs_operations_lock->rdlock();
  if (gcs_interface != nullptr && gcs_mysql_net_provider != nullptr &&
      gcs_interface->is_initialized()) {
    result = gcs_mysql_net_provider.get();
  }
  gcs_operations_lock->unlock();

  return result;
}

const std::string &Gcs_operations::get_gcs_engine() { return gcs_engine; }

bool Gcs_operations::is_initialized() {
  gcs_operations_lock->rdlock();
  bool ret = nullptr != gcs_interface;
  gcs_operations_lock->unlock();
  return ret;
}
