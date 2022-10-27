/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/sql_service/sql_service_command.h"

#include <mysql/group_replication_priv.h>
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "plugin/group_replication/include/plugin_constants.h"
#include "plugin/group_replication/include/plugin_psi.h"

using std::string;

Sql_service_command_interface::Sql_service_command_interface()
    : connection_thread_isolation(PSESSION_USE_THREAD),
      m_server_interface(nullptr),
      m_plugin_session_thread(nullptr) {}

Sql_service_command_interface::~Sql_service_command_interface() {
  terminate_connection_fields();
}

int Sql_service_command_interface::establish_session_connection(
    enum_plugin_con_isolation isolation_param, const char *user,
    void *plugin_pointer) {
  assert(m_server_interface == nullptr);

  int error = 0;
  connection_thread_isolation = isolation_param;
  switch (connection_thread_isolation) {
    case PSESSION_USE_THREAD:
      m_server_interface = new Sql_service_interface();
      error = m_server_interface->open_session();
      if (!error) error = m_server_interface->set_session_user(user);
      break;
    case PSESSION_INIT_THREAD:
      m_server_interface = new Sql_service_interface();
      error = m_server_interface->open_thread_session(plugin_pointer);
      if (!error) error = m_server_interface->set_session_user(user);
      break;
    case PSESSION_DEDICATED_THREAD:
      m_plugin_session_thread =
          new Session_plugin_thread(&sql_service_commands);
      error =
          m_plugin_session_thread->launch_session_thread(plugin_pointer, user);
      if (!error)
        m_server_interface = m_plugin_session_thread->get_service_interface();
      break;
  }

  if (error) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_CONN_INTERNAL_PLUGIN_FAIL); /* purecov: begin inspected */
    if (m_plugin_session_thread) {
      m_plugin_session_thread->terminate_session_thread();
      delete m_plugin_session_thread;
      m_plugin_session_thread = nullptr;
    } else {
      delete m_server_interface;
      m_server_interface = nullptr;
    }
    return error;
    /* purecov: end */
  }

  return error;
}

int Sql_service_command_interface::reestablish_connection(
    enum_plugin_con_isolation isolation_param, const char *user,
    void *plugin_pointer) {
  terminate_connection_fields();
  return establish_session_connection(isolation_param, user, plugin_pointer);
}

bool Sql_service_command_interface::is_session_valid() {
  return m_server_interface != nullptr;
}

bool Sql_service_command_interface::is_session_killed() {
  DBUG_ENTER("Sql_service_command_interface::is_session_killed");
  assert(m_server_interface != nullptr);
  if (m_server_interface->is_session_killed(m_server_interface->get_session()))
    DBUG_RETURN(true);
  DBUG_RETURN(false);
}

void Sql_service_command_interface::terminate_connection_fields() {
  if (m_server_interface != nullptr) {
    if (m_plugin_session_thread) {
      m_plugin_session_thread->terminate_session_thread();
      delete m_plugin_session_thread;
      m_plugin_session_thread = nullptr;
      m_server_interface = nullptr;
    } else {
      delete m_server_interface;
      m_server_interface = nullptr;
    }
  }
}

Sql_service_interface *
Sql_service_command_interface::get_sql_service_interface() {
  return m_server_interface;
}

int Sql_service_command_interface::set_interface_user(const char *user) {
  return m_server_interface->set_session_user(user);
}

long Sql_service_commands::internal_kill_session(
    Sql_service_interface *sql_interface, void *session_id) {
  DBUG_TRACE;

  assert(sql_interface != nullptr);

  Sql_resultset rset;
  long srv_err = 0;
  if (!sql_interface->is_session_killed(sql_interface->get_session())) {
    COM_DATA data;
    data.com_kill.id = *((unsigned long *)session_id);
    srv_err = sql_interface->execute(data, COM_PROCESS_KILL, &rset);
    if (srv_err == 0) {
      LogPluginErr(
          INFORMATION_LEVEL, ER_GRP_RPL_KILLED_SESSION_ID, data.com_kill.id,
          sql_interface->is_session_killed(sql_interface->get_session()));
    } else {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_KILLED_FAILED_ID,
                   data.com_kill.id, srv_err); /* purecov: inspected */
    }
  }
  return srv_err;
}

long Sql_service_command_interface::kill_session(unsigned long session_id) {
  DBUG_TRACE;
  long error = 0;
  unsigned long *id_pointer = &session_id;
  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_kill_session(
        m_server_interface, (void *)id_pointer); /* purecov: inspected */
  } else {
    m_plugin_session_thread->set_return_pointer((void *)id_pointer);
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_kill_session, false);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  return error;
}

long Sql_service_command_interface::clone_server(
    std::string &host, std::string &port, std::string &user, std::string &pass,
    bool use_ssl, std::string &error_msg) {
  DBUG_ENTER("Sql_service_command_interface::clone_server");
  long error = 0;

  std::tuple<std::string, std::string, std::string, std::string, bool,
             std::string *>
      variable_args(host, port, user, pass, use_ssl, &error_msg);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_clone_server(
        m_server_interface, static_cast<void *>(&variable_args));
  } else {
    m_plugin_session_thread->set_return_pointer(
        static_cast<void *>(&variable_args));
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_clone_server);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_clone_server(
    Sql_service_interface *sql_interface, void *var_args) {
  DBUG_ENTER("Sql_service_commands::internal_clone_server");

  assert(sql_interface != nullptr);

  std::tuple<std::string, std::string, std::string, std::string, bool,
             std::string *> *variable_args =
      static_cast<std::tuple<std::string, std::string, std::string, std::string,
                             bool, std::string *> *>(var_args);

  std::string q_user(std::get<2>(*variable_args));
  plugin_escape_string(q_user);
  std::string q_hostname(std::get<0>(*variable_args));
  plugin_escape_string(q_hostname);
  std::string q_password(std::get<3>(*variable_args));
  plugin_escape_string(q_password);

  std::string query = "CLONE INSTANCE FROM \'";
  query.append(q_user);
  query.append("\'@\'");
  query.append(q_hostname);
  query.append("\':");
  query.append(std::get<1>(*variable_args));
  query.append(" IDENTIFIED BY \'");
  query.append(q_password);
  bool use_ssl = std::get<4>(*variable_args);
  if (use_ssl)
    query.append("\' REQUIRE SSL;");
  else
    query.append("\' REQUIRE NO SSL;");

  Sql_resultset rset;
  long srv_err = sql_interface->execute_query(query, &rset);
  if (srv_err) {
    /* purecov: begin inspected */
    std::string *error_msg = std::get<5>(*variable_args);
    error_msg->assign("Error number: ");
    error_msg->append(std::to_string(rset.sql_errno()));
    error_msg->append(" Error message: ");
    error_msg->append(rset.err_msg());

    std::string sanitized_query = "CLONE INSTANCE FROM \'";
    sanitized_query.append(q_user);
    sanitized_query.append("\'@\'");
    sanitized_query.append(q_hostname);
    sanitized_query.append("\':");
    sanitized_query.append(std::get<1>(*variable_args));
    sanitized_query.append(" IDENTIFIED BY \'");
    sanitized_query.append("*****");
    bool use_ssl = std::get<4>(*variable_args);
    if (use_ssl)
      sanitized_query.append("\' REQUIRE SSL;");
    else
      sanitized_query.append("\' REQUIRE NO SSL;");

    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INTERNAL_QUERY,
                 sanitized_query.c_str(), srv_err);
    DBUG_RETURN(rset.sql_errno());
    /* purecov: end */
  }
  DBUG_RETURN(0);

  return 0;
}

long Sql_service_command_interface::execute_query(std::string &query) {
  DBUG_ENTER("Sql_service_command_interface::execute_query(query)");
  std::string error_msg_discarded;
  DBUG_RETURN(execute_query(query, error_msg_discarded));
}

long Sql_service_command_interface::execute_query(std::string &query,
                                                  std::string &error_msg) {
  DBUG_ENTER("Sql_service_command_interface::execute_query(query,error)");
  long error = 0;

  std::pair<std::string, std::string *> variable_args(query, &error_msg);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_execute_query(
        m_server_interface, static_cast<void *>(&variable_args));
  } else {
    m_plugin_session_thread->set_return_pointer(
        static_cast<void *>(&variable_args));
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_execute_query);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_execute_query(
    Sql_service_interface *sql_interface, void *var_args) {
  DBUG_ENTER("Sql_service_commands::internal_execute_query");

  assert(sql_interface != nullptr);

  std::pair<std::string, std::string *> *variable_args =
      static_cast<std::pair<std::string, std::string *> *>(var_args);

  std::string query = variable_args->first;

  Sql_resultset rset;
  long srv_err = sql_interface->execute_query(query, &rset);
  if (srv_err) {
    /* purecov: begin inspected */
    variable_args->second->assign("Error number: ");
    variable_args->second->append(std::to_string(rset.sql_errno()));
    variable_args->second->append(" Error message: ");
    variable_args->second->append(rset.err_msg());

    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INTERNAL_QUERY, query.c_str(),
                 srv_err);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  DBUG_RETURN(0);

  return 0;
}

long Sql_service_command_interface::execute_conditional_query(
    std::string &query, bool *result) {
  DBUG_ENTER("Sql_service_command_interface::execute_query(query)");
  std::string error_msg_discarded;
  DBUG_RETURN(execute_conditional_query(query, result, error_msg_discarded));
}

long Sql_service_command_interface::execute_conditional_query(
    std::string &query, bool *result, std::string &error_msg) {
  DBUG_ENTER("Sql_service_command_interface::execute_conditional_query(q,r,e)");
  long error = 0;

  std::tuple<std::string, bool *, std::string *> variable_args(query, result,
                                                               &error_msg);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_execute_conditional_query(
        m_server_interface, static_cast<void *>(&variable_args));
  } else {
    m_plugin_session_thread->set_return_pointer(
        static_cast<void *>(&variable_args));
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_execute_conditional_query);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_execute_conditional_query(
    Sql_service_interface *sql_interface, void *var_args) {
  DBUG_ENTER("Sql_service_commands::internal_execute_conditional_query");

  assert(sql_interface != nullptr);

  std::tuple<std::string, bool *, std::string *> *variable_args =
      static_cast<std::tuple<std::string, bool *, std::string *> *>(var_args);

  std::string query = std::get<0>(*variable_args);

  Sql_resultset rset;
  long srv_err = sql_interface->execute_query(query, &rset);

  bool *result = std::get<1>(*variable_args);

  if (srv_err) {
    std::string *error_string = std::get<2>(*variable_args);
    /* purecov: begin inspected */
    error_string->assign("Error number: ");
    error_string->append(std::to_string(rset.sql_errno()));
    error_string->append(" Error message: ");
    error_string->append(rset.err_msg());

    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INTERNAL_QUERY, query.c_str(),
                 srv_err);
    *result = false;
    DBUG_RETURN(1);
    /* purecov: end */
  } else {
    *result = rset.getLong(0);
  }
  DBUG_RETURN(0);

  return 0;
}

Session_plugin_thread::Session_plugin_thread(
    Sql_service_commands *command_interface)
    : command_interface(command_interface),
      m_server_interface(nullptr),
      incoming_methods(nullptr),
      m_plugin_pointer(nullptr),
      m_method_execution_completed(false),
      m_method_execution_return_value(0),
      m_session_thread_state(),
      m_session_thread_terminate(false),
      m_session_thread_error(0) {
  mysql_mutex_init(key_GR_LOCK_session_thread_run, &m_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_session_thread_run, &m_run_cond);
  mysql_mutex_init(key_GR_LOCK_session_thread_method_exec, &m_method_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_session_thread_method_exec, &m_method_cond);
  this->incoming_methods =
      new Synchronized_queue<st_session_method *>(key_sql_service_command_data);
}

Session_plugin_thread::~Session_plugin_thread() {
  if (this->incoming_methods) {
    while (!this->incoming_methods->empty()) {
      st_session_method *method = nullptr;
      this->incoming_methods->pop(&method);
      my_free(method);
    }
    delete incoming_methods;
  }

  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
  mysql_mutex_destroy(&m_method_lock);
  mysql_cond_destroy(&m_method_cond);
}

void Session_plugin_thread::queue_new_method_for_application(
    long (Sql_service_commands::*method)(Sql_service_interface *, void *),
    bool terminate) {
  st_session_method *method_to_execute;
  method_to_execute = (st_session_method *)my_malloc(
      key_sql_service_command_data, sizeof(st_session_method), MYF(0));
  method_to_execute->method = method;
  method_to_execute->terminated = terminate;
  m_method_execution_completed = false;
  incoming_methods->push(method_to_execute);
}

long Session_plugin_thread::wait_for_method_execution() {
  mysql_mutex_lock(&m_method_lock);
  while (!m_method_execution_completed) {
    DBUG_PRINT("sleep",
               ("Waiting for the plugin session thread to execute a method"));
    mysql_cond_wait(&m_method_cond, &m_method_lock);
  }
  mysql_mutex_unlock(&m_method_lock);
  return m_method_execution_return_value;
}

static void *launch_handler_thread(void *arg) {
  Session_plugin_thread *handler = (Session_plugin_thread *)arg;
  handler->session_thread_handler();
  return nullptr;
}

int Session_plugin_thread::launch_session_thread(void *plugin_pointer_var,
                                                 const char *user) {
  DBUG_TRACE;

  // avoid concurrency calls against stop invocations
  mysql_mutex_lock(&m_run_lock);

  m_session_thread_error = 0;
  m_session_thread_terminate = false;
  m_plugin_pointer = plugin_pointer_var;
  session_user = user;

  if ((mysql_thread_create(key_GR_THD_plugin_session, &m_plugin_session_pthd,
                           get_connection_attrib(), launch_handler_thread,
                           (void *)this))) {
    mysql_mutex_unlock(&m_run_lock); /* purecov: inspected */
    return 1;                        /* purecov: inspected */
  }
  m_session_thread_state.set_created();

  while (m_session_thread_state.is_alive_not_running() &&
         !m_session_thread_error) {
    DBUG_PRINT("sleep", ("Waiting for the plugin session thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }

  mysql_mutex_unlock(&m_run_lock);
  return m_session_thread_error;
}

int Session_plugin_thread::terminate_session_thread() {
  DBUG_TRACE;
  mysql_mutex_lock(&m_run_lock);

  m_session_thread_terminate = true;
  m_method_execution_completed = true;
  queue_new_method_for_application(nullptr, true);

  int stop_wait_timeout = GR_PLUGIN_SESSION_THREAD_TIMEOUT;

  while (m_session_thread_state.is_thread_alive()) {
    DBUG_PRINT("loop", ("killing plugin session thread"));

    mysql_cond_broadcast(&m_run_cond);

    struct timespec abstime;
    set_timespec(&abstime, 1);
#ifndef NDEBUG
    int error =
#endif
        mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
    if (stop_wait_timeout >= 1) {
      stop_wait_timeout = stop_wait_timeout - 1;
    } else if (m_session_thread_state.is_thread_alive())  // quit waiting
    {
      mysql_mutex_unlock(&m_run_lock);
      return 1;
    }
    assert(error == ETIMEDOUT || error == 0);
  }

  assert(!m_session_thread_state.is_running());

  while (!this->incoming_methods->empty()) {
    st_session_method *method = nullptr;
    this->incoming_methods->pop(&method);
    my_free(method);
  }

  mysql_mutex_unlock(&m_run_lock);

  return 0;
}

int Session_plugin_thread::session_thread_handler() {
  DBUG_TRACE;

  st_session_method *method = nullptr;
  m_server_interface = new Sql_service_interface();
  m_session_thread_error =
      m_server_interface->open_thread_session(m_plugin_pointer);
  DBUG_EXECUTE_IF("group_replication_sql_service_force_error",
                  { m_session_thread_error = 1; });
  if (!m_session_thread_error)
    m_session_thread_error = m_server_interface->set_session_user(session_user);

  mysql_mutex_lock(&m_run_lock);
  m_session_thread_state.set_running();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  if (m_session_thread_error) goto end;

  while (!m_session_thread_terminate) {
    DBUG_EXECUTE_IF("group_replication_session_plugin_handler_before_pop", {
      st_session_method *m = nullptr;
      this->incoming_methods->front(&m);
      const char act[] =
          "now signal "
          "signal.group_replication_session_plugin_handler_before_pop_"
          "reached "
          "wait_for "
          "signal.group_replication_session_plugin_handler_before_pop_"
          "continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    this->incoming_methods->pop(&method);

    if (method->terminated) {
      my_free(method);
      break;
    }

    long (Sql_service_commands::*method_to_execute)(Sql_service_interface *,
                                                    void *) = method->method;
    m_method_execution_return_value = (command_interface->*method_to_execute)(
        m_server_interface, return_object);
    my_free(method);
    mysql_mutex_lock(&m_method_lock);
    m_method_execution_completed = true;
    mysql_cond_broadcast(&m_method_cond);
    mysql_mutex_unlock(&m_method_lock);
  }

  mysql_mutex_lock(&m_run_lock);
  while (!m_session_thread_terminate) {
    DBUG_PRINT("sleep", ("Waiting for the plugin session thread"
                         " to be signaled termination"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }
  mysql_mutex_unlock(&m_run_lock);

end:
  delete m_server_interface;
  m_server_interface = nullptr;

  mysql_mutex_lock(&m_run_lock);
  auto ret = m_session_thread_error;
  m_session_thread_state.set_terminated();
  mysql_mutex_unlock(&m_run_lock);

  return ret;
}

Sql_service_interface *Session_plugin_thread::get_service_interface() {
  return m_server_interface;
}
