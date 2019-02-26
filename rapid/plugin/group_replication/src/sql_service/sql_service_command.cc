/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_service_command.h"
#include "plugin_log.h"
#include "plugin_psi.h"
#include <mysql/group_replication_priv.h>
#include <sstream>

using std::string;

Sql_service_command_interface::Sql_service_command_interface()
  : connection_thread_isolation(PSESSION_USE_THREAD), m_server_interface(NULL),
    m_plugin_session_thread(NULL)
{}

Sql_service_command_interface::~Sql_service_command_interface()
{
  if (m_server_interface != NULL)
  {
    if (m_plugin_session_thread)
    {
      m_plugin_session_thread->terminate_session_thread();
      delete m_plugin_session_thread;
    } else
    {
      delete m_server_interface;
    }
  }
}

int Sql_service_command_interface::
establish_session_connection(enum_plugin_con_isolation isolation_param,
                             void *plugin_pointer)
{
  DBUG_ASSERT(m_server_interface == NULL);

  int error = 0;
  connection_thread_isolation= isolation_param;
  switch (connection_thread_isolation)
  {
    case PSESSION_USE_THREAD:
      m_server_interface = new Sql_service_interface();
      error = m_server_interface->open_session();
      break;
    case PSESSION_INIT_THREAD:
      m_server_interface = new Sql_service_interface();
      error = m_server_interface->open_thread_session(plugin_pointer);
      break;
    case PSESSION_DEDICATED_THREAD:
      m_plugin_session_thread = new Session_plugin_thread(&sql_service_commands);
      error = m_plugin_session_thread->launch_session_thread(plugin_pointer);
      if (!error)
        m_server_interface = m_plugin_session_thread->get_service_interface();
      break;
  }

  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't establish a internal server connection to execute"
                  " plugin operations");

    if (m_plugin_session_thread)
    {
      m_plugin_session_thread->terminate_session_thread();
      delete m_plugin_session_thread;
      m_plugin_session_thread = NULL;
    } else
    {
      delete m_server_interface;
      m_server_interface = NULL;
    }
    return error;
    /* purecov: end */
  }

  return error;
}

Sql_service_interface*
Sql_service_command_interface::get_sql_service_interface()
{
  return m_server_interface;
}

int Sql_service_command_interface::set_interface_user(const char* user)
{
  return m_server_interface->set_session_user(user);
}


long Sql_service_command_interface::set_super_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::set_super_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_set_super_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
        queue_new_method_for_application(&Sql_service_commands::internal_set_super_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_command_interface::set_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::set_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_set_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
        queue_new_method_for_application(&Sql_service_commands::internal_set_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_set_super_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_set_super_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long srv_err= sql_interface->execute_query("SET GLOBAL super_read_only= 1;");
  if (srv_err == 0)
  {
#ifndef DBUG_OFF
    long err;
    err = sql_interface->execute_query("SELECT @@GLOBAL.super_read_only;", &rset);

    DBUG_ASSERT(!err && rset.get_rows() > 0 && rset.getLong(0) == 1);
    log_message(MY_INFORMATION_LEVEL, "Setting super_read_only=ON.");
#endif
  }

  DBUG_RETURN(srv_err);
}

long Sql_service_commands::
internal_set_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_set_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long srv_err= sql_interface->execute_query("SET GLOBAL read_only= 1;");
  if (srv_err == 0)
  {
#ifndef DBUG_OFF
    sql_interface->execute_query("SELECT @@GLOBAL.read_only;", &rset);
    DBUG_ASSERT(rset.getLong(0) == 1);
    log_message(MY_INFORMATION_LEVEL, "Setting read_only=ON.");
#endif
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "'SET read_only= 1' query execution"
      " resulted in failure. errno: %d", srv_err); /* purecov: inspected */
  }

  DBUG_RETURN(srv_err);
}



long Sql_service_command_interface::reset_super_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::reset_super_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_reset_super_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
      queue_new_method_for_application(&Sql_service_commands::internal_reset_super_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_reset_super_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_reset_super_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;

  const char * query= "SET GLOBAL super_read_only= 0";
  long srv_err= sql_interface->execute_query(query);
#ifndef DBUG_OFF
  if (srv_err == 0)
  {
    long err;
    query= "SELECT @@GLOBAL.super_read_only;";
    err= sql_interface->execute_query(query, &rset);

    DBUG_ASSERT(!err && rset.get_rows() > 0 && rset.getLong(0) == 0);
    log_message(MY_INFORMATION_LEVEL, "Setting super_read_only=OFF.");
  }
#endif
  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::reset_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::reset_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_reset_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
      queue_new_method_for_application(&Sql_service_commands::internal_reset_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_reset_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_reset_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;

  const char* query= "SET GLOBAL read_only= 0";
  long srv_err= sql_interface->execute_query(query);

#ifndef DBUG_OFF
  if (srv_err == 0)
  {
    long err;
    query= "SELECT @@GLOBAL.read_only";
    err= sql_interface->execute_query(query, &rset);

    DBUG_ASSERT(!err && rset.get_rows() > 0 && rset.getLong(0) == 0);
    log_message(MY_INFORMATION_LEVEL, "Setting read_only=OFF.");
  }
#endif

  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::kill_session(uint32_t session_id,
                                       MYSQL_SESSION session)
{
  DBUG_ENTER("Sql_service_command_interface::kill_session");

  DBUG_ASSERT(m_server_interface != NULL);

  Sql_resultset rset;
  long srv_err= 0;
  if (!m_server_interface->is_session_killed(session))
  {
    COM_DATA data;
    data.com_kill.id= session_id;
    srv_err= m_server_interface->execute(data, COM_PROCESS_KILL, &rset);
    if (srv_err == 0)
    {
      log_message(MY_INFORMATION_LEVEL, "killed session id: %d status: %d",
                  session_id, m_server_interface->is_session_killed(session));
    }
    else
    {
      log_message(MY_INFORMATION_LEVEL, "killed failed id: %d failed: %d",
                  session_id, srv_err); /* purecov: inspected */
    }
  }
  DBUG_RETURN(srv_err);
}

long Sql_service_command_interface::get_server_super_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::get_server_super_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_get_server_super_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
      queue_new_method_for_application(&Sql_service_commands::internal_get_server_super_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_get_server_super_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_get_server_super_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long server_super_read_only= -1;

  long srv_error=
    sql_interface->execute_query("SELECT @@GLOBAL.super_read_only", &rset);
  if (srv_error == 0 && rset.get_rows() > 0)
  {
    server_super_read_only= rset.getLong(0);
  }

  DBUG_RETURN(server_super_read_only);
}

long Sql_service_command_interface::get_server_read_only()
{
  DBUG_ENTER("Sql_service_command_interface::get_server_read_only");
  long error=0;

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.internal_get_server_read_only(m_server_interface);
  }
  else
  {
    m_plugin_session_thread->
      queue_new_method_for_application(&Sql_service_commands::internal_get_server_read_only);
    error= m_plugin_session_thread->wait_for_method_execution();
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_get_server_read_only(Sql_service_interface *sql_interface)
{
  DBUG_ENTER("Sql_service_commands::internal_get_server_read_only");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  longlong server_read_only= -1;
  long srv_error= sql_interface->execute_query("SELECT @@GLOBAL.read_only", &rset);
  if (srv_error == 0 && rset.get_rows())
  {
    server_read_only= rset.getLong(0);
  }

  DBUG_RETURN(server_read_only);
}

int Sql_service_command_interface::get_server_gtid_executed(string& gtid_executed)
{
  DBUG_ENTER("Sql_service_command_interface::get_server_gtid_executed");
  long error=0;

  /* No support for this method on thread isolation mode */
  DBUG_ASSERT(connection_thread_isolation != PSESSION_DEDICATED_THREAD);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.
               internal_get_server_gtid_executed(m_server_interface,
                                                 gtid_executed);
  }

  DBUG_RETURN(error);
}

int Sql_service_commands::
internal_get_server_gtid_executed(Sql_service_interface *sql_interface,
                                  std::string& gtid_executed)
{
  DBUG_ENTER("Sql_service_command_interface::get_server_gtid_executed");

  DBUG_ASSERT(sql_interface != NULL);

  Sql_resultset rset;
  long srv_err=
    sql_interface->execute_query("SELECT @@GLOBAL.gtid_executed", &rset);
  if (srv_err == 0 && rset.get_rows() > 0)
  {
    gtid_executed.assign(rset.getString(0));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

long Sql_service_command_interface::
wait_for_server_gtid_executed(std::string& gtid_executed,
                              int timeout)
{
  DBUG_ENTER("Sql_service_command_interface::wait_for_server_gtid_executed");
  long error=0;

  /* No support for this method on thread isolation mode */
  DBUG_ASSERT(connection_thread_isolation != PSESSION_DEDICATED_THREAD);

  if (connection_thread_isolation != PSESSION_DEDICATED_THREAD)
  {
    error= sql_service_commands.
      internal_wait_for_server_gtid_executed(m_server_interface,
                                             gtid_executed, timeout);
  }

  DBUG_RETURN(error);
}

long Sql_service_commands::
internal_wait_for_server_gtid_executed(Sql_service_interface *sql_interface,
                                       std::string& gtid_executed,
                                       int timeout)
{
  DBUG_ENTER("Sql_service_commands::internal_wait_for_server_gtid_executed");

  DBUG_ASSERT(sql_interface != NULL);

  DBUG_EXECUTE_IF("sql_int_wait_for_gtid_executed_no_timeout", { timeout= 0; });

  std::stringstream ss;
  ss << "SELECT WAIT_FOR_EXECUTED_GTID_SET('" << gtid_executed << "'";
  if (timeout > 0)
  {
    ss << ", " << timeout << ")";
  }
  else
  {
    ss << ")";
  }

  std::string query= ss.str();
  Sql_resultset rset;
  long srv_err= sql_interface->execute_query(query, &rset);
  if (srv_err)
  {
    /* purecov: begin inspected */
    std::stringstream errorstream;
    errorstream << "Internal query: " << query;
    errorstream << " result in error. Error number: " << srv_err;

    log_message(MY_ERROR_LEVEL, errorstream.str().c_str());
    DBUG_RETURN(1);
    /* purecov: end */
  }
  else if(rset.get_rows() > 0)
  {
    if (rset.getLong(0) == 1)
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


Session_plugin_thread::
Session_plugin_thread(Sql_service_commands* command_interface)
  : command_interface(command_interface), m_server_interface(NULL),
   incoming_methods(NULL), m_plugin_pointer(NULL),
   m_method_execution_completed(false), m_method_execution_return_value(0),
   m_session_thread_running(false), m_session_thread_starting(false),
   m_session_thread_terminate(false),
   m_session_thread_error(0)
{
  mysql_mutex_init(key_GR_LOCK_session_thread_run, &m_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_session_thread_run, &m_run_cond);
  mysql_mutex_init(key_GR_LOCK_session_thread_method_exec, &m_method_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_session_thread_method_exec, &m_method_cond);
  this->incoming_methods= new Synchronized_queue<st_session_method*>();
}

Session_plugin_thread::~Session_plugin_thread()
{
  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
  mysql_mutex_destroy(&m_method_lock);
  mysql_cond_destroy(&m_method_cond);
  delete incoming_methods;
}

void Session_plugin_thread::
queue_new_method_for_application(long (Sql_service_commands::*method)(Sql_service_interface*),
                                 bool terminate)
{
  st_session_method* method_to_execute;
  method_to_execute= (st_session_method*)my_malloc(PSI_NOT_INSTRUMENTED,
                                                   sizeof(st_session_method),
                                                   MYF(0));
  method_to_execute->method= method;
  method_to_execute->terminated=terminate;
  m_method_execution_completed= false;
  incoming_methods->push(method_to_execute);
}


long Session_plugin_thread::wait_for_method_execution()
{
  mysql_mutex_lock(&m_method_lock);
  while (!m_method_execution_completed)
  {
    DBUG_PRINT("sleep",("Waiting for the plugin session thread to execute a method"));
    mysql_cond_wait(&m_method_cond, &m_method_lock);
  }
  mysql_mutex_unlock(&m_method_lock);
  return m_method_execution_return_value;
}

static void *launch_handler_thread(void* arg)
{
  Session_plugin_thread *handler= (Session_plugin_thread*) arg;
  handler->session_thread_handler();
  return 0;
}

int
Session_plugin_thread::launch_session_thread(void* plugin_pointer_var)
{
  DBUG_ENTER("Session_plugin_thread::launch_session_thread(plugin_pointer)");

  //avoid concurrency calls against stop invocations
  mysql_mutex_lock(&m_run_lock);

  m_session_thread_error= 0;
  m_session_thread_terminate= false;
  m_session_thread_starting= true;
  m_plugin_pointer= plugin_pointer_var;

  if ((mysql_thread_create(key_GR_THD_plugin_session,
                           &m_plugin_session_pthd,
                           get_connection_attrib(),
                           launch_handler_thread,
                           (void*)this)))
  {
    m_session_thread_starting= false;
    mysql_mutex_unlock(&m_run_lock); /* purecov: inspected */
    DBUG_RETURN(1);                /* purecov: inspected */
  }

  while (!m_session_thread_running && !m_session_thread_error)
  {
    DBUG_PRINT("sleep",("Waiting for the plugin session thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }

  mysql_mutex_unlock(&m_run_lock);
  DBUG_RETURN(m_session_thread_error);
}

int
Session_plugin_thread::terminate_session_thread()
{
  DBUG_ENTER("Session_plugin_thread::terminate_session_thread()");
  mysql_mutex_lock(&m_run_lock);

  m_session_thread_terminate= true;
  m_method_execution_completed=true;
  queue_new_method_for_application(NULL,true);

  int stop_wait_timeout= GR_PLUGIN_SESSION_THREAD_TIMEOUT;

  while (m_session_thread_running || m_session_thread_starting)
  {
    DBUG_PRINT("loop", ("killing plugin session thread"));

    mysql_cond_broadcast(&m_run_cond);

    struct timespec abstime;
    set_timespec(&abstime, 1);
#ifndef DBUG_OFF
    int error=
#endif
      mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
    if (stop_wait_timeout >= 1)
    {
      stop_wait_timeout= stop_wait_timeout - 1;
    }
    else if (m_session_thread_running || m_session_thread_starting) // quit waiting
    {
      mysql_mutex_unlock(&m_run_lock);
      DBUG_RETURN(1);
    }
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!m_session_thread_running);

  while (!this->incoming_methods->empty())
  {
    st_session_method *method= NULL;
    this->incoming_methods->pop(&method);
    my_free(method);
  }

  mysql_mutex_unlock(&m_run_lock);

  DBUG_RETURN(0);
}

int
Session_plugin_thread::session_thread_handler()
{
  DBUG_ENTER("Session_plugin_thread::session_thread_handler()");

  st_session_method *method= NULL;
  m_server_interface= new Sql_service_interface();
  m_session_thread_error=
    m_server_interface->open_thread_session(m_plugin_pointer);
  DBUG_EXECUTE_IF("group_replication_sql_service_force_error",
                  { m_session_thread_error= 1; });

  mysql_mutex_lock(&m_run_lock);
  m_session_thread_starting= false;
  m_session_thread_running= true;
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  if (m_session_thread_error)
    goto end;

  while (!m_session_thread_terminate)
  {
    this->incoming_methods->pop(&method);

    if (method->terminated)
    {
      my_free(method);
      break;
    }

    long (Sql_service_commands::*method_to_execute)(Sql_service_interface*)= method->method;
    m_method_execution_return_value= (command_interface->*method_to_execute)(m_server_interface);

    my_free(method);
    mysql_mutex_lock(&m_method_lock);
    m_method_execution_completed= true;
    mysql_cond_broadcast(&m_method_cond);
    mysql_mutex_unlock(&m_method_lock);
  }

  mysql_mutex_lock(&m_run_lock);
  while (!m_session_thread_terminate)
  {
    DBUG_PRINT("sleep",("Waiting for the plugin session thread"
      " to be signaled termination"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }
  mysql_mutex_unlock(&m_run_lock);

  end:
  delete m_server_interface;
  m_server_interface = NULL;

  mysql_mutex_lock(&m_run_lock);
  m_session_thread_running= false;
  mysql_mutex_unlock(&m_run_lock);

  DBUG_RETURN(m_session_thread_error);
}

Sql_service_interface*
Session_plugin_thread::get_service_interface()
{
  return m_server_interface;
}
