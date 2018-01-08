/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef REPLICATION_THREADS_API_INCLUDE
#define REPLICATION_THREADS_API_INCLUDE

#include <mysql/group_replication_priv.h>
#include <stddef.h>
#include <string>

#include "my_inttypes.h"

#define DEFAULT_THREAD_PRIORITY 0
//Applier thread InnoDB priority
#define GROUP_REPLICATION_APPLIER_THREAD_PRIORITY 1

class Replication_thread_api
{

public:
  Replication_thread_api(const char *channel_interface)
    :stop_wait_timeout(LONG_TIMEOUT),
    interface_channel(channel_interface)
    {};

  Replication_thread_api()
    :stop_wait_timeout(LONG_TIMEOUT),
    interface_channel(NULL)
    {};

  ~Replication_thread_api(){}

  /**
    Set the channel name to be used on the interface method invocation.

    @param channel_name the name to be used.
  */
  void set_channel_name(const char *channel_name)
  {
    interface_channel= channel_name;
  }

  /**
    Initializes a channel connection in a similar way to a change master command.

    @param hostname      The channel hostname
    @param port          The channel port
    @param user          The user used in the receiver connection
    @param password      The password used in the receiver connection
    @param use_ssl       Force the use of SSL on recovery connections
    @param ssl_ca        SSL trusted certificate authorities file
    @param ssl_capath    A directory with trusted CA files
    @param ssl_cert      The certificate file for secure connections
    @param ssl_cipher    The list of ciphers to use
    @param ssl_key       The SSL key file
    @param ssl_crl       SSL revocation list file
    @param ssl_crlpath   Path with revocation list files
    @param ssl_verify_server_cert  verify the hostname against the certificate
    @param priority      The channel priority on event application
    @param retry_count   The number of retries when connecting
    @param preserve_logs If logs should be always preserved
    @param public_key_path The file with public key path information
    @param get_public_key Preference to get public key if unavailable.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on channel creation
  */
  int initialize_channel(char* hostname, uint port,
                         char* user, char* password,
                         bool use_ssl,
                         char *ssl_ca,
                         char *ssl_capath,
                         char *ssl_cert,
                         char *ssl_cipher,
                         char *ssl_key,
                         char *ssl_crl,
                         char *ssl_crlpath,
                         bool ssl_verify_server_cert,
                         int priority,
                         int retry_count,
                         bool preserve_logs,
                         char *public_key_path,
                         bool get_public_key);

  /**
    Start the Applier/Receiver threads according to the given options.
    If the receiver thread is to be started, connection credential must be
    supported.

    @param start_receiver       Is the receiver thread to be started
    @param start_applier        Is the applier thread to be started
    @param view_id              The view id, that can be used to activate the
                                until view id clause.
    @param wait_for_connection  If when starting the receiver, the method should
                                wait for the connection to succeed

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_START_ERROR
        Error when launching on of the threads
      @retval REPLICATION_THREAD_START_IO_NOT_CONNECTED
        Error when the threads start, but the IO thread cannot connect
   */
  int start_threads(bool start_receiver, bool start_applier,
                    std::string* view_id, bool wait_for_connection);

  /**
    Stops the channel threads according to the given options.

    @param stop_receiver if the receiver thread should be stopped
    @param stop_applier  if the applier thread should be stopped

    @return the operation status
      @retval 0      OK
      @retval !=0    Error stopping channel thread
  */
  int stop_threads(bool stop_receiver, bool stop_applier);

  /**
    Purges the relay logs.

    @param reset_all  If true, the method will purge logs and remove the channel
                      If false, the channel logs will be deleted and recreated
                                but the channel info will be preserved.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error purging channel logs
  */
  int purge_logs(bool reset_all);

  /**
     Checks if the receiver thread is running.

     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_receiver_thread_running();

  /**
     Checks if the receiver thread is stopping.

     @return the thread status
      @retval true      the thread is stopping
      @retval false     the thread is not stopping
  */
  bool is_receiver_thread_stopping();

  /**
     Checks if the applier thread is running.

     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_applier_thread_running();

  /**
     Checks if the applier thread is stopping.

     @return the thread status
      @retval true      the thread is stopping
      @retval false     the thread is not stopping
  */
  bool is_applier_thread_stopping();

  /**
    Queues a event packet into the current active channel relay log.

    @param buf         the event buffer
    @param event_len  the event buffer length

    @return the operation status
      @retval 0      OK
      @retval != 0   Error on queue
  */
  int queue_packet(const char* buf, ulong event_len);

  /**
    Checks if the applier, and its workers when parallel applier is
    enabled, has already consumed all relay log, that is, applier is
    waiting for transactions to be queued.

    @return the applier status
      @retval true      the applier is waiting
      @retval false     otherwise
  */
  bool is_applier_thread_waiting();

  /**
    Checks if all the queued transactions were executed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied

    @return the operation status
      @retval 0   All transactions were executed
      @retval REPLICATION_THREAD_WAIT_TIMEOUT_ERROR     A timeout occurred
      @retval REPLICATION_THREAD_WAIT_NO_INFO_ERROR     An error occurred
  */
  int wait_for_gtid_execution(double timeout);

  /**
    Method to get applier ids from the configured channel

    @param[out] thread_ids The retrieved thread ids.

    @return the number of appliers
      @retval <= 0  Some error occurred or the applier is not present
      @retval >  0  Number of appliers
  */
  int get_applier_thread_ids(unsigned long** thread_ids);

  /**
     Checks if the given id matches any of the event applying threads for
     the configured channel.

     @param id  the thread id
     @param channel_name  the channel name which needs to be checked. It is
                          an optional parameter.

     @return if it belongs to a thread
       @retval true   the id matches a SQL or worker thread
       @retval false  the id doesn't match any thread
   */
  bool is_own_event_applier(my_thread_id id, const char* channel_name= NULL);

  /**
     Checks if the given id matches the receiver thread for
     the configured channel.

     @param id  the thread id

     @return if it belongs to a thread
       @retval true   the id matches an IO thread
       @retval false  the id doesn't match any thread
   */
  bool is_own_event_receiver(my_thread_id id);

  /**
    Returns last GNO from the applier for a given UUID.

    @param sidno    the SIDNO of the group UUID, so that we get the
                    last GNO of group's already certified transactions
                    on relay log.

    @return
      @retval       GNO value
  */
  rpl_gno get_last_delivered_gno(rpl_sidno sidno);

  /**
    Sets the threads shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout (ulong timeout){
    stop_wait_timeout= timeout;
  }

  /**
    Returns the retrieved gtid set from the receiver thread.

    @param[out] retrieved_set the set in string format.
    @param channel_name the name of the channel to get the information.

    @return
      @retval true there was an error.
      @retval false the operation has succeeded.
  */
  bool get_retrieved_gtid_set(std::string& retrieved_set,
                              const char* channel_name= NULL);

  /**
    Checks if the channel's relay log contains partial transaction.
    @return
      @retval true  If relaylog contains partial transaction.
      @retval false If relaylog does not contain partial transaction.
  */
  bool is_partial_transaction_on_relay_log();

private:
  ulong stop_wait_timeout;
  const char* interface_channel;
};

#endif /* REPLICATION_THREADS_API_INCLUDE */
