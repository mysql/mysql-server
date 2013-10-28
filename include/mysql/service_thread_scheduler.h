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
extern "C" {
#endif

struct Connection_handler_functions;
struct THD_event_functions;

extern struct my_thread_scheduler_service {
  int (*connection_handler_set)(struct Connection_handler_functions *,
                                struct THD_event_functions *);
  int (*connection_handler_reset)();
} *my_thread_scheduler_service;


#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_connection_handler_set(F, M) \
  my_thread_scheduler_service->connection_handler_set((F), (M))
#define my_connection_handler_reset() \
  my_thread_scheduler_service->connection_handler_reset()

#else

/**
   Instantiates Plugin_connection_handler based on the supplied
   Conection_handler_functions and sets it as the current
   connection handler.

   Also sets the THD_event_functions functions which will
   be called by the server when e.g. begining a wait.

   Remembers the existing connection handler so that it can be restored later.

   @param chf  struct with functions to be called when e.g. handling
               new clients.
   @param tef  struct with functions to be called when events
               (e.g. lock wait) happens.

   @note Both pointers (i.e. not the structs themselves) will be copied,
         so the structs must not disappear.

   @note We don't support dynamically loading more than one connection handler.

   @retval 1  failure
   @retval 0  success
*/
int my_connection_handler_set(struct Connection_handler_functions *chf,
                              struct THD_event_functions *tef);

/**
   Destroys the current connection handler and restores the previous.
   Should only be called after calling my_connection_handler_set().

   @retval 1  failure
   @retval 0  success
*/
int my_connection_handler_reset();

#endif /* MYSQL_DYNAMIC_PLUGIN */

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_THREAD_SCHEDULER_INCLUDED */
