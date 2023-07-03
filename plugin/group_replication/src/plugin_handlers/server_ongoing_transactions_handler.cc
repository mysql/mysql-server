/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/server_ongoing_transactions_handler.h"
#include "mysql/components/services/ongoing_transaction_query_service.h"
#include "plugin/group_replication/include/plugin.h"

#include <vector>

Server_ongoing_transactions_handler::Server_ongoing_transactions_handler()
    : generic_service(nullptr) {
  mysql_mutex_init(key_GR_LOCK_server_ongoing_transaction_handler,
                   &query_wait_lock, MY_MUTEX_INIT_FAST);
}

bool Server_ongoing_transactions_handler::initialize_server_service(
    Plugin_stage_monitor_handler *stage_handler_arg) {
  SERVICE_TYPE(registry) *registry = nullptr;
  if (!registry_module || !(registry = registry_module->get_registry_handle()))
    return true; /* purecov: inspected */
  registry->acquire("mysql_ongoing_transactions_query", &generic_service);
  stage_handler = stage_handler_arg;
  return false;
}

Server_ongoing_transactions_handler::~Server_ongoing_transactions_handler() {
  mysql_mutex_destroy(&query_wait_lock);
  SERVICE_TYPE(registry) *registry = nullptr;
  if (!registry_module ||
      !(registry = registry_module->get_registry_handle())) {
    assert(0); /* purecov: inspected */
    return;
  }
  registry->release(generic_service);
}

bool Server_ongoing_transactions_handler::get_server_running_transactions(
    ulong **ids, ulong *size) {
  SERVICE_TYPE(mysql_ongoing_transactions_query) * server_transaction_service;
  server_transaction_service =
      reinterpret_cast<SERVICE_TYPE(mysql_ongoing_transactions_query) *>(
          generic_service);
  if (generic_service)
    return server_transaction_service->get_ongoing_server_transactions(ids,
                                                                       size);
  else
    return true; /* purecov: inspected */
}

int Server_ongoing_transactions_handler::
    wait_for_current_transaction_load_execution(bool *abort_flag,
                                                my_thread_id id_to_ignore) {
  group_transaction_observation_manager->register_transaction_observer(this);
  unsigned long *thread_id_array = nullptr;
  unsigned long size = 0;
  bool error = get_server_running_transactions(&thread_id_array, &size);

  std::set<my_thread_id> transactions_to_wait;
  if (!error)
    transactions_to_wait.insert(thread_id_array, thread_id_array + size);
  my_free(thread_id_array);
  thread_id_array = nullptr;

  if (id_to_ignore) {
    transactions_to_wait.erase(id_to_ignore);
    size = transactions_to_wait.size();
  }

  ulong transactions_to_wait_size = size;
  if (stage_handler) stage_handler->set_estimated_work(size);

  while (!transactions_to_wait.empty() && !(*abort_flag) && !error) {
    mysql_mutex_lock(&query_wait_lock);

    while (!thread_ids_finished.empty() && !transactions_to_wait.empty()) {
      transactions_to_wait.erase(thread_ids_finished.front());
      thread_ids_finished.pop();
    }
    mysql_mutex_unlock(&query_wait_lock);

    if (stage_handler) {
      ulong transactions_ended =
          transactions_to_wait_size - transactions_to_wait.size();
      stage_handler->set_completed_work(transactions_ended);
    }

    // Sleep to give some more transactions time to finish.
    my_sleep(100);

    error = get_server_running_transactions(&thread_id_array, &size);
    std::set<my_thread_id> current_transactions;
    current_transactions.insert(thread_id_array, thread_id_array + size);
    my_free(thread_id_array);
    thread_id_array = nullptr;

    mysql_mutex_lock(&query_wait_lock);
    for (my_thread_id thread_id : transactions_to_wait) {
      if (current_transactions.find(thread_id) == current_transactions.end()) {
        thread_ids_finished.push(thread_id);
      }
    }
    mysql_mutex_unlock(&query_wait_lock);
  }

  group_transaction_observation_manager->unregister_transaction_observer(this);
  return error;
}

/*
  These methods are necessary to fulfil the Group_transaction_listener
  interface.
*/
/* purecov: begin inspected */
int Server_ongoing_transactions_handler::before_transaction_begin(
    my_thread_id, ulong, ulong, enum_rpl_channel_type) {
  return 0;
}

int Server_ongoing_transactions_handler::before_commit(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}

int Server_ongoing_transactions_handler::before_rollback(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}
/* purecov: end */

int Server_ongoing_transactions_handler::after_rollback(
    my_thread_id thread_id) {
  mysql_mutex_lock(&query_wait_lock);
  thread_ids_finished.push(thread_id);
  mysql_mutex_unlock(&query_wait_lock);
  return 0;
}
int Server_ongoing_transactions_handler::after_commit(my_thread_id thread_id,
                                                      rpl_sidno, rpl_gno) {
  mysql_mutex_lock(&query_wait_lock);
  thread_ids_finished.push(thread_id);
  mysql_mutex_unlock(&query_wait_lock);

  return 0;
}
