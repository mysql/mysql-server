/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_REPLICATION_THREADS_API_INCLUDE
#define GCS_REPLICATION_THREADS_API_INCLUDE

#include "gcs_plugin_utils.h"

#include <rpl_info_factory.h>
#include <rpl_slave.h>
#include <rpl_rli.h>
#include <gcs_replication.h>
#include <string>

using std::string;

//Errors as given in the start/stop/init methods
#define REPLICATION_THREAD_REPOSITORY_CREATION_ERROR 1
#define REPLICATION_THREAD_MI_INIT_ERROR 2
#define REPLICATION_THREAD_RLI_INIT_ERROR 3
#define REPLICATION_THREAD_START_ERROR 4
#define REPLICATION_THREAD_START_NO_INFO_ERROR 5
#define REPLICATION_THREAD_START_IO_NOT_CONNECTED 6
#define REPLICATION_THREAD_STOP_ERROR 7
#define REPLICATION_THREAD_STOP_RL_FLUSH_ERROR 8
#define REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR 9
#define REPLICATION_THREAD_REPOSITORY_MI_PURGE_ERROR 10


//Error for the wait event consuption, equal to the server wait for gtid method
#define REPLICATION_THREAD_WAIT_TIMEOUT_ERROR -1
#define REPLICATION_THREAD_WAIT_NO_INFO_ERROR -2


//Applier thread InnoDB priority
#define GCS_APPLIER_THREAD_PRIORITY 1


class Replication_thread_api
{

public:
  Replication_thread_api()
    :mi(NULL), rli(NULL), stop_wait_timeout(LONG_TIMEOUT) {};
  ~Replication_thread_api(){}

  /**
    Initilializes the master info and relay log info repositories setting some
    basic options.

    @param relay_log_name       the name for the relay logs and index files
    @param relay_log_info_name  the name for the relay log info file

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_REPOSITORY_CREATION_ERROR
        Error when creating the master info and relay log info coordinators
      @retval REPLICATION_THREAD_MI_INIT_ERROR
        Error when initializing the master info repository
      @retval REPLICATION_THREAD_RLI_INIT_ERROR
        Error when initializing the relay log info repository
  */
  int initialize_repositories(char* relay_log_name,
                              char* relay_log_info_name);

  /**
    Creates both a Master info and a Relay log info repository whose types are
    defined as parameters.

    @todo Make the repository a pluggable component.
    @todo Use generic programming to make it easier and clearer to
          add a new repositories' types and Rpl_info objects.

    @param[in]  mi_option  Type of the Master info repository.
    @param[out] mi         Reference to the Master_info.
    @param[in]  rli_option Type of the Relay log info repository.
    @param[out] rli        Reference to the Relay_log_info.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int create_coordinators(uint mi_option, Master_info **mi,
                          uint rli_option, Relay_log_info **rli);

  /**
    Initializes the connection related parameters for master connection.

    @note hostname and the master log name are set to dummy values if no value
          is passed
    @note the connection user and password are only set if not null
    @note the retry count is only set if a variable greater than 0 is passed

    @param hostname          the master's hostname
    @param port              the master's port
    @param user              the user used to connect to the master
    @param password          the password used to connect to the master
    @param master_log_name   the master log name to fetch
    @param retry_count       the retry count on connection to use
  */
  void initialize_connection_parameters(const string* hostname,
                                        uint port, char* user, char* password,
                                        char* master_log_name, int retry_count);
  /**
    Initializes the SQL thread until condition to wait for the given view id.

    @param view_id  the expected view id
  */
  void initialize_view_id_until_condition(const char* view_id);

  /**
    Start the SQL/IO threads according to the given thread mask option

    @param thread_mask          threads to start (SLAVE_SQL and/or SLAVE_IO)
    @param wait_for_connection  wait for the IO thread to connect

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_START_ERROR
        Error when launching on of the threads
      @retval REPLICATION_THREAD_START_NO_INFO_ERROR
        Error caused by a not present but needed master info repository
      @retval REPLICATION_THREAD_START_IO_NOT_CONNECTED
        Error when the threads start, but the IO thread cannot connect
  */
  int start_replication_threads(int thread_mask,
                                bool wait_for_connection);

  /**
    Purges the relay logs and clears the GTID retrieved

    @param just_reset  If true, the method will only purge the relay log and
                       delete the log files not initializing any new ones.
                       @see Relay_log_info::purge_relay_logs

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR
        Error when relay log purging fails before the thread starts
  */
  int purge_relay_logs(bool just_reset);

  /**
   Cleans the master info object

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_REPOSITORY_MI_PURGE_ERROR
        Error when clearing the master info repository
  */
  int purge_master_info();

  /**
    Stops the SQL/IO threads according to the given thread mask option.

    @note if no thread mask is given, the threads marked as running are stopped

    @param flush_relay_logs  if relay logs should be flushed after stop.
                             used when writing to a relay log without an IO thread
    @param thread_mask       the thread mask used to stop the threads
                             if not given, it is calculated

    @return the operation status
      @retval 0      OK
      @retval REPLICATION_THREAD_STOP_ERROR
        Error when stopping the threads. The threads probably stayed running.
      @retval REPLICATION_THREAD_STOP_RL_FLUSH_ERROR
        Error when flushing the relay logs.
  */
  int stop_threads(bool flush_relay_logs, int thread_mask= -1);

  /**
    Deletes the master and relay info repositories.
  */
  void clean_thread_repositories();

  /**
     Checks if the IO thread is running.
     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_io_thread_running();

  /**
     Checks if the SQL thread is running.
     @return the thread status
      @retval true      the thread is running
      @retval false     the thread is stopped
  */
  bool is_sql_thread_running();

  /**
    Queues a event packet into the current active relay log.

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
     Checks if the given id matches any of  the event applying threads
     @param id  the thread id

     @return if it belongs to a thread
       @retval true   the id matches a SQL or worker thread
       @retval false  the id doesn't match any thread
   */
  bool is_own_event_channel(my_thread_id id);

  /**
    Returns last GNO from applier relay log from a given UUID.

    @param sidno    the SIDNO of the cluster UUID, so that we get the
                    last GNO of cluster already certified transactions
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
  Master_info *mi;
  Relay_log_info *rli;
  ulong stop_wait_timeout;
};

#endif /* GCS_REPLICATION_THREADS_API_INCLUDE */
