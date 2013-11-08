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

#include "gcs_applier.h"
#include <mysqld.h>
#include <mysqld_thd_manager.h>  // Global_THD_manager
#include <thr_alarm.h>
#include <rpl_slave.h>
#include <gcs_replication.h>
#include <debug_sync.h>

static void *launch_handler_thread(void* arg)
{
  Applier_module *handler= (Applier_module*) arg;
  handler->applier_thread_handle();
  return 0;
}

Applier_module::Applier_module()
  :applier_running(false), applier_aborted(false), incoming(NULL),
   pipeline(NULL), stop_wait_timeout(LONG_TIMEOUT)
{

  PSI_cond_info applier_conds[]=
  {
    { &run_key_cond, "COND_applier_run", 0}
  };

  PSI_mutex_info applier_mutexes[]=
  {
    { &run_key_mutex, "LOCK_applier_run", 0}
  };

  register_gcs_psi_keys(applier_mutexes, applier_conds);
  mysql_mutex_init(run_key_mutex, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(run_key_cond, &run_cond, 0);
}

Applier_module::~Applier_module(){
  if (this->incoming)
  {
    while (!this->incoming->empty())
    {
      Packet *packet= NULL;
      this->incoming->pop(&packet);
      delete packet;
    }
    delete incoming;
  }

  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
}

int
Applier_module::setup_applier_module(Handler_pipeline_type pipeline_type,
                                     ulong stop_timeout)
{
  DBUG_ENTER("ApplierModule::setup");

  //create the receiver queue
  this->incoming= new Synchronized_queue<Packet*>();

  stop_wait_timeout= stop_timeout;

  pipeline= NULL;

  DBUG_RETURN(get_pipeline(pipeline_type, &pipeline));
}

void
Applier_module::set_applier_thread_context()
{
  my_thread_init();
  applier_thd= new THD;
  applier_thd->thread_stack= (char*) &applier_thd;
  applier_thd->store_globals();
  init_thr_lock();

  applier_thd->slave_thread= true;
  //TODO: See of the creation of a new type is desirable.
  applier_thd->system_thread= SYSTEM_THREAD_SLAVE_IO;
  applier_thd->security_ctx->skip_grants();

  Global_THD_manager::get_instance()->add_thd(applier_thd);

  applier_thd->init_for_queries();
  set_slave_thread_options(applier_thd);
}

void
Applier_module::clean_applier_thread_context()
{
  applier_thd->release_resources();
  THD_CHECK_SENTRY(applier_thd);
  Global_THD_manager::get_instance()->remove_thd(applier_thd);

  delete applier_thd;

  my_thread_end();
  pthread_exit(0);
}


int
Applier_module::applier_thread_handle()
{
  DBUG_ENTER("ApplierModule::applier_thread_handle()");

  //set the thread context
  set_applier_thread_context();

  Packet *packet= NULL;
  int error= 0;

  applier_running= true;

  //broadcast if the invoker thread is waiting.
  mysql_cond_broadcast(&run_cond);

  while (!error)
  {
    if (is_applier_thread_aborted())
      break;

    if ((error= this->incoming->pop(&packet))) // blocking
    {
      log_message(MY_ERROR_LEVEL, "Error when reading from applier's queue");
      break;
    }

    if (packet == NULL)
    {
      // a NULL packet was added to release the queue
      if (is_applier_thread_aborted())
        break;
      else //something bad happened
      {
        log_message(MY_ERROR_LEVEL, "Error: Null packet on applier's queue");
        error= 1;
        break;
      }
    }

    uchar* payload= packet->payload;
    uchar* payload_end= packet->payload+packet->len;

    Format_description_log_event* fde_evt=
      new Format_description_log_event(BINLOG_VERSION);
    Continuation* cont= new Continuation();

    while ((payload != payload_end) && !error)
    {
      uint event_len= uint4korr(((uchar*)payload) + EVENT_LEN_OFFSET);

      Packet* new_packet= new Packet(payload,event_len);
      payload= payload + event_len;

      PipelineEvent* pevent= new PipelineEvent(new_packet, fde_evt);
      pipeline->handle(pevent, cont);

      if ((error= cont->wait()))
        log_message(MY_ERROR_LEVEL, "Error at event handling! Got error: %d \n", error);

      delete pevent;
    }
    delete packet;
  }

  log_message(MY_INFORMATION_LEVEL, "The applier thread was killed");

  DBUG_EXECUTE_IF("applier_thd_timeout",
                  {
                    const char act[]= "now wait_for signal.applier_continue";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
  applier_running= false;

  clean_applier_thread_context();

  DBUG_RETURN(error);
}

int
Applier_module::initialize_applier_thread()
{
  DBUG_ENTER("ApplierModule::initialize_applier_thd");

  //avoid concurrency calls against stop invocations
  mysql_mutex_lock(&run_lock);

#ifdef HAVE_PSI_INTERFACE
  PSI_thread_info threads[]= {
    { &key_thread_receiver,
      "gcs-applier-module", PSI_FLAG_GLOBAL
    }
  };
  mysql_thread_register("gcs-applier-module", threads, 1);
#endif

  if ((mysql_thread_create(key_thread_receiver,
                           &applier_pthd,
                           get_connection_attrib(),
                           launch_handler_thread,
                           (void*)this)))
  {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(1);
  }

  while (!applier_running)
  {
    DBUG_PRINT("sleep",("Waiting for applier thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }

  mysql_mutex_unlock(&run_lock);
  DBUG_RETURN(0);
}

int
Applier_module::terminate_applier_pipeline()
{
  int error= 0;
  if (pipeline != NULL)
  {
    if ((error= pipeline->terminate_pipeline()))
    {
      log_message(MY_WARNING_LEVEL,
                  "The pipeline was not properly disposed. "
                  "Check the error log for further info.");
    }
    //delete anyway, as we can't do much on error cases
    delete pipeline;
    pipeline= NULL;
  }
  return error;
}

int
Applier_module::terminate_applier_thread()
{
  DBUG_ENTER("ApplierModule::terminate_applier_thread");

  /* This lock code needs to be re-written from scratch*/
  mysql_mutex_lock(&run_lock);

  applier_aborted= true;

  if (!applier_running)
  {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(0);
  }

  while (applier_running)
  {
    DBUG_PRINT("loop", ("killing gcs applier thread"));

    mysql_mutex_lock(&applier_thd->LOCK_thd_data);
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
    int err __attribute__((unused))= pthread_kill(applier_thd->real_id, thr_client_alarm);
    DBUG_ASSERT(err != EINVAL);
    applier_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&applier_thd->LOCK_thd_data);

    //before waiting for termination, signal the queue to unlock.
    incoming->push(NULL);

    /*
      There is a small chance that thread might miss the first
      alarm. To protect against it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
#ifndef DBUG_OFF
    int error=
#endif
      mysql_cond_timedwait(&run_cond, &run_lock, &abstime);
    if (stop_wait_timeout >= 2)
    {
      stop_wait_timeout= stop_wait_timeout - 2;
    }
    else if (applier_running) // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(1);
    }
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!applier_running);

  //The thread ended properly so we can terminate the pipeline
  terminate_applier_pipeline();

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}
