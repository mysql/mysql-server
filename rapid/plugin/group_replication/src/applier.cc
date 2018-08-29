/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "plugin.h"
#include "single_primary_message.h"

char applier_module_channel_name[] = "group_replication_applier";
bool applier_thread_is_exiting= false;

static void *launch_handler_thread(void* arg)
{
  Applier_module *handler= (Applier_module*) arg;
  handler->applier_thread_handle();
  return 0;
}

Applier_module::Applier_module()
  :applier_running(false), applier_aborted(false), applier_error(0),
   suspended(false), waiting_for_applier_suspension(false),
   shared_stop_write_lock(NULL), incoming(NULL), pipeline(NULL),
   fde_evt(BINLOG_VERSION), stop_wait_timeout(LONG_TIMEOUT),
   applier_channel_observer(NULL)
{
  mysql_mutex_init(key_GR_LOCK_applier_module_run, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_applier_module_run, &run_cond);
  mysql_mutex_init(key_GR_LOCK_applier_module_suspend, &suspend_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_applier_module_suspend, &suspend_cond);
  mysql_cond_init(key_GR_COND_applier_module_wait, &suspension_waiting_condition);
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
  delete applier_channel_observer;

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
                                     rpl_sidno group_sidno,
                                     ulonglong gtid_assignment_block_size,
                                     Shared_writelock *shared_stop_lock)
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

  reset_applier_logs= reset_logs;
  group_replication_sidno= group_sidno;
  this->gtid_assignment_block_size= gtid_assignment_block_size;

  shared_stop_write_lock= shared_stop_lock;

  DBUG_RETURN(error);
}


int
Applier_module::purge_applier_queue_and_restart_applier_module()
{
  DBUG_ENTER("Applier_module::purge_applier_queue_and_restart_applier_module");
  int error= 0;

  /*
    Here we are stopping applier thread intentionally and we will be starting
    the applier thread after purging the relay logs. So we should ignore any
    errors during the stop (eg: error due to stopping the applier thread in the
    middle of applying the group of events). Hence unregister the applier channel
    observer temporarily till the required work is done.
  */
  channel_observation_manager->unregister_channel_observer(applier_channel_observer);

  /* Stop the applier thread */
  Pipeline_action *stop_action= new Handler_stop_action();
  error= pipeline->handle_action(stop_action);
  delete stop_action;
  if (error)
    DBUG_RETURN(error); /* purecov: inspected */

  /* Purge the relay logs and initialize the channel*/
  Handler_applier_configuration_action *applier_conf_action=
    new Handler_applier_configuration_action(applier_module_channel_name,
                                             true, /* purge relay logs always*/
                                             stop_wait_timeout,
                                             group_replication_sidno);

  error= pipeline->handle_action(applier_conf_action);
  delete applier_conf_action;
  if (error)
    DBUG_RETURN(error); /* purecov: inspected */

  channel_observation_manager->register_channel_observer(applier_channel_observer);

  /* Start the applier thread */
  Pipeline_action *start_action = new Handler_start_action();
  error= pipeline->handle_action(start_action);
  delete start_action;

  DBUG_RETURN(error);
}


int
Applier_module::setup_pipeline_handlers()
{
  DBUG_ENTER("Applier_module::setup_pipeline_handlers");

  int error= 0;

  //Configure the applier handler trough a configuration action
  Handler_applier_configuration_action *applier_conf_action=
    new Handler_applier_configuration_action(applier_module_channel_name,
                                             reset_applier_logs,
                                             stop_wait_timeout,
                                             group_replication_sidno);

  error= pipeline->handle_action(applier_conf_action);
  delete applier_conf_action;
  if (error)
    DBUG_RETURN(error); /* purecov: inspected */

  Handler_certifier_configuration_action *cert_conf_action=
    new Handler_certifier_configuration_action(group_replication_sidno,
                                               gtid_assignment_block_size);

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

  thd->get_protocol_classic()->init_net(0);
  thd->slave_thread= true;
  //TODO: See of the creation of a new type is desirable.
  thd->system_thread= SYSTEM_THREAD_SLAVE_IO;
  thd->security_context()->skip_grants();

  global_thd_manager_add_thd(thd);

  thd->init_for_queries();
  set_slave_thread_options(thd);
#ifndef _WIN32
  THD_STAGE_INFO(thd, stage_executing);
#endif
  applier_thd= thd;
}

void
Applier_module::clean_applier_thread_context()
{
  applier_thd->get_protocol_classic()->end_net();
  applier_thd->release_resources();
  THD_CHECK_SENTRY(applier_thd);
  global_thd_manager_remove_thd(applier_thd);
}

int
Applier_module::inject_event_into_pipeline(Pipeline_event* pevent,
                                           Continuation* cont)
{
  int error= 0;
  pipeline->handle_event(pevent, cont);

  if ((error= cont->wait()))
    log_message(MY_ERROR_LEVEL, "Error at event handling! Got error: %d", error);

  return error;
}



bool Applier_module::apply_action_packet(Action_packet *action_packet)
{
  enum_packet_action action= action_packet->packet_action;

  //packet used to break the queue blocking wait
  if (action == TERMINATION_PACKET)
  {
     return true;
  }
  //packet to signal the applier to suspend
  if (action == SUSPENSION_PACKET)
  {
    suspend_applier_module();
    return false;
  }
  return false; /* purecov: inspected */
}

int
Applier_module::apply_view_change_packet(View_change_packet *view_change_packet,
                                         Format_description_log_event *fde_evt,
                                         IO_CACHE *cache,
                                         Continuation *cont)
{
  int error= 0;

  Gtid_set *group_executed_set= NULL;
  Sid_map *sid_map= NULL;
  if (!view_change_packet->group_executed_set.empty())
  {
    sid_map= new Sid_map(NULL);
    group_executed_set= new Gtid_set(sid_map, NULL);
    if (intersect_group_executed_sets(view_change_packet->group_executed_set,
                                      group_executed_set))
    {
       log_message(MY_WARNING_LEVEL,
                   "Error when extracting group GTID execution information, "
                   "some recovery operations may face future issues"); /* purecov: inspected */
       delete sid_map;            /* purecov: inspected */
       delete group_executed_set; /* purecov: inspected */
       group_executed_set= NULL;  /* purecov: inspected */
    }
  }

  if (group_executed_set != NULL)
  {
    if (get_certification_handler()->get_certifier()->
        set_group_stable_transactions_set(group_executed_set))
    {
      log_message(MY_WARNING_LEVEL,
                  "An error happened when trying to reduce the Certification "
                  " information size for transmission"); /* purecov: inspected */
    }
    delete sid_map;
    delete group_executed_set;
  }

  View_change_log_event* view_change_event
      = new View_change_log_event((char*)view_change_packet->view_id.c_str());

  Pipeline_event* pevent= new Pipeline_event(view_change_event, fde_evt, cache);
  pevent->mark_event(SINGLE_VIEW_EVENT);
  error= inject_event_into_pipeline(pevent, cont);
  delete pevent;

  return error;
}

int Applier_module::apply_data_packet(Data_packet *data_packet,
                                      Format_description_log_event *fde_evt,
                                      IO_CACHE *cache,
                                      Continuation *cont)
{
  int error= 0;
  uchar* payload= data_packet->payload;
  uchar* payload_end= data_packet->payload + data_packet->len;

  DBUG_EXECUTE_IF("group_replication_before_apply_data_packet", {
    const char act[] = "now wait_for continue_apply";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  if (check_single_primary_queue_status())
    return 1; /* purecov: inspected */

  while ((payload != payload_end) && !error)
  {
    uint event_len= uint4korr(((uchar*)payload) + EVENT_LEN_OFFSET);

    Data_packet* new_packet= new Data_packet(payload, event_len);
    payload= payload + event_len;

    Pipeline_event* pevent= new Pipeline_event(new_packet, fde_evt, cache);
    error= inject_event_into_pipeline(pevent, cont);

    delete pevent;
    DBUG_EXECUTE_IF("stop_applier_channel_after_reading_write_rows_log_event",
                    {
                      if (payload[EVENT_TYPE_OFFSET] == binary_log::WRITE_ROWS_EVENT)
                      {
                        error=1;
                      }
                    }
                   );
  }

  return error;
}

int
Applier_module::apply_single_primary_action_packet(Single_primary_action_packet *packet)
{
  int error= 0;
  Certifier_interface *certifier= get_certification_handler()->get_certifier();

  switch (packet->action)
  {
    case Single_primary_action_packet::NEW_PRIMARY:
      certifier->enable_conflict_detection();
      break;
    case Single_primary_action_packet::QUEUE_APPLIED:
      certifier->disable_conflict_detection();
      break;
    default:
      DBUG_ASSERT(0); /* purecov: inspected */
  }

  return error;
}


int
Applier_module::applier_thread_handle()
{
  DBUG_ENTER("ApplierModule::applier_thread_handle()");

  //set the thread context
  set_applier_thread_context();

  Handler_THD_setup_action *thd_conf_action= NULL;
  Format_description_log_event* fde_evt= NULL;
  Continuation* cont= NULL;
  Packet *packet= NULL;
  bool loop_termination = false;
  int packet_application_error= 0;

  IO_CACHE *cache= (IO_CACHE*) my_malloc(PSI_NOT_INSTRUMENTED,
                                         sizeof(IO_CACHE),
                                         MYF(MY_ZEROFILL));
  if (!cache || (!my_b_inited(cache) &&
                 open_cached_file(cache, mysql_tmpdir,
                                  "group_replication_pipeline_applier_cache",
                                  SHARED_EVENT_IO_CACHE_SIZE,
                                  MYF(MY_WME))))
  {
    my_free(cache);   /* purecov: inspected */
    cache= NULL;      /* purecov: inspected */
    log_message(MY_ERROR_LEVEL,
                "Failed to create group replication pipeline applier cache!"); /* purecov: inspected */
    applier_error= 1; /* purecov: inspected */
    goto end;         /* purecov: inspected */
  }

  applier_error= setup_pipeline_handlers();

  applier_channel_observer= new Applier_channel_state_observer();
  channel_observation_manager
      ->register_channel_observer(applier_channel_observer);

  if (!applier_error)
  {
    Pipeline_action *start_action = new Handler_start_action();
    applier_error= pipeline->handle_action(start_action);
    delete start_action;
  }

  if (applier_error)
  {
    goto end;
  }

  mysql_mutex_lock(&run_lock);
  applier_thread_is_exiting= false;
  applier_running= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  fde_evt= new Format_description_log_event(BINLOG_VERSION);
  cont= new Continuation();

  //Give the handlers access to the applier THD
  thd_conf_action= new Handler_THD_setup_action(applier_thd);
  // To prevent overwrite last error method
  applier_error+= pipeline->handle_action(thd_conf_action);
  delete thd_conf_action;

  //applier main loop
  while (!applier_error && !packet_application_error && !loop_termination)
  {
    if (is_applier_thread_aborted())
      break;

    this->incoming->front(&packet); // blocking

    switch (packet->get_packet_type())
    {
      case ACTION_PACKET_TYPE:
          this->incoming->pop();
          loop_termination= apply_action_packet((Action_packet*)packet);
          break;
      case VIEW_CHANGE_PACKET_TYPE:
          packet_application_error=
            apply_view_change_packet((View_change_packet*)packet,
                                     fde_evt, cache, cont);
          this->incoming->pop();
          break;
      case DATA_PACKET_TYPE:
          packet_application_error= apply_data_packet((Data_packet*)packet,
                                                      fde_evt, cache, cont);
          //Remove from queue here, so the size only decreases after packet handling
         this->incoming->pop();
          break;
      case SINGLE_PRIMARY_PACKET_TYPE:
          packet_application_error=
              apply_single_primary_action_packet((Single_primary_action_packet*)packet);
          this->incoming->pop();
          break;
      default:
        DBUG_ASSERT(0); /* purecov: inspected */
    }

    delete packet;
  }
  if (packet_application_error)
    applier_error= packet_application_error;
  delete fde_evt;
  delete cont;

end:

  //always remove the observer even the thread is no longer running
  channel_observation_manager
      ->unregister_channel_observer(applier_channel_observer);

  //only try to leave if the applier managed to start
  if (applier_error && applier_running)
    leave_group_on_failure();

  //Even on error cases, send a stop signal to all handlers that could be active
  Pipeline_action *stop_action= new Handler_stop_action();
  int local_applier_error= pipeline->handle_action(stop_action);
  delete stop_action;

  Gcs_interface_factory::cleanup(Gcs_operations::get_gcs_engine());

  log_message(MY_INFORMATION_LEVEL, "The group replication applier thread"
                                    " was killed");

  DBUG_EXECUTE_IF("applier_thd_timeout",
                  {
                    const char act[]= "now wait_for signal.applier_continue";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });

  if (cache != NULL)
  {
    close_cached_file(cache);
    my_free(cache);
  }

  clean_applier_thread_context();

  mysql_mutex_lock(&run_lock);
  delete applier_thd;

  /*
    Don't overwrite applier_error when stop_applier_thread() doesn't return
    error. So applier_error which is also referred by main thread
    doesn't return true from initialize_applier_thread() when
    start_applier_thread() fails and stop_applier_thread() succeeds.
    Also use local var - local_applier_error, as the applier can be deleted
    before the thread returns.
  */
  if (local_applier_error)
    applier_error= local_applier_error; /* purecov: inspected */
  else
    local_applier_error= applier_error;

  applier_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  my_thread_end();
  applier_thread_is_exiting= true;
  my_thread_exit(0);

  DBUG_RETURN(local_applier_error); /* purecov: inspected */
}

int
Applier_module::initialize_applier_thread()
{
  DBUG_ENTER("Applier_module::initialize_applier_thd");

  //avoid concurrency calls against stop invocations
  mysql_mutex_lock(&run_lock);

  applier_error= 0;

  if ((mysql_thread_create(key_GR_THD_applier_module_receiver,
                           &applier_pthd,
                           get_connection_attrib(),
                           launch_handler_thread,
                           (void*)this)))
  {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    DBUG_RETURN(1);                /* purecov: inspected */
  }

  while (!applier_running && !applier_error)
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
                  " disposed. Check the error log for further info."); /* purecov: inspected */
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
    goto delete_pipeline;
  }

  while (applier_running)
  {
    DBUG_PRINT("loop", ("killing group replication applier thread"));

    mysql_mutex_lock(&applier_thd->LOCK_thd_data);

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

delete_pipeline:

  //The thread ended properly so we can terminate the pipeline
  terminate_applier_pipeline();

  while (!applier_thread_is_exiting)
  {
    /* Check if applier thread is exiting per microsecond. */
    my_sleep(1);
  }

  /*
    Give applier thread one microsecond to exit completely after
    it set applier_thread_is_exiting to true.
  */
  my_sleep(1);

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

void Applier_module::inform_of_applier_stop(char* channel_name,
                                            bool aborted)
{
  DBUG_ENTER("Applier_module::inform_of_applier_stop");

  if (!strcmp(channel_name, applier_module_channel_name) &&
      aborted && applier_running )
  {
    log_message(MY_ERROR_LEVEL,
                "The applier thread execution was aborted."
                " Unable to process more transactions,"
                " this member will now leave the group.");

    applier_error= 1;

    //before waiting for termination, signal the queue to unlock.
    add_termination_packet();

    //also awake the applier in case it is suspended
    awake_applier_module();
  }

  DBUG_VOID_RETURN;
}

void Applier_module::leave_group_on_failure()
{
  DBUG_ENTER("Applier_module::leave_group_on_failure");

  log_message(MY_ERROR_LEVEL,
              "Fatal error during execution on the Applier process of "
              "Group Replication. The server will now leave the group.");

  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR);

  bool set_read_mode= false;
  if (view_change_notifier != NULL &&
      !view_change_notifier->is_view_modification_ongoing())
  {
    view_change_notifier->start_view_modification();
  }
  Gcs_operations::enum_leave_state state= gcs_module->leave();

  int error= channel_stop_all(CHANNEL_APPLIER_THREAD|CHANNEL_RECEIVER_THREAD,
                              stop_wait_timeout);
  if (error)
  {
    log_message(MY_ERROR_LEVEL,
                "Error stopping all replication channels while server was"
                " leaving the group. Please check the error log for additional"
                " details. Got error: %d", error);
  }

  std::stringstream ss;
  plugin_log_level log_severity= MY_WARNING_LEVEL;
  switch (state)
  {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      ss << "Unable to confirm whether the server has left the group or not. "
            "Check performance_schema.replication_group_members to check group membership information.";
      log_severity= MY_ERROR_LEVEL;
      break;
    case Gcs_operations::ALREADY_LEAVING:
      ss << "Skipping leave operation: concurrent attempt to leave the group is on-going."; /* purecov: inspected */
      break; /* purecov: inspected */
    case Gcs_operations::ALREADY_LEFT:
      ss << "Skipping leave operation: member already left the group."; /* purecov: inspected */
      break; /* purecov: inspected */
    case Gcs_operations::NOW_LEAVING:
      set_read_mode= true;
      ss << "The server was automatically set into read only mode after an error was detected.";
      log_severity= MY_ERROR_LEVEL;
      break;
  }
  log_message(log_severity, ss.str().c_str());

  kill_pending_transactions(set_read_mode, false);

  DBUG_VOID_RETURN;
}

void Applier_module::kill_pending_transactions(bool set_read_mode,
                                               bool threaded_sql_session)
{
  DBUG_ENTER("Applier_module::kill_pending_transactions");

  //Stop any more transactions from waiting
  bool already_locked= shared_stop_write_lock->try_grab_write_lock();

  //kill pending transactions
  blocked_transaction_handler->unblock_waiting_transactions();

  if (!already_locked)
    shared_stop_write_lock->release_write_lock();

  if (set_read_mode)
  {
    if (threaded_sql_session)
      enable_server_read_mode(PSESSION_INIT_THREAD);
    else
      enable_server_read_mode(PSESSION_USE_THREAD);
  }

  if (view_change_notifier != NULL)
  {
    log_message(MY_INFORMATION_LEVEL, "Going to wait for view modification");
    if (view_change_notifier->wait_for_view_modification())
    {
      log_message(MY_ERROR_LEVEL, "On shutdown there was a timeout receiving a "
                                  "view change. This can lead to a possible "
                                  "inconsistent state. Check the log for more "
                                  "details");
    }
  }

  /*
    Only abort() if we successfully asked to leave() the group (and we have
    group_replication_exit_state_action set to ABORT_SERVER).
    We don't want to abort() during the execution of START GROUP_REPLICATION or
    STOP GROUP_REPLICATION.
  */
  if (set_read_mode &&
      exit_state_action_var == EXIT_STATE_ACTION_ABORT_SERVER)
  {
    abort_plugin_process("Fatal error during execution of Group Replication");
  }

  DBUG_VOID_RETURN;
}

int
Applier_module::wait_for_applier_complete_suspension(bool *abort_flag,
                                                     bool wait_for_execution)
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
  while (!suspended && !(*abort_flag) && !applier_aborted && !applier_error)
  {
    mysql_cond_wait(&suspension_waiting_condition, &suspend_lock);
  }

  mysql_mutex_unlock(&suspend_lock);

  if (applier_aborted || applier_error)
      return APPLIER_THREAD_ABORTED; /* purecov: inspected */

  /**
    Wait for the applier execution of pre suspension events (blocking method)
    while(the wait method times out)
      wait()
  */
  if (wait_for_execution)
  {
    error= APPLIER_GTID_CHECK_TIMEOUT_ERROR; //timeout error
    while (error == APPLIER_GTID_CHECK_TIMEOUT_ERROR && !(*abort_flag))
      error= wait_for_applier_event_execution(1, true); //blocking
  }

  return (error == APPLIER_RELAY_LOG_NOT_INITED);
}

void
Applier_module::interrupt_applier_suspension_wait()
{
  mysql_mutex_lock(&suspend_lock);
  mysql_cond_broadcast(&suspension_waiting_condition);
  mysql_mutex_unlock(&suspend_lock);
}

bool
Applier_module::is_applier_thread_waiting()
{
  DBUG_ENTER("Applier_module::is_applier_thread_waiting");
  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, APPLIER, &event_applier);

  if (event_applier == NULL)
    return false; /* purecov: inspected */

  bool result= ((Applier_handler*)event_applier)->is_applier_thread_waiting();

  DBUG_RETURN(result);
}

int
Applier_module::wait_for_applier_event_execution(double timeout,
                                                 bool check_and_purge_partial_transactions)
{
  DBUG_ENTER("Applier_module::wait_for_applier_event_execution");
  int error= 0;
  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, APPLIER, &event_applier);

  if (event_applier &&
      !(error= ((Applier_handler*)event_applier)->wait_for_gtid_execution(timeout)))
  {
    /*
      After applier thread is done, check if there is partial transaction
      in the relay log. If so, applier thread must be holding the lock on it
      and will never release it because there will not be any more events
      coming into this channel. In this case, purge the relaylogs and restart
      the applier thread will release the lock and update the applier thread
      execution position correctly and safely.
    */
    if (check_and_purge_partial_transactions &&
        ((Applier_handler*)event_applier)->is_partial_transaction_on_relay_log())
    {
        error= purge_applier_queue_and_restart_applier_module();
    }
  }
  DBUG_RETURN(error);
}


Certification_handler* Applier_module::get_certification_handler(){

  Event_handler* event_applier= NULL;
  Event_handler::get_handler_by_role(pipeline, CERTIFIER, &event_applier);

  //The only certification handler for now
  return (Certification_handler*) event_applier;
}

int
Applier_module::intersect_group_executed_sets(std::vector<std::string>& gtid_sets,
                                              Gtid_set* output_set)
{
  Sid_map* sid_map= output_set->get_sid_map();

  std::vector<std::string>::iterator set_iterator;
  for (set_iterator= gtid_sets.begin();
       set_iterator!= gtid_sets.end();
       set_iterator++)
  {

    Gtid_set member_set(sid_map, NULL);
    Gtid_set intersection_result(sid_map, NULL);

    std::string exec_set_str= (*set_iterator);

    if (member_set.add_gtid_text(exec_set_str.c_str()) != RETURN_STATUS_OK)
    {
      return 1; /* purecov: inspected */
    }

    if (output_set->is_empty())
    {
      if (output_set->add_gtid_set(&member_set))
      {
      return 1; /* purecov: inspected */
      }
    }
    else
    {
      /*
        We have three sets:
          member_set:          the one sent from a given member;
          output_set:        the one that contains the intersection of
                               the computed sets until now;
          intersection_result: the intersection between set and
                               intersection_result.
        So we compute the intersection between member_set and output_set, and
        set that value to output_set to be used on the next intersection.
      */
      if (member_set.intersection(output_set, &intersection_result) != RETURN_STATUS_OK)
      {
        return 1; /* purecov: inspected */
      }

      output_set->clear();
      if (output_set->add_gtid_set(&intersection_result) != RETURN_STATUS_OK)
      {
        return 1; /* purecov: inspected */
      }
    }
  }

#if !defined(DBUG_OFF)
  char *executed_set_string;
  output_set->to_string(&executed_set_string);
  DBUG_PRINT("info", ("View change GTID information: output_set: %s",
             executed_set_string));
  my_free(executed_set_string);
#endif

  return 0;
}

int Applier_module::check_single_primary_queue_status()
{
  /*
    If the 1) group is on single primary mode, 2) this member is the
    primary one, and 3) the group replication applier did apply all
    previous primary transactions, we can switch off conflict
    detection since all transactions will originate from the same
    primary.
  */
  if (get_certification_handler()->get_certifier()->is_conflict_detection_enable() &&
      local_member_info->in_primary_mode() &&
      local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY &&
      is_applier_thread_waiting())
  {
    Single_primary_message
        single_primary_message(Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE);
    if (gcs_module->send_message(single_primary_message))
    {
      log_message(MY_ERROR_LEVEL,
                  "Error sending single primary message informing "
                  "that primary did apply relay logs"); /* purecov: inspected */
      return 1; /* purecov: inspected */
    }
  }

  return 0;
}
