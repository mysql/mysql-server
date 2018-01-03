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

#include "plugin/group_replication/include/replication_threads_api.h"

#include "my_dbug.h"

using std::string;

int
Replication_thread_api::initialize_channel(char* hostname,
                                           uint  port,
                                           char* user,
                                           char* password,
                                           bool  use_ssl,
                                           char* ssl_ca,
                                           char* ssl_capath,
                                           char* ssl_cert,
                                           char* ssl_cipher,
                                           char* ssl_key,
                                           char* ssl_crl,
                                           char* ssl_crlpath,
                                           bool  ssl_verify_server_cert,
                                           int   priority,
                                           int   retry_count,
                                           bool  preserve_logs,
                                           char* public_key_path,
                                           bool  get_public_key)
{
  DBUG_ENTER("Replication_thread_api::initialize");
  int error= 0;

  Channel_creation_info info;
  initialize_channel_creation_info(&info);
  Channel_ssl_info ssl_info;
  initialize_channel_ssl_info(&ssl_info);

  info.user= user;
  info.password= password;
  info.hostname= hostname;
  info.port= port;

  info.auto_position= true;
  info.replicate_same_server_id= true;
  if (priority == GROUP_REPLICATION_APPLIER_THREAD_PRIORITY)
  {
    info.thd_tx_priority= GROUP_REPLICATION_APPLIER_THREAD_PRIORITY;
  }
  info.type= GROUP_REPLICATION_CHANNEL;

  info.retry_count= retry_count;

  info.preserve_relay_logs= preserve_logs;

  if (public_key_path != NULL)
    info.public_key_path= public_key_path;

  info.get_public_key= get_public_key;

  if( use_ssl || ssl_ca != NULL || ssl_capath != NULL || ssl_cert != NULL ||
      ssl_cipher!= NULL || ssl_key != NULL || ssl_crl != NULL ||
      ssl_crlpath != NULL || ssl_verify_server_cert)
  {
    ssl_info.use_ssl= use_ssl;
    ssl_info.ssl_ca_file_name= ssl_ca;
    ssl_info.ssl_ca_directory= ssl_capath;
    ssl_info.ssl_cert_file_name= ssl_cert;
    ssl_info.ssl_cipher= ssl_cipher;
    ssl_info.ssl_key= ssl_key;
    ssl_info.ssl_crl_file_name= ssl_crl;
    ssl_info.ssl_crl_directory= ssl_crlpath;
    ssl_info.ssl_verify_server_cert= ssl_verify_server_cert;
    info.ssl_info= &ssl_info;
  }

  error= channel_create(interface_channel, &info);

  /*
    Flush relay log to indicate a new start.
  */
  if (!error)
    error= channel_flush(interface_channel);

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

int Replication_thread_api::purge_logs(bool reset_all)
{
  DBUG_ENTER("Replication_thread_api::purge_logs");

  //If there is no channel, no point in invoking the method
  if (!channel_is_active(interface_channel, CHANNEL_NO_THD))
      DBUG_RETURN(0);

  int error= channel_purge_queue(interface_channel, reset_all);

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

bool Replication_thread_api::is_receiver_thread_stopping()
{
  return(channel_is_stopping(interface_channel, CHANNEL_RECEIVER_THREAD));
}

bool Replication_thread_api::is_applier_thread_running()
{
  return(channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD));
}

bool Replication_thread_api::is_applier_thread_stopping()
{
  return(channel_is_stopping(interface_channel, CHANNEL_APPLIER_THREAD));
}

int
Replication_thread_api::queue_packet(const char* buf, ulong event_len)
{
  return channel_queue_packet(interface_channel, buf, event_len);
}

bool Replication_thread_api::is_applier_thread_waiting()
{
  return (channel_is_applier_waiting(interface_channel) == 1);
}

int
Replication_thread_api::wait_for_gtid_execution(double timeout)
{
  DBUG_ENTER("Replication_thread_api::wait_for_gtid_execution");

  int error= channel_wait_until_apply_queue_applied(interface_channel, timeout);

  /*
    Check that applier relay log is indeed consumed.
    This is different from channel_wait_until_apply_queue_applied()
    on the following case: if transactions on relay log are already
    on GTID_EXECUTED, applier thread still needs to read the relay
    log and update log positions. So despite transactions on relay
    log are applied, applier thread is still updating log positions
    on info tables.
  */
  if (!error)
  {
    if (channel_is_applier_waiting(interface_channel) != 1)
      error= REPLICATION_THREAD_WAIT_TIMEOUT_ERROR;
  }

  DBUG_RETURN(error);
}

rpl_gno
Replication_thread_api::get_last_delivered_gno(rpl_sidno sidno)
{
  DBUG_ENTER("Replication_thread_api::get_last_delivered_gno");
  DBUG_RETURN(channel_get_last_delivered_gno(interface_channel, sidno));
}

int Replication_thread_api::get_applier_thread_ids(unsigned long** thread_ids)
{
  DBUG_ENTER("Replication_thread_api::get_applier_thread_ids");
  DBUG_RETURN(channel_get_thread_id(interface_channel,
                                    CHANNEL_APPLIER_THREAD,
                                    thread_ids));
}

bool Replication_thread_api::is_own_event_applier(my_thread_id id,
                                                  const char* channel_name)
{
  DBUG_ENTER("Replication_thread_api::is_own_event_applier");

  bool result= false;
  unsigned long* thread_ids= NULL;
  const char* name= channel_name ? channel_name : interface_channel;

  //Fetch all applier thread ids for this channel.
  int number_appliers= channel_get_thread_id(name,
                                             CHANNEL_APPLIER_THREAD,
                                             &thread_ids);

  //If none are found return false
  if (number_appliers <= 0)
  {
    goto end;
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

end:
  my_free(thread_ids);

  //The given id is not an id of the channel applier threads, return false
  DBUG_RETURN(result);
}

bool Replication_thread_api::is_own_event_receiver(my_thread_id id)
{
  DBUG_ENTER("Replication_thread_api::is_own_event_receiver");

  bool result= false;
  unsigned long* thread_id= NULL;

  //Fetch the receiver thread id for this channel
  int number_receivers= channel_get_thread_id(interface_channel,
                                              CHANNEL_RECEIVER_THREAD,
                                              &thread_id);

  //If one is found
  if (number_receivers > 0)
  {
    result= (*thread_id == id);
  }
  my_free(thread_id);

  //The given id is not the id of the channel receiver thread, return false
  DBUG_RETURN(result);
}

bool Replication_thread_api::get_retrieved_gtid_set(std::string& retrieved_set,
                                                    const char* channel_name)
{
  DBUG_ENTER("Replication_thread_api::get_retrieved_gtid_set");

  const char* name= channel_name ? channel_name : interface_channel;
  char *receiver_retrieved_gtid_set= NULL;
  int error;

  error= channel_get_retrieved_gtid_set(name,
                                        &receiver_retrieved_gtid_set);
  if (!error)
    retrieved_set.assign(receiver_retrieved_gtid_set);

  my_free(receiver_retrieved_gtid_set);

  DBUG_RETURN((error != 0));
}

bool Replication_thread_api::is_partial_transaction_on_relay_log()
{
  return is_partial_transaction_on_channel_relay_log(interface_channel);
}
