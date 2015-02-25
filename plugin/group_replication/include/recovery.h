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

#ifndef GCS_RECOVERY_INCLUDE
#define GCS_RECOVERY_INCLUDE

#include <list>

#include "applier.h"
#include "gcs_communication_interface.h"
#include "gcs_control_interface.h"
#include <mysql/group_replication_priv.h>

class Recovery_module
{

public:
/**
  Recovery_module constructor

  @param applier
            reference to the applier module
  @param comm_if
            reference to the communication interface of the current cluster
  @param ctrl_if
            reference to the control interface of the current cluster
  @param local_info
            reference to the local node information
  @param cluster_info_if
            reference to the Global cluster view manager
  @param gcs_component_stop_timeout
            timeout value for the recovery module during shutdown.
 */
  Recovery_module(Applier_module_interface *applier,
                  Gcs_communication_interface *comm_if,
                  Gcs_control_interface *ctrl_if,
                  Cluster_member_info *local_info,
                  Cluster_member_info_manager_interface* cluster_info_if,
                  ulong gcs_components_stop_timeout);

  ~Recovery_module();

  void set_applier_module(Applier_module_interface *applier)
  {
    applier_module= applier;
  }

  /**
    Starts the recovery process, initializing the recovery thread.
    This method is designed to be as light as possible, as if it involved any
    major computation or wait process that would block the view change process
    delaying the cluster.

    @note this method only returns when the recovery thread is already running

    @param group_name       the joiner's group name
    @param rec_view_id          the new view id

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int start_recovery(const string& group_name,
                     char* rec_view_id);

  /**
   Checks to see if the recovery IO/SQL thread is still running, probably caused
   by an timeout on shutdown.
   If the threads are still running, we try to stop them again.
   If not possible, an error is reported.

   @return are the threads stopped
      @retval 0      All is stopped.
      @retval !=0    Threads are still running
  */
  int check_recovery_thread_status();

  /**
    Recovery thread main execution method.

    Here, the donor is selected, the connection to the donor is established,
    and several safe keeping assurances are guaranteed, such as the applier
    being suspended.
  */
  int recovery_thread_handle();

  /**
    Set retrieved certification info from a GCS channel extracted from
    a given View_change event.

    @param info  the given view_change_event

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int set_retrieved_cert_info(void* info);

  /**
    Stops the recovery process, shutting down the recovery thread.
    If the thread does not stop in a user designated time interval, a timeout
    is issued.

    @note this method only returns when the thread is stopped or on timeout

    @return the operation status
      @retval 0      OK
      @retval !=0    Timeout
  */
  int stop_recovery();

  /**
    This method executes what action to take whenever a node exists the cluster.
    It can for the joiner:
      If it exited, then terminate the recovery process.
      If the donor left, and the state transfer is still ongoing, then pick a
      new one and restart the transfer.

    @param did_nodes_left states if members left the view

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int update_recovery_process(bool did_nodes_left);


  int donor_failover();

  //Methods for variable updates

  /**Sets the user for the account used when connecting to a donor.*/
  void set_recovery_donor_connection_user(const char *user)
  {
    (void) strncpy(donor_connection_user, user, strlen(user)+1);
  }

   /**Sets the password for the account used when connecting to a donor.*/
  void set_recovery_donor_connection_password(const char *pass)
  {
    (void) strncpy(donor_connection_password, pass, strlen(pass)+1);
  }

  /**Sets the number of times recovery tries to connect to a given donor*/
  void set_recovery_donor_retry_count(ulong retry_count)
  {
    max_connection_attempts_to_donors= retry_count;
  }

  /**
    Sets the recovery shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout (ulong timeout){
    stop_wait_timeout= timeout;
    donor_connection_interface.set_stop_wait_timeout(timeout);
  }

  /**
     Checks if the given id matches the recovery applier thread
     @param id  the thread id

     @return if it belongs to a thread
       @retval true   the id matches a SQL or worker thread
       @retval false  the id doesn't match any thread
   */
  bool is_own_event_channel(my_thread_id id);

private:

   /**
     Selects a donor among the cluster nodes.
     Being open to several implementations, for now this method simply picks
     the first non joining node in the list.

     @return operation statue
       @retval 0   Donor found
       @retval 1   N suitable donor found
   */
   bool select_donor();

  /**
    Establish a master/slave connection to the selected donor.

    @param failover  failover to another a donor,
                     so only the IO is important

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int establish_donor_connection(bool failover= false);

  /**
    Initializes the structures for the donor connection threads.

    @param purge_logs  if we are failing over to another a donor,
                       we don't need to purge the recovery channel

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_donor_connection(bool purge_logs);

  /**
    Initializes the connection parameters for the donor connection.

    @return
      @retval false Everything OK
      @retval true  In case of the selected donor is not available
   */
  bool initialize_connection_parameters();

  /**
    Starts the recovery slave threads to receive data from the donor.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int start_recovery_donor_threads();

  /**
    Terminates the connection to the donor

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate_recovery_slave_threads();

  /**
    Purges relay logs and the master info object

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR
        Error when purging the relay logs
      @retval REPLICATION_THREAD_REPOSITORY_MI_PURGE_ERROR
        Error when cleaning the master info repository
  */
  int purge_recovery_slave_threads_repos();

  /**
    Terminates the connection to the donor

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  void set_recovery_thread_context();

  /**
    Cleans the recovery thread related options/structures.
  */
  void clean_recovery_thread_context();

  /**
    Starts a wait process until the node fulfills the necessary condition to be
    acknowledge as being online.
    As of now, this condition is met when the applier module's queue size drops
    below a certain margin.
  */
  void wait_for_applier_module_recovery();

  /**
    Sends a message throughout the cluster acknowledge the node as online.
  */
  void notify_cluster_recovery_end();

  //recovery thread variables
  my_thread_handle recovery_pthd;
#ifdef HAVE_PSI_INTERFACE
  PSI_thread_key key_thread_recovery;
#endif
  THD *recovery_thd;

  /* GCS control interface*/
  Gcs_control_interface* gcs_control_interface;
  /* Communication interface for GCS*/
  Gcs_communication_interface* gcs_communication_interface;

  /*Information about the local node*/
  Cluster_member_info* local_node_information;

  /* The plugin's applier module interface*/
  Applier_module_interface *applier_module;

  /* The group to which the recovering node belongs*/
  string group_name;
  /* The associated view id for the current recovery session */
  string view_id;

  /* The selected donor node uuid*/
  string selected_donor_uuid;
  /* Pointer to the Cluster Member manager*/
  Cluster_member_info_manager_interface *cluster_info;

  /* Donors who recovery could not connect */
  std::list<string> rejected_donors;
  /* Retry count on donor connections*/
  long donor_connection_retry_count;

  /* Recovery running flag */
  bool recovery_running;
  /* Recovery abort flag */
  bool recovery_aborted;
  /*  Flag that signals when the donor transfered all it's data */
  bool donor_transfer_finished;
  /* Are we successfully connected to a donor*/
  bool connected_to_donor;

  //Recovery connection related structures
  /** Interface class to interact with the donor connection threads*/
  Replication_thread_api donor_connection_interface;
  /** User defined rep user to be use on donor connection*/
  char donor_connection_user[USERNAME_LENGTH + 1];
  /** User defined password to be use on donor connection*/
  char donor_connection_password[MAX_PASSWORD_LENGTH + 1];

  //run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t  run_cond;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key run_mutex_key;
  PSI_cond_key  run_cond_key;
#endif

  /* The lock for the recovery wait condition */
  mysql_mutex_t recovery_lock;
  /* The condition for the recovery wait */
  mysql_cond_t recovery_condition;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key recovery_mutex_key;
  PSI_cond_key recovery_cond_key;
#endif

  mysql_mutex_t donor_selection_lock;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key donor_selection_mutex_key;
#endif

  /* Recovery module's timeout on shutdown */
  ulong stop_wait_timeout;
  /* Recovery max number of retries due to failures*/
  long max_connection_attempts_to_donors;
};

#endif /* GCS_RECOVERY_INCLUDE */
