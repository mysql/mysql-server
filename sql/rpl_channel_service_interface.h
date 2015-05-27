/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_SERVICE_INTERFACE_INCLUDE
#define RPL_SERVICE_INTERFACE_INCLUDE

//Channel errors

#define RPL_CHANNEL_SERVICE_RECEIVER_CONNECTION_ERROR      -1
#define RPL_CHANNEL_SERVICE_DEFAULT_CHANNEL_CREATION_ERROR -2
#define RPL_CHANNEL_SERVICE_SLAVE_SKIP_COUNTER_ACTIVE      -3
#define RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR  -4
//Error for the wait event consumption, equal to the server wait for GTID method
#define REPLICATION_THREAD_WAIT_TIMEOUT_ERROR -1
#define REPLICATION_THREAD_WAIT_NO_INFO_ERROR -2

//Settings

//Used whenever a parameter should take the server default value
#define RPL_SERVICE_SERVER_DEFAULT -1

//Channel creation settings

/**
  Types of channels
*/
enum enum_channel_type
{
  SLAVE_REPLICATION_CHANNEL,  //Master slave replication channels
  GROUP_REPLICATION_CHANNEL   //Group replication channels
};

/**
  Know parallelization options that can be applied to channel appliers
*/
enum enum_multi_threaded_workers_type
{
  CHANNEL_MTS_PARALLEL_TYPE_DB_NAME,
  CHANNEL_MTS_PARALLEL_TYPE_LOGICAL_CLOCK
};

/**
 Creation information for a channel.
 It includes the data that is usually associated to a change master command
*/
struct st_channel_info
{
  enum_channel_type type;
  char* hostname;
  int port;
  char* user;
  char* password;
  int auto_position;
  int channel_mts_parallel_type;
  int channel_mts_parallel_workers;
  int channel_mts_checkpoint_group;
  int replicate_same_server_id;
  int thd_tx_priority;           //The applier thread priority
  int sql_delay;
  int connect_retry;             //How many seconds to wait between retries.
  int retry_count;               //Limits the number of reconnection attempts
  bool preserve_relay_logs;      //If the logs should be preserved on creation
};
typedef struct st_channel_info Channel_creation_info;

void initialize_channel_creation_info(Channel_creation_info* channel_info);

//Start settings

/**
  The known types of channel threads.
  All new types should be power of 2
*/
enum enum_channel_thread_types
{
  CHANNEL_NO_THD=0,
  CHANNEL_RECEIVER_THREAD=1,
  CHANNEL_APPLIER_THREAD=2
};

/**
  The known until conditions that can be applied to channels
*/
enum enum_channel_until_condition
{
  CHANNEL_NO_UNTIL_CONDITION,
  CHANNEL_UNTIL_APPLIER_BEFORE_GTIDS,
  CHANNEL_UNTIL_APPLIER_AFTER_GTIDS,
  CHANNEL_UNTIL_APPLIER_AFTER_GAPS,
  CHANNEL_UNTIL_VIEW_ID
};

/**
  Channel information to connect to a receiver
*/
struct st_channel_connection_info
{
  int until_condition; //base on enum_channel_until_condition
  char* gtid;          //Gtids to wait on a until condition
  char* view_id;       //The view id to wait on a until condition
};

typedef struct st_channel_connection_info Channel_connection_info;

void
initialize_channel_connection_info(Channel_connection_info* channel_info);

/**
  Initializes a channel connection in a similar way to a change master command.

  @note If the channel exists, it is reconfigured with the new options.
        About the logs, the preserve_relay_logs option allows the user to
        maintain them untouched.

  @param channel              The channel name
  @param channel_information  Channel creation information.

  @return the operation status
    @retval 0      OK
    @retval !=0    Error on channel creation
*/
int channel_create(const char* channel,
                   Channel_creation_info* channel_information);

/**
  Start the Applier/Receiver threads according to the given options.
  If the receiver thread is to be started, connection credential must be
  supported.

  @param channel              The channel name
  @param connection_info      Channel connection information
  @param threads_to_start     The types of threads to be started
  @param wait_for_connection  If when starting the receiver, the method should
                              wait for the connection to succeed

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
 */
int channel_start(const char* channel,
                  Channel_connection_info* connection_info,
                  int threads_to_start,
                  int wait_for_connection);

/**
  Stops the channel threads according to the given options.

  @param channel              The channel name
  @param threads_to_stop      The types of threads to be stopped
  @param timeout              The expected time in which the thread should stop
  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int channel_stop(const char* channel,
                 int threads_to_stop,
                 long timeout);

/**
  Purges the channel logs

  @param reset_all  If true, the method will purge logs and remove the channel
                    If false, only the channel information will be reset.

  @return the operation status
    @retval 0      OK
    @retval !=0    Error
*/
int channel_purge_queue(const char* channel, bool reset_all);

/**
  Tells if the selected component of the channel is active or not.
  If no component is passed, this method returns if the channel exists or not

  @param channel  The channel name
  @param type     The thread that should be checked.
                  If 0, this method applies to the channel existence.

  @return is the channel (component) active
    @retval true    Yes
    @retval false   No
*/
bool channel_is_active(const char* channel, enum_channel_thread_types type);

/**
  Returns the ids of the channel appliers.
  If more than one applier exists, a channel is returned

  @param[in]  channel      The channel name
  @param[out] appliers_id  The array of id(s)

  @return the number of returned ids
    @retval <=0  the channel does no exists, or the applier is not present
    @retval >0 the number of applier ids returned.
*/
int channel_get_appliers_thread_id(const char* channel,
                                   unsigned long** appliers_id);

/**
  Returns last GNO from applier from a given UUID.

  @param channel the channel name
  @param sidno   the uuid associated to the desired gno

  @return the last applier gno
    @retval <0 the channel does no exists, or the applier is not present
    @retval >0 the gno
*/
long long channel_get_last_delivered_gno(const char* channel, int sidno);

/**
  Queues a event packet into the current active channel.

  @param buf         the event buffer
  @param event_len  the event buffer length

  @return the operation status
    @retval 0      OK
    @retval != 0   Error on queue
*/
int channel_queue_packet(const char* channel, const char* buf, unsigned long len);

/**
  Checks if all the queued transactions were executed.

  @note This method assumes that the channel is not receiving any more events.
        If it is still receiving, then the method should wait for execution of
        transactions that were present when this method was invoked.

  @param timeout  the time (seconds) after which the method returns if the
                  above condition was not satisfied

  @return the operation status
    @retval 0   All transactions were executed
    @retval REPLICATION_THREAD_WAIT_TIMEOUT_ERROR     A timeout occurred
    @retval REPLICATION_THREAD_WAIT_NO_INFO_ERROR     An error occurred
*/
int channel_wait_until_apply_queue_empty(char* channel, long long timeout);

/**
  Initializes channel structures if needed.

  @return the operation status
    @retval 0      OK
    @retval != 0   Error on queue
*/
int initialize_channel_service_interface();

#endif //RPL_SERVICE_INTERFACE_INCLUDE