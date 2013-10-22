/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_APPLIER_INCLUDE
#define GCS_APPLIER_INCLUDE

#include <vector>
#include "gcs_plugin_utils.h"
#include <my_global.h>
#include <applier_interfaces.h>
#include "pipeline_factory.h"

class Applier_module
{

public:
  Applier_module();
  ~Applier_module();

  /**
    Initializes and launches the applier thread

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_applier_thread();

  /**
    Terminates the applier thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    A timeout occurred
  */
  int terminate_applier_thread();

  /**
    Is the applier marked for shutdown?

    @return is the applier on shutdown
      @retval 0      no
      @retval !=0    yes
  */
  bool is_applier_thread_aborted()
  {
    return (applier_aborted || applier_thd->killed);
  }

  /**
    Is the applier running?

    @return applier running?
      @retval 0      no
      @retval !=0    yes
  */
  bool is_running()
  {
    return applier_running;
  }

  /**
    Configure the applier pipeline according to the given configuration

    @param[in]  pipeline_type    the chosen pipeline
    @param[in]  stop_timeout     the timeout when waiting on shutdown

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int setup_applier_module(Handler_pipeline_type pipeline_type,
                           ulong stop_timeout);

  /**
    Runs the applier thread process, reading events and processing them.

    @note When killed, the thread will finish handling the current packet, and
    then die, ignoring all possible existing events in the incoming queue.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int applier_thread_handle();

  /**
    Queues the packet coming from the reader for future application.

    @param[in]  data      the packet data
    @param[in]  len       the packet length

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle(const char *data, uint len)
  {
    return this->incoming->push(new Packet((uchar*)data, len));
  }

  /**
    Terminates the pipeline, shutting down the handlers and deleting them.

    @note the pipeline will always be deleted even if an error occurs.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on pipeline termination
  */
  int terminate_applier_pipeline();

  /**
    Sets the applier shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout (ulong timeout){
    stop_wait_timeout= timeout;
  }

private:

  /**
    Cleans the thread context for the applier thread
    This includes such tasks as removing the thread from the global thread list
  */
  void clean_applier_thread_context();

  /**
    Set the thread context for the applier thread.
    This allows the thread to behave like an slave thread and perform
    such tasks as queuing to a relay log.
  */
  void set_applier_thread_context();

  //applier thread variables
  pthread_t applier_pthd;
#ifdef HAVE_PSI_INTERFACE
  PSI_thread_key key_thread_receiver;
#endif
  THD *applier_thd;

  //run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t  run_cond;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key run_key_mutex;
  PSI_cond_key  run_key_cond;
#endif
  //Applier running flag
  bool applier_running;
  //Applier abort flag
  bool applier_aborted;

  //The incoming event queue
  Synchronized_queue<Packet *> *incoming;

  //The applier pipeline for event execution
  EventHandler *pipeline;

  //Applier timeout on shutdown
  ulong stop_wait_timeout;
};

#endif /* GCS_APPLIER_INCLUDE */
