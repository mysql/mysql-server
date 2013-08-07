/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SERVICE_THREAD_SCHEDULER_INCLUDED
#define SERVICE_THREAD_SCHEDULER_INCLUDED

#ifdef __cplusplus
class Connection_handler;
#define MYSQL_CONNECTION_HANDLER Connection_handler*
#else
#define MYSQL_CONNECTION_HANDLER void*
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Connection_handler_callback;

extern struct my_thread_scheduler_service {
  int (*connection_handler_set)(MYSQL_CONNECTION_HANDLER conn_handler,
                                struct Connection_handler_callback *cb);
  int (*connection_handler_reset)();
} *my_thread_scheduler_service;
#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_connection_handler_set(F, M) my_thread_scheduler_service->connection_handler_set((F), (M))
#define my_connection_handler_reset() my_thread_scheduler_service->connection_handler_reset()

#else

int my_connection_handler_set(MYSQL_CONNECTION_HANDLER conn_handler,
                              struct Connection_handler_callback *cb);
int my_connection_handler_reset();

#endif

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_THREAD_SCHEDULER_INCLUDED */
