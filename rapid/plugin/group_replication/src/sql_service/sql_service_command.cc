/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include <sstream>

using std::string;

Sql_service_command::Sql_service_command()
  : server_interface(NULL)
{}

Sql_service_command::~Sql_service_command()
{
  if(server_interface != NULL)
  {
    delete server_interface;
  }
}

int Sql_service_command::
establish_session_connection(bool threaded, void *plugin_pointer)
{
  DBUG_ASSERT(server_interface == NULL);

  int error= 0;
  server_interface= new Sql_service_interface();
  if(!threaded)
    error= server_interface->open_session();
  else
    error= server_interface->open_thread_session(plugin_pointer);

  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't establish a internal server connection to execute"
                " plugin operations");
    delete server_interface;
    server_interface= NULL;
    return error;
    /* purecov: end */
  }

  return error;
}

Sql_service_interface* Sql_service_command::get_sql_service_interface()
{
  return server_interface;
}

int Sql_service_command::set_interface_user(const char* user)
{
  return server_interface->set_session_user(user);
}


long Sql_service_command::set_super_read_only()
{
  DBUG_ENTER("Sql_service_command::set_super_read_only");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;
  long srv_err= server_interface->execute_query("SET GLOBAL super_read_only= 1;");
  if (srv_err == 0)
  {
#ifndef DBUG_OFF
    server_interface->execute_query("SELECT @@GLOBAL.super_read_only;", &rset);
    DBUG_ASSERT(rset.getLong(0) == 1);
    log_message(MY_INFORMATION_LEVEL, "Setting super_read_only mode on the "
                "server");
#endif
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "'SET super_read_only= 1' query execution"
                " resulted in failure. errno: %d", srv_err); /* purecov: inspected */
  }

  DBUG_RETURN(srv_err);
}

long Sql_service_command::reset_super_read_only()
{
  DBUG_ENTER("Sql_service_command::reset_super_read_only");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;

  const char * query= "SET GLOBAL super_read_only= 0";
  int srv_err= server_interface->execute_query(query);
  if(srv_err)
  {
    log_message(MY_ERROR_LEVEL, "SET super_read_only query execution "
                "resulted in failure. errno: %d", srv_err); /* purecov: inspected */
  }
#ifndef DBUG_OFF
  else
  {
    query= "SELECT @@GLOBAL.super_read_only;";
    server_interface->execute_query(query, &rset);
    DBUG_ASSERT(rset.getLong(0) == 0);

    log_message(MY_INFORMATION_LEVEL, "Resetting super_read_only mode on the"
                  " server ");
  }
#endif
  DBUG_RETURN(srv_err);
}

long Sql_service_command::reset_read_only()
{
  DBUG_ENTER("Sql_service_command::reset_read_only");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;

  const char* query= "SET GLOBAL read_only= 0";
  long srv_err= server_interface->execute_query(query);

  if (srv_err)
  {

    log_message(MY_ERROR_LEVEL, "SET read_only query execution "
                "resulted in failure. errno: %d", srv_err); /* purecov: inspected */
  }
#ifndef DBUG_OFF
  else
  {
    query= "SELECT @@GLOBAL.read_only";
    server_interface->execute_query(query, &rset);
    DBUG_ASSERT(rset.getLong(0) == 0);
    log_message(MY_INFORMATION_LEVEL, "Resetting read_only mode on the server ");
  }
#endif

  DBUG_RETURN(srv_err);
}

long Sql_service_command::kill_session(uint32_t session_id,
                                       MYSQL_SESSION session)
{
  DBUG_ENTER("Sql_service_command::kill_session");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;
  long srv_err= 0;
  if (!server_interface->is_session_killed(session))
  {
    COM_DATA data;
    data.com_kill.id= session_id;
    srv_err= server_interface->execute(data, COM_PROCESS_KILL, &rset);
    if (srv_err == 0)
    {
      log_message(MY_INFORMATION_LEVEL, "killed session id: %d status: %d",
                  session_id, server_interface->is_session_killed(session));
    }
    else
    {
      log_message(MY_INFORMATION_LEVEL, "killed failed id: %d failed: %d",
                  session_id, srv_err); /* purecov: inspected */
    }
  }
  DBUG_RETURN(srv_err);
}

long Sql_service_command::get_server_super_read_only()
{
  DBUG_ENTER("Sql_service_command::get_server_super_read_only");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;
  long server_super_read_only= -1;

  long srv_error=
      server_interface->execute_query("SELECT @@GLOBAL.super_read_only", &rset);
  if (srv_error == 0)
  {
    server_super_read_only= rset.getLong(0);
  }
  else
  {
    log_message(MY_ERROR_LEVEL, " SELECT @@GLOBAL.read_only "
                "resulted in failure. errno: %d", srv_error); /* purecov: inspected */
  }

  DBUG_RETURN(server_super_read_only);
}

long Sql_service_command::get_server_read_only()
{
  DBUG_ENTER("Sql_service_command::get_server_read_only");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;
  long server_read_only= -1;
  long srv_error= server_interface->execute_query("SELECT @@GLOBAL.read_only", &rset);
  if (srv_error == 0)
  {
    server_read_only= rset.getLong(0);
  }
  else
  {
    log_message(MY_ERROR_LEVEL, " SELECT @@GLOBAL.read_only "
                "resulted in failure. errno: %d", srv_error); /* purecov: inspected */
  }

  DBUG_RETURN(server_read_only);
}

int Sql_service_command::get_server_gtid_executed(string& gtid_executed)
{
  DBUG_ENTER("Sql_service_command::get_server_gtid_executed");

  DBUG_ASSERT(server_interface != NULL);

  Sql_resultset rset;
  long srv_err=
      server_interface->execute_query("SELECT @@GLOBAL.gtid_executed", &rset);
  if (srv_err == 0)
  {
    gtid_executed.assign(rset.getString(0));
    DBUG_RETURN(0);
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "Internal query: SELECT GLOBAL.gtid_executed"
                " resulted in failure. errno: %d", srv_err); /* purecov: inspected */
  }
  DBUG_RETURN(1);
}

long Sql_service_command::wait_for_server_gtid_executed(std::string& gtid_executed,
                                                       int timeout)
{
  DBUG_ENTER("Sql_service_command::wait_for_server_gtid_executed");

  DBUG_ASSERT(server_interface != NULL);

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
  long srv_err= server_interface->execute_query(query);
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
  DBUG_RETURN(0);
}
