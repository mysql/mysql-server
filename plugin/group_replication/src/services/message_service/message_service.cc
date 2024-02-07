/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/services/message_service/message_service.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_message_service.h>
#include <mysql/components/services/registry.h>
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "string_with_len.h"

DEFINE_BOOL_METHOD(send, (const char *tag, const unsigned char *data,
                          const size_t data_length)) {
  DBUG_TRACE;

  if (nullptr == local_member_info) return true;

  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status != Group_member_info::MEMBER_ONLINE &&
      member_status != Group_member_info::MEMBER_IN_RECOVERY) {
    return true;
  }

  Group_service_message msg;
  if (msg.set_tag(tag) || msg.set_data(data, data_length) ||
      gcs_module->send_message(msg)) {
    return true;
  }
  return false;
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_message_service_send)
send, END_SERVICE_IMPLEMENTATION();

bool register_gr_message_service_send() {
  DBUG_TRACE;

  DBUG_EXECUTE_IF("gr_message_service_disable_send", return false;);

  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  using group_replication_message_service_send_t =
      SERVICE_TYPE_NO_CONST(group_replication_message_service_send);
  return reg->register_service(
      "group_replication_message_service_send.group_replication",
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_message_service_send_t *>(
              &SERVICE_IMPLEMENTATION(
                  group_replication, group_replication_message_service_send))));
}

bool unregister_gr_message_service_send() {
  DBUG_TRACE;
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  return reg->unregister(
      "group_replication_message_service_send.group_replication");
}

static void *launch_message_service_handler_thread(void *arg) {
  DBUG_TRACE;
  Message_service_handler *handler = (Message_service_handler *)arg;
  handler->dispatcher();
  return nullptr;
}

Message_service_handler::Message_service_handler()
    : m_aborted(false), m_message_service_thd_state(), m_incoming(nullptr) {
  mysql_mutex_init(key_GR_LOCK_message_service_run, &m_message_service_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_message_service_run, &m_message_service_run_cond);

  m_incoming = new Abortable_synchronized_queue<Group_service_message *>(
      key_message_service_queue);
}

Message_service_handler::~Message_service_handler() {
  mysql_mutex_destroy(&m_message_service_run_lock);
  mysql_cond_destroy(&m_message_service_run_cond);

  // clear queue
  Group_service_message *service_message = nullptr;

  if (m_incoming) {
    while (m_incoming->size()) {
      /* purecov: begin inspected */
      if (m_incoming->pop(&service_message)) break;
      delete service_message;
      /* purecov: end */
    }
  }

  delete m_incoming;
  m_incoming = nullptr;
}

int Message_service_handler::initialize() {
  DBUG_TRACE;

  mysql_mutex_lock(&m_message_service_run_lock);
  if (m_message_service_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&m_message_service_run_lock); /* purecov: inspected */
    return 0;                                        /* purecov: inspected */
  }

  if ((mysql_thread_create(key_GR_THD_message_service_handler,
                           &m_message_service_pthd, get_connection_attrib(),
                           launch_message_service_handler_thread,
                           (void *)this))) {
    mysql_mutex_unlock(&m_message_service_run_lock); /* purecov: inspected */
    return 1;                                        /* purecov: inspected */
  }
  m_message_service_thd_state.set_created();

  while (m_message_service_thd_state.is_alive_not_running()) {
    struct timespec abstime;
    set_timespec(&abstime, 1);

    mysql_cond_timedwait(&m_message_service_run_cond,
                         &m_message_service_run_lock, &abstime);
  }
  mysql_mutex_unlock(&m_message_service_run_lock);

  return 0;
}

void Message_service_handler::dispatcher() {
  DBUG_TRACE;

  bool pop_failed = false;

  // Thread context operations
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  thd->set_skip_readonly_check();
  global_thd_manager_add_thd(thd);

  mysql_mutex_lock(&m_message_service_run_lock);
  m_message_service_thd_state.set_running();
  mysql_cond_broadcast(&m_message_service_run_cond);
  mysql_mutex_unlock(&m_message_service_run_lock);

  while (!m_aborted) {
    if (thd->killed) {
      m_aborted = true;
      break;
    }

    DBUG_EXECUTE_IF("group_replication_message_service_dispatcher_before_pop", {
      Group_service_message *m = nullptr;
      m_incoming->front(&m);
      const char act[] =
          "now signal "
          "signal.group_replication_message_service_dispatcher_before_pop_"
          "reached "
          "wait_for "
          "signal.group_replication_message_service_dispatcher_before_pop_"
          "continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    Group_service_message *service_message = nullptr;
    pop_failed = m_incoming->pop(&service_message);

    DBUG_EXECUTE_IF("group_replication_message_service_hold_messages", {
      const char act[] =
          "now signal "
          "signal.group_replication_message_service_hold_messages_reached "
          "wait_for signal.notification_continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    if (pop_failed || service_message == nullptr) break;

    if (notify_message_service_recv(service_message)) {
      m_aborted = true;
      const char *exit_state_action_abort_log_message =
          "Message delivery error on message service of Group Replication.";
      leave_group_on_failure::mask leave_actions;
      leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
      leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
      leave_group_on_failure::leave(
          leave_actions, ER_GRP_RPL_MESSAGE_SERVICE_FATAL_ERROR, nullptr,
          exit_state_action_abort_log_message);
    }

    delete service_message;

    DBUG_EXECUTE_IF("group_replication_message_service_delete_messages", {
      const char act[] =
          "now SIGNAL "
          "signal.group_replication_message_service_delete_messages_reached "
          "WAIT_FOR "
          "signal.group_replication_message_service_delete_messages_continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });
  }

  thd->release_resources();
  global_thd_manager_remove_thd(thd);
  delete thd;
  my_thread_end();

  mysql_mutex_lock(&m_message_service_run_lock);
  m_message_service_thd_state.set_terminated();
  mysql_cond_broadcast(&m_message_service_run_cond);
  mysql_mutex_unlock(&m_message_service_run_lock);

  my_thread_exit(nullptr);
}

int Message_service_handler::terminate() {
  DBUG_TRACE;

  mysql_mutex_lock(&m_message_service_run_lock);
  m_aborted = true;
  m_incoming->abort(true);

  while (m_message_service_thd_state.is_thread_alive()) {
    struct timespec abstime;
    set_timespec(&abstime, 1);

    mysql_cond_timedwait(&m_message_service_run_cond,
                         &m_message_service_run_lock, &abstime);
  }
  mysql_mutex_unlock(&m_message_service_run_lock);

  return 0;
}

void Message_service_handler::add(Group_service_message *message) {
  DBUG_TRACE;

  // In case error no further action is needed, since the only way
  // of returning error on push is to abort the queue, action which make the
  // member leave the group, move to ERROR state and follow
  // group_replication_exit_state_action.
  if (m_incoming->push(message)) delete message;
}

#ifdef __clang__
// Clang UBSAN false positive?
// Call to function through pointer to incorrect function type
bool Message_service_handler::notify_message_service_recv(
    Group_service_message *service_message) SUPPRESS_UBSAN {
#else

bool Message_service_handler::notify_message_service_recv(
    Group_service_message *service_message) {
#endif  // __clang__
  DBUG_TRACE;

  const char *service_name = "group_replication_message_service_recv";
  bool error = false;
  bool is_service_default_implementation = true;
  my_h_service_iterator iterator;
  std::list<std::string> listeners_names;

  my_service<SERVICE_TYPE(registry_query)> reg_query("registry_query",
                                                     get_plugin_registry());
  /*
    Create iterator to navigate message service recv implementations.
  */
  if (reg_query->create(service_name, &iterator)) {
    // no listeners registered we can terminate notification
    if (iterator) {
      reg_query->release(iterator);
    }
    return false;
  }

  /*
    To avoid acquire multiple re-entrant locks on the services
    registry, which would happen after registry_module is acquired
    after calling registry_module::create(), we store the services names
    and only notify them after release the iterator.
  */
  for (; iterator != nullptr && !reg_query->is_valid(iterator);
       reg_query->next(iterator)) {
    const char *name = nullptr;
    if (reg_query->get(iterator, &name)) {
      error |= true;
      continue;
    }

    std::string s(name);
    if (s.find(service_name) == std::string::npos) break;

    /*
      The iterator currently contains more service implementations than
      those named after the given service name, the first registered
      service will be listed twice: 1) default service, 2) regular service.
      The spec says that the name given is used to position the iterator
      start on the first registered service implementation prefixed with
      that name. We need to skip the first service since it will be listed
      twice.
    */
    if (is_service_default_implementation) {
      is_service_default_implementation = false;
      continue;
    }

    listeners_names.push_back(s);
  }

  /* release the iterator */
  if (iterator) reg_query->release(iterator);

  /* notify all listeners */
  for (std::string listener_name : listeners_names) {
    my_service<SERVICE_TYPE(group_replication_message_service_recv)> svc(
        listener_name.c_str(), get_plugin_registry());
    if (!svc.is_valid() || svc->recv(service_message->get_tag().c_str(),
                                     service_message->get_data(),
                                     service_message->get_data_length())) {
      error |= true;
    }
  }

  return error;
}
