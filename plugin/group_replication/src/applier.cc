/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <signal.h>
#include "applier.h"
#include <mysql/group_replication_priv.h>
#include "plugin_log.h"

#define APPLIER_GTID_CHECK_TIMEOUT_ERROR -1
#define APPLIER_RELAY_LOG_NOT_INITED -2

char applier_module_channel_name[] = "group_replication_applier";

static void *launch_handler_thread(void* arg)
{
  Applier_module *handler= (Applier_module*) arg;
  handler->applier_thread_handle();
  return 0;
}

Applier_module::Applier_module()
  :applier_running(false), applier_aborted(false), applier_error(0), suspended(false),
   waiting_for_applier_suspension(false), incoming(NULL), pipeline(NULL),
   fde_evt(BINLOG_VERSION), stop_wait_timeout(LONG_TIMEOUT)
{
#ifdef HAVE_PSI_INTERFACE
  PSI_cond_info applier_conds[]=
  {
    { &run_key_cond, "COND_applier_run", 0},
    { &suspend_key_cond, "COND_applier_suspend", 0},
    { &suspend_wait_key_cond, "COND_applier_wait_suspend", 0}
  };

  PSI_mutex_info applier_mutexes[]=
  {
    { &run_key_mutex, "LOCK_applier_run", 0},
    { &suspend_key_mutex, "LOCK_applier_suspend", 0}
  };

  register_group_replication_psi_keys(applier_mutexes, 2,
                                      applier_conds, 3);
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(run_key_mutex, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(run_key_cond, &run_cond);
  mysql_mutex_init(suspend_key_mutex, &suspend_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(suspend_key_cond, &suspend_cond);
  mysql_cond_init(suspend_wait_key_cond, &suspension_waiting_condition);
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
  mysql_mutex_destroy(&suspend_lock);
  mysql_cond_destroy(&suspend_cond);
  mysql_cond_destroy(&suspension_waiting_condition);
}

int
Applier_module::setup_applier_module(Handler_pipeline_type pipeline_type,
                                     bool reset_logs,
                                     ulong stop_timeout,
                                     rpl_sidno group_sidno)
{
  DBUG_ENTER("Applier_module::setup_applier_module");

  int error= 0;

  //create the receiver queue
  this->incoming= new Synchronized_queue<Packet*>();

  stop_wait_timeout= stop_timeout;

  pipeline= NULL;

  if ( (error= get_pipeline(pipeline_type, &pipeline)) )
  {
    DBUG_RETURN(error);
  }

  //Configure the applier handler trough a configuration action
  Handler_applier_configuration_action *applier_conf_action=
    new Handler_applier_configuration_action(applier_module_channel_name,
                                             reset_logs,
                                             stop_timeout,
                                             group_sidno);

  error= pipeline->handle_action(applier_conf_action);

  if (error)
  {
    delete applier_conf_action;
    DBUG_RETURN(error);
  }

  //Extract the last known queued gno to configure the certifier
  rpl_gno last_queued_gno= applier_conf_action->get_last_queued_gno();
  delete applier_conf_action;

  Handler_certifier_configuration_action *cert_conf_action=
    new Handler_certifier_configuration_action(last_queued_gno, group_sidno);

  error = pipeline->handle_action(cert_conf_action);

  delete cert_conf_action;

  DBUG_RETURN(error);
}

void
Applier_module::set_applier_thread_context()
{
  my_thread_init();
  THD *thd= new THD;
  thd->set_new_thread_id();
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  my_net_init(&thd->net, 0);
  thd->slave_thread= true;
  //TODO: See of the creation of a new type is desirable.
  thd->system_thread= SYSTEM_THREAD_SLAVE_IO;
  thd->security_context()->skip_grants();

  global_thd_manager_add_thd(thd);

  thd->init_for_queries();
  set_slave_thread_options(thd);
  THD_STAGE_INFO(thd, stage_executing);
  applier_thd= thd;
}

void
Applier_module::clean_applier_thread_context()
{
  net_end(&applier_thd->net);
  applier_thd->release_resources();
  THD_CHECK_SENTRY(applier_thd);
  global_thd_manager_remove_thd(applier_thd);

  delete applier_thd;

  my_thread_end();
  pthread_exit(0);
}

int
Applier_module::inject_event_into_pipeline(Pipeline_event* pevent,
                                           Continuation* cont)
{
  int error= 0;
  pipeline->handle_event(pevent, cont);

  if ((error= cont->wait()))
    log_message(MY_ERROR_LEVEL, "Error at event handling! Got error: %d \n", error);

  return error;
}

int
Applier_module::applier_thread_handle()
{
  DBUG_ENTER("ApplierModule::applier_thread_handle()");

  //set the thread context
  set_applier_thread_context();

  Packet *packet= NULL;
  applier_error= 0;

  mysql_mutex_lock(&run_lock);
  applier_running= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  Format_description_log_event* fde_evt=
    new Format_description_log_event(BINLOG_VERSION);
  Continuation* cont= new Continuation();

  while (!applier_error)
  {
    if (is_applier_thread_aborted())
      break;

    if ((applier_error= this->incoming->pop(&packet))) // blocking
    {
      log_message(MY_ERROR_LEVEL, "Error when reading from group replication "
                                  "applier's transaction queue");
      break;
    }
    if(packet->get_packet_type() == ACTION_PACKET_TYPE)
    {
      enum_packet_action action= ((Action_packet*)packet)->packet_action;
      //packet used to break the queue blocking wait
      if (action == TERMINATION_PACKET)
      {
        delete packet;
        break;
      }
      //packet to signal the applier to suspend
      if (action == SUSPENSION_PACKET)
      {
        suspend_applier_module();
        delete packet;
        continue;
      }
      //signals the injection of a view change event into the pipeline
      if (action == VIEW_CHANGE_PACKET)
      {
        View_change_log_event* view_change_event
                       = new View_change_log_event
                                     ((char*)((Action_packet*)packet)->payload);

        Pipeline_event* pevent= new Pipeline_event(view_change_event, fde_evt);
        applier_error= inject_event_into_pipeline(pevent, cont);
        delete pevent;
        delete packet;
        continue;
      }
    }//end if action type

    Data_packet* data_packet = ((Data_packet*)packet);

    uchar* payload= data_packet->payload;
    uchar* payload_end= data_packet->payload + data_packet->len;

    /**
      TODO: handle the applier error in a way that it causes the member to leave
      the group with proper status change
    */
    while ((payload != payload_end) && !applier_error)
    {
      uint event_len= uint4korr(((uchar*)payload) + EVENT_LEN_OFFSET);

      Data_packet* new_packet= new Data_packet(payload, event_len);
      payload= payload + event_len;

      Pipeline_event* pevent= new Pipeline_event(new_packet, fde_evt);
      applier_error= inject_event_into_pipeline(pevent, cont);

      delete pevent;
    }

    delete packet;
  }
  delete fde_evt;
  delete cont;

  //Even on error cases, send a stop signal to all handlers that could be active
  Pipeline_action *stop_action= new Handler_stop_action();
  applier_error= pipeline->handle_action(stop_action);
  delete stop_action;

  log_message(MY_INFORMATION_LEVEL, "The group replication applier thread"
                                    " was killed");

  DBUG_EXECUTE_IF("applier_thd_timeout",
                  {
                    const char act[]= "now wait_for signal.applier_continue";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
  applier_running= false;

  clean_applier_thread_context();

  DBUG_RETURN(applier_error);
}

int
Applier_module::initialize_applier_thread()
{
  DBUG_ENTER("Applier_module::initialize_applier_thd");

  //avoid concurrency calls against stop invocations
  mysql_mutex_lock(&run_lock);

  applier_error= 0;

  Pipeline_action *start_action = new Handler_start_action();
  applier_error= pipeline->handle_action(start_action);
  delete start_action;

  if (applier_error)
  {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(applier_error);
  }

#ifdef HAVE_PSI_INTERFACE
  PSI_thread_info threads[]= {
    { &key_thread_receiver,
      "group-replication-applier-module", PSI_FLAG_GLOBAL
    }
  };
  mysql_thread_register("group-replication-applier-module", threads, 1);
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
  DBUG_RETURN(applier_error);
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
                  "The group replication applier pipeline was not properly"
                  " disposed. Check the error log for further info.");
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
  DBUG_ENTER("Applier_module::terminate_applier_thread");

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
    DBUG_PRINT("loop", ("killing group replication applier thread"));

    mysql_mutex_lock(&applier_thd->LOCK_thd_data);
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
    int err __attribute__((unused))= pthread_kill(applier_thd->real_id, SIGUSR1);
    DBUG_ASSERT(err != EINVAL);
    applier_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&applier_thd->LOCK_thd_data);

    //before waiting for termination, signal the queue to unlock.
    add_termination_packet();

    //also awake the applier in case it is suspended
    awake_applier_module();

    /*
      There is a small chance that thread might miss the first
      alarm. To protect against it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(&abstime, 2);
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

int
Applier_module::wait_for_applier_complete_suspension(bool *abort_flag)
{
  int error= 0;

  mysql_mutex_lock(&suspend_lock);

  /*
   We use an external flag to avoid race conditions.
   A local flag could always lead to the scenario of
     wait_for_applier_complete_suspension()

   >> thread switch

     break_applier_suspension_wait()
       we_are_waiting = false;
       awake

   thread switch <<

      we_are_waiting = true;
      wait();
  */
  while (!suspended && !(*abort_flag))
  {
    mysql_cond_wait(&suspension_waiting_condition, &suspend_lock);
  }

  mysql_mutex_unlock(&suspend_lock);

  /**
    Wait for the applier execution of pre suspension events (blocking method)
    while(the wait method times out)
      wait()
  */
  error= APPLIER_GTID_CHECK_TIMEOUT_ERROR; //timeout error
  while (error == APPLIER_GTID_CHECK_TIMEOUT_ERROR && !(*abort_flag))
    error= wait_for_applier_event_execution(1); //blocking

  return (error == APPLIER_RELAY_LOG_NOT_INITED);
}

void
Applier_module::interrupt_applier_suspension_wait()
{
  mysql_mutex_lock(&suspend_lock);
  mysql_cond_broadcast(&suspension_waiting_condition);
  mysql_mutex_unlock(&suspend_lock);
}

int
Applier_module::wait_for_applier_event_execution(ulonglong timeout)
{
  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, APPLIER, &event_applier);

  //Nothing to wait?
  if (event_applier == NULL)
    return 0;

  //The only event applying handler by now
  return ((Applier_handler*)event_applier)->wait_for_gtid_execution(timeout);
}

bool
Applier_module::is_own_event_channel(my_thread_id id){

  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, APPLIER, &event_applier);

  //No applier exists so return false
  if (event_applier == NULL)
    return false;

  //The only event applying handler by now
  return ((Applier_handler*)event_applier)->is_own_event_applier(id);
}

Certification_handler* Applier_module::get_certification_handler(){

  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, CERTIFIER, &event_applier);

  //The only certification handler for now
  return (Certification_handler*) event_applier;
}

