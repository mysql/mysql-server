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

#include "plugin/x/src/xpl_server.h"

#include <openssl/err.h>
#include "my_config.h"
#include "my_inttypes.h"
#include "my_thread_local.h"
#include "mysql/plugin.h"
#include "mysql/service_ssl_wrapper.h"

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/listener_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs/socket_acceptors_task.h"
#include "plugin/x/ngs/include/ngs/socket_events.h"
#include "plugin/x/ngs/include/ngs/timeout_callback.h"
#include "plugin/x/ngs/include/ngs_common/config.h"
#include "plugin/x/ngs/include/ngs_common/ssl_context.h"
#include "plugin/x/src/auth_challenge_response.h"
#include "plugin/x/src/auth_plain.h"
#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/io/xpl_listener_factory.h"
#include "plugin/x/src/mysql_show_variable_wrapper.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/services/mysqlx_group_member_status_listener.h"
#include "plugin/x/src/services/mysqlx_group_membership_listener.h"
#include "plugin/x/src/services/mysqlx_maintenance.h"
#include "plugin/x/src/services/service_registrator.h"
#include "plugin/x/src/sha256_password_cache.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/udf/mysqlx_error.h"
#include "plugin/x/src/udf/registrator.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_session.h"
#include "plugin/x/src/xpl_system_variables.h"

std::atomic<bool> xpl::g_cache_plugin_started{false};

class Session_scheduler : public ngs::Scheduler_dynamic {
 public:
  Session_scheduler(const char *name, void *plugin)
      : ngs::Scheduler_dynamic(name, KEY_thread_x_worker),
        m_plugin_ptr(plugin) {}

  virtual bool thread_init() {
    if (srv_session_init_thread(m_plugin_ptr) != 0) {
      log_error(ER_XPLUGIN_SRV_SESSION_INIT_THREAD_FAILED);
      return false;
    }

#ifdef HAVE_PSI_THREAD_INTERFACE
    // Reset user name and hostname stored in PFS_thread
    // which were copied from parent thread
    PSI_THREAD_CALL(set_thread_account)("", 0, "", 0);
#endif  // HAVE_PSI_THREAD_INTERFACE

    ngs::Scheduler_dynamic::thread_init();

#if defined(__APPLE__) || defined(HAVE_PTHREAD_SETNAME_NP)
    char thread_name[16];
    static int worker = 0;
    snprintf(thread_name, sizeof(thread_name), "xpl_worker%i", worker++);
#ifdef __APPLE__
    pthread_setname_np(thread_name);
#else
    pthread_setname_np(pthread_self(), thread_name);
#endif
#endif

    return true;
  }

  virtual void thread_end() {
    ngs::Scheduler_dynamic::thread_end();
    srv_session_deinit_thread();

    ssl_wrapper_thread_cleanup();
  }

 private:
  void *m_plugin_ptr;
};

class Worker_scheduler_monitor
    : public ngs::Scheduler_dynamic::Monitor_interface {
 public:
  virtual void on_worker_thread_create() {
    ++xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  virtual void on_worker_thread_destroy() {
    --xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  virtual void on_task_start() {
    ++xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }

  virtual void on_task_end() {
    --xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }
};

xpl::Server *xpl::Server::instance;
xpl::RWLock xpl::Server::instance_rwl;
bool xpl::Server::exiting = false;

xpl::Server::Server(
    ngs::shared_ptr<ngs::Socket_acceptors_task> acceptors,
    ngs::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
    ngs::shared_ptr<ngs::Protocol_config> config,
    ngs::shared_ptr<ngs::Timeout_callback_interface> timeout_callback)
    : m_client_id(0),
      m_num_of_connections(0),
      m_config(config),
      m_wscheduler(wscheduler),
      m_nscheduler(ngs::allocate_shared<ngs::Scheduler_dynamic>(
          "network", KEY_thread_x_acceptor)),
      m_notice_input_queue(new Notice_input_queue()),
      m_server(m_nscheduler, wscheduler, this, config, &m_properties,
               {acceptors, m_notice_input_queue->create_broker_task()},
               timeout_callback) {}

void xpl::Server::start_verify_server_state_timer() {
  m_server.add_callback(1000, [this]() -> bool {
    on_verify_server_state();
    return true;
  });
}

void xpl::Server::initialize_xmessages() {
  /* Workaround for initialization of protobuf data.
     Call default_instance for first msg from every
     protobuf file.

     This should have be changed to a proper fix.
   */
  Mysqlx::ServerMessages::default_instance();
  Mysqlx::Sql::StmtExecute::default_instance();
  Mysqlx::Session::AuthenticateStart::default_instance();
  Mysqlx::Resultset::ColumnMetaData::default_instance();
  Mysqlx::Notice::Warning::default_instance();
  Mysqlx::Expr::Expr::default_instance();
  Mysqlx::Expect::Open::default_instance();
  Mysqlx::Datatypes::Any::default_instance();
  Mysqlx::Crud::Update::default_instance();
  Mysqlx::Connection::Capabilities::default_instance();
}

/** Timer handler that polls whether X plugin event loop should stop.

This can be triggered when:
- server is shutting down
- plugin is being uninstalled

Because this is called by the timer handler from the acceptor event loop,
it is guaranteed that it'll run in the acceptor thread.
*/
bool xpl::Server::on_verify_server_state() {
  if (is_exiting()) {
    if (!exiting) log_debug("Shutdown triggered by mysqld abort flag");

    // closing clients has been moved to other thread
    // this thread have to gracefully shutdown io operations
    if (m_wscheduler->is_running()) {
      typedef ngs::Scheduler_dynamic::Task Task;
      Task *task = ngs::allocate_object<Task>(
          ngs::bind(&ngs::Server::close_all_clients, &m_server));
      if (!m_wscheduler->post(task)) {
        log_debug("Unable to schedule closing all clients ");
        ngs::free_object(task);
      }
    }

    const bool is_called_from_timeout_handler = true;
    m_server.stop(is_called_from_timeout_handler);

    return false;
  }
  return true;
}

ngs::shared_ptr<ngs::Client_interface> xpl::Server::create_client(
    std::shared_ptr<ngs::Vio_interface> connection) {
  ngs::shared_ptr<ngs::Client_interface> result;
  auto global_timeouts = m_config->get_global_timeouts();
  result = ngs::allocate_shared<xpl::Client>(
      connection, ngs::ref(m_server), ++m_client_id,
      ngs::allocate_object<xpl::Protocol_monitor>(), global_timeouts);
  return result;
}

ngs::shared_ptr<ngs::Session_interface> xpl::Server::create_session(
    ngs::Client_interface &client, ngs::Protocol_encoder_interface &proto,
    const Session::Session_id session_id) {
  return ngs::shared_ptr<ngs::Session>(
      ngs::allocate_shared<xpl::Session>(ngs::ref(client), &proto, session_id));
}

void xpl::Server::on_client_closed(const ngs::Client_interface &) {
  ++Global_status_variables::instance().m_closed_connections_count;

  // Only accepted clients are calling on_client_closed
  --m_num_of_connections;
}

bool xpl::Server::will_accept_client(const ngs::Client_interface &) {
  MUTEX_LOCK(lock, m_accepting_mutex);

  ++m_num_of_connections;

  log_debug("num_of_connections: %i, max_num_of_connections: %i",
            (int)m_num_of_connections,
            (int)xpl::Plugin_system_variables::max_connections);
  bool can_be_accepted = m_num_of_connections <=
                         (int)xpl::Plugin_system_variables::max_connections;

  if (!can_be_accepted || is_terminating()) {
    --m_num_of_connections;
    return false;
  }

  return true;
}

void xpl::Server::did_accept_client(const ngs::Client_interface &) {
  ++Global_status_variables::instance().m_accepted_connections_count;
}

void xpl::Server::did_reject_client(
    ngs::Server_delegate::Reject_reason reason) {
  switch (reason) {
    case ngs::Server_delegate::AcceptError:
      ++Global_status_variables::instance().m_connection_errors_count;
      ++Global_status_variables::instance().m_connection_accept_errors_count;
      break;
    case ngs::Server_delegate::TooManyConnections:
      ++Global_status_variables::instance().m_rejected_connections_count;
      break;
  }
}

void xpl::Server::plugin_system_variables_changed() {
  const unsigned int min = m_wscheduler->set_num_workers(
      Plugin_system_variables::min_worker_threads);
  if (min < Plugin_system_variables::min_worker_threads)
    Plugin_system_variables::min_worker_threads = min;

  m_wscheduler->set_idle_worker_timeout(
      Plugin_system_variables::idle_worker_thread_timeout * 1000);

  m_config->m_interactive_timeout =
      Plugin_system_variables::m_interactive_timeout;
  m_config->max_message_size = Plugin_system_variables::max_allowed_packet;
  m_config->connect_timeout =
      ngs::chrono::seconds(Plugin_system_variables::connect_timeout);
}

void xpl::Server::update_global_timeout_values() {
  m_config->set_global_timeouts(get_global_timeouts());
}

bool xpl::Server::is_terminating() const { return mysqld::is_terminating(); }

bool xpl::Server::is_exiting() {
  return mysqld::is_terminating() || xpl::Server::exiting;
}

int xpl::Server::main(MYSQL_PLUGIN p) {
  xpl::plugin_handle = p;

  uint32 listen_backlog = 50 + Plugin_system_variables::max_connections / 5;
  if (listen_backlog > 900) listen_backlog = 900;

  try {
    initialize_xmessages();

    Global_status_variables::instance().reset();

    ngs::shared_ptr<ngs::Scheduler_dynamic> thd_scheduler(
        ngs::allocate_shared<Session_scheduler>("work", p));

    Plugin_system_variables::setup_system_variable_from_env_or_compile_opt(
        Plugin_system_variables::socket, "MYSQLX_UNIX_PORT", MYSQLX_UNIX_ADDR);

    Listener_factory listener_factory;
    auto config(ngs::allocate_shared<ngs::Protocol_config>());
    auto events(ngs::allocate_shared<ngs::Socket_events>());
    auto timeout_callback(ngs::allocate_shared<ngs::Timeout_callback>(events));
    auto acceptors(ngs::allocate_shared<ngs::Socket_acceptors_task>(
        ngs::ref(listener_factory), Plugin_system_variables::bind_address,
        Plugin_system_variables::port,
        Plugin_system_variables::port_open_timeout,
        Plugin_system_variables::socket, listen_backlog, events));

    instance_rwl.wlock();

    exiting = false;
    instance = ngs::allocate_object<Server>(acceptors, thd_scheduler, config,
                                            timeout_callback);

    const bool use_only_through_secure_connection = true,
               use_only_in_non_secure_connection = false;

    // Cache cleaning plugin started before the X plugin so cache was not
    // enabled yet
    if (g_cache_plugin_started) instance->m_sha256_password_cache.enable();

    instance->server().add_sha256_password_cache(
        &instance->get_sha256_password_cache());
    instance->server().add_authentication_mechanism(
        "PLAIN", Sasl_plain_auth::create, use_only_through_secure_connection);
    instance->server().add_authentication_mechanism(
        "MYSQL41", Sasl_mysql41_auth::create,
        use_only_in_non_secure_connection);
    instance->server().add_authentication_mechanism(
        "MYSQL41", Sasl_mysql41_auth::create,
        use_only_through_secure_connection);
    instance->server().add_authentication_mechanism(
        "SHA256_MEMORY", Sasl_sha256_memory_auth::create,
        use_only_in_non_secure_connection);
    instance->server().add_authentication_mechanism(
        "SHA256_MEMORY", Sasl_sha256_memory_auth::create,
        use_only_through_secure_connection);

    instance->plugin_system_variables_changed();

    thd_scheduler->set_monitor(
        ngs::allocate_object<Worker_scheduler_monitor>());
    thd_scheduler->launch();
    instance->m_nscheduler->launch();

    xpl::Plugin_system_variables::registry_callback(
        ngs::bind(&Server::plugin_system_variables_changed, instance));
    xpl::Plugin_system_variables::registry_callback(
        ngs::bind(&Server::update_global_timeout_values, instance));

    instance->m_nscheduler->post(ngs::bind(&Server::net_thread, instance));

    instance->register_services();
    instance->register_udfs();

    instance_rwl.unlock();
  } catch (const std::exception &e) {
    if (instance) instance->server().start_failed();
    instance_rwl.unlock();
    log_error(ER_XPLUGIN_STARTUP_FAILED, e.what());
    return 1;
  }

  return 0;
}

int xpl::Server::exit(MYSQL_PLUGIN) {
  // this flag will trigger the on_verify_server_state() timer to trigger an
  // acceptor thread exit
  exiting = true;

  log_debug("Exiting");
  if (instance) {
    instance->unregister_udfs();
    instance->unregister_services();

    // Following writelock sometimes blocks network thread in  bool
    // Server::on_net_startup() and call to  self->server().stop() wait for
    // network thread to exit thus its going hand forever. Still we already
    // changed the value of instance. Thus we should exit successful
    instance->server().stop();
    instance->m_nscheduler->stop();

    xpl::Plugin_system_variables::clean_callbacks();

    // This is needed to clean up internal data from protobuf, but
    // once it's called, protobuf can't be used again (and we'll
    // probably crash if the plugin is reloaded)
    //
    // Ideally, this would only be called when the server exits.
    // google::protobuf::ShutdownProtobufLibrary();
  }

  {
    RWLock_writelock slock(instance_rwl);
    ngs::free_object(instance);
    instance = NULL;
  }

  log_debug("Exit done");

  xpl::plugin_handle = nullptr;

  return 0;
}

void xpl::Server::verify_mysqlx_user_grants(Sql_data_context &context) {
  Sql_data_result sql_result(context);
  int num_of_grants = 0;
  bool has_no_privileges = false;
  bool has_select_on_mysql_user = false;
  bool has_super = false;

  // This method checks if mysqlxsys has correct permissions to
  // access mysql.user table and the SUPER privilege (for killing sessions)
  // There are three possible states:
  // 1) User has permissions to the table but no SUPER
  // 2) User has permissions to the table and SUPER
  // 2) User has no permissions, thus previous try of
  //    creation failed, account is accepted and GRANTS should be
  //    applied again

  std::string grants;
  std::string::size_type p;

  sql_result.query("SHOW GRANTS FOR " MYSQLXSYS_ACCOUNT);

  do {
    sql_result.get_next_field(grants);
    ++num_of_grants;
    if (grants == "GRANT USAGE ON *.* TO `" MYSQL_SESSION_USER
                  "`@`" MYSQLXSYS_HOST "`")
      has_no_privileges = true;

    bool on_all_schemas = false;

    if ((p = grants.find("ON *.*")) != std::string::npos) {
      grants.resize(p);  // truncate the non-priv list part of the string
      on_all_schemas = true;
    } else if ((p = grants.find("ON `mysql`.*")) != std::string::npos ||
               (p = grants.find("ON `mysql`.`user`")) != std::string::npos)
      grants.resize(p);  // truncate the non-priv list part of the string
    else
      continue;

    if (grants.find(" ALL ") != std::string::npos) {
      has_select_on_mysql_user = true;
      if (on_all_schemas) has_super = true;
    }
    if (grants.find(" SELECT ") != std::string::npos ||
        grants.find(" SELECT,") != std::string::npos)
      has_select_on_mysql_user = true;
    if (grants.find(" SUPER ") != std::string::npos ||
        grants.find(" SUPER,") != std::string::npos)
      has_super = true;
  } while (sql_result.next_row());

  if (has_select_on_mysql_user && has_super) {
    log_debug(
        "Using %s account for authentication which has all required \
permissions",
        MYSQLXSYS_ACCOUNT);
    return;
  }

  // If user has no permissions (only default) or only SELECT on mysql.user
  // lets accept it, and apply the grants
  if (has_no_privileges && (num_of_grants == 1 ||
                            (num_of_grants == 2 && has_select_on_mysql_user))) {
    log_warning(ER_XPLUGIN_EXISTING_USER_ACCOUNT_WITH_INCOMPLETE_GRANTS,
                MYSQLXSYS_ACCOUNT);
    throw ngs::Error(ER_X_MYSQLX_ACCOUNT_MISSING_PERMISSIONS,
                     "%s account without any grants", MYSQLXSYS_ACCOUNT);
  }

  // Users with some custom grants and without access to mysql.user should be
  // rejected
  throw ngs::Error(
      ER_X_BAD_CONFIGURATION,
      "%s account already exists but does not have the expected grants",
      MYSQLXSYS_ACCOUNT);
}

void xpl::Server::net_thread() {
  srv_session_init_thread(xpl::plugin_handle);

#if defined(__APPLE__)
  pthread_setname_np("xplugin_acceptor");
#elif defined(HAVE_PTHREAD_SETNAME_NP)
  pthread_setname_np(pthread_self(), "xplugin_acceptor");
#endif

  if (on_net_startup()) {
    log_debug("Server starts handling incoming connections");
    m_server.start();
    log_debug("Stopped handling incoming connections");
  }

  ssl_wrapper_thread_cleanup();

  srv_session_deinit_thread();
}

static xpl::Ssl_config choose_ssl_config(const bool mysqld_have_ssl,
                                         const xpl::Ssl_config &mysqld_ssl,
                                         const xpl::Ssl_config &mysqlx_ssl) {
  if (!mysqlx_ssl.is_configured() && mysqld_have_ssl) {
    log_info(ER_XPLUGIN_USING_SSL_CONF_FROM_SERVER);
    return mysqld_ssl;
  }

  if (mysqlx_ssl.is_configured()) {
    log_info(ER_XPLUGIN_USING_SSL_CONF_FROM_MYSQLX);
    return mysqlx_ssl;
  }

  log_info(ER_XPLUGIN_FAILED_TO_USE_SSL_CONF);

  return xpl::Ssl_config();
}

bool xpl::Server::on_net_startup() {
  try {
    // Ensure to call the start method only once
    if (server().is_running()) return true;

    Sql_data_context sql_context(NULL, true);

    if (!sql_context.wait_api_ready(&is_exiting))
      throw ngs::Error_code(ER_X_SERVICE_ERROR,
                            "Service isn't ready after pulling it few times");

    ngs::Error_code error = sql_context.init();

    if (error) throw error;

    Sql_data_result sql_result(sql_context);
    try {
      sql_context.switch_to_local_user(MYSQL_SESSION_USER);
      sql_result.query(
          "SELECT @@skip_networking, @@skip_name_resolve, @@have_ssl='YES', "
          "@@ssl_key, "
          "@@ssl_ca, @@ssl_capath, @@ssl_cert, @@ssl_cipher, @@ssl_crl, "
          "@@ssl_crlpath, @@tls_version;");
    } catch (const ngs::Error_code &) {
      log_error(ER_XPLUGIN_UNABLE_TO_USE_USER_SESSION_ACCOUNT);
      log_info(ER_XPLUGIN_REFERENCE_TO_USER_ACCOUNT_DOC_SECTION);
      throw;
    }

    sql_context.detach();

    Ssl_config ssl_config;
    bool mysqld_have_ssl = false;
    bool skip_networking = false;
    bool skip_name_resolve = false;
    char *tls_version = NULL;

    sql_result.get_next_field(skip_networking);
    sql_result.get_next_field(skip_name_resolve);
    sql_result.get_next_field(mysqld_have_ssl);
    sql_result.get_next_field(ssl_config.ssl_key);
    sql_result.get_next_field(ssl_config.ssl_ca);
    sql_result.get_next_field(ssl_config.ssl_capath);
    sql_result.get_next_field(ssl_config.ssl_cert);
    sql_result.get_next_field(ssl_config.ssl_cipher);
    sql_result.get_next_field(ssl_config.ssl_crl);
    sql_result.get_next_field(ssl_config.ssl_crlpath);
    sql_result.get_next_field(tls_version);

    instance->start_verify_server_state_timer();

    std::unique_ptr<ngs::Ssl_context> ssl_ctx(new ngs::Ssl_context());

    ssl_config = choose_ssl_config(mysqld_have_ssl, ssl_config,
                                   xpl::Plugin_system_variables::ssl_config);

    // wolfSSL doesn't support CRL according to vio
    const char *crl = IS_WOLFSSL_OR_OPENSSL(NULL, ssl_config.ssl_crl);
    const char *crlpath = IS_WOLFSSL_OR_OPENSSL(NULL, ssl_config.ssl_crlpath);

    const bool ssl_setup_result =
        ssl_ctx->setup(tls_version, ssl_config.ssl_key, ssl_config.ssl_ca,
                       ssl_config.ssl_capath, ssl_config.ssl_cert,
                       ssl_config.ssl_cipher, crl, crlpath);

    if (ssl_setup_result) {
      const char *is_wolfssl_or_openssl =
          IS_WOLFSSL_OR_OPENSSL("WolfSSL", "OpenSSL");
      log_info(ER_XPLUGIN_USING_SSL_FOR_TLS_CONNECTION, is_wolfssl_or_openssl);
    } else {
      log_info(ER_XPLUGIN_REFERENCE_TO_SECURE_CONN_WITH_XPLUGIN);
    }

    if (instance->server().prepare(ngs::move(ssl_ctx), skip_networking,
                                   skip_name_resolve))
      return true;
  } catch (const ngs::Error_code &e) {
    // The plugin was unloaded while waiting for service
    if (is_exiting()) {
      instance->m_server.start_failed();
      return false;
    }
    log_error(ER_XPLUING_NET_STARTUP_FAILED, e.message.c_str());
  }

  instance->server().close_all_clients();
  instance->m_server.start_failed();

  return false;
}

ngs::Error_code xpl::Server::kill_client(uint64_t client_id,
                                         Session &requester) {
  ngs::unique_ptr<Mutex_lock> lock(
      new Mutex_lock(server().get_client_exit_mutex(), __FILE__, __LINE__));
  ngs::Client_ptr found_client = server().get_client_list().find(client_id);

  // Locking exit mutex of ensures that the client wont exit Client::run until
  // the kill command ends, and shared_ptr (found_client) will be released
  // before the exit_lock is released. Following ensures that the final instance
  // of Clients will be released in its thread (Scheduler, Client::run).

  if (found_client &&
      ngs::Client_interface::Client_closed != found_client->get_state()) {
    xpl::Client_ptr xpl_client =
        ngs::static_pointer_cast<xpl::Client>(found_client);

    if (client_id == requester.client().client_id_num()) {
      lock.reset();
      xpl_client->kill();
      return ngs::Success();
    }

    bool is_session = false;
    uint64_t mysql_session_id = 0;

    {
      MUTEX_LOCK(lock_session_exit, xpl_client->get_session_exit_mutex());
      auto session = xpl_client->session_smart_ptr();

      is_session = (nullptr != session.get());

      if (is_session)
        mysql_session_id = session->data_context().mysql_session_id();
    }

    if (is_session) {
      // try to kill the MySQL session
      ngs::Error_code error =
          requester.data_context().execute_kill_sql_session(mysql_session_id);
      if (error) return error;

      bool is_killed = false;
      {
        MUTEX_LOCK(lock_session_exit, xpl_client->get_session_exit_mutex());
        auto session = xpl_client->session_smart_ptr();

        if (session) is_killed = session->data_context().is_killed();
      }

      if (is_killed) {
        xpl_client->kill();
        return ngs::Success();
      }
    }
    return ngs::Error(ER_KILL_DENIED_ERROR, "Cannot kill client %llu",
                      static_cast<unsigned long long>(client_id));
  }
  return ngs::Error(ER_NO_SUCH_THREAD, "Unknown MySQLx client id %llu",
                    static_cast<unsigned long long>(client_id));
}

std::string xpl::Server::get_property(const ngs::Server_property_ids id) const {
  if (m_properties.empty()) return "";

  if (0 == m_properties.count(id)) return ngs::PROPERTY_NOT_CONFIGURED;

  return m_properties.at(id);
}

std::string xpl::Server::get_socket_file() {
  return get_property(ngs::Server_property_ids::k_unix_socket);
}

std::string xpl::Server::get_tcp_port() {
  return get_property(ngs::Server_property_ids::k_tcp_port);
}

std::string xpl::Server::get_tcp_bind_address() {
  return get_property(ngs::Server_property_ids::k_tcp_bind_address);
}

void xpl::Server::register_udfs() {
  udf::Registrator r;
  r.registration(udf::get_mysqlx_error_record(), &m_udf_names);
}

void xpl::Server::unregister_udfs() {
  udf::Registrator r;
  r.unregistration(&m_udf_names);
}

void xpl::Server::register_services() const {
  Service_registrator r;

  r.register_service(SERVICE(mysql_server, mysqlx_maintenance));
  r.register_service(SERVICE(mysqlx, group_membership_listener));
  r.register_service(SERVICE(mysqlx, group_member_status_listener));
}

void xpl::Server::unregister_services() const {
  try {
    Service_registrator r;

    r.unregister_service(SERVICE_ID(mysql_server, mysqlx_maintenance));
    r.unregister_service(SERVICE_ID(mysqlx, group_membership_listener));
    r.unregister_service(SERVICE_ID(mysqlx, group_member_status_listener));
  } catch (const std::exception &e) {
    log_error(ER_XPLUGIN_FAILED_TO_STOP_SERVICES, e.what());
  }
}

void xpl::Server::reset_globals() {
  int64 worker_thread_count =
      xpl::Global_status_variables::instance().m_worker_thread_count.load();
  xpl::Global_status_variables::instance().reset();
  xpl::Global_status_variables::instance().m_worker_thread_count +=
      worker_thread_count;
  m_client_id = 0;
}

bool xpl::Server::reset() {
  instance_rwl.wlock();
  const bool r = instance->server().reset_globals();
  if (r) instance->reset_globals();
  instance_rwl.unlock();
  return r;
}

void xpl::Server::stop() {
  exiting = true;
  if (instance) instance->server().stop();
}
