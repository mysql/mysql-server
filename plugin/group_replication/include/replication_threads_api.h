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

#ifndef REPLICATION_THREADS_API_INCLUDE
#define REPLICATION_THREADS_API_INCLUDE

#include <string>
#include <mysql/group_replication_priv.h>

#define DEFAULT_THREAD_PRIORITY 0
//Applier thread InnoDB priority
#define GROUP_REPLICATION_APPLIER_THREAD_PRIORITY 1

class Replication_thread_api
{

public:
  Replication_thread_api(char *channel_interface)
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
  void set_channel_name(char *channel_name)
  {
    interface_channel= channel_name;
  }

  /**
    Initializes a channel connection in a similar way to a change master command.

    @param hostname      The channel hostname
    @param port          The channel port
    @param user          The user used in the receiver connection
    @param password      The password used in the receiver connection
    @param priority      The channel priority on event application
    @param disable_mts   If MTS should be disable for this channel
    @param retry_count   The number of retries when connecting
    @param preserve_logs If logs should be always preserved

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on channel creation
  */
  int initialize_channel(char* hostname, uint port,
                         char* user, char* password,
                         int priority, bool disable_mts,
                         int retry_count,
                         bool preserve_logs);

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
      @retval !=0    Error on channel creation
  */
  int stop_threads(bool stop_receiver, bool stop_applier);

  /**
    Purges the relay logs.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on channel creation
  */
  int purge_logs();

  /**
     Checks if the receiver thread is running.

     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_receiver_thread_running();

  /**
     Checks if the applier thread is running.

     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_applier_thread_running();

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
    Checks if all the queued transactions were executed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied

    @return the operation status
      @retval 0   All transactions were executed
      @retval REPLICATION_THREAD_WAIT_TIMEOUT_ERROR     A timeout occurred
      @retval REPLICATION_THREAD_WAIT_NO_INFO_ERROR     An error occurred
  */
  int wait_for_gtid_execution(longlong timeout);

  /**
     Checks if the given id matches any of the event applying threads for
     the configured channel.

     @param id  the thread id

     @return if it belongs to a thread
       @retval true   the id matches a SQL or worker thread
       @retval false  the id doesn't match any thread
   */
  bool is_own_event_applier(my_thread_id id);

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

private:
  ulong stop_wait_timeout;
  char* interface_channel;
};

#endif /* REPLICATION_THREADS_API_INCLUDE */
