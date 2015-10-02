/*  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
#ifndef MYSQL_SRV_SESSION_SERVICE_INCLUDED
#define MYSQL_SRV_SESSION_SERVICE_INCLUDED

/**
  @file
  Header file for the Server session service. This service is to provide
  of creating sessions with the server. These sessions can be furtherly used
  together with the Command service to execute commands in the server.
*/


#ifdef __cplusplus
class Srv_session;
typedef class Srv_session* MYSQL_SESSION;
#else
struct Srv_session;
typedef struct Srv_session* MYSQL_SESSION;
#endif

#ifndef MYSQL_ABI_CHECK
#include "mysql/plugin.h"  /* MYSQL_THD */
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*srv_session_error_cb)(void *ctx,
                                     unsigned int sql_errno,
                                     const char *err_msg);

extern struct srv_session_service_st
{
  int (*init_session_thread)(const void *plugin);

  void (*deinit_session_thread)();

  MYSQL_SESSION (*open_session)(srv_session_error_cb error_cb,
                                void *plugix_ctx);

  int (*detach_session)(MYSQL_SESSION session);

  int (*close_session)(MYSQL_SESSION session);

  int (*server_is_available)();
} *srv_session_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define srv_session_init_thread(plugin) \
        srv_session_service->init_session_thread((plugin))

#define srv_session_deinit_thread() \
        srv_session_service->deinit_session_thread()

#define srv_session_open(cb, ctx) \
        srv_session_service->open_session((cb), (ctx))

#define srv_session_detach(session) \
        srv_session_service->detach_session((session))

#define srv_session_close(session) \
        srv_session_service->close_session((session))

#define srv_session_server_is_available() \
        srv_session_service->server_is_available()

#else

/**
  Initializes the current physical thread to use with session service.

  Call this function ONLY in physical threads which are not initialized in
  any way by the server.

  @param plugin  Pointer to the plugin structure, passed to the plugin over
                 the plugin init function.

  @return
    0  success
    1  failure
*/
int srv_session_init_thread(const void *plugin);

/**
  Deinitializes the current physical thread to use with session service.


  Call this function ONLY in physical threads which were initialized using
  srv_session_init_thread().
*/
void srv_session_deinit_thread();

/**
  Opens a server session.

  In a thread not initialized by the server itself, this function should be
  called only after srv_session_init_thread() has already been called.

  @param error_cb    Default completion callback
  @param plugin_ctx  Plugin's context, opaque pointer that would
                     be provided to callbacks. Might be NULL.
  @return
    session   on success
    NULL      on failure
*/
MYSQL_SESSION srv_session_open(srv_session_error_cb cb, void *plugix_ctx);

/**
  Detaches a session from current physical thread.

  Detaches a previously attached session. Sessions are automatically attached
  when they are used with the Command service (command_service_run_command()).
  If the session is opened in a spawned thread, then it will stay attached
  after command_service_run_command() until another session is used in the
  same physical thread. The command services will detach the previously used
  session and attach the one to be used for execution.

  This function should be called in case the session has to be used in
  different physical thread. It will unbound the session from the current
  physical thread. After that the session can be used in a different thread.

  @param session  Session to detach

  @returns
    0  success
    1  failure
*/
int srv_session_detach(MYSQL_SESSION session);

/**
  Closes a previously opened session.

  @param session  Session to close

  @return
    0  success
    1  failure
*/
int srv_session_close(MYSQL_SESSION session);

/**
  Returns if the server is available (not booting or shutting down)

  @return
    0  not available
    1  available
*/
int srv_session_server_is_available();


#endif

#ifdef __cplusplus
}
#endif

#endif /* MYSQL_SRV_SESSION_SERVICE_INCLUDED */
