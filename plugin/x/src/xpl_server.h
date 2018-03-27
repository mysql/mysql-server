/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "mysql/plugin.h"

#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs/server.h"
#include "plugin/x/ngs/include/ngs_common/ssl_context_options_interface.h"
#include "plugin/x/ngs/include/ngs_common/ssl_session_options.h"
#include "plugin/x/ngs/include/ngs_common/ssl_session_options_interface.h"
#include "plugin/x/src/mysql_show_variable_wrapper.h"
#include "plugin/x/src/sha256_password_cache.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {

extern std::atomic<bool> g_cache_plugin_started;

class Session;
class Sql_data_context;
class Server;
struct Ssl_config;

typedef ngs::shared_ptr<Server> Server_ptr;

class Server : public ngs::Server_delegate {
 public:
  Server(ngs::shared_ptr<ngs::Server_acceptors> acceptors,
         ngs::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
         ngs::shared_ptr<ngs::Protocol_config> config);

  static int main(MYSQL_PLUGIN p);
  static int exit(MYSQL_PLUGIN p);
  static bool reset();

  ngs::Server &server() { return m_server; }

  ngs::Error_code kill_client(uint64_t client_id, Session &requester);

  std::string get_socket_file();
  std::string get_tcp_bind_address();
  std::string get_tcp_port();

  typedef ngs::Locked_container<Server, ngs::RWLock_readlock, ngs::RWLock>
      Server_with_lock;
  typedef ngs::Memory_instrumented<Server_with_lock>::Unique_ptr Server_ptr;

  static Server_ptr get_instance() {
    // TODO: ngs::Locked_container add container that supports shared_ptrs
    return instance ? Server_ptr(ngs::allocate_object<Server_with_lock>(
                          ngs::ref(*instance), ngs::ref(instance_rwl)))
                    : Server_ptr();
  }

  SHA256_password_cache &get_sha256_password_cache() {
    return m_sha256_password_cache;
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

  virtual ngs::shared_ptr<ngs::Client_interface> create_client(
      std::shared_ptr<ngs::Vio_interface> connection);
  virtual ngs::shared_ptr<ngs::Session_interface> create_session(
      ngs::Client_interface &client, ngs::Protocol_encoder_interface &proto,
      const ngs::Session_interface::Session_id session_id);

  virtual bool will_accept_client(const ngs::Client_interface &client);
  virtual void did_accept_client(const ngs::Client_interface &client);
  virtual void did_reject_client(ngs::Server_delegate::Reject_reason reason);

  virtual void on_client_closed(const ngs::Client_interface &client);
  virtual bool is_terminating() const;

  void register_services() const;
  void unregister_services() const;
  void register_udfs();
  void unregister_udfs();

  static Server *instance;
  static ngs::RWLock instance_rwl;
  static MYSQL_PLUGIN plugin_ref;

  ngs::Client_interface::Client_id m_client_id;
  std::atomic<int> m_num_of_connections;
  ngs::shared_ptr<ngs::Protocol_config> m_config;
  ngs::shared_ptr<ngs::Server_acceptors> m_acceptors;
  ngs::shared_ptr<ngs::Scheduler_dynamic> m_wscheduler;
  ngs::shared_ptr<ngs::Scheduler_dynamic> m_nscheduler;
  ngs::Mutex m_accepting_mutex;
  ngs::Server m_server;
  std::set<std::string> m_udf_names;

  static bool exiting;
  static bool is_exiting();

  SHA256_password_cache m_sha256_password_cache;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_SERVER_H_
