/*  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file
  Server session service implementation. For more information please check
  the function comments.
*/
#include "srv_session.h"
/*
 service_srv_session.h should not be first to be included as it will include
 - include/mysql/service_srv_session.h
 - include/mysql/plugin.h
 - include/mysql/services.h
 - include/mysql/service_command.h
 which in turn will need MYSQL_SESSION but won't see it, as it will be
 declared after the includes.
 */
#include "mysql/service_srv_session.h" 
#include "sql_class.h"
#include "conn_handler/connection_handler_manager.h"

extern "C"
{

/**
  Initializes physical thread to use with session service.

  @return
    0  success
    1  failure
*/
int srv_session_init_thread(const void *plugin)
{
  return Srv_session::init_thread(plugin);
}


/**
  Deinitializes physical thread to use with session service
*/
void srv_session_deinit_thread()
{
  Srv_session::deinit_thread();
}


/**
  Opens server session

  @param error_cb              Default completion callback
  @param plugin_ctx            Plugin's context, opaque pointer that would
                               be provided to callbacks. Might be NULL.
  @return
    handler of session   on success
    NULL                 on failure
*/
Srv_session* srv_session_open(srv_session_error_cb error_cb, void *plugin_ctx)
{
  DBUG_ENTER("srv_session_open");

  if (!srv_session_server_is_available())
  {
    if (error_cb)
      error_cb(plugin_ctx, ER_SERVER_ISNT_AVAILABLE,
               ER_DEFAULT(ER_SERVER_ISNT_AVAILABLE));
    DBUG_RETURN(NULL);
  }

  bool simulate_reach_max_connections= false;
  DBUG_EXECUTE_IF("simulate_reach_max_connections",
                  simulate_reach_max_connections= true;);

  Connection_handler_manager *conn_manager=
      Connection_handler_manager::get_instance();

  if (simulate_reach_max_connections ||
      !conn_manager->check_and_incr_conn_count())
  {
    if (error_cb)
      error_cb(plugin_ctx, ER_CON_COUNT_ERROR, ER_DEFAULT(ER_CON_COUNT_ERROR));
    DBUG_RETURN(NULL);
  }

  Srv_session* session= new (std::nothrow) class Srv_session(error_cb, plugin_ctx);

  if (!session)
  {
    DBUG_PRINT("error", ("Can't allocate a Srv_session object"));
    connection_errors_internal++;
    if (error_cb)
      error_cb(plugin_ctx, ER_OUT_OF_RESOURCES, ER_DEFAULT(ER_OUT_OF_RESOURCES));
  }
  else
  {
    THD *current= current_thd;
    THD *stack_thd= session->get_thd();

    session->get_thd()->thread_stack= reinterpret_cast<char *>(&stack_thd);
    session->get_thd()->store_globals();

    bool result= session->open();

    session->get_thd()->restore_globals();

    if (result)
    {
      delete session;
      session= NULL;
    }

    if (current)
      current->store_globals();
  }
  DBUG_RETURN(session);
}


/**
  Detaches a session from current physical thread.

  @param session  Session handle to detach

  @returns
    0  success
    1  failure
*/
int srv_session_detach(Srv_session* session)
{
  DBUG_ENTER("srv_session_detach");

  if (!session || !Srv_session::is_valid(session))
  {
    DBUG_PRINT("error", ("Session is not valid"));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(session->detach());
}


/**
  Closes a session.

  @param session  Session handle to close

  @returns
    0  Session successfully closed
    1  Session wasn't found or key doesn't match
*/
int srv_session_close(Srv_session* session)
{
  DBUG_ENTER("srv_session_close");

  if (!session || !Srv_session::is_valid(session))
  {
    DBUG_PRINT("error", ("Session is not valid"));
    DBUG_RETURN(1);
  }

  session->close();
  delete session;

  /*
    Here we don't need to reattach the previous session, as the next
    function (run_command() for example) will attach to whatever is needed.
  */
  DBUG_RETURN(0);
}

/**
  Returns if the server is available (not booting or shutting down)

  @return
    0  not available
    1  available
*/
int srv_session_server_is_available()
{
  return get_server_state() == SERVER_OPERATING;
}

} /* extern "C" */
