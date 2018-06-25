/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
      m_server_interface(NULL),
      m_plugin_session_thread(NULL) {}

Sql_service_command_interface::~Sql_service_command_interface() {
  if (m_server_interface != NULL) {
    if (m_plugin_session_thread) {
      m_plugin_session_thread->terminate_session_thread();
      delete m_plugin_session_thread;
    } else {
      delete m_server_interface;
    }
  }
}

int Sql_service_command_interface::establish_session_connection(
    enum_plugin_con_isolation isolation_param, const char *user,
    void *plugin_pointer) {
  DBUG_ASSERT(m_server_interface == NULL);

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
      m_plugin_session_thread = NULL;
    } else {
      delete m_server_interface;
      m_server_interface = NULL;
    }
    return error;
    /* purecov: end */
  }

  return error;
}

Sql_service_interface *
Sql_service_command_interface::get_sql_service_interface() {
  return m_server_interface;
}

int Sql_service_command_interface::set_interface_user(const char *user) {
  return m_server_interface->set_session_user(user);
}

long Sql_service_command_interface::set_super_read_only() {
  DBUG_ENTER("Sql_service_command_interface::set_super_read_only");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error =
        sql_service_commands.internal_set_super_read_only(m_server_interface);
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_set_super_read_only);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_set_super_read_only(
    Sql_service_interface *sql_interface, void *) {
  DBUG_ENTER("Sql_service_commands::internal_set_super_read_only");

  // These debug branches are repeated here due to THD support variations on
  // invocation
  DBUG_EXECUTE_IF("group_replication_read_mode_error", { DBUG_RETURN(1); });
  DBUG_EXECUTE_IF("group_replication_skip_read_mode", { DBUG_RETURN(0); });

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long srv_err = sql_interface->execute_query("SET GLOBAL super_read_only= 1;");
  if (srv_err == 0) {
#ifndef DBUG_OFF
    long err;
    err =
        sql_interface->execute_query("SELECT @@GLOBAL.super_read_only", &rset);

    DBUG_ASSERT(err || (!err && rset.get_rows() > 0 && rset.getLong(0) == 1));
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SUPER_READ_ON);
#endif
  }

  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::reset_super_read_only() {
  DBUG_ENTER("Sql_service_command_interface::reset_super_read_only");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error =
        sql_service_commands.internal_reset_super_read_only(m_server_interface);
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_reset_super_read_only);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_reset_super_read_only(
    Sql_service_interface *sql_interface, void *) {
  DBUG_ENTER("Sql_service_commands::internal_reset_super_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;

  const char *query = "SET GLOBAL super_read_only= 0";
  long srv_err = sql_interface->execute_query(query);
#ifndef DBUG_OFF
  if (srv_err == 0) {
    long err;
    query = "SELECT @@GLOBAL.super_read_only";
    err = sql_interface->execute_query(query, &rset);

    DBUG_ASSERT(!err && rset.get_rows() > 0 && rset.getLong(0) == 0);
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SUPER_READ_OFF);
  }
#endif
  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::reset_read_only() {
  DBUG_ENTER("Sql_service_command_interface::reset_read_only");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_reset_read_only(m_server_interface);
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_reset_read_only);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_reset_read_only(
    Sql_service_interface *sql_interface, void *) {
  DBUG_ENTER("Sql_service_commands::internal_reset_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;

  const char *query = "SET GLOBAL read_only= 0";
  long srv_err = sql_interface->execute_query(query);

#ifndef DBUG_OFF
  if (srv_err == 0) {
    long err;
    query = "SELECT @@GLOBAL.read_only";
    err = sql_interface->execute_query(query, &rset);

    DBUG_ASSERT(!err && rset.get_rows() > 0 && rset.getLong(0) == 0);
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SUPER_READ_OFF);
  }
#endif

  DBUG_RETURN(srv_err);
}

long Sql_service_commands::internal_kill_session(
    Sql_service_interface *sql_interface, void *session_id) {
  DBUG_ENTER("Sql_service_commands::internal_kill_session");

  DBUG_ASSERT(sql_interface != NULL);

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
  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::kill_session(unsigned long session_id) {
  DBUG_ENTER("Sql_service_command_interface::kill_session(id)");
  long error = 0;
  unsigned long *id_pointer = &session_id;
  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_kill_session(
        m_server_interface, (void *)id_pointer); /* purecov: inspected */
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_kill_session, false);
    m_plugin_session_thread->set_return_pointer((void *)id_pointer);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_command_interface::get_server_super_read_only() {
  DBUG_ENTER("Sql_service_command_interface::get_server_super_read_only");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_get_server_super_read_only(
        m_server_interface);
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_get_server_super_read_only);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_get_server_super_read_only(
    Sql_service_interface *sql_interface, void *) {
  DBUG_ENTER("Sql_service_commands::internal_get_server_super_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long server_super_read_only = -1;

  long srv_error =
      sql_interface->execute_query("SELECT @@GLOBAL.super_read_only", &rset);
  if (srv_error == 0 && rset.get_rows() > 0) {
    server_super_read_only = rset.getLong(0);
  }

  DBUG_RETURN(server_super_read_only);
}

long Sql_service_command_interface::get_server_read_only() {
  DBUG_ENTER("Sql_service_command_interface::get_server_read_only");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error =
        sql_service_commands.internal_get_server_read_only(m_server_interface);
  } else {
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_get_server_read_only);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_get_server_read_only(
    Sql_service_interface *sql_interface, void *) {
  DBUG_ENTER("Sql_service_commands::internal_get_server_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  longlong server_read_only = -1;
  long srv_error =
      sql_interface->execute_query("SELECT @@GLOBAL.read_only", &rset);
  if (srv_error == 0 && rset.get_rows()) {
    server_read_only = rset.getLong(0);
  }

  DBUG_RETURN(server_read_only);
}

int Sql_service_command_interface::get_server_gtid_executed(
    string &gtid_executed) {
  DBUG_ENTER("Sql_service_command_interface::get_server_gtid_executed");
  long error = 0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_get_server_gtid_executed(
        m_server_interface, gtid_executed);
  } else {
    m_plugin_session_thread->set_return_pointer((void *)&gtid_executed);
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_get_server_gtid_executed_generic);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

int Sql_service_commands::internal_get_server_gtid_executed(
    Sql_service_interface *sql_interface, std::string &gtid_executed) {
  DBUG_ENTER("Sql_service_command_interface::get_server_gtid_executed");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long srv_err =
      sql_interface->execute_query("SELECT @@GLOBAL.gtid_executed", &rset);
  if (srv_err == 0 && rset.get_rows() > 0) {
    gtid_executed.assign(rset.getString(0));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

long Sql_service_commands::internal_get_server_gtid_executed_generic(
    Sql_service_interface *sql_interface, void *gtid_executed_arg) {
  DBUG_ENTER("Sql_service_commands::internal_get_server_gtid_executed_generic");

  std::string *gtid_executed = (std::string *)gtid_executed_arg;
  DBUG_RETURN(internal_get_server_gtid_executed(sql_interface, *gtid_executed));
}

long Sql_service_command_interface::wait_for_server_gtid_executed(
    std::string &gtid_executed, int timeout) {
  DBUG_ENTER("Sql_service_command_interface::wait_for_server_gtid_executed");
  long error = 0;

  /* No support for this method on thread isolation mode */
  DBUG_ASSERT(connection_thread_isolation != PSESSION_DEDICATED_THREAD);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_wait_for_server_gtid_executed(
        m_server_interface, gtid_executed, timeout);
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::internal_wait_for_server_gtid_executed(
    Sql_service_interface *sql_interface, std::string &gtid_executed,
    int timeout) {
  DBUG_ENTER("Sql_service_commands::internal_wait_for_server_gtid_executed");

  DBUG_ASSERT(sql_interface != NULL);

  DBUG_EXECUTE_IF("sql_int_wait_for_gtid_executed_no_timeout",
                  { timeout = 0; });

  std::stringstream ss;
  ss << "SELECT WAIT_FOR_EXECUTED_GTID_SET('" << gtid_executed << "'";
  if (timeout > 0) {
    ss << ", " << timeout << ")";
  } else {
    ss << ")";
  }

  std::string query = ss.str();
  long srv_err = sql_interface->execute_query(query);
  if (srv_err) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INTERNAL_QUERY, query.c_str(),
                 srv_err);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  DBUG_RETURN(0);
}

long Sql_service_commands::internal_set_persist_only_variable(
    Sql_service_interface *sql_interface, void *var_args) {
  DBUG_ENTER("Sql_service_commands::internal_set_persistent_variable");

  std::pair<std::string, std::string> *variable_args =
      (std::pair<std::string, std::string> *)var_args;

  DBUG_ASSERT(sql_interface != NULL);

  std::string query = "SET PERSIST_ONLY ";
  query.append(variable_args->first);
  query.append(" = ");
  query.append(variable_args->second);

  long srv_err = sql_interface->execute_query(query);
  if (srv_err) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INTERNAL_QUERY, query.c_str(),
                 srv_err);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  DBUG_RETURN(0);

  return 0;
}

long Sql_service_command_interface::set_persist_only_variable(
    std::string &variable, std::string &value) {
  DBUG_ENTER("Sql_service_command_interface::set_persistent_variable");
  long error = 0;

  std::pair<std::string, std::string> variable_args(variable, value);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD) {
    error = sql_service_commands.internal_set_persist_only_variable(
        m_server_interface, (void *)&variable_args);
  } else {
    m_plugin_session_thread->set_return_pointer((void *)&variable_args);
    m_plugin_session_thread->queue_new_method_for_application(
        &Sql_service_commands::internal_set_persist_only_variable);
    error = m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

Session_plugin_thread::Session_plugin_thread(
    Sql_service_commands *command_interface)
    : command_interface(command_interface),
      m_server_interface(NULL),
      incoming_methods(NULL),
      m_plugin_pointer(NULL),
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
  this->incoming_methods = new Synchronized_queue<st_session_method *>();
}

Session_plugin_thread::~Session_plugin_thread() {
  if (this->incoming_methods) {
    while (!this->incoming_methods->empty()) {
      st_session_method *method = NULL;
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
      PSI_NOT_INSTRUMENTED, sizeof(st_session_method), MYF(0));
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
  return 0;
}

int Session_plugin_thread::launch_session_thread(void *plugin_pointer_var,
                                                 const char *user) {
  DBUG_ENTER(
      "Session_plugin_thread::launch_session_thread(plugin_pointer, user)");

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
    DBUG_RETURN(1);                  /* purecov: inspected */
  }
  m_session_thread_state.set_created();

  while (m_session_thread_state.is_alive_not_running() &&
         !m_session_thread_error) {
    DBUG_PRINT("sleep", ("Waiting for the plugin session thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }

  mysql_mutex_unlock(&m_run_lock);
  DBUG_RETURN(m_session_thread_error);
}

int Session_plugin_thread::terminate_session_thread() {
  DBUG_ENTER("Session_plugin_thread::terminate_session_thread()");
  mysql_mutex_lock(&m_run_lock);

  m_session_thread_terminate = true;
  m_method_execution_completed = true;
  queue_new_method_for_application(NULL, true);

  int stop_wait_timeout = GR_PLUGIN_SESSION_THREAD_TIMEOUT;

  while (m_session_thread_state.is_thread_alive()) {
    DBUG_PRINT("loop", ("killing plugin session thread"));

    mysql_cond_broadcast(&m_run_cond);

    struct timespec abstime;
    set_timespec(&abstime, 1);
#ifndef DBUG_OFF
    int error =
#endif
        mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
    if (stop_wait_timeout >= 1) {
      stop_wait_timeout = stop_wait_timeout - 1;
    } else if (m_session_thread_state.is_thread_alive())  // quit waiting
    {
      mysql_mutex_unlock(&m_run_lock);
      DBUG_RETURN(1);
    }
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!m_session_thread_state.is_running());

  while (!this->incoming_methods->empty()) {
    st_session_method *method = NULL;
    this->incoming_methods->pop(&method);
    my_free(method);
  }

  mysql_mutex_unlock(&m_run_lock);

  DBUG_RETURN(0);
}

int Session_plugin_thread::session_thread_handler() {
  DBUG_ENTER("Session_plugin_thread::session_thread_handler()");

  st_session_method *method = NULL;
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
  m_server_interface = NULL;

  mysql_mutex_lock(&m_run_lock);
  m_session_thread_state.set_terminated();
  mysql_mutex_unlock(&m_run_lock);

  DBUG_RETURN(m_session_thread_error);
}

Sql_service_interface *Session_plugin_thread::get_service_interface() {
  return m_server_interface;
}
