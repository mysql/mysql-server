/*  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <stdint.h>

#include "key.h"
#include "my_thread_local.h"
#include "sql_class.h"
#include "srv_session.h"
#include "violite.h"

/**
  @file
  Implementation of setters and getters of some properties of a session.
*/

#define VALID_SESSION(s) ((s) && Srv_session::is_valid((s)))

/**
  Returns the THD of a session.

  @param session  Session
*/
extern "C"
THD* srv_session_info_get_thd(Srv_session *session)
{
  return VALID_SESSION(session)? session->get_thd() : NULL;
}


/**
  Returns the ID of a session.

  The value returned from THD::thread_id()

  @param session  Session
*/
extern "C"
my_thread_id srv_session_info_get_session_id(Srv_session *session)
{
  return VALID_SESSION(session)? session->get_session_id() : 0;
}


/**
  Returns the client port of a session.

  @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
  This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.

  @param session  Session
*/
extern "C"
uint16_t srv_session_info_get_client_port(Srv_session *session)
{
  return VALID_SESSION(session)? session->get_client_port() : 0;
}


/**
  Sets the client port of a session.

  @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
  This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.

  @param session  Session
  @param port     Port number

  @return
    0 success
    1 failure
*/
extern "C"
int srv_session_info_set_client_port(Srv_session *session, uint16_t port)
{
  return VALID_SESSION(session)? session->set_client_port(port),0 : 1;
}


/**
  Returns the current database of a session.

  @param session  Session
*/
extern "C"
LEX_CSTRING srv_session_info_get_current_db(Srv_session *session)
{
  static LEX_CSTRING empty= { NULL, 0 };
  return VALID_SESSION(session)? session->get_current_database() : empty;
}


/**
  Sets the connection type of a session.

  @see enum_vio_type

  @note If NO_VIO_TYPE passed as type the call will fail.

  @return
    0  success
    1  failure
*/
extern "C"
int srv_session_info_set_connection_type(Srv_session *session,
                                         enum_vio_type type)
{
  return VALID_SESSION(session)? session->set_connection_type(type) : 1;
}


/**
  Returns whether the session was killed

  @param session  Session

  @return
    0  not killed
    1  killed
*/
extern "C"
int srv_session_info_killed(Srv_session *session)
{
  return (!VALID_SESSION(session) || session->get_thd()->killed)? 1:0;
}

/**
  Returns the number opened sessions in thread initialized by srv_session
  service.
*/
unsigned int srv_session_info_session_count()
{
  return Srv_session::session_count();
}


/**
  Returns the number opened sessions in thread initialized by srv_session
  service.

  @param plugin Pointer to the plugin structure, passed to the plugin over
                the plugin init function.
*/
unsigned int srv_session_info_thread_count(const void *plugin)
{
  return Srv_session::thread_count(plugin);
}
