/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#include <gcs_replication.h>
#include <mysqld.h>
#include <log.h>
#include <rpl_info_factory.h>
#include <rpl_slave.h>

/* Maps a sequence number to it's thread_id. */
typedef std::map<my_thread_id, std::pair<rpl_sidno, rpl_gno> > seq_num_map;
/* A Map that maps, a thread_id to a sequence number. */
seq_num_map thread_to_seq_num_map;
/* An iterator to traverse the map. */
seq_num_map::iterator seq_num_iterator;

Gcs_replication_handler::Gcs_replication_handler() :
  plugin(NULL), plugin_handle(NULL)
{
  plugin_name.str= (char*) "gcs_replication_plugin";
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

bool Gcs_replication_handler::get_gcs_stats_info(RPL_GCS_STATS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_gcs_stats_info(info);
  return true;
}

bool Gcs_replication_handler::get_gcs_nodes_info(RPL_GCS_NODES_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_gcs_nodes_info(info);
  return true;
}

bool Gcs_replication_handler::is_gcs_rpl_running()
{
  if (plugin_handle)
    return plugin_handle->is_gcs_rpl_running();
  return false;
}

int Gcs_replication_handler::gcs_init()
{
  plugin= my_plugin_lock_by_name(0, &plugin_name, MYSQL_GCS_RPL_PLUGIN);
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

int add_transaction_certification_result(my_thread_id thread_id,
                                         rpl_gno seq_num,
                                         rpl_sidno cluster_id)
{
  DBUG_ENTER("add_transaction_certification_result");
  mysql_mutex_lock(&LOCK_seq_num_map);

  std::pair <rpl_sidno, rpl_gno> temp= std::make_pair (cluster_id, seq_num);
  std::pair<std::map<my_thread_id, std::pair <rpl_sidno, rpl_gno> >::iterator,bool> ret;
  ret= thread_to_seq_num_map.insert(std::pair<my_thread_id, std::pair <rpl_sidno, rpl_gno> >(thread_id, temp));

  mysql_mutex_unlock(&LOCK_seq_num_map);
  // If a map insert fails, ret.second is false
  if(ret.second)
    DBUG_RETURN(0);
  DBUG_RETURN(1);
}

std::pair <rpl_sidno, rpl_gno>
get_transaction_certification_result(my_thread_id thread_id)
{
  DBUG_ENTER("get_transaction_certification_result");

  seq_num_map::const_iterator iterator;
  mysql_mutex_lock(&LOCK_seq_num_map);

  iterator= thread_to_seq_num_map.find(thread_id);
  if (iterator == thread_to_seq_num_map.end())
  {
    mysql_mutex_unlock(&LOCK_seq_num_map);
    DBUG_RETURN(std::make_pair(-1, -1));
  }
  mysql_mutex_unlock(&LOCK_seq_num_map);
  DBUG_RETURN(iterator->second);
}

void delete_transaction_certification_result(my_thread_id thread_id)
{
  DBUG_ENTER("delete_transaction_certification_result");

  seq_num_map::iterator iterator;
  mysql_mutex_lock(&LOCK_seq_num_map);
  iterator= thread_to_seq_num_map.find(thread_id);
  if (iterator == thread_to_seq_num_map.end())
  {
    mysql_mutex_unlock(&LOCK_seq_num_map);
    DBUG_VOID_RETURN;
  }
  thread_to_seq_num_map.erase(iterator);
  mysql_mutex_unlock(&LOCK_seq_num_map);
  DBUG_VOID_RETURN;
}

Gcs_replication_handler* gcs_rpl_handler= NULL;

int init_gcs_rpl()
{
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

bool get_gcs_stats(RPL_GCS_STATS_INFO *info)
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_stats_info(info);
  return true;
}

bool get_gcs_nodes_stats(RPL_GCS_NODES_INFO *info)
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->get_gcs_nodes_info(info);
  return true;
}

bool is_running_gcs_rpl()
{
  if (gcs_rpl_handler)
    return gcs_rpl_handler->is_gcs_rpl_running();
  return false;
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

char *set_relay_log_info_name(char* name){
  char *original_relay_info_file= relay_log_info_file;
  relay_log_info_file=  name;
  return original_relay_info_file;
}

void get_server_host_port_uuid(char **hostname, uint *port, char** uuid)
{
  *hostname= glob_hostname;
  *port= mysqld_port;
  *uuid= server_uuid;
  return;
}
