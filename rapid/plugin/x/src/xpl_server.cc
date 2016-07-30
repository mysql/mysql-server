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
#include "mysql_variables.h"
#include "mysql_show_variable_wrapper.h"
#include "sql_data_result.h"
#include "auth_plain.h"
#include "auth_mysql41.h"
#include "xpl_error.h"
#include "ngs/scheduler.h"
#include "ngs/protocol_authentication.h"
#include "ngs/protocol/protocol_config.h"
#include <mysql/plugin.h>
#include <mysql/service_my_plugin_log.h>
#include "my_atomic.h"
#include "my_thread_local.h"
#include "mysql/service_ssl_wrapper.h"

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


class Worker_scheduler_monitor : public ngs::Scheduler_dynamic::Monitor
{
public:
  virtual void on_worker_thread_create()
  {
    xpl::Global_status_variables::instance().increment_worker_thread_count();
  }

  virtual void on_worker_thread_destroy()
  {
    xpl::Global_status_variables::instance().decrement_worker_thread_count();
  }

  virtual void on_task_start()
  {
    xpl::Global_status_variables::instance().increment_active_worker_thread_count();
  }

  virtual void on_task_end()
  {
    xpl::Global_status_variables::instance().decrement_active_worker_thread_count();
  }
};


xpl::Server* xpl::Server::instance;
ngs::RWLock  xpl::Server::instance_rwl;
bool         xpl::Server::exiting = false;


xpl::Server::Server(my_socket tcp_socket, boost::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
                    boost::shared_ptr<ngs::Protocol_config> config)
: m_client_id(0),
  m_num_of_connections(0),
  m_config(config),
  m_wscheduler(wscheduler),
  m_server(tcp_socket, wscheduler, this, config)
{
  m_acceptor_thread.thread = 0;
}


xpl::Server::~Server()
{
}


void xpl::Server::start_verify_server_state_timer()
{
  m_server.add_timer(1000, boost::bind(&Server::on_verify_server_state, this));
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
      Task *task = new Task(boost::bind(&ngs::Server::close_all_clients, &m_server));
      if (!m_wscheduler->post(task))
      {
        log_debug("Unable to schedule closing all clients ");
        delete task;
      }
    }

    // stop the server, making the event loop stop looping around
    m_server.stop();

    return false;
  }
  return true;
}


boost::shared_ptr<ngs::Client> xpl::Server::create_client(ngs::Connection_ptr connection)
{
  return boost::make_shared<xpl::Client>(connection, &m_server, ++m_client_id, new xpl::Protocol_monitor());
}


boost::shared_ptr<ngs::Session> xpl::Server::create_session(boost::shared_ptr<ngs::Client> client,
                                                       ngs::Protocol_encoder *proto,
                                                       Session::Session_id session_id)
{
  return boost::make_shared<xpl::Session>(boost::ref(*client), proto, session_id);
}


void xpl::Server::on_client_closed(boost::shared_ptr<ngs::Client> client)
{
  Global_status_variables::instance().increment_closed_connections_count();

  // Only accepted clients are calling on_client_closed
  --m_num_of_connections;
}


bool xpl::Server::will_accept_client(boost::shared_ptr<ngs::Client> client)
{
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


void xpl::Server::did_accept_client(boost::shared_ptr<ngs::Client> client)
{
  Global_status_variables::instance().increment_accepted_connections_count();
}


void xpl::Server::did_reject_client(ngs::Server_delegate::Reject_reason reason)
{
  switch (reason)
  {
    case ngs::Server_delegate::AcceptError:
      Global_status_variables::instance().increment_connection_errors_count();
      Global_status_variables::instance().increment_connection_accept_errors_count();
      break;
    case ngs::Server_delegate::TooManyConnections:
      Global_status_variables::instance().increment_connection_reject_count();
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
  m_config->connect_timeout = ngs::seconds(Plugin_system_variables::connect_timeout);
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

  try
  {
    Global_status_variables::instance().reset();

    boost::shared_ptr<ngs::Scheduler_dynamic> thd_scheduler(new Session_scheduler("work", p));

    my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "X plugin tcp connection enable at port %u.", Plugin_system_variables::xport);

    // Lets pre-create the socket, verify it later
    my_socket tcp_socket = ngs::Connection_vio::create_and_bind_socket(Plugin_system_variables::xport);

    instance_rwl.wlock();

    exiting = false;
    instance = new Server(tcp_socket, thd_scheduler, boost::make_shared<ngs::Protocol_config>());

    const bool use_only_with_tls = true, use_only_in_raw_mode = false;

    instance->server().add_authentication_mechanism("PLAIN",   Sasl_plain_auth::create,   use_only_with_tls);
    instance->server().add_authentication_mechanism("MYSQL41", Sasl_mysql41_auth::create, use_only_in_raw_mode);
    instance->server().add_authentication_mechanism("MYSQL41", Sasl_mysql41_auth::create, use_only_with_tls);

    instance->plugin_system_variables_changed();

    thd_scheduler->set_monitor(new Worker_scheduler_monitor);
    thd_scheduler->launch();

    xpl::Plugin_system_variables::registry_callback(boost::bind(&Server::plugin_system_variables_changed, instance));

    thread_create(KEY_thread_x_acceptor, &instance->m_acceptor_thread,
                  &Server::net_thread, instance);

    instance_rwl.unlock();
    my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "X plugin initialization successes");
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

    if (0 != instance->m_acceptor_thread.thread)
    {
      void *ret;
      log_info("Waiting for acceptor thread to finish...");
      ngs::thread_join(&instance->m_acceptor_thread, &ret);
      log_info("Acceptor thread finished");
    }

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
    delete instance;
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


void *xpl::Server::net_thread(void *arg)
{
  xpl::Server *self = (xpl::Server*)arg;

  srv_session_init_thread(xpl::plugin_handle);

#if defined(__APPLE__)
  pthread_setname_np("xplugin_acceptor");
#elif defined(HAVE_PTHREAD_SETNAME_NP)
  pthread_setname_np(pthread_self(), "xplugin_acceptor");
#endif

  if (self->on_net_startup())
  {
    log_info("Server starts handling incoming connections");
    if (!self->m_server.run())
    {
      log_error("Error starting acceptor");
    }
    log_info("Stopped handling incoming connections");
    self->on_net_shutdown();
  }

  ssl_wrapper_thread_cleanup();

  srv_session_deinit_thread();
  return NULL;
}


static xpl::Ssl_config choose_ssl_config(const bool mysqld_have_ssl,
    const xpl::Ssl_config &mysqld_ssl,
    const xpl::Ssl_config & mysqlx_ssl)
{
  if (!mysqlx_ssl.is_configured() && mysqld_have_ssl)
    return mysqld_ssl;

  if (mysqlx_ssl.is_configured())
    return mysqlx_ssl;

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
    sql_result.query("SELECT @@skip_name_resolve, @@have_ssl='YES', @@ssl_key, @@ssl_ca,"
                     "@@ssl_capath, @@ssl_cert, @@ssl_cipher, @@ssl_crl, @@ssl_crlpath, @@tls_version;");

    sql_context.detach();

    Ssl_config ssl_config;
    bool mysqld_have_ssl = false;
    bool skip_networking = false;
    bool skip_name_resolve = false;
    char *tls_version = NULL;
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

    ngs::Ssl_context_unique_ptr ssl_ctx(new ngs::Ssl_context());
    try
    {
      ssl_config = choose_ssl_config(mysqld_have_ssl,
                                     ssl_config,
                                     xpl::Plugin_system_variables::ssl_config);

#ifdef HAVE_YASSL
      // YaSSL doesn't support CRL according to vio
      const char *crl = NULL;
      const char *crlpath = NULL;
#else
      const char *crl = ssl_config.ssl_crl;
      const char *crlpath = ssl_config.ssl_crlpath;
#endif
      ssl_ctx->setup(tls_version,
                     ssl_config.ssl_key,
                     ssl_config.ssl_ca,
                     ssl_config.ssl_capath,
                     ssl_config.ssl_cert,
                     ssl_config.ssl_cipher,
                     crl, crlpath);
      instance->server().set_ssl_context(boost::move(ssl_ctx));
#if !defined(HAVE_YASSL)
      my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "Using OpenSSL for TCP connections");
#else
      my_plugin_log_message(&xpl::plugin_handle, MY_INFORMATION_LEVEL, "Using YaSSL for TCP connections");
#endif
    }
    catch (std::exception &e)
    {
      throw ngs::Error_code(ER_X_SERVICE_ERROR, std::string("SSL context setup failed: \"") + e.what() + std::string("\""));
    }

    if (!instance->server().prepare(skip_networking, skip_name_resolve))
      throw ngs::Error_code(ER_X_SERVICE_ERROR, "Error preparing to accept connections");

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

  log_error("Delayed startup failed. Plugin is unable to accept connections.");

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
  boost::scoped_ptr<Mutex_lock> lock(new Mutex_lock(server().get_client_exit_mutex()));
  ngs::Client_ptr found_client = server().get_client_list().find(client_id);

  // Locking exit mutex of ensures that the client wont exit Client::run until
  // the kill command ends, and shared_ptr (found_client) will be released before
  // the exit_lock is released. Following ensures that the final instance of Clients will be
  // released in its thread (Scheduler, Client::run).

  if (found_client &&
      ngs::Client::Client_closed != found_client->get_state())
  {
    xpl::Client_ptr xpl_client =  boost::static_pointer_cast<xpl::Client>(found_client);

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
      boost::shared_ptr<xpl::Session> session = xpl_client->get_session();

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
        boost::shared_ptr<xpl::Session> session = xpl_client->get_session();

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
    return boost::dynamic_pointer_cast<Client>(*i);

  return Client_ptr();
}
