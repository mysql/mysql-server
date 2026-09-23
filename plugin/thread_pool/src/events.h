#pragma once

/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "my_dbug.h"

struct tp_client_low_level_t;
struct tp_group_low_level_t;
struct tp_group_t;
struct tp_thread_t;

/**
  @file

  @brief Thread Pool events - shared with poll() and epoll() versions.
*/

/**
  Process one direct query

  @param my_thread_data     Thread data
  @param my_tp_group        Thread group data

*/
void process_direct_query(tp_thread_t *my_thread_data, tp_group_t *my_tp_group);

/**
  Check if we can process query direct from wait for events handling

  @param             my_tp_group          Thread group data
  @param             events_total         Total number of events received
  @param             allowed_direct       Is it allowed to handle direct
  processing

  @retval            true                 Process direct from events handler
  @retval            false                Don't process direct from events
  handler
*/
bool can_process_direct(tp_group_t *my_tp_group, int events_total,
                        bool allowed_direct);

/**
  Insert event into next_events array

  @param      next_events     Array of client contexts that are ready for
                              processing a query
  @param      events_copied   Counter of events copied to array, incremented
                              by the method
  @param      client_cntx     Client context for connection ready to execute
                              query
*/
void insert_next_events(tp_client_low_level_t **next_events, int *events_copied,
                        tp_client_low_level_t *client_cntx);

/**
  Check if we should continue checking for events

  @param           my_tp_group     Thread group data
  @param           my_thread_data  Thread data
  @param           loop            Loop counter

  @retval          true            Continue waiting for events
  @retval          false           Stop waiting for events
*/
bool continue_wait_events(tp_group_t *my_tp_group, tp_thread_t *my_thread_data,
                          int loop);

/**
  Read all wake flags received

  @param group_cntx        Low level group context
  @param client_cntx_array Array of low level client contexts
  @param added_events      Number of events added
*/
void waiter_flush(tp_group_low_level_t *group_cntx,
                  tp_client_low_level_t **client_cntx_array, int *added_events,
                  my_socket read_fd);

/**
  Handle flush events

  @param            group_cntx        Low level group context
  @param            next_events       Array of client contexts ready for query
                                      processing
  @param            added_events      Number of events in array of client
                                      contexts
  @param            fd                Socket fd of our end of the socketpair
  @param            waiter_flush_flag Flag indicating data is available on
                                      internal socket
*/
inline void handle_waiter_flush(tp_group_low_level_t *group_cntx,
                                tp_client_low_level_t **next_events,
                                int *added_events, my_socket fd,
                                bool waiter_flush_flag) {
  if (waiter_flush_flag) {
    DBUG_PRINT("tp", ("added_events = %d before waiter_flush", *added_events));
    waiter_flush(group_cntx, next_events, added_events, fd);
    DBUG_PRINT("tp", ("added_events = %d after waiter_flush", *added_events));
  }
}
