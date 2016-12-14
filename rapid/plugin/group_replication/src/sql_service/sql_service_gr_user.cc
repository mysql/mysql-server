/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_service_gr_user.h"
#include "plugin.h"

long execute_user_query(Sql_service_interface *sql_interface, std::string query)
{
  DBUG_ENTER("execute_user_query");
  long srv_err= sql_interface->execute_query(query);
  if(srv_err)
  {
    log_message(MY_ERROR_LEVEL,
                "The internal plugin query '%s' resulted in failure. errno: %d",
                query.c_str(), srv_err); /* purecov: inspected */
  }
  DBUG_RETURN(srv_err);
}

int create_group_replication_user(bool threaded,
                                  Sql_service_interface *sql_interface)
{
  DBUG_ENTER("create_group_replication_user");

  int error = 0;
  Sql_service_interface *server_interface= NULL;
  if (sql_interface == NULL) {
    server_interface= new Sql_service_interface();
    if (!threaded)
      error = server_interface->open_session();
    else
      error = server_interface->open_thread_session(get_plugin_pointer());

    if (error) {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "Can't establish a internal server connection to execute"
                  " plugin operations");
      delete server_interface;
      DBUG_RETURN(error);
      /* purecov: end */
    }
  }
  else
  {
    server_interface= sql_interface;
  }

  error= server_interface->set_session_user("root");
  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't use the root account to create the plugin associated user"
                " account to access the server.");
    if (sql_interface == NULL)
      delete server_interface;
    DBUG_RETURN(error);
    /* purecov: end */
  }

  long srv_err= 0;
  std::string query;
  query.assign("SET @GR_OLD_LOG_BIN=@@SQL_LOG_BIN;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("SET SESSION SQL_LOG_BIN=0;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("CREATE USER IF NOT EXISTS " GROUPREPL_ACCOUNT " IDENTIFIED"
               " WITH mysql_native_password AS"
               " '*7CF5CA9067EC647187EB99FCC27548FBE4839AE3' ACCOUNT LOCK;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("GRANT SELECT ON performance_schema.replication_connection_status"
               " TO " GROUPREPL_ACCOUNT);
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("GRANT SUPER ON *.* TO " GROUPREPL_ACCOUNT);
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("FLUSH PRIVILEGES;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

err:
  query.assign("SET SESSION SQL_LOG_BIN=@GR_OLD_LOG_BIN;");
  srv_err+= execute_user_query(server_interface,query);

  if (sql_interface == NULL)
  {
    delete server_interface;
  }

  DBUG_RETURN(srv_err);
}


int remove_group_replication_user(bool threaded,
                                  Sql_service_interface *sql_interface)
{
  DBUG_ENTER("remove_group_replication_user");

  int error = 0;
  Sql_service_interface *server_interface= NULL;
  if (sql_interface == NULL) {
    /* purecov: begin inspected */
    server_interface= new Sql_service_interface();
    if (!threaded)
      error = server_interface->open_session();
    else
      error = server_interface->open_thread_session(get_plugin_pointer());

    if (error) {
      log_message(MY_ERROR_LEVEL,
                  "Can't establish a internal server connection to execute"
                  " plugin operations");
      delete server_interface;
      DBUG_RETURN(error);
    }
    /* purecov: end */
  }
  else
  {
    server_interface= sql_interface;
  }

  error= server_interface->set_session_user("root");
  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't use the root account to create the plugin associated"
                " user account to access the server.");
    delete server_interface;
    DBUG_RETURN(error);
    /* purecov: end */
  }

  error= server_interface->is_acl_disabled();
  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Internal account %s can't be removed because server is running"
                " without user privileges (\"skip-grant-tables\" switch)",
                GROUPREPL_ACCOUNT);
    delete server_interface;
    DBUG_RETURN(error);
    /* purecov: end */
  }

  long srv_err= 0;
  std::string query;

  query.assign("SET @GR_OLD_LOG_BIN=@@SQL_LOG_BIN;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("SET SESSION SQL_LOG_BIN=0;");
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

  query.assign("DROP USER " GROUPREPL_ACCOUNT);
  if ((srv_err= execute_user_query(server_interface,query)))
    goto err; /* purecov: inspected */

err:
  query.assign("SET SESSION SQL_LOG_BIN=@GR_OLD_LOG_BIN;");
  srv_err+= execute_user_query(server_interface,query);

  if (sql_interface == NULL)
  {
    delete server_interface; /* purecov: inspected */
  }
  DBUG_RETURN(srv_err);
}

int check_group_replication_user(bool threaded,
                                 Sql_service_interface *sql_interface)
{
  DBUG_ENTER("check_group_replication_user");

  int error = 0;
  Sql_service_interface *server_interface= NULL;
  if (sql_interface == NULL) {
    /* purecov: begin inspected */
    server_interface= new Sql_service_interface();
    if (!threaded)
      error = server_interface->open_session();
    else
      error = server_interface->open_thread_session(get_plugin_pointer());

    if (error) {
      log_message(MY_ERROR_LEVEL,
                  "Can't establish a internal server connection to execute"
                  " plugin operations");
      delete server_interface;
      DBUG_RETURN(-1);
    }
    /* purecov: end */
  }
  else
  {
    server_interface= sql_interface;
  }

  error= server_interface->set_session_user("root");
  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't use the root account to create the plugin associated user"
                " account to access the server.");
    if (sql_interface == NULL)
      delete server_interface;
    DBUG_RETURN(-1);
    /* purecov: end */
  }

  int exists= 0;
  Sql_resultset rset;
  std::string query;
  query.assign("SELECT COUNT(*) FROM mysql.user where user='" GROUPREPL_USER "';");
  long srv_err=
    server_interface->execute_query(query, &rset);
  if (srv_err == 0)
  {
    exists= rset.getLong(0) > 0;
  }
  else
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "The internal plugin query '%s' resulted in failure. errno: %d",
                query.c_str(), srv_err);
    exists= -1;
    /* purecov: end */
  }

  if (sql_interface == NULL)
  {
    delete server_interface; /* purecov: inspected */
  }

  DBUG_RETURN(exists);
}
