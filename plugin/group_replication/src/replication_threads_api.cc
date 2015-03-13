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

#include "replication_threads_api.h"

using std::string;

int
Replication_thread_api::initialize_channel(char* hostname,
                                           uint port,
                                           char* user,
                                           char* password,
                                           int priority,
                                           bool disable_mts,
                                           int retry_count,
                                           bool preserve_logs)
{
  DBUG_ENTER("Replication_thread_api::initialize");
  int error= 0;

  Channel_creation_info info;
  initialize_channel_creation_info(&info);

  info.user= user;
  info.password= password;
  info.hostname= hostname;
  info.port= port;

  info.auto_position= true;
  if (priority == GROUP_REPLICATION_APPLIER_THREAD_PRIORITY)
  {
    info.replicate_same_server_id= true;
    info.thd_tx_priority= GROUP_REPLICATION_APPLIER_THREAD_PRIORITY;
  }
  info.type= GROUP_REPLICATION_CHANNEL;

  if (disable_mts)
  {
    info.channel_mts_parallel_workers= 0;
  }

  info.retry_count= retry_count;

  info.preserve_relay_logs= preserve_logs;

  error= channel_create(interface_channel, &info);

  DBUG_RETURN(error);

}

int
Replication_thread_api::start_threads(bool start_receiver,
                                      bool start_applier,
                                      string* view_id,
                                      bool wait_for_connection)
{
  DBUG_ENTER("Replication_thread_api::start_threads");

  Channel_connection_info info;
  initialize_channel_connection_info(&info);

  char* cview_id= NULL;

  if (view_id)
  {
    cview_id= new char[view_id->size() + 1];
    memcpy(cview_id, view_id->c_str(), view_id->size() + 1);

    info.until_condition= CHANNEL_UNTIL_VIEW_ID;
    info.view_id= cview_id;
  }

  int thread_mask= 0;
  if (start_applier)
  {
    thread_mask |= CHANNEL_APPLIER_THREAD;
  }
  if (start_receiver)
  {
    thread_mask |= CHANNEL_RECEIVER_THREAD;
  }

  int error= channel_start(interface_channel,
                           &info,
                           thread_mask,
                           wait_for_connection);

  if (view_id)
  {
    delete [] cview_id;
  }

  DBUG_RETURN(error);
}

int Replication_thread_api::purge_logs()
{
  DBUG_ENTER("Replication_thread_api::purge_logs");

  //If there is no channel, no point in invoking the method
  if (!channel_is_active(interface_channel, CHANNEL_NO_THD))
      DBUG_RETURN(0);

  int error= channel_purge_queue(interface_channel, true);

  DBUG_RETURN(error);
}

int Replication_thread_api::stop_threads(bool stop_receiver, bool stop_applier)
{
  DBUG_ENTER("Replication_thread_api::stop_threads");

  stop_receiver= stop_receiver && is_receiver_thread_running();
  stop_applier= stop_applier && is_applier_thread_running();

  //If there is nothing to do, return 0
  if (!stop_applier && !stop_receiver)
    DBUG_RETURN(0);

  int thread_mask= 0;
  if (stop_applier)
  {
    thread_mask |= CHANNEL_APPLIER_THREAD;
  }
  if (stop_receiver)
  {
    thread_mask |= CHANNEL_RECEIVER_THREAD;
  }

  int error= channel_stop(interface_channel,
                          thread_mask,
                          stop_wait_timeout);

  DBUG_RETURN(error);
}

bool Replication_thread_api::is_receiver_thread_running()
{
  return(channel_is_active(interface_channel, CHANNEL_RECEIVER_THREAD));
}

bool Replication_thread_api::is_applier_thread_running()
{
  return(channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD));
}

int
Replication_thread_api::queue_packet(const char* buf, ulong event_len)
{
  return channel_queue_packet(interface_channel, buf, event_len);
}

int
Replication_thread_api::wait_for_gtid_execution(longlong timeout)
{
  DBUG_ENTER("Replication_thread_api::wait_for_gtid_execution");

  int error= channel_wait_until_apply_queue_empty(interface_channel, timeout);

  DBUG_RETURN(error);
}

rpl_gno
Replication_thread_api::get_last_delivered_gno(rpl_sidno sidno)
{
  DBUG_ENTER("Replication_thread_api::get_last_delivered_gno");
  DBUG_RETURN(channel_get_last_delivered_gno(interface_channel, sidno));
}

bool Replication_thread_api::is_own_event_applier(my_thread_id id)
{
  DBUG_ENTER("Replication_thread_api::is_own_event_channel");

  bool result= false;
  unsigned long* thread_ids= NULL;

  //Fetch all applier thread ids for this channel
  int number_appliers= channel_get_appliers_thread_id(interface_channel,
                                                      &thread_ids);

  //If none are found return false
  if (number_appliers <= 0)
  {
    DBUG_RETURN(result);
  }

  if (number_appliers == 1)  //One applier, check its id
  {
    result= (*thread_ids == id);
  }
  else //The channel has  more than one applier, check if the id is in the list
  {
    for (int i = 0; i < number_appliers; i++)
    {
      unsigned long thread_id= thread_ids[i];
      if (thread_id == id)
      {
        result= true;
        break;
      }
    }
  }

  my_free(thread_ids);

  //The given id is not an id of the channel applier threads, return false
  DBUG_RETURN(result);
}
