/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"
#include "plugin/group_replication/include/plugin_observers/channel_observation_manager.h"
#include "plugin/group_replication/include/recovery_state_transfer.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"

class Recovery_module {
 public:
  /* The error status for Recovery Metadata received. */
  enum class enum_recovery_metadata_error {
    // Metadata received without error.
    RECOVERY_METADATA_RECEIVED_NO_ERROR,
    // Time-out waiting for Metadata.
    RECOVERY_METADATA_RECEIVED_TIMEOUT_ERROR,
    // Recovery aborted.
    RECOVERY_METADATA_RECOVERY_ABORTED_ERROR,
    // Error fetching metadata.
    RECOVERY_METADATA_RECEIVED_ERROR
  };

  /**
    Recovery_module constructor

    @param applier
              reference to the applier
    @param channel_obsr_mngr
              reference to the channel hooks observation manager
   */
  Recovery_module(Applier_module_interface *applier,
                  Channel_observation_manager *channel_obsr_mngr);

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
    @param view_id             the view id to use for the recovery.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int start_recovery(const std::string &group_name, const std::string &view_id);

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

    @param wait_for_termination  wait for thread termination or not

    @note this method only returns when the thread is stopped or on timeout

    @return the operation status
      @retval 0      OK
      @retval !=0    Timeout
  */
  int stop_recovery(bool wait_for_termination = true);

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
     @param tls_version             the list of TLS versions to use
     @param tls_ciphersuites        the list of TLS ciphersuites to use
  */
  void set_recovery_ssl_options(bool use_ssl, const char *ssl_ca,
                                const char *ssl_capath, const char *ssl_cert,
                                const char *ssl_cipher, const char *ssl_key,
                                const char *ssl_crl, const char *ssl_crlpath,
                                bool ssl_verify_server_cert, char *tls_version,
                                char *tls_ciphersuites) {
    recovery_state_transfer.set_recovery_use_ssl(use_ssl);
    if (ssl_ca != nullptr) recovery_state_transfer.set_recovery_ssl_ca(ssl_ca);
    if (ssl_capath != nullptr)
      recovery_state_transfer.set_recovery_ssl_capath(ssl_capath);
    if (ssl_cert != nullptr)
      recovery_state_transfer.set_recovery_ssl_cert(ssl_cert);
    if (ssl_cipher != nullptr)
      recovery_state_transfer.set_recovery_ssl_cipher(ssl_cipher);
    if (ssl_key != nullptr)
      recovery_state_transfer.set_recovery_ssl_key(ssl_key);
    if (ssl_crl != nullptr)
      recovery_state_transfer.set_recovery_ssl_crl(ssl_crl);
    if (ssl_crlpath != nullptr)
      recovery_state_transfer.set_recovery_ssl_crlpath(ssl_crlpath);
    recovery_state_transfer.set_recovery_ssl_verify_server_cert(
        ssl_verify_server_cert);
    if (tls_version != nullptr)
      recovery_state_transfer.set_recovery_tls_version(tls_version);
    recovery_state_transfer.set_recovery_tls_ciphersuites(tls_ciphersuites);
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

  /** Set TLS version to be used */
  void set_recovery_tls_version(const char *tls_version) {
    recovery_state_transfer.set_recovery_tls_version(tls_version);
  }

  /** Set TLS ciphersuites to be used */
  void set_recovery_tls_ciphersuites(const char *tls_ciphersuites) {
    recovery_state_transfer.set_recovery_tls_ciphersuites(tls_ciphersuites);
  }

  /**
    @return Is recovery configured to use SSL
  */
  bool get_recovery_use_ssl() {
    return recovery_state_transfer.get_recovery_use_ssl();
  }

  /**
    Get SSL options configured for recovery

    @param[out]  ssl_ca    the ssl ca
    @param[out]  ssl_cert  the ssl cert
    @param[out]  ssl_key   the ssl key
  */
  void get_recovery_base_ssl_options(std::string *ssl_ca, std::string *ssl_cert,
                                     std::string *ssl_key) {
    recovery_state_transfer.get_recovery_base_ssl_options(ssl_ca, ssl_cert,
                                                          ssl_key);
  }
  /**
    Sets the recovery shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout) {
    recovery_state_transfer.set_stop_wait_timeout(timeout);
  }

  /** Set a public key file*/
  void set_recovery_public_key_path(const char *public_key_path) {
    if (public_key_path != nullptr)
      recovery_state_transfer.set_recovery_public_key_path(public_key_path);
  }

  /** Get public key automatically */
  void set_recovery_get_public_key(bool set) {
    recovery_state_transfer.set_recovery_get_public_key(set);
  }

  /** Set compression algorithm */
  void set_recovery_compression_algorithm(const char *name) {
    recovery_state_transfer.set_recovery_compression_algorithm(name);
  }

  /** Set compression level */
  void set_recovery_zstd_compression_level(uint level) {
    recovery_state_transfer.set_recovery_zstd_compression_level(level);
  }

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

  // Recovery Metadata related function and variables - start
  /**
    Awakes recovery thd, waiting for the donor to send recovery metadata.
    If send recovery metadata fails it sets the error, so that waiting recovery
    thd unblocks and stops with error, otherwise on successful receive of
    recovery metadata it awakes waiting recovery thd without error.

    @param error  Error status in recovery metadata fetching.
  */
  void awake_recovery_metadata_suspension(bool error = false);

  /**
    Suspend recovery thd, so that member can wait to receive the recovery
    metadata.
  */
  void suspend_recovery_metadata();

  /**
    Set the recovery metadata message.

    @param[in] recovery_metadata_message  the recovery metadata message pointer.

    @return the error status
      @retval true   Error
      @retval false  Success
  */
  bool set_recovery_metadata_message(
      Recovery_metadata_message *recovery_metadata_message);

  /**
    Delete recovery metadata object.
  */
  void delete_recovery_metadata_message();

  /**
    Return the flag which determine if VCLE is enabled.

    @return the status which determine if VCLE is enabled.
  */
  bool is_vcle_enable();

  /**
    Set the View ID on which the joiner joined.

    @param  is_vcle_enabled  the flag determine if View_change_log_event
                             is enabled.
  */
  void set_vcle_enabled(bool is_vcle_enabled);

 private:
  /** Flag to determine if recovery should use VCLE */
  bool m_is_vcle_enable{false};

  /** Recovery metadata received on group members. */
  Recovery_metadata_message *m_recovery_metadata_message{nullptr};

  // Recovery Metadata related function and variables - end

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

  /**
    Starts a wait process until the recovery metadata is successfully send by
    the donor.

    @return the error status. Check enum_recovery_metadata_error for details.
  */
  enum_recovery_metadata_error wait_for_recovery_metadata_gtid_executed();

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

  /*
    The replication until condition that can be applied to
    channels for the recovery.
  */
  enum_channel_until_condition m_until_condition{CHANNEL_UNTIL_VIEW_ID};

  /*
    The maximum time till which recovery thread will wait for recovery metadata
    from sender.
  */
  unsigned int m_max_metadata_wait_time;

  // run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t run_cond;

  /* The return value from state transfer operation*/
  State_transfer_status m_state_transfer_return;

  /* Recovery metadata receive status. */
  bool m_recovery_metadata_received{false};

  /** Error while fetching Recovery metadata. */
  bool m_recovery_metadata_received_error{false};

  /** Recovery metadata receive error status. */
  enum_recovery_metadata_error m_recovery_metadata_error_status;

  // condition and lock used to suspend/awake the recovery module
  /* The lock for suspending/wait for the awake of the recovery module */
  mysql_mutex_t m_recovery_metadata_receive_lock;

  /* The condition for suspending/wait for the awake of the recovery module */
  mysql_cond_t m_recovery_metadata_receive_waiting_condition;
};

#endif /* RECOVERY_INCLUDE */
