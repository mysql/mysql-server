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
#include "recovery.h"
#include "recovery_message.h"
#include "member_info.h"
#include "plugin_log.h"

using std::list;
using std::vector;

/** Default user for donor connection*/
static char DEFAULT_USER[]= "root";
/** Default password for donor connection*/
static char DEFAULT_PASSWORD[]= "";
/** The number of queued transactions below which we declare the member online */
static uint RECOVERY_TRANSACTION_THRESHOLD= 0;

/** The relay log name*/
static char recovery_channel_name[]= "group_replication_recovery";

static void *launch_handler_thread(void* arg)
{
  Recovery_module *handler= (Recovery_module*) arg;
  handler->recovery_thread_handle();
  return 0;
}

Recovery_module::
Recovery_module(Applier_module_interface *applier,
                Gcs_communication_interface *comm_if,
                Gcs_control_interface *ctrl_if,
                Group_member_info *local_info,
                Group_member_info_manager_interface *group_info_if,
                ulong components_stop_timeout)
  : gcs_control_interface(ctrl_if),gcs_communication_interface(comm_if),
    local_member_info(local_info), applier_module(applier),
    group_info(group_info_if), donor_connection_retry_count(0),
    recovery_running(false), donor_transfer_finished(false),
    connected_to_donor(false), donor_connection_interface(recovery_channel_name),
    stop_wait_timeout(components_stop_timeout),
    max_connection_attempts_to_donors(-1)
{
  selected_donor_uuid.clear();

  (void) strncpy(donor_connection_user, DEFAULT_USER, strlen(DEFAULT_USER)+1);
  (void) strncpy(donor_connection_password,
                 DEFAULT_PASSWORD, strlen(DEFAULT_PASSWORD)+1);

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_info recovery_mutexes[]=
  {
    { &recovery_mutex_key, "recovery_LOCK", 0},
    { &run_mutex_key, "recovery_run_LOCK", 0},
    { &donor_selection_mutex_key, "donor_selection_LOCK", 0}
  };

  PSI_cond_info recovery_conds[]=
  {
    { &run_cond_key, "recovery_run_COND", 0},
    { &recovery_cond_key, "recovery_COND", 0}
  };

  register_group_replication_psi_keys(recovery_mutexes, 3, recovery_conds, 2);
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(run_mutex_key, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(run_cond_key, &run_cond);
  mysql_mutex_init(recovery_mutex_key, &recovery_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(recovery_cond_key, &recovery_condition);
  mysql_mutex_init(donor_selection_mutex_key,
                   &donor_selection_lock,
                   MY_MUTEX_INIT_FAST);
}

Recovery_module::~Recovery_module()
{
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
}

int
Recovery_module::start_recovery(const string& group_name,
                                char* rec_view_id)
{
  DBUG_ENTER("Recovery_module::start_recovery");

  mysql_mutex_lock(&run_lock);

  this->group_name= group_name;

  this->view_id.clear();
  this->view_id.append(rec_view_id);

  if (check_recovery_thread_status())
  {
    log_message(MY_ERROR_LEVEL,
                "A previous recovery session is still running. "
                "Please stop the group replication plugin and"
                " wait for it to stop.");
    DBUG_RETURN(1);
  }

  //reset the recovery aborted status here to avoid concurrency
  recovery_aborted= false;

  //Set the retry count to be the max number of possible donors
  if (max_connection_attempts_to_donors == -1)
  {
    log_message(MY_INFORMATION_LEVEL,
                "The number of group replication recovery connections attempts was not set. "
                "Defaulting to the maximum number of possible donors.");
    max_connection_attempts_to_donors= group_info->get_number_of_members() -1;
  }

#ifdef HAVE_PSI_INTERFACE
  PSI_thread_info threads[]= {
    { &key_thread_recovery,
      "group-replication-recovery-module", PSI_FLAG_GLOBAL
    }
  };
  mysql_thread_register("group-replication-recovery-module", threads, 1);
#endif

  if (mysql_thread_create(key_thread_recovery,
                           &recovery_pthd,
                           get_connection_attrib(),
                           launch_handler_thread,
                           (void*)this))
  {
    DBUG_RETURN(1);
  }

  while (!recovery_running)
  {
    DBUG_PRINT("sleep",("Waiting for applier thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int
Recovery_module::check_recovery_thread_status()
{
  //if some of the threads are running
  if (donor_connection_interface.is_receiver_thread_running() ||
      donor_connection_interface.is_applier_thread_running())
  {
    return terminate_recovery_slave_threads();
  }
  return 0;
}

int
Recovery_module::stop_recovery()
{
  DBUG_ENTER("Recovery_module::stop_recovery");

  mysql_mutex_lock(&run_lock);

  if (!recovery_running)
  {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(0);
  }

  recovery_aborted= true;

  while (recovery_running)
  {
    DBUG_PRINT("loop", ("killing group replication recovery thread"));

    mysql_mutex_lock(&recovery_thd->LOCK_thd_data);
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
    int err __attribute__((unused))= pthread_kill(recovery_thd->real_id,
                                                  SIGUSR1);
    DBUG_ASSERT(err != EINVAL);
    recovery_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&recovery_thd->LOCK_thd_data);

    //Break the wait for the applier suspension
    applier_module->interrupt_applier_suspension_wait();
    //Break the wait for view change event
    mysql_mutex_lock(&recovery_lock);
    mysql_cond_broadcast(&recovery_condition);
    mysql_mutex_unlock(&recovery_lock);

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
    else if (recovery_running) // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(1);
    }
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!recovery_running);

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int
Recovery_module::update_recovery_process(bool did_members_left)
{
  DBUG_ENTER("Recovery_module::update_recovery_process");

  int error= 0;

  if (recovery_running)
  {
    //If i left the Group... the group manager will only have me
    if (group_info->get_number_of_members() == 1)
    {
      stop_recovery();
    }
    else
    {
      /*
        Lock to avoid concurrency between this code that handles failover and
        the establish_donor_connection method. We either:
        1) lock first and see that the method did not run yet, updating the list
           of group members that will be used there.
        2) lock after the method executed, and if the selected donor is leaving
           we stop the connection thread and select a new one.
      */
      mysql_mutex_lock(&donor_selection_lock);

      //if some member left, reset the counter as potential failed members left
      if (did_members_left)
      {
        donor_connection_retry_count= 0;
        rejected_donors.clear();
      }

      /*
       It makes sense to cut our connection to the donor if:
       1) The donor has left the building and
       2) We are already connected to him.
      */

      Group_member_info* donor=
          group_info->get_group_member_info(selected_donor_uuid);

      if ((donor == NULL) && connected_to_donor)
      {
        /*
         The donor_transfer_finished flag is not lock protected on the recovery
         thread so we have the scenarios.
         1) The flag is true and we do nothing
         2) The flag is false and remains false so we restart the connection, and
         that new connection will deliver the rest of the data
         3) The flag turns true while we are restarting the connection. In this
         case we will probably create a new connection that won't be needed and
         will be terminated the instant the lock is freed.
        */
        if (!donor_transfer_finished)
        {
          log_message(MY_INFORMATION_LEVEL,
                      "Killing the current group replication recovery connection"
                      " as the donor %s has unexpectedly disappeared.",
                      selected_donor_uuid.c_str());
          if (donor_failover())
          {
            /*
             We can't failover, nothing to do, better exit the group.
             There is yet a possibility that the donor transfer terminated in
             the meanwhile, rendering the error unimportant.
            */
            if (!donor_transfer_finished)
            {
              log_message(MY_ERROR_LEVEL,
                      "Failover to another donor failed, rendering group recovery"
                      " impossible. The server will now leave the group");
              mysql_mutex_unlock(&donor_selection_lock);
              gcs_control_interface->leave();
              DBUG_RETURN(error);
            }
            else
            {
              log_message(MY_WARNING_LEVEL,
                          "Failover to another donor failed, but group "
                          "replication recovery already received all the data.");
            }
          }
        }
        //else do nothing
      }
      else
      {
        delete donor;
      }
      mysql_mutex_unlock(&donor_selection_lock);
    } //member not on left
  } // recovery_running

  DBUG_RETURN(error);
}

int Recovery_module::donor_failover()
{
  DBUG_ENTER("Recovery_module::donor_failover");

  //Stop the threads before reconfiguring the connection
  int error= 0;
  if ((error= donor_connection_interface.stop_threads(true, true)))
  {
    log_message(MY_ERROR_LEVEL,
                "Can't kill the current group replication recovery process. "
                "Recovery will shutdown.");
    DBUG_RETURN(error);
  }
  DBUG_RETURN(establish_donor_connection(true));
}

int
Recovery_module::recovery_thread_handle()
{
  DBUG_ENTER("Recovery_module::recovery_thread_handle");

  int error= 0;
  donor_transfer_finished= false;

  set_recovery_thread_context();

  mysql_mutex_lock(&run_lock);
  recovery_running= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  THD_STAGE_INFO(recovery_thd, stage_executing);

  //a new recovery round is starting, clean the status
  rejected_donors.clear();

  //wait for the appliers suspension
  if (!recovery_aborted &&
     (error= applier_module->wait_for_applier_complete_suspension(&recovery_aborted)))
  {
    log_message(MY_ERROR_LEVEL,
                "Can't evaluate the group replication applier execution status. "
                "Group replication recovery will shutdown to avoid data "
                "corruption.");
    goto cleanup;
  }

  if (group_info->get_number_of_members() == 1)
  {
    log_message(MY_INFORMATION_LEVEL,
                "Only one server alive."
                "Declaring this server as online within the replication group");

    goto single_member_online;
  }

  THD_STAGE_INFO(recovery_thd, stage_connecting_to_master);

  if (!recovery_aborted)
  {
    //if the connection to the donor failed, abort recovery
    if ((error = establish_donor_connection()))
    {
      goto cleanup;
    }
  }

  THD_STAGE_INFO(recovery_thd, stage_executing);

  mysql_mutex_lock(&recovery_lock);
  while (!donor_transfer_finished && !recovery_aborted)
  {
    mysql_cond_wait(&recovery_condition, &recovery_lock);
  }
  mysql_mutex_unlock(&recovery_lock);

  terminate_recovery_slave_threads();
  connected_to_donor= false;

  /**
    If recovery fails or is aborted, it never makes sense to awake the applier,
    as that would lead to the certification and execution of transactions on
    the wrong context.
  */

single_member_online:
  if (!recovery_aborted)
    applier_module->awake_applier_module();

  wait_for_applier_module_recovery();

cleanup:

  //if finished, declare the member online
  if (!recovery_aborted && !error)
    notify_group_recovery_end();

  mysql_mutex_lock(&run_lock);
  recovery_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  /*
   If recovery failed, it's no use to continue in the group as the member cannot
   take an active part in it, so it must leave.

   This code can only be invoked after recovery being declared as terminated as
   otherwise it would deadlock with the method waiting for the last view, and
   last view waiting for this thread to die.
  */
  if (error)
  {
    log_message(MY_ERROR_LEVEL,
                "Fatal error during the Recovery process of Group Replication. "
                "The server will leave the group.");
    gcs_control_interface->leave();
  }

  clean_recovery_thread_context();

  DBUG_RETURN(error);
}

int
Recovery_module::set_retrieved_cert_info(void* info)
{
  DBUG_ENTER("Recovery_module::set_retrieved_cert_info");

  mysql_mutex_lock(&recovery_lock);

  View_change_log_event* view_change_event= static_cast<View_change_log_event*>(info);
  // Transmit the certification info into the pipeline
  Handler_certifier_information_action *cert_action=
    new Handler_certifier_information_action(view_change_event->get_certification_info(),
                                             view_change_event->get_seq_number());
  applier_module->handle_pipeline_action(cert_action);
  delete cert_action;

  donor_transfer_finished= true;
  mysql_cond_broadcast(&recovery_condition);
  mysql_mutex_unlock(&recovery_lock);

  DBUG_RETURN(0);
}

void
Recovery_module::set_recovery_thread_context()
{
  my_thread_init();
  THD* thd= new THD;
  thd->set_new_thread_id();
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  global_thd_manager_add_thd(thd);
  thd->security_context()->skip_grants();
  recovery_thd= thd;
}

void
Recovery_module::clean_recovery_thread_context()
{
  recovery_thd->release_resources();
  THD_CHECK_SENTRY(recovery_thd);
  global_thd_manager_remove_thd(recovery_thd);

  delete recovery_thd;

  my_thread_end();
}

bool Recovery_module::select_donor()
{
  DBUG_ENTER("Recovery_module::select_donor");

  bool no_available_donors= false;
  bool clean_run= rejected_donors.empty();
  while (!no_available_donors)
  {
    std::vector<Group_member_info*>* member_set= group_info->get_all_members();
    std::vector<Group_member_info*>::iterator it= member_set->begin();
    //select the first online member
    while (it != member_set->end())
    {
      Group_member_info* member= *it;
      //is online and it's not me and didn't error out before
      string m_uuid= *member->get_uuid();

      bool is_online= member->get_recovery_status() ==
          Group_member_info::MEMBER_ONLINE;
      bool not_self= m_uuid.compare(*local_member_info->get_uuid());
      bool not_rejected= std::find(rejected_donors.begin(),
                                   rejected_donors.end(),
                                   m_uuid) == rejected_donors.end();

      if ( is_online && not_self && not_rejected )
      {
        selected_donor_uuid.clear();
        selected_donor_uuid.append(*(*it)->get_uuid());

        delete member_set;

        DBUG_RETURN(no_available_donors);
      }
      ++it;
    }

    delete member_set;

    //no donor was found
    if (!clean_run)
    {
      //There were donors that threw an error before, try again with those
      rejected_donors.clear();
      clean_run= true;
    }
    else
    {
      //no more donor to try with, just report an error
      no_available_donors= true;
      log_message(MY_ERROR_LEVEL,
                  "No suitable donor found, "
                  "group replication recovery aborting.");
      break;
    }
  }
  DBUG_RETURN(no_available_donors);
}

int Recovery_module::establish_donor_connection(bool failover)
{
  DBUG_ENTER("Recovery_module::establish_donor_connection");

  int error= -1;
  connected_to_donor= false;

  while (error != 0 && !recovery_aborted)
  {
    if (!failover)
      mysql_mutex_lock(&donor_selection_lock);

    if ((error= select_donor())) //select a donor
    {
      if (!failover)
        mysql_mutex_unlock(&donor_selection_lock);
      DBUG_RETURN(error); //no available donors, abort
    }

#ifndef DBUG_OFF
    DBUG_EXECUTE_IF("recovery_thread_wait",
                  {
                    const char act[]= "now wait_for signal.recovery_continue";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
#endif // DBUG_OFF

    if (!error)
    {
      if ((error= initialize_donor_connection(!failover)))
      {
        log_message(MY_ERROR_LEVEL,
                    "Error when configuring the group recovery"
                    " connection to the donor.");
      }
    }

    if (!error && !recovery_aborted)
    {
      error= start_recovery_donor_threads();
    }

    if (error)
    {
      if (donor_connection_retry_count == max_connection_attempts_to_donors)
      {
        log_message(MY_ERROR_LEVEL,
                    "Maximum number of retries when trying to "
                    "connect to a donor reached. "
                    "Aborting group replication recovery.");
        if (!failover)
          mysql_mutex_unlock(&donor_selection_lock);
        DBUG_RETURN(error); // max number of retries reached, abort
      }
      else
      {
        donor_connection_retry_count++;
        rejected_donors.push_back(selected_donor_uuid);
        log_message(MY_INFORMATION_LEVEL,
                    "Retrying group recovery connection with another donor. "
                    "Attempt %d/%d",
                    donor_connection_retry_count,
                    max_connection_attempts_to_donors);
      }
    }
    else
    {
      connected_to_donor = true;
    }
    if (!failover)
      mysql_mutex_unlock(&donor_selection_lock);
  }

  DBUG_RETURN(error);
}

int Recovery_module::initialize_donor_connection(bool purge_logs){

  DBUG_ENTER("Recovery_module::initialize_donor_connection");

  int error= 0;

  Group_member_info* selected_donor=
      group_info->get_group_member_info(selected_donor_uuid);

  if (selected_donor == NULL)
  {
    DBUG_RETURN(1);
  }

  /*
    When initializing a donor connection, if not on failover reset the channel.
    One of two things happen:
    1) The channel does not exists so nothing happens.
    2) The channel exists, so a connection error occurred or there is a failed
       recovery session that should be cleaned.
  */
  if (purge_logs)
  {
    donor_connection_interface.purge_logs();
  }

  char* hostname= const_cast<char*>(selected_donor->get_hostname()->c_str());
  uint port= selected_donor->get_port();

  error= donor_connection_interface.initialize_channel(hostname, port,
                                                       donor_connection_user,
                                                       donor_connection_password,
                                                       DEFAULT_THREAD_PRIORITY,
                                                       true, 1, false);

  if (!error)
  {
    log_message(MY_INFORMATION_LEVEL,
                "Establishing connection to a group replication recovery donor"
                " %s at %s port: %d.",
                selected_donor_uuid.c_str(),
                hostname,
                port);
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Error while creating the group replication recovery channel "
                "with donor %s at %s port: %d.",
                selected_donor_uuid.c_str(),
                hostname,
                port);
  }

  delete selected_donor;

  DBUG_RETURN(error);
}

int Recovery_module::start_recovery_donor_threads()
{
  DBUG_ENTER("Recovery_module::start_recovery_donor_threads");

  int error= donor_connection_interface.start_threads(true, true,
                                                      &view_id, true);

  if (error)
  {

    if (error == RPL_CHANNEL_SERVICE_RECEIVER_CONNECTION_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "There was an error when connecting to the donor server. "
                  "Check group replication recovery's connection credentials.");
    }
    else
    {
      log_message(MY_ERROR_LEVEL,
                  "Error while starting the group replication recovery "
                  "receiver/applier threads");
    }
  }

  DBUG_RETURN(error);
}

int Recovery_module::terminate_recovery_slave_threads()
{
  DBUG_ENTER("Recovery_module::terminate_recovery_slave_threads");

  log_message(MY_INFORMATION_LEVEL,
              "Terminating existing group replication donor connection "
              "and purging the corresponding logs.");
  /*
   Lock to avoid concurrent donor failover attempts when we are already
   terminating the threads.
  */
  mysql_mutex_lock(&donor_selection_lock);

  int error= 0;

  //If the threads never started, the method just returns
  if ((error= donor_connection_interface.stop_threads(true, true)))
  {
    log_message(MY_ERROR_LEVEL,
                "Error when stopping the group replication recovery's donor"
                " connection");
  }
  else
  {
    //If there is no repository in place nothing happens
    error = purge_recovery_slave_threads_repos();
  }

  mysql_mutex_unlock(&donor_selection_lock);
  DBUG_RETURN(error);
}

int Recovery_module::purge_recovery_slave_threads_repos()
{
  DBUG_ENTER("Recovery_module::purge_recovery_slave_threads_repos");

  int error= 0;
  if ((error = donor_connection_interface.purge_logs()))
  {
    log_message(MY_ERROR_LEVEL,
                "Error when purging the group replication recovery's relay logs");
    DBUG_RETURN(error);
  }

  DBUG_RETURN(error);
}


void Recovery_module::wait_for_applier_module_recovery()
{

  bool applier_monitoring= true;
  while (!recovery_aborted && applier_monitoring)
  {
    ulong queue_size = applier_module->get_message_queue_size();
    if (queue_size <= RECOVERY_TRANSACTION_THRESHOLD)
    {
      applier_monitoring= false;
    }
    else
    {
      usleep(100 * queue_size);
    }
  }
}

void Recovery_module::notify_group_recovery_end()
{
  DBUG_ENTER("Recovery_module::notify_group_recovery_end");

  Recovery_message *recovery_msg=
      new Recovery_message(Recovery_message::RECOVERY_END_MESSAGE,
                           local_member_info->get_uuid());

  vector<uchar> encoded_recovery_msg;
  recovery_msg->encode(&encoded_recovery_msg);

  Gcs_group_identifier destination(string(this->group_name));
  Gcs_member_identifier *origin= gcs_control_interface->get_local_information();

  Gcs_message* msg= new Gcs_message(*origin, destination, UNIFORM);

  msg->append_to_payload(&encoded_recovery_msg.front(),
                         encoded_recovery_msg.size());

  bool msg_error= gcs_communication_interface->send_message(msg);

  if (msg_error)
  {
    log_message(MY_ERROR_LEVEL,
                "Error while sending message for group replication recovery");
  }

  delete recovery_msg;
  delete msg;

  DBUG_VOID_RETURN;
}

bool Recovery_module::is_own_event_channel(my_thread_id id)
{
  DBUG_ENTER("Recovery_module::is_own_event_channel");
  DBUG_RETURN(donor_connection_interface.is_own_event_applier(id));
}
