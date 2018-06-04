/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RECOVERY_STATE_TRANSFER_INCLUDE
#define RECOVERY_STATE_TRANSFER_INCLUDE

#include "channel_observation_manager.h"
#include "replication_threads_api.h"
#include "member_info.h"
#include <mysql/group_replication_priv.h>

#include <string>
#include <vector>


class Recovery_state_transfer
{
public:

  /**
    Recovery state transfer constructor
    @param recovery_channel_name  The channel name to be used
    @param member_uuid            This member uuid
    @param channel_obsr_mngr      The channel state observer manager
  */
  Recovery_state_transfer(char* recovery_channel_name,
                          const std::string& member_uuid,
                          Channel_observation_manager *channel_obsr_mngr);

  ~Recovery_state_transfer();

  // Base methods: init / abort / end

  /**
    Initialize the state transfer class and reset the class flags

    @param rec_view_id  The view id to use on this round
  */
  void initialize(const std::string& rec_view_id);

  /** Abort the state transfer */
  void abort_state_transfer();

  /**
    Signals that the data was received so the process can end.
  */
  void end_state_transfer();

  //Methods for variable updates

  /** Sets the number of times recovery tries to connect to a given donor */
  void set_recovery_donor_retry_count(ulong retry_count)
  {
    max_connection_attempts_to_donors= retry_count;
  }

  /** Sets the sleep time between connection attempts to all possible donors */
  void set_recovery_donor_reconnect_interval(ulong reconnect_interval)
  {
    donor_reconnect_interval= reconnect_interval;
  }

  /**
    Sets all the SSL option to use on recovery.

     @param use_ssl                 force the use of SSL on recovery connections
     @param ssl_ca                  SSL trusted certificate authorities file
     @param ssl_capath              a directory with trusted CA files
     @param ssl_cert                the certificate file for secure connections
     @param ssl_cipher              the list of ciphers to use
     @param ssl_key                 the SSL key file
     @param ssl_crl                 SSL revocation list file
     @param ssl_crlpath             path with revocation list files
     @param ssl_verify_server_cert  verify the hostname against the certificate
  */
  void set_recovery_ssl_options(bool use_ssl,
                                const char *ssl_ca,
                                const char *ssl_capath,
                                const char *ssl_cert,
                                const char *ssl_cipher,
                                const char *ssl_key,
                                const char *ssl_crl,
                                const char *ssl_crlpath,
                                bool ssl_verify_server_cert)
  {
    recovery_use_ssl= use_ssl;
    if (ssl_ca != NULL)
      set_recovery_ssl_ca(ssl_ca);
    if (ssl_capath != NULL)
      set_recovery_ssl_capath(ssl_capath);
    if (ssl_cert != NULL)
      set_recovery_ssl_cert(ssl_cert);
    if (ssl_cipher != NULL)
      set_recovery_ssl_cipher(ssl_cipher);
    if (ssl_key != NULL)
      set_recovery_ssl_key(ssl_key);
    if (ssl_crl != NULL)
      set_recovery_ssl_crl(ssl_crl);
    if (ssl_crlpath != NULL)
      set_recovery_ssl_crl(ssl_crlpath);
    recovery_ssl_verify_server_cert= ssl_verify_server_cert;
  }

  /** Set the option that forces the use of SSL on recovery connections */
  void set_recovery_use_ssl(char use_ssl)
  {
    this->recovery_use_ssl= use_ssl;
  }

  /** Set a SSL trusted certificate authorities file */
  void set_recovery_ssl_ca(const char* ssl_ca)
  {
    memcpy(recovery_ssl_ca, ssl_ca, strlen(ssl_ca)+1);
  }

  /** Set a folder with SSL trusted CA files */
  void set_recovery_ssl_capath(const char* ssl_capath)
  {
    memcpy(recovery_ssl_capath, ssl_capath, strlen(ssl_capath)+1);
  }

  /** Set a SSL certificate for connection */
  void set_recovery_ssl_cert(const char* ssl_cert)
  {
    memcpy(recovery_ssl_cert, ssl_cert, strlen(ssl_cert)+1);
  }

  /** Set a SSL ciphers to be used */
  void set_recovery_ssl_cipher(const char* ssl_cipher)
  {
    memcpy(recovery_ssl_cipher, ssl_cipher, strlen(ssl_cipher)+1);
  }

  /** Set a SSL key for connections */
  void set_recovery_ssl_key(const char* ssl_key)
  {
    memcpy(recovery_ssl_key, ssl_key, strlen(ssl_key)+1);
  }

  /** Set a SSL revocation list file*/
  void set_recovery_ssl_crl(const char* ssl_crl)
  {
    memcpy(recovery_ssl_crl, ssl_crl, strlen(ssl_crl)+1);
  }

  /** Set a folder with SSL revocation list files*/
  void set_recovery_ssl_crlpath(const char* ssl_crlpath)
  {
    memcpy(recovery_ssl_crlpath, ssl_crlpath, strlen(ssl_crlpath)+1);
  }

  /** Set if recovery shall compare the used hostname against the certificate */
  void set_recovery_ssl_verify_server_cert(char ssl_verify_server_cert)
  {
    this->recovery_ssl_verify_server_cert= ssl_verify_server_cert;
  }

  /**
    Sets the recovery shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout (ulong timeout){
    donor_connection_interface.set_stop_wait_timeout(timeout);
  }

  //Methods that update the state transfer process

  /** This method initializes the group membership info */
  void initialize_group_info();

  /**
    This method decides what action to take when a member exits the group.
    If the donor left, and the state transfer is still ongoing, then pick a
    new one and restart the transfer.

    @param did_members_left states if members left the view

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int update_recovery_process(bool did_members_left);

  /**
    Method that informs recovery that the donor channel applier was stopped.

    @param thread_id  the applier thread id
    @param aborted    if the applier was aborted or stopped
  */
  void inform_of_applier_stop(my_thread_id thread_id, bool aborted);

  /**
    Method that informs recovery that the donor channel receiver was stopped.

    @param thread_id  the applier thread id
  */
  void inform_of_receiver_stop(my_thread_id thread_id);

  //Status methods

  /**
    Checks if the given id matches the recovery applier thread
    @param id  the thread id

    @return if it belongs to a thread
      @retval true   the id matches a SQL or worker thread
      @retval false  the id doesn't match any thread
  */
  bool is_own_event_channel(my_thread_id id);

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

  //class core method

  /**
    Execute state transfer
    @param recovery_thd  The recovery thread handle to report the status

    @return the operation status
      @retval 0      OK
      @retval !=0    Recovery state transfer failed
   */
  int state_transfer(THD *recovery_thd);

private:

  /**
    Removes the old list of group members and enquires about the current members

    @param[in]  update_donor  update the selected donor pointer when updating
  */
  void update_group_membership(bool update_donor);

  /**
    Based on the group list, build a random order list with all suitable donors.

    @param selected_donor the current selected donor to update its pointer
  */
  void build_donor_list(std::string* selected_donor);

  /** Method that sets the failover status to true and awakes recovery */
  void donor_failover();

  /**
    Establish a master/slave connection to the selected donor.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int establish_donor_connection();

  /**
    Initializes the structures for the donor connection threads.
    Recovery channel is always purged.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_donor_connection();

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

private:

  /* The member uuid*/
  std::string member_uuid;
  /* The associated view id for the current recovery session */
  std::string view_id;

  /* The selected donor member*/
  Group_member_info* selected_donor;
  /* Vector with group members info*/
  std::vector<Group_member_info*>* group_members;
  /* Member with suitable donors for use on recovery*/
  std::vector<Group_member_info*> suitable_donors;

  /* Retry count on donor connections*/
  long donor_connection_retry_count;

  /* Recovery abort flag */
  bool recovery_aborted;
  /*  Flag that signals when the donor transfered all it's data */
  bool donor_transfer_finished;
  /* Are we successfully connected to a donor*/
  bool connected_to_donor;
  /* Are we on failover mode*/
  bool on_failover;
  /* Did an error happened in one of the threads*/
  bool donor_channel_thread_error;

  //Recovery connection related structures

  /** Interface class to interact with the donor connection threads*/
  Replication_thread_api donor_connection_interface;

  /* The plugin's control module for channel status observation */
  Channel_observation_manager* channel_observation_manager;

  /* The recovery channel state observer */
  Channel_state_observer* recovery_channel_observer;

  /** If the use of SSL is obligatory on recovery connections */
  bool recovery_use_ssl;
  /** The configured SSL trusted certificate authorities file */
  char recovery_ssl_ca[FN_REFLEN];
  /** The configured directory that contains trusted SSL CA files*/
  char recovery_ssl_capath[FN_REFLEN];
  /** The configured SSL certificate file to use for a secure connection*/
  char recovery_ssl_cert[FN_REFLEN];
  /** The configured SSL list of permissible ciphers to use for encryption.*/
  char recovery_ssl_cipher[FN_REFLEN];
  /** The configured SSL key file to use for establishing a secure connection.*/
  char recovery_ssl_key[FN_REFLEN];
  /** The configured SSL file containing certificate revocation lists*/
  char recovery_ssl_crl[FN_REFLEN];
  /** The configured directory that contains certificate revocation list files*/
  char recovery_ssl_crlpath[FN_REFLEN];
  /** If the server's Common Name value checks against donor sent certificate.*/
  bool recovery_ssl_verify_server_cert;

  /* The lock for the recovery wait condition */
  mysql_mutex_t recovery_lock;
  /* The condition for the recovery wait */
  mysql_cond_t recovery_condition;
  mysql_mutex_t donor_selection_lock;

  /* Recovery max number of retries due to failures*/
  long max_connection_attempts_to_donors;
  /* Sleep time between connection attempts to all possible donors*/
  long donor_reconnect_interval;
};
#endif /* RECOVERY_INCLUDE */
