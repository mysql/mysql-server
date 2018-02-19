/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RECOVERY_INCLUDE
#define RECOVERY_INCLUDE

#include <mysql/group_replication_priv.h>
#include <stddef.h>
#include <list>
#include <string>

#include "plugin/group_replication/include/applier.h"
#include "plugin/group_replication/include/channel_observation_manager.h"
#include "plugin/group_replication/include/recovery_state_transfer.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"

/* The possible policies used on recovery when applying cached transactions */
enum enum_recovery_completion_policies {
  RECOVERY_POLICY_WAIT_CERTIFIED =
      0,                          // Wait for the certification of transactions
  RECOVERY_POLICY_WAIT_EXECUTED,  // Wait for the execution of transactions
};

class Recovery_module {
 public:
  /**
    Recovery_module constructor

    @param applier
              reference to the applier
    @param channel_obsr_mngr
              reference to the channel hooks observation manager
    @param components_stop_timeout
              timeout value for the recovery module during shutdown.
   */
  Recovery_module(Applier_module_interface *applier,
                  Channel_observation_manager *channel_obsr_mngr,
                  ulong components_stop_timeout);

  ~Recovery_module();

  void set_applier_module(Applier_module_interface *applier) {
    applier_module = applier;
  }

  /**
    Starts the recovery process, initializing the recovery thread.
    This method is designed to be as light as possible, as if it involved any
    major computation or wait process that would block the view change process
    delaying the group.

    @note this method only returns when the recovery thread is already running

    @param group_name          the joiner's group name
    @param rec_view_id         the new view id

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int start_recovery(const std::string &group_name,
                     const std::string &rec_view_id);

  /**
    Recovery thread main execution method.

    Here, the donor is selected, the connection to the donor is established,
    and several safe keeping assurances are guaranteed, such as the applier
    being suspended.
  */
  int recovery_thread_handle();

  /**
    Set retrieved certification info from a group replication channel extracted
    from a given View_change event.

    @param info  the given view_change_event

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int set_retrieved_cert_info(void *info);

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
    This method decides what action to take when a member exits the group and
    executes it.
    It can for the joiner:
      If it exited, then terminate the recovery process.
      If the donor left, and the state transfer is still ongoing, then pick a
      new one and restart the transfer.

    @param did_members_left states if members left the view
    @param is_leaving true if the member is leaving the group

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int update_recovery_process(bool did_members_left, bool is_leaving);

  // Methods for variable updates

  /** Sets the number of times recovery tries to connect to a given donor. */
  void set_recovery_donor_retry_count(ulong retry_count) {
    recovery_state_transfer.set_recovery_donor_retry_count(retry_count);
  }

  /** Sets the sleep time between connection attempts to all possible donors */
  void set_recovery_donor_reconnect_interval(ulong reconnect_interval) {
    recovery_state_transfer.set_recovery_donor_reconnect_interval(
        reconnect_interval);
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
  void set_recovery_ssl_options(bool use_ssl, const char *ssl_ca,
                                const char *ssl_capath, const char *ssl_cert,
                                const char *ssl_cipher, const char *ssl_key,
                                const char *ssl_crl, const char *ssl_crlpath,
                                bool ssl_verify_server_cert) {
    recovery_state_transfer.set_recovery_use_ssl(use_ssl);
    if (ssl_ca != NULL) recovery_state_transfer.set_recovery_ssl_ca(ssl_ca);
    if (ssl_capath != NULL)
      recovery_state_transfer.set_recovery_ssl_capath(ssl_capath);
    if (ssl_cert != NULL)
      recovery_state_transfer.set_recovery_ssl_cert(ssl_cert);
    if (ssl_cipher != NULL)
      recovery_state_transfer.set_recovery_ssl_cipher(ssl_cipher);
    if (ssl_key != NULL) recovery_state_transfer.set_recovery_ssl_key(ssl_key);
    if (ssl_crl != NULL) recovery_state_transfer.set_recovery_ssl_crl(ssl_crl);
    if (ssl_crlpath != NULL)
      recovery_state_transfer.set_recovery_ssl_crlpath(ssl_crlpath);
    recovery_state_transfer.set_recovery_ssl_verify_server_cert(
        ssl_verify_server_cert);
  }

  /** Set the option that forces the use of SSL on recovery connections */
  void set_recovery_use_ssl(char use_ssl) {
    recovery_state_transfer.set_recovery_use_ssl(use_ssl);
  }

  /** Set a SSL trusted certificate authorities file */
  void set_recovery_ssl_ca(const char *ssl_ca) {
    recovery_state_transfer.set_recovery_ssl_ca(ssl_ca);
  }

  /** Set a folder with SSL trusted CA files */
  void set_recovery_ssl_capath(const char *ssl_capath) {
    recovery_state_transfer.set_recovery_ssl_capath(ssl_capath);
  }

  /** Set a SSL certificate for connection */
  void set_recovery_ssl_cert(const char *ssl_cert) {
    recovery_state_transfer.set_recovery_ssl_cert(ssl_cert);
  }

  /** Set a SSL ciphers to be used */
  void set_recovery_ssl_cipher(const char *ssl_cipher) {
    recovery_state_transfer.set_recovery_ssl_cipher(ssl_cipher);
  }

  /** Set a SSL key for connections */
  void set_recovery_ssl_key(const char *ssl_key) {
    recovery_state_transfer.set_recovery_ssl_key(ssl_key);
  }

  /** Set a SSL revocation list file*/
  void set_recovery_ssl_crl(const char *ssl_crl) {
    recovery_state_transfer.set_recovery_ssl_crl(ssl_crl);
  }

  /** Set a folder with SSL revocation list files*/
  void set_recovery_ssl_crlpath(const char *ssl_crlpath) {
    recovery_state_transfer.set_recovery_ssl_crlpath(ssl_crlpath);
  }

  /** Set if recovery shall compare the used hostname against the certificate */
  void set_recovery_ssl_verify_server_cert(char ssl_verify_server_cert) {
    recovery_state_transfer.set_recovery_ssl_verify_server_cert(
        ssl_verify_server_cert);
  }

  /**
    Sets the recovery shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout) {
    stop_wait_timeout = timeout;
    recovery_state_transfer.set_stop_wait_timeout(timeout);
  }

  /**
    Sets recovery threshold policy on what to wait when handling transactions
    @param completion_policy  if recovery shall wait for execution
                              or certification
  */
  void set_recovery_completion_policy(
      enum_recovery_completion_policies completion_policy) {
    this->recovery_completion_policy = completion_policy;
  }

  /** Set a public key file*/
  void set_recovery_public_key_path(const char *public_key_path) {
    if (public_key_path != NULL)
      recovery_state_transfer.set_recovery_public_key_path(public_key_path);
  }

  /** Get public key automatically */
  void set_recovery_get_public_key(bool set) {
    recovery_state_transfer.set_recovery_get_public_key(set);
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
  /** Sets the thread context */
  void set_recovery_thread_context();

  /**
    Handles code for removing the member in case of a failure during
    recovery.
  */
  void leave_group_on_recovery_failure();

  /** Cleans the recovery thread related options/structures. */
  void clean_recovery_thread_context();

  /**
    Starts a wait process until the applier fulfills the necessary condition for
    the member to be acknowledge as being online.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int wait_for_applier_module_recovery();

  /**
    Sends a message throughout the group stating the member as online.
  */
  void notify_group_recovery_end();

  // recovery thread variables
  my_thread_handle recovery_pthd;
  THD *recovery_thd;

  /* The plugin's applier module interface*/
  Applier_module_interface *applier_module;

  /* The group to which the recovering member belongs */
  std::string group_name;

  /* The recovery state transfer class */
  Recovery_state_transfer recovery_state_transfer;

  /* Recovery thread state */
  thread_state recovery_thd_state;
  /* Recovery abort flag */
  bool recovery_aborted;

  // run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t run_cond;

  /* Recovery strategy when waiting for the cache transaction handling*/
  enum_recovery_completion_policies recovery_completion_policy;

  /* Recovery module's timeout on shutdown */
  ulong stop_wait_timeout;
};

#endif /* RECOVERY_INCLUDE */
