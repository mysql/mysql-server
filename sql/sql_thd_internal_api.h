/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_THD_INTERNAL_API_INCLUDED
#define SQL_THD_INTERNAL_API_INCLUDED

/*
  This file defines THD-related API calls that are meant for internal
  usage (e.g. InnoDB, Thread Pool) only. There are therefore no stabilty
  guarantees.
*/

#include "my_global.h"

class THD;

/**
  Set up various THD data for a new connection

  @param              thd            THD object
  @param              stack_start    Start of stack for connection
*/
int thd_init(THD *thd, char *stack_start);

/**
  Create a THD and do proper initialization of it.

  @param enable_plugins     Should dynamic plugin support be enabled?
  @param background_thread  Is this a background thread?

  @note Dynamic plugin support is only possible for THDs that
        are created after the server has initialized properly.
  @note THDs for background threads are currently not added to
        the global THD list. So they will e.g. not be visible in
        SHOW PROCESSLIST and the server will not wait for them to
        terminate during shutdown.
*/
THD *create_thd(bool enable_plugins, bool background_thread);

/**
  Cleanup the THD object, remove it from the global list of THDs
  and delete it.

  @param    THD   pointer to THD object.
*/
void destroy_thd(THD *thd);

/**
  Set thread stack in THD object

  @param thd              Thread object
  @param stack_start      Start of stack to set in THD object
*/
void thd_set_thread_stack(THD *thd, const char *stack_start);

#endif // SQL_THD_INTERNAL_API_INCLUDED
