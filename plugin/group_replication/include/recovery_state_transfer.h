/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef RECOVERY_STATE_TRANSFER_INCLUDE
#define RECOVERY_STATE_TRANSFER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <string>
#include <vector>

#include "compression.h"
#include "my_io.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_observers/channel_observation_manager.h"
#include "plugin/group_replication/include/replication_threads_api.h"

typedef enum st_state_transfer_status {
  STATE_TRANSFER_OK,            // OK
  STATE_TRANSFER_STOP,          // Fail to stop replica threads
  STATE_TRANSFER_PURGE,         // Fail to purge replica threads
  STATE_TRANSFER_NO_CONNECTION  // No connection to donor
} State_transfer_status;

class Recovery_state_transfer {
 public:
  /**
    Recovery state transfer constructor
    @param recovery_channel_name  The channel name to be used
    @param member_uuid            This member uuid
    @param channel_obsr_mngr      The channel state observer manager
  */
  Recovery_state_transfer(char *recovery_channel_name,
                          const std::string &member_uuid,
                          Channel_observation_manager *channel_obsr_mngr);

  ~Recovery_state_transfer();

  // Base methods: init / abort / end

  /**
    Initialize the state transfer class and reset the class flags

    @param rec_view_id  The view id to use on this round
  */
  void initialize(const std::string &rec_view_id);

  /** Abort the state transfer */
  void abort_state_transfer();

  /**
    Signals that the data was received so the process can end.
  */
  void end_state_transfer();

  // Methods for variable updates

  /** Sets the number of times recovery tries to connect to a given donor */
  void set_recovery_donor_retry_count(ulong retry_count) {
    max_connection_attempts_to_donors = retry_count;
  }

  /** Sets the sleep time between connection attempts to all possible donors */
  void set_recovery_donor_reconnect_interval(ulong reconnect_interval) {
    donor_reconnect_interval = reconnect_interval;
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
    recovery_use_ssl = use_ssl;
    if (ssl_ca != nullptr) set_recovery_ssl_ca(ssl_ca);
    if (ssl_capath != nullptr) set_recovery_ssl_capath(ssl_capath);
    if (ssl_cert != nullptr) set_recovery_ssl_cert(ssl_cert);
    if (ssl_cipher != nullptr) set_recovery_ssl_cipher(ssl_cipher);
    if (ssl_key != nullptr) set_recovery_ssl_key(ssl_key);
    if (ssl_crl != nullptr) set_recovery_ssl_crl(ssl_crl);
    if (ssl_crlpath != nullptr) set_recovery_ssl_crl(ssl_crlpath);
    recovery_ssl_verify_server_cert = ssl_verify_server_cert;
    if (tls_version != nullptr) set_recovery_tls_version(tls_version);
    set_recovery_tls_ciphersuites(tls_ciphersuites);
  }

  /** Set the option that forces the use of SSL on recovery connections */
  void set_recovery_use_ssl(char use_ssl) { this->recovery_use_ssl = use_ssl; }

  /** Set a SSL trusted certificate authorities file */
  void set_recovery_ssl_ca(const char *ssl_ca) {
    memcpy(recovery_ssl_ca, ssl_ca, strlen(ssl_ca) + 1);
  }

  /** Set a folder with SSL trusted CA files */
  void set_recovery_ssl_capath(const char *ssl_capath) {
    memcpy(recovery_ssl_capath, ssl_capath, strlen(ssl_capath) + 1);
  }

  /** Set a SSL certificate for connection */
  void set_recovery_ssl_cert(const char *ssl_cert) {
    memcpy(recovery_ssl_cert, ssl_cert, strlen(ssl_cert) + 1);
  }

  /** Set a SSL ciphers to be used */
  void set_recovery_ssl_cipher(const char *ssl_cipher) {
    memcpy(recovery_ssl_cipher, ssl_cipher, strlen(ssl_cipher) + 1);
  }

  /** Set a SSL key for connections */
  void set_recovery_ssl_key(const char *ssl_key) {
    memcpy(recovery_ssl_key, ssl_key, strlen(ssl_key) + 1);
  }

  /** Set a SSL revocation list file*/
  void set_recovery_ssl_crl(const char *ssl_crl) {
    memcpy(recovery_ssl_crl, ssl_crl, strlen(ssl_crl) + 1);
  }

  /** Set a folder with SSL revocation list files*/
  void set_recovery_ssl_crlpath(const char *ssl_crlpath) {
    memcpy(recovery_ssl_crlpath, ssl_crlpath, strlen(ssl_crlpath) + 1);
  }

  /** Set if recovery shall compare the used hostname against the certificate */
  void set_recovery_ssl_verify_server_cert(char ssl_verify_server_cert) {
    this->recovery_ssl_verify_server_cert = ssl_verify_server_cert;
  }

  /** Set a TLS versions to be used */
  void set_recovery_tls_version(const char *tls_version) {
    memcpy(recovery_tls_version, tls_version, strlen(tls_version) + 1);
  }

  /** Set a TLS ciphersuites to be used */
  void set_recovery_tls_ciphersuites(const char *tls_ciphersuites) {
    if (nullptr == tls_ciphersuites) {
      recovery_tls_ciphersuites_null = true;
    } else {
      recovery_tls_ciphersuites_null = false;
      memcpy(recovery_tls_ciphersuites, tls_ciphersuites,
             strlen(tls_ciphersuites) + 1);
    }
  }

  /**
    @return Is recovery configured to use SSL
  */
  bool get_recovery_use_ssl() { return this->recovery_use_ssl; }

  /**
    Get SSL options configured for recovery

    @param[out]  ssl_ca    the ssl ca
    @param[out]  ssl_cert  the ssl cert
    @param[out]  ssl_key   the ssl key
  */
  void get_recovery_base_ssl_options(std::string *ssl_ca, std::string *ssl_cert,
                                     std::string *ssl_key) {
    ssl_ca->assign(recovery_ssl_ca);
    ssl_cert->assign(recovery_ssl_cert);
    ssl_key->assign(recovery_ssl_key);
  }

  /**
    Sets the recovery shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout) {
    donor_connection_interface.set_stop_wait_timeout(timeout);
  }

  /** Set a public key file*/
  void set_recovery_public_key_path(const char *public_key_path) {
    if (public_key_path != nullptr) {
      memcpy(recovery_public_key_path, public_key_path,
             strlen(public_key_path) + 1);
    }
  }

  /** Get preference to get public key */
  void set_recovery_get_public_key(bool set) { recovery_get_public_key = set; }

  /** Set compression algorithm */
  void set_recovery_compression_algorithm(const char *name) {
    memcpy(recovery_compression_algorithm, name, strlen(name) + 1);
  }

  /** Set compression level */
  void set_recovery_zstd_compression_level(uint level) {
    recovery_zstd_compression_level = level;
  }

  // Methods that update the state transfer process

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

  // Status methods

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

  // class core method

  /**
    Execute state transfer
    @param stage_handler  Stage handler to update the system tables

    @return the operation status
      @retval 0      OK
      @retval !=0    Recovery state transfer failed
   */
  State_transfer_status state_transfer(
      Plugin_stage_monitor_handler &stage_handler);

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
  void build_donor_list(std::string *selected_donor);

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

    @param hostname hostname of current selected donor
    @param port port of current selected donor

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_donor_connection(std::string hostname, uint port);

  /**
    Initializes the connection parameters for the donor connection.

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

    @param purge_logs  purge recovery logs

    @return the operation status
      @retval STATE_TRANSFER_OK      OK
      @retval !=STATE_TRANSFER_OK    Error
  */
  State_transfer_status terminate_recovery_slave_threads(
      bool purge_logs = true);

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
  Group_member_info *selected_donor;
  /* The selected donor member hostname */
  std::string selected_donor_hostname;
  /* Vector with group members info*/
  Group_member_info_list *group_members;
  /* Member with suitable donors for use on recovery*/
  Group_member_info_list suitable_donors;

  /* Retry count on donor connections*/
  long donor_connection_retry_count;

  /* Recovery abort flag */
  bool recovery_aborted;
  /*  Flag that signals when the donor transferred all its data */
  bool donor_transfer_finished;
  /* Are we successfully connected to a donor*/
  bool connected_to_donor;
  /* Are we on failover mode*/
  bool on_failover;
  /* Did an error happened in one of the threads*/
  bool donor_channel_thread_error;

  // Recovery connection related structures

  /** Interface class to interact with the donor connection threads*/
  Replication_thread_api donor_connection_interface;

  /* The plugin's control module for channel status observation */
  Channel_observation_manager *channel_observation_manager;

  /* The recovery channel state observer */
  Channel_state_observer *recovery_channel_observer;

  /** If the use of SSL is obligatory on recovery connections */
  bool recovery_use_ssl;
  /** Get public key */
  bool recovery_get_public_key;
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
  /** Public key information */
  char recovery_public_key_path[FN_REFLEN];
  /** Permitted TLS versions. */
  char recovery_tls_version[FN_REFLEN];
  /** Permitted TLS 1.3 ciphersuites. */
  bool recovery_tls_ciphersuites_null;
  char recovery_tls_ciphersuites[FN_REFLEN];

  /* The lock for the recovery wait condition */
  mysql_mutex_t recovery_lock;
  /* The condition for the recovery wait */
  mysql_cond_t recovery_condition;
  mysql_mutex_t donor_selection_lock;

  /* Recovery max number of retries due to failures*/
  long max_connection_attempts_to_donors;
  /* Sleep time between connection attempts to all possible donors*/
  long donor_reconnect_interval;
  /* compression algorithm to be used for communication */
  char recovery_compression_algorithm[COMPRESSION_ALGORITHM_NAME_LENGTH_MAX];
  /* compression level to be used for compression */
  uint recovery_zstd_compression_level;
};
#endif /* RECOVERY_INCLUDE */
