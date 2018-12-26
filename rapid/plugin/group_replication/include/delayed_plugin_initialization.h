/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DELAYED_PLUGIN_INITIALIZATION_INCLUDE
#define DELAYED_PLUGIN_INITIALIZATION_INCLUDE

#include "read_mode_handler.h"

class Delayed_initialization_thread
{

public:

  Delayed_initialization_thread();

  /**
    The class destructor
  */
  ~Delayed_initialization_thread();

  /**
    The thread handler.

    @return
      @retval 0      OK
      @retval !=0    Error
  */
  int initialization_thread_handler();

  /**
    Initialize a thread where the plugin services will be initialized

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int launch_initialization_thread();

  /**
    Signals the plugin initialization thread that the server is ready.
  */
  void signal_thread_ready();

  /**
    Wait for the initialization thread to do its job.
  */
  void wait_for_thread_end();

  /**
    Signal that the read mode is set on the server.
  */
  void signal_read_mode_ready();

  /**
    Wait for the read mode to be set by the thread process.
  */
  void wait_for_read_mode();

private:

  //Delayed_initialization_thread variables

  /* Is the thread running */
  bool thread_running;

  /* Is the server ready*/
  bool is_server_ready;

  /* Is the read mode already set*/
  bool is_super_read_only_set;

  /* Thread related structures */

  my_thread_handle delayed_init_pthd;
  //run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t  run_cond;
  mysql_mutex_t server_ready_lock;
  mysql_cond_t  server_ready_cond;
};

#endif /* DELAYED_PLUGIN_INITIALIZATION_INCLUDE */
