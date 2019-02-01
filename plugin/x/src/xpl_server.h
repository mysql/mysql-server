/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_XPL_SERVER_H_
#define PLUGIN_X_SRC_XPL_SERVER_H_

#include <atomic>
#include <string>
#include <vector>

#include "mq/broker_task.h"
#include "mq/notice_input_queue.h"
#include "mysql/plugin.h"

#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_interface.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_options_interface.h"
#include "plugin/x/ngs/include/ngs/interface/timeout_callback_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs/server.h"
#include "plugin/x/ngs/include/ngs/server_properties.h"
#include "plugin/x/src/helper/multithread/lock_container.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/mysql_show_variable_wrapper.h"
#include "plugin/x/src/sha256_password_cache.h"
#include "plugin/x/src/ssl_session_options.h"
#include "plugin/x/src/udf/registry.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_performance_schema.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {

extern std::atomic<bool> g_cache_plugin_started;

class Session;
class Sql_data_context;
class Server;
struct Ssl_config;

typedef std::shared_ptr<Server> Server_ptr;

class Server : public ngs::Server_delegate {
 public:
  Server(std::shared_ptr<ngs::Socket_acceptors_task> acceptors,
         std::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
         std::shared_ptr<ngs::Protocol_config> config,
         std::shared_ptr<ngs::Timeout_callback_interface> timeout_callback);

  static int main(MYSQL_PLUGIN p);
  static int exit(MYSQL_PLUGIN p);
  static bool reset();
  static void stop();
  static std::string get_document_id(const THD *thd, const uint16_t offset,
                                     const uint16_t increment);
  static bool get_prepared_statement_id(const THD *thd,
                                        const uint32_t client_stmt_id,
                                        uint32_t *stmt_id);

  ngs::Server &server() { return m_server; }

  ngs::Error_code kill_client(uint64_t client_id,
                              ngs::Session_interface &requester);

  std::string get_socket_file();
  std::string get_tcp_bind_address();
  std::string get_tcp_port();

  typedef Locked_container<Server, RWLock_readlock, RWLock> Server_with_lock;
  typedef ngs::Memory_instrumented<Server_with_lock>::Unique_ptr Server_ptr;

  static Server_ptr get_instance() {
    // TODO: ngs::Locked_container add container that supports shared_ptrs
    return instance ? Server_ptr(ngs::allocate_object<Server_with_lock>(
                          std::ref(*instance), std::ref(instance_rwl)))
                    : Server_ptr();
  }

  SHA256_password_cache &get_sha256_password_cache() {
    return m_sha256_password_cache;
  }

  Notice_input_queue &get_broker_input_queue() const {
    return *m_notice_input_queue;
  }

  void reset_globals();

 private:
  static void verify_mysqlx_user_grants(Sql_data_context &context);
  static void initialize_xmessages();

  bool on_net_startup();

  void net_thread();

  void start_verify_server_state_timer();
  bool on_verify_server_state();

  void plugin_system_variables_changed();
  void update_global_timeout_values();

  virtual std::shared_ptr<ngs::Client_interface> create_client(
      std::shared_ptr<ngs::Vio_interface> connection);
  virtual std::shared_ptr<ngs::Session_interface> create_session(
      ngs::Client_interface &client, ngs::Protocol_encoder_interface &proto,
      const ngs::Session_interface::Session_id session_id);

  virtual bool will_accept_client(const ngs::Client_interface &client);
  virtual void did_accept_client(const ngs::Client_interface &client);
  virtual void did_reject_client(ngs::Server_delegate::Reject_reason reason);

  virtual void on_client_closed(const ngs::Client_interface &client);
  virtual bool is_terminating() const;
  std::string get_property(const ngs::Server_property_ids id) const;

  void register_services() const;
  void unregister_services() const;
  void register_udfs();
  void unregister_udfs();

  static Server *instance;
  static RWLock instance_rwl;
  static MYSQL_PLUGIN plugin_ref;

  ngs::Client_interface::Client_id m_client_id;
  std::atomic<int> m_num_of_connections;
  std::shared_ptr<ngs::Protocol_config> m_config;
  std::shared_ptr<ngs::Scheduler_dynamic> m_wscheduler;
  std::shared_ptr<ngs::Scheduler_dynamic> m_nscheduler;
  Mutex m_accepting_mutex{KEY_mutex_x_xpl_server_accepting};
  ngs::Server_properties m_properties;
  std::shared_ptr<Notice_input_queue> m_notice_input_queue;
  ngs::Server m_server;
  udf::Registry m_udf_registry;

  static std::atomic<bool> exiting;
  static bool is_exiting();

  SHA256_password_cache m_sha256_password_cache;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_SERVER_H_
