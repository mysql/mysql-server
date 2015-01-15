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

#include "my_global.h"
#include <rpl_info_factory.h>
#include <gcs_replication.h>
#include <mysqld.h>
#include <log.h>
#include <rpl_slave.h>
#include "rpl_channel_service_interface.h"

Gcs_replication_handler::Gcs_replication_handler() :
  plugin(NULL), plugin_handle(NULL)
{
  plugin_name.str= "gcs_replication_plugin";
  plugin_name.length= 22;
}

Gcs_replication_handler::~Gcs_replication_handler()
{
  if (plugin_handle)
    plugin_handle->gcs_rpl_stop();
}

int Gcs_replication_handler::gcs_handler_init()
{
  int error= 0;
  if (!plugin_handle)
    if ((error = gcs_init()))
      return error;
  return 0;
}

int Gcs_replication_handler::gcs_rpl_start()
{
  if (plugin_handle)
    return plugin_handle->gcs_rpl_start();
  return 1;
}

int Gcs_replication_handler::gcs_rpl_stop()
{
  if (plugin_handle)
    return plugin_handle->gcs_rpl_stop();
  return 1;
}

bool
Gcs_replication_handler::
get_gcs_connection_status(RPL_GCS_CONNECTION_STATUS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_gcs_connection_status_info(info);
  return true;
}

bool
Gcs_replication_handler::
get_gcs_group_members(uint index, RPL_GCS_GROUP_MEMBERS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_gcs_group_members_info(index, info);
  return true;
}

bool
Gcs_replication_handler::
get_gcs_group_member_stats(RPL_GCS_GROUP_MEMBER_STATS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_gcs_group_member_stats_info(info);
  return true;
}


uint Gcs_replication_handler::get_gcs_number_of_members()
{
  if (plugin_handle)
    return plugin_handle->get_gcs_members_number_info();
  return 0;
}

bool Gcs_replication_handler::is_gcs_rpl_running()
{
  if (plugin_handle)
    return plugin_handle->is_gcs_rpl_running();
  return false;
}

int Gcs_replication_handler::gcs_set_retrieved_cert_info(View_change_log_event* view_change_event)
{
  if (plugin_handle)
    return plugin_handle->gcs_set_retrieved_cert_info(view_change_event);
  return 1;
}

int Gcs_replication_handler::gcs_init()
{
  plugin= my_plugin_lock_by_name(0, plugin_name, MYSQL_GCS_RPL_PLUGIN);
  if (plugin)
  {
    plugin_handle= (st_mysql_gcs_rpl *) plugin_decl(plugin)->info;
    plugin_unlock(0, plugin);
  }
  else
  {
    plugin_handle= NULL;
    return 1;
  }
  return 0;
}

Gcs_replication_handler* gcs_rpl_handler= NULL;

int init_gcs_rpl()
{
  intialize_channel_service_interface();

  if (gcs_rpl_handler != NULL)
    return 1;

  gcs_rpl_handler= new Gcs_replication_handler();

  if (gcs_rpl_handler)
    return gcs_rpl_handler->gcs_handler_init();
  return 1;
}

int start_gcs_rpl()
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->gcs_rpl_start();
  return 1;
}

int stop_gcs_rpl()
{
  if (gcs_rpl_handler)
   return gcs_rpl_handler->gcs_rpl_stop();
  return 1;
}

bool get_gcs_connection_status_info(RPL_GCS_CONNECTION_STATUS_INFO *info)
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_connection_status(info);
  return true;
}

bool get_gcs_group_members_info(uint index, RPL_GCS_GROUP_MEMBERS_INFO *info)
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_group_members(index, info);
  return true;
}

bool get_gcs_group_member_stats_info(RPL_GCS_GROUP_MEMBER_STATS_INFO* info)
{
  if(gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_group_member_stats(info);
  return true;
}

uint get_gcs_members_number_info()
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_number_of_members();
  return 0;
}

bool is_running_gcs_rpl()
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->is_gcs_rpl_running();
  return false;
}

int set_gcs_retrieved_cert_info(View_change_log_event* view_change_event)
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->gcs_set_retrieved_cert_info(view_change_event);
  return 1;
}

int cleanup_gcs_rpl()
{
  if(!gcs_rpl_handler)
    return 0;

  delete gcs_rpl_handler;
  gcs_rpl_handler= NULL;
  return 0;
}

bool is_gcs_plugin_loaded()
{
  if (gcs_rpl_handler)
    return true;
  return false;
}

/* Server access methods  */

bool is_server_engine_ready()
{
  return (tc_log != NULL);
}

uint get_opt_mts_checkpoint_group()
{
  return opt_mts_checkpoint_group;
}

ulong get_opt_mts_slave_parallel_workers()
{
  return opt_mts_slave_parallel_workers;
}

ulong get_opt_rli_repository_id()
{
  return opt_rli_repository_id;
}

char *set_relay_log_name(char* name){
  char *original_relaylog_name= opt_relay_logname;
  opt_relay_logname= name;
  return original_relaylog_name;
}

char *set_relay_log_index_name(char* name){
  char *original_relaylog_index_name= opt_relaylog_index_name;
  opt_relaylog_index_name= name;
  return original_relaylog_index_name;
}

#ifdef HAVE_REPLICATION

char *set_relay_log_info_name(char* name){
  char *original_relay_info_file= relay_log_info_file;
  relay_log_info_file= name;

  Rpl_info_factory::init_relay_log_file_metadata();

  return original_relay_info_file;
}

#endif

void get_server_host_port_uuid(char **hostname, uint *port, char** uuid)
{
  *hostname= glob_hostname;
  *port= mysqld_port;
  *uuid= server_uuid;
  return;
}

#ifdef HAVE_REPLICATION
void
get_server_startup_prerequirements(Trans_context_info& requirements)
{
  requirements.binlog_enabled= opt_bin_log;
  requirements.binlog_format= global_system_variables.binlog_format;
  requirements.binlog_checksum_options= binlog_checksum_options;
  requirements.gtid_mode= gtid_mode;
  requirements.transaction_write_set_extraction=
    global_system_variables.transaction_write_set_extraction;
  requirements.mi_repository_type= opt_mi_repository_id;
  requirements.rli_repository_type= opt_rli_repository_id;
}
#endif //HAVE_REPLICATION

bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      uint *length)
{
  DBUG_ASSERT(gtid_mode > 0);

  global_sid_lock->wrlock();
  const Gtid_set *executed_gtids= gtid_state->get_executed_gtids();
  *length= executed_gtids->get_encoded_length();
  *encoded_gtid_executed= (uchar*) my_malloc(key_memory_Gtid_set_to_string,
                                             *length, MYF(MY_WME));
  if (*encoded_gtid_executed == NULL)
  {
    global_sid_lock->unlock();
    return true;
  }

  executed_gtids->encode(*encoded_gtid_executed);
  global_sid_lock->unlock();
  return false;
}

#if !defined(DBUG_OFF)
char* encoded_gtid_set_to_string(uchar *encoded_gtid_set,
                                 uint length)
{
  /* No sid_lock because this is a completely local object. */
  Sid_map sid_map(NULL);
  Gtid_set set(&sid_map);

  if (set.add_gtid_encoding(encoded_gtid_set, length) !=
      RETURN_STATUS_OK)
    return NULL;

  return set.to_string();
}
#endif
