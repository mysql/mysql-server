/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#if !defined(MYSQL_DYNAMIC_PLUGIN) && defined(WIN32) && !defined(XPLUGIN_UNIT_TESTS)
// Needed for importing PERFORMANCE_SCHEMA plugin API.
#define MYSQL_DYNAMIC_PLUGIN 1
#endif // WIN32

#include "xpl_server.h"
#include "xpl_client.h"
#include "xpl_session.h"
#include "xpl_system_variables.h"
#include "io/xpl_listener_factory.h"
#include "mysql_variables.h"
#include "mysql_show_variable_wrapper.h"
#include "sql_data_result.h"
#include "auth_plain.h"
#include "auth_mysql41.h"
#include "xpl_error.h"
#include "ngs/scheduler.h"
#include "ngs/protocol_authentication.h"
#include "ngs/protocol/protocol_config.h"
#include "ngs/interface/listener_interface.h"
#include "ngs/server_acceptors.h"
#include <mysql/plugin.h>
#include "my_thread_local.h"
#include "mysql/service_ssl_wrapper.h"
#include "mysqlx_version.h"

#if !defined(HAVE_YASSL)
#include <openssl/err.h>
#endif

class Session_scheduler : public ngs::Scheduler_dynamic
{
public:
  Session_scheduler(const char* name, void *plugin)
  : ngs::Scheduler_dynamic(name, KEY_thread_x_worker), m_plugin_ptr(plugin)
  {
  }

  virtual bool thread_init()
  {
    if (srv_session_init_thread(m_plugin_ptr) != 0)
    {
      log_error("srv_session_init_thread returned error");
      return false;
    }

#ifdef HAVE_PSI_THREAD_INTERFACE
    // Reset user name and hostname stored in PFS_thread
    // which were copied from parent thread
    PSI_THREAD_CALL(set_thread_account) (
        "", 0, "", 0);
#endif // HAVE_PSI_THREAD_INTERFACE

    ngs::Scheduler_dynamic::thread_init();

#if defined(__APPLE__) || defined(HAVE_PTHREAD_SETNAME_NP)
    char thread_name[16];
    static int worker = 0;
    my_snprintf(thread_name, sizeof(thread_name), "xpl_worker%i", worker++);
#ifdef __APPLE__
    pthread_setname_np(thread_name);
#else
    pthread_setname_np(pthread_self(), thread_name);
#endif
#endif

    return true;
  }

  virtual void thread_end()
  {
    ngs::Scheduler_dynamic::thread_end();
    srv_session_deinit_thread();

    ssl_wrapper_thread_cleanup();
  }
private:
  void *m_plugin_ptr;
};


class Worker_scheduler_monitor : public ngs::Scheduler_dynamic::Monitor_interface
{
public:
  virtual void on_worker_thread_create()
  {
    ++xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  virtual void on_worker_thread_destroy()
  {
    --xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  virtual void on_task_start()
  {
    ++xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }

  virtual void on_task_end()
  {
    --xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }
};


namespace
{

const char *STATUS_VALUE_FOR_NOT_CONFIGURED_INTERFACE = "UNDEFINED";

} // namespace

xpl::Server* xpl::Server::instance;
ngs::RWLock  xpl::Server::instance_rwl;
bool         xpl::Server::exiting = false;

xpl::Server::Server(ngs::shared_ptr<ngs::Server_acceptors> acceptors,
                    ngs::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
                    ngs::shared_ptr<ngs::Protocol_config> config)
: m_client_id(0),
  m_num_of_connections(0),
  m_config(config),
  m_acceptors(acceptors),
  m_wscheduler(wscheduler),
  m_nscheduler(ngs::allocate_shared<ngs::Scheduler_dynamic>("network", KEY_thread_x_acceptor)),
  m_server(acceptors, m_nscheduler, wscheduler, this, config)
{
}


void xpl::Server::start_verify_server_state_timer()
{
  m_server.add_timer(1000, ngs::bind(&Server::on_verify_server_state, this));
}


/** Timer handler that polls whether X plugin event loop should stop.

This can be triggered when:
- server is shutting down
- plugin is being uninstalled

Because this is called by the timer handler from the acceptor event loop,
it is guaranteed that it'll run in the acceptor thread.
*/
bool xpl::Server::on_verify_server_state()
{
  if (is_exiting())
  {
    if (!exiting)
      log_info("Shutdown triggered by mysqld abort flag");

    // closing clients has been moved to other thread
    // this thread have to gracefully shutdown io operations
    if (m_wscheduler->is_running())
    {
      typedef ngs::Scheduler_dynamic::Task Task;
      Task *task = ngs::allocate_object<Task>(ngs::bind(&ngs::Server::close_all_clients, &m_server));
      if (!m_wscheduler->post(task))
      {
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


ngs::shared_ptr<ngs::Client_interface> xpl::Server::create_client(ngs::Connection_ptr connection)
{
  ngs::shared_ptr<ngs::Client_interface> result;
  result = ngs::allocate_shared<xpl::Client>(connection, ngs::ref(m_server), ++m_client_id,
                                             ngs::allocate_object<xpl::Protocol_monitor>());
  return result;
}


ngs::shared_ptr<ngs::Session_interface> xpl::Server::create_session(ngs::Client_interface &client,
                                                            ngs::Protocol_encoder &proto,
                                                            Session::Session_id session_id)
{
  return ngs::shared_ptr<ngs::Session>(
           ngs::allocate_shared<xpl::Session>(ngs::ref(client), &proto, session_id));
}


void xpl::Server::on_client_closed(const ngs::Client_interface &client)
{
  ++Global_status_variables::instance().m_closed_connections_count;

  // Only accepted clients are calling on_client_closed
  --m_num_of_connections;
}


bool xpl::Server::will_accept_client(const ngs::Client_interface &client)
{
  Mutex_lock lock(m_accepting_mutex);

  ++m_num_of_connections;

  log_debug("num_of_connections: %i, max_num_of_connections: %i",(int)m_num_of_connections, (int)xpl::Plugin_system_variables::max_connections);
  bool can_be_accepted = m_num_of_connections <= (int)xpl::Plugin_system_variables::max_connections;

  if (!can_be_accepted || is_terminating())
  {
    --m_num_of_connections;
    return false;
  }

  return true;
}


void xpl::Server::did_accept_client(const ngs::Client_interface &client)
{
  ++Global_status_variables::instance().m_accepted_connections_count;
}


void xpl::Server::did_reject_client(ngs::Server_delegate::Reject_reason reason)
{
  switch (reason)
  {
    case ngs::Server_delegate::AcceptError:
      ++Global_status_variables::instance().m_connection_errors_count;
      ++Global_status_variables::instance().m_connection_accept_errors_count;
      break;
    case ngs::Server_delegate::TooManyConnections:
      ++Global_status_variables::instance().m_rejected_connections_count;
      break;
  }
}


void xpl::Server::plugin_system_variables_changed()
{
  const unsigned int min = m_wscheduler->set_num_workers(Plugin_system_variables::min_worker_threads);
  if (min < Plugin_system_variables::min_worker_threads)
    Plugin_system_variables::min_worker_threads = min;

  m_wscheduler->set_idle_worker_timeout(Plugin_system_variables::idle_worker_thread_timeout * 1000);

  m_config->max_message_size = Plugin_system_variables::max_allowed_packet;
  m_config->connect_timeout = ngs::chrono::seconds(Plugin_system_variables::connect_timeout);
}


bool xpl::Server::is_terminating() const
{
  return mysqld::is_terminating();
}


bool xpl::Server::is_exiting()
{
  return mysqld::is_terminating() || xpl::Server::exiting;
}


int xpl::Server::main(MYSQL_PLUGIN p)
{
  xpl::plugin_handle = p;

  uint32 listen_backlog = 50 + Plugin_system_variables::max_connections / 5;
  if (listen_backlog > 900)
    listen_backlog= 900;

  try
  {
    Global_status_variables::instance().reset();

    ngs::shared_ptr<ngs::Scheduler_dynamic> thd_scheduler(ngs::allocate_shared<Session_scheduler>("work", p));

    Plugin_system_variables::setup_system_variable_from_env_or_compile_opt(
        Plugin_system_variables::socket,
        "MYSQLX_UNIX_PORT",
        MYSQLX_UNIX_ADDR);

    Listener_factory listener_factory;
    ngs::shared_ptr<ngs::Server_acceptors> acceptors(ngs::allocate_shared<ngs::Server_acceptors>(
         ngs::ref(listener_factory),
         Plugin_system_variables::bind_address,
         Plugin_system_variables::port,
         Plugin_system_variables::port_open_timeout,
         Plugin_system_variables::socket,
         listen_backlog));

    instance_rwl.wlock();

    exiting = false;
    instance = ngs::allocate_object<Server>(acceptors, thd_scheduler, ngs::allocate_shared<ngs::Protocol_config>());

    const bool use_only_through_secure_connection = true, use_only_in_non_secure_connection = false;

    instance->server().add_authentication_mechanism("PLAIN",   Sasl_plain_auth::create,   use_only_through_secure_connection);
    instance->server().add_authentication_mechanism("MYSQL41", Sasl_mysql41_auth::create, use_only_in_non_secure_connection);
    instance->server().add_authentication_mechanism("MYSQL41", Sasl_mysql41_auth::create, use_only_through_secure_connection);

    instance->plugin_system_variables_changed();

    thd_scheduler->set_monitor(ngs::allocate_object<Worker_scheduler_monitor>());
    thd_scheduler->launch();
    instance->m_nscheduler->launch();

    xpl::Plugin_system_variables::registry_callback(ngs::bind(&Server::plugin_system_variables_changed, instance));

    instance->m_nscheduler->post(ngs::bind(&Server::net_thread, instance));

    instance_rwl.unlock();
  }
  catch(const std::exception &e)
  {
    if (instance)
      instance->server().start_failed();
    instance_rwl.unlock();
    my_plugin_log_message(&xpl::plugin_handle, MY_ERROR_LEVEL, "Startup failed with error \"%s\"", e.what());
    return 1;
  }

  return 0;
}


int xpl::Server::exit(MYSQL_PLUGIN p)
{
  // this flag will trigger the on_verify_server_state() timer to trigger an acceptor thread exit
  exiting = true;
  my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "Exiting");

  if (instance)
  {
    // Following writelock sometimes blocks network thread in  bool Server::on_net_startup()
    // and call to  self->server().stop() wait for network thread to exit
    // thus its going hand forever. Still we already changed the value of instance. Thus we should exit
    // successful
    instance->server().stop();
    instance->m_nscheduler->stop();

    xpl::Plugin_system_variables::clean_callbacks();

    // This is needed to clean up internal data from protobuf, but
    // once it's called, protobuf can't be used again (and we'll
    // probably crash if the plugin is reloaded)
    //
    // Ideally, this would only be called when the server exits.
    //google::protobuf::ShutdownProtobufLibrary();
  }

  {
    ngs::RWLock_writelock slock(instance_rwl);
    ngs::free_object(instance);
    instance = NULL;
  }

  my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "Exit done");
  return 0;
}


ngs::Error_code xpl::Server::let_mysqlx_user_verify_itself(Sql_data_context &context)
{
  try
  {
    context.switch_to_local_user(MYSQLXSYS_USER);

    if (!context.is_acl_disabled())
    {
      verify_mysqlx_user_grants(context);
    }

    return ngs::Success();
  }
  catch (const ngs::Error_code &error)
  {
    if (ER_MUST_CHANGE_PASSWORD == error.error)
      log_error("Password for %s account has been expired", MYSQLXSYS_ACCOUNT);

    return error;
  }
}

void xpl::Server::verify_mysqlx_user_grants(Sql_data_context &context)
{
  Sql_data_result  sql_result(context);
  int              num_of_grants = 0;
  bool             has_no_privileges   = false;
  bool             has_select_on_mysql_user = false;
  bool             has_super = false;

  // This method checks if mysqlxsys has correct permissions to
  // access mysql.user table and the SUPER privilege (for killing sessions)
  // There are three possible states:
  // 1) User has permissions to the table but no SUPER
  // 2) User has permissions to the table and SUPER
  // 2) User has no permissions, thus previous try of
  //    creation failed, account is accepted and GRANTS should be
  //    applied again

  std::string            grants;
  std::string::size_type p;

  sql_result.query("SHOW GRANTS FOR " MYSQLXSYS_ACCOUNT);

  do
  {
    sql_result.get_next_field(grants);
    ++num_of_grants;
    if (grants == "GRANT USAGE ON *.* TO '" MYSQLXSYS_USER "'@'" MYSQLXSYS_HOST "'")
      has_no_privileges = true;

    bool on_all_schemas = false;

    if ((p = grants.find("ON *.*")) != std::string::npos)
    {
      grants.resize(p); // truncate the non-priv list part of the string
      on_all_schemas = true;
    }
    else if ((p = grants.find("ON `mysql`.*")) != std::string::npos ||
        (p = grants.find("ON `mysql`.`user`")) != std::string::npos)
      grants.resize(p); // truncate the non-priv list part of the string
    else
      continue;

    if (grants.find(" ALL ") != std::string::npos)
    {
      has_select_on_mysql_user = true;
      if (on_all_schemas)
        has_super = true;
    }
    if (grants.find(" SELECT ") != std::string::npos ||
        grants.find(" SELECT,") != std::string::npos)
      has_select_on_mysql_user = true;
    if (grants.find(" SUPER ") != std::string::npos)
      has_super = true;
  } while (sql_result.next_row());

  if (has_select_on_mysql_user && has_super)
  {
    log_info("Using %s account for authentication which has all required permissions", MYSQLXSYS_ACCOUNT);
    return;
  }

  // If user has no permissions (only default) or only SELECT on mysql.user
  // lets accept it, and apply the grants
  if (has_no_privileges && (num_of_grants == 1 || (num_of_grants == 2 && has_select_on_mysql_user)))
  {
    log_info("Using existing %s account for authentication. Incomplete grants will be fixed", MYSQLXSYS_ACCOUNT);
    throw ngs::Error(ER_X_MYSQLX_ACCOUNT_MISSING_PERMISSIONS, "%s account without any grants", MYSQLXSYS_ACCOUNT);
  }

  // Users with some custom grants and without access to mysql.user should be rejected
  throw ngs::Error(ER_X_BAD_CONFIGURATION, "%s account already exists but does not have the expected grants", MYSQLXSYS_ACCOUNT);
}


void xpl::Server::create_mysqlx_user(Sql_data_context &context)
{
  Sql_data_result sql_result(context);

  try
  {
    context.switch_to_local_user("root");

    sql_result.disable_binlog();

    // pwd doesn't matter because the account is locked
    sql_result.query("CREATE USER IF NOT EXISTS " MYSQLXSYS_ACCOUNT " IDENTIFIED WITH mysql_native_password AS '*7CF5CA9067EC647187EB99FCC27548FBE4839AE3' ACCOUNT LOCK;");

    try
    {
      if (sql_result.statement_warn_count() > 0)
        verify_mysqlx_user_grants(context);
    }
    catch (const ngs::Error_code &error)
    {
      if (ER_X_MYSQLX_ACCOUNT_MISSING_PERMISSIONS != error.error)
        throw error;
    }

    sql_result.query("GRANT SELECT ON mysql.user TO " MYSQLXSYS_ACCOUNT);
    sql_result.query("GRANT SUPER ON *.* TO " MYSQLXSYS_ACCOUNT);
    sql_result.query("FLUSH PRIVILEGES;");

    sql_result.restore_binlog();
  }
  catch (const ngs::Error_code &error)
  {
    sql_result.restore_binlog();

    if (ER_MUST_CHANGE_PASSWORD != error.error)
      throw error;

    throw ngs::Error(ER_X_BAD_CONFIGURATION, "Can't setup %s account - root password expired", MYSQLXSYS_ACCOUNT);
  }
}


void xpl::Server::net_thread()
{
  srv_session_init_thread(xpl::plugin_handle);

#if defined(__APPLE__)
  pthread_setname_np("xplugin_acceptor");
#elif defined(HAVE_PTHREAD_SETNAME_NP)
  pthread_setname_np(pthread_self(), "xplugin_acceptor");
#endif

  if (on_net_startup())
  {
    log_info("Server starts handling incoming connections");
    m_server.start();
    log_info("Stopped handling incoming connections");
    on_net_shutdown();
  }

  ssl_wrapper_thread_cleanup();

  srv_session_deinit_thread();
}


static xpl::Ssl_config choose_ssl_config(const bool mysqld_have_ssl,
    const xpl::Ssl_config &mysqld_ssl,
    const xpl::Ssl_config & mysqlx_ssl)
{
  if (!mysqlx_ssl.is_configured() && mysqld_have_ssl)
  {
    my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL,
        "Using SSL configuration from MySQL Server");

    return mysqld_ssl;
  }

  if (mysqlx_ssl.is_configured())
  {
    my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL,
        "Using SSL configuration from Mysqlx Plugin");
    return mysqlx_ssl;
  }

  my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL,
      "Neither MySQL Server nor Mysqlx Plugin has valid SSL configuration");

  return xpl::Ssl_config();
}

bool xpl::Server::on_net_startup()
{
  try
  {
    // Ensure to call the start method only once
    if (server().is_running())
      return true;

    Sql_data_context sql_context(NULL, true);

    if (!sql_context.wait_api_ready(&is_exiting))
      throw ngs::Error_code(ER_X_SERVICE_ERROR, "Service isn't ready after pulling it few times");

    ngs::Error_code error = sql_context.init();

    if (error)
      throw error;

    if (let_mysqlx_user_verify_itself(sql_context))
      create_mysqlx_user(sql_context);

    Sql_data_result sql_result(sql_context);
    sql_result.query("SELECT @@skip_networking, @@skip_name_resolve, @@have_ssl='YES', @@ssl_key, "
                     "@@ssl_ca, @@ssl_capath, @@ssl_cert, @@ssl_cipher, @@ssl_crl, @@ssl_crlpath, @@tls_version;");

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

    ngs::Ssl_context_unique_ptr ssl_ctx(ngs::allocate_object<ngs::Ssl_context>());

    ssl_config = choose_ssl_config(mysqld_have_ssl,
                                   ssl_config,
                                   xpl::Plugin_system_variables::ssl_config);

    // YaSSL doesn't support CRL according to vio
    const char *crl = IS_YASSL_OR_OPENSSL(NULL, ssl_config.ssl_crl);
    const char *crlpath = IS_YASSL_OR_OPENSSL(NULL, ssl_config.ssl_crlpath);

    const bool ssl_setup_result = ssl_ctx->setup(tls_version, ssl_config.ssl_key,
                                                 ssl_config.ssl_ca,
                                                 ssl_config.ssl_capath,
                                                 ssl_config.ssl_cert,
                                                 ssl_config.ssl_cipher,
                                                 crl, crlpath);

    if (ssl_setup_result)
    {
      my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL,
          "Using " IS_YASSL_OR_OPENSSL("YaSSL", "OpenSSL") " for TLS connections");
    }
    else
    {
      my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL,
          "For more information, please see the Using Secure Connections with X Plugin section in the MySQL documentation.");
    }

    if (instance->server().prepare(ngs::move(ssl_ctx), skip_networking, skip_name_resolve, true))
        return true;
  }
  catch (const ngs::Error_code &e)
  {
    // The plugin was unloaded while waiting for service
    if (is_exiting())
    {
      instance->m_server.start_failed();
      return false;
    }
    log_error("%s", e.message.c_str());
  }

  instance->server().close_all_clients();
  instance->m_server.start_failed();

  return false;
}


void xpl::Server::on_net_shutdown()
{
  if (!mysqld::is_terminating())
  {
    try
    {
      Sql_data_context sql_context(NULL, true);

      if (!sql_context.init())
      {
        Sql_data_result  sql_result(sql_context);

        sql_context.switch_to_local_user("root");

        sql_result.disable_binlog();

        try
        {
          if (!sql_context.is_acl_disabled())
            sql_result.query("DROP USER " MYSQLXSYS_ACCOUNT);
          else
            log_warning("Internal account %s can't be removed because server is running without user privileges (\"skip-grant-tables\" switch)", MYSQLXSYS_ACCOUNT);

          sql_result.restore_binlog();
        }
        catch (const ngs::Error_code &error)
        {
          sql_result.restore_binlog();
          throw error;
        }

        sql_context.detach();
      }
    }
    catch (const ngs::Error_code &ec)
    {
      log_error("%s", ec.message.c_str());
    }
  }
}

ngs::Error_code xpl::Server::kill_client(uint64_t client_id, Session &requester)
{
  ngs::unique_ptr<Mutex_lock> lock(new Mutex_lock(server().get_client_exit_mutex()));
  ngs::Client_ptr found_client = server().get_client_list().find(client_id);

  // Locking exit mutex of ensures that the client wont exit Client::run until
  // the kill command ends, and shared_ptr (found_client) will be released before
  // the exit_lock is released. Following ensures that the final instance of Clients will be
  // released in its thread (Scheduler, Client::run).

  if (found_client &&
      ngs::Client_interface::Client_closed != found_client->get_state())
  {
    xpl::Client_ptr xpl_client =  ngs::static_pointer_cast<xpl::Client>(found_client);

    if (client_id == requester.client().client_id_num())
    {
      lock.reset();
      xpl_client->kill();
      return ngs::Success();
    }

    bool     is_session = false;
    uint64_t mysql_session_id = 0;

    {
      Mutex_lock lock_session_exit(xpl_client->get_session_exit_mutex());
      ngs::shared_ptr<xpl::Session> session = xpl_client->get_session();

      is_session = NULL != session.get();
      if (is_session)
        mysql_session_id = session->data_context().mysql_session_id();
    }

    if (is_session)
    {
      // try to kill the MySQL session
      ngs::Error_code error = requester.data_context().execute_kill_sql_session(mysql_session_id);
      if (error)
        return error;

      bool is_killed = false;
      {
        Mutex_lock lock_session_exit(xpl_client->get_session_exit_mutex());
        ngs::shared_ptr<xpl::Session> session = xpl_client->get_session();

        if (session)
          is_killed = session->data_context().is_killed();
      }

      if (is_killed)
      {
        xpl_client->kill();
        return ngs::Success();
      }
    }
    return ngs::Error(ER_KILL_DENIED_ERROR, "Cannot kill client %llu", static_cast<unsigned long long>(client_id));
  }
  return ngs::Error(ER_NO_SUCH_THREAD, "Unknown MySQLx client id %llu", static_cast<unsigned long long>(client_id));
}

std::string xpl::Server::get_socket_file()
{
  if (!m_server.is_terminating())
  {
    if (!m_acceptors->was_prepared())
      return "";

    if (m_acceptors->was_unix_socket_configured())
    {
      return Plugin_system_variables::socket;
    }
  }

  return ::STATUS_VALUE_FOR_NOT_CONFIGURED_INTERFACE;
}

std::string xpl::Server::get_tcp_port()
{
  if (!m_server.is_terminating())
  {
    if (!m_acceptors->was_prepared())
      return "";

    std::string bind_address;

    if (m_acceptors->was_tcp_server_configured(bind_address))
    {
      char buffer[100];

      sprintf(buffer, "%u",Plugin_system_variables::port);

      return buffer;
    }
  }

  return ::STATUS_VALUE_FOR_NOT_CONFIGURED_INTERFACE;
}

std::string xpl::Server::get_tcp_bind_address()
{
  if (!m_server.is_terminating())
  {
    if (!m_acceptors->was_prepared())
      return "";

    std::string bind_address;

    if (m_acceptors->was_tcp_server_configured(bind_address))
    {
      return bind_address;
    }
  }

  return ::STATUS_VALUE_FOR_NOT_CONFIGURED_INTERFACE;
}

struct Client_check_handler_thd
{
  Client_check_handler_thd(THD *thd)
  : m_thd(thd)
  {
  }

  bool operator() (ngs::Client_ptr &client)
  {
    xpl::Client *xpl_client = (xpl::Client *)client.get();

    return xpl_client->is_handler_thd(m_thd);
  }

  THD *m_thd;
};


xpl::Client_ptr xpl::Server::get_client_by_thd(Server_ref &server, THD *thd)
{
  std::vector<ngs::Client_ptr> clients;
  Client_check_handler_thd     client_check_thd(thd);

  (*server)->server().get_client_list().get_all_clients(clients);

  std::vector<ngs::Client_ptr>::iterator i = std::find_if(clients.begin(), clients.end(), client_check_thd);
  if (clients.end() != i)
    return ngs::dynamic_pointer_cast<Client>(*i);

  return Client_ptr();
}
