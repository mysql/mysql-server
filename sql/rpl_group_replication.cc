/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_group_replication.h"
#include "rpl_channel_service_interface.h"
#include "rpl_info_factory.h"
#include "log.h"
#include "mysqld_thd_manager.h"
#include "mysqld.h"                             // glob_hostname mysqld_port ..


/*
  Group Replication plugin handler.
*/
Group_replication_handler::
Group_replication_handler(const char* plugin_name_arg)
  :plugin(NULL), plugin_handle(NULL)
{
  plugin_name.assign(plugin_name_arg);
}

Group_replication_handler::~Group_replication_handler()
{
  if (plugin_handle)
    plugin_handle->stop();
}

int Group_replication_handler::init()
{
  int error= 0;
  if (!plugin_handle)
    if ((error = plugin_init()))
      return error;
  return 0;
}

int Group_replication_handler::start()
{
  if (plugin_handle)
    return plugin_handle->start();
  return 1;
}

int Group_replication_handler::stop()
{
  if (plugin_handle)
    return plugin_handle->stop();
  return 1;
}

bool Group_replication_handler::is_running()
{
  if (plugin_handle)
    return plugin_handle->is_running();
  return false;
}

int
Group_replication_handler::
set_retrieved_certification_info(View_change_log_event* view_change_event)
{
  if (plugin_handle)
    return plugin_handle->set_retrieved_certification_info(view_change_event);
  return 1;
}

bool
Group_replication_handler::
get_connection_status_info(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_connection_status_info(info);
  return true;
}

bool
Group_replication_handler::
get_group_members_info(unsigned int index, GROUP_REPLICATION_GROUP_MEMBERS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_group_members_info(index, info);
  return true;
}

bool
Group_replication_handler::
get_group_member_stats_info(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO *info)
{
  if (plugin_handle)
    return plugin_handle->get_group_member_stats_info(info);
  return true;
}

unsigned int Group_replication_handler::get_members_number_info()
{
  if (plugin_handle)
    return plugin_handle->get_members_number_info();
  return 0;
}

int Group_replication_handler::plugin_init()
{
  plugin= my_plugin_lock_by_name(0, to_lex_cstring(plugin_name.c_str()),
                                 MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin)
  {
    plugin_handle= (st_mysql_group_replication*) plugin_decl(plugin)->info;
    plugin_unlock(0, plugin);
  }
  else
  {
    plugin_handle= NULL;
    return 1;
  }
  return 0;
}

Group_replication_handler* group_replication_handler= NULL;


/*
  Group Replication plugin handler function accessors.
*/
int group_replication_init(const char* plugin_name)
{
  intialize_channel_service_interface();

  if (group_replication_handler != NULL)
    return 1;

  group_replication_handler= new Group_replication_handler(plugin_name);

  if (group_replication_handler)
    return group_replication_handler->init();
  return 1;
}

int group_replication_cleanup()
{
  if (!group_replication_handler)
    return 0;

  delete group_replication_handler;
  group_replication_handler= NULL;
  return 0;
}

bool is_group_replication_plugin_loaded()
{
  if (group_replication_handler)
    return true;
  return false;
}

int group_replication_start()
{
  if (group_replication_handler)
  {
    /*
      We need to take global_sid_lock because
      group_replication_handler->start function will (among other
      things) do the following:

       1. Call get_server_startup_prerequirements, which calls get_gtid_mode.
       2. Set plugin-internal state that ensures that
          is_group_replication_running() returns true.

      In order to prevent a concurrent client from executing SET
      GTID_MODE=ON_PERMISSIVE between 1 and 2, we must hold
      gtid_mode_lock.
    */
    gtid_mode_lock->rdlock();
    int ret= group_replication_handler->start();
    gtid_mode_lock->unlock();
    return ret;
  }
  return 1;
}

int group_replication_stop()
{
  if (group_replication_handler)
   return group_replication_handler->stop();
  return 1;
}

bool is_group_replication_running()
{
  if (group_replication_handler)
    return group_replication_handler->is_running();
  return false;
}

int set_group_replication_retrieved_certification_info(View_change_log_event *view_change_event)
{
  if (group_replication_handler)
    return group_replication_handler->set_retrieved_certification_info(view_change_event);
  return 1;
}

bool get_group_replication_connection_status_info(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info)
{
  if (group_replication_handler)
    return group_replication_handler->get_connection_status_info(info);
  return true;
}

bool get_group_replication_group_members_info(unsigned int index, GROUP_REPLICATION_GROUP_MEMBERS_INFO *info)
{
  if (group_replication_handler)
    return group_replication_handler->get_group_members_info(index, info);
  return true;
}

bool get_group_replication_group_member_stats_info(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO* info)
{
  if (group_replication_handler)
    return group_replication_handler->get_group_member_stats_info(info);
  return true;
}

unsigned int get_group_replication_members_number_info()
{
  if (group_replication_handler)
    return group_replication_handler->get_members_number_info();
  return 0;
}


/*
  Server methods exported to plugin through
  include/mysql/group_replication_priv.h
*/

bool is_server_engine_ready()
{
  return (tc_log != NULL);
}

void get_server_host_port_uuid(char **hostname, uint *port, char** uuid)
{
  *hostname= glob_hostname;
  *port= mysqld_port;
  *uuid= server_uuid;
  return;
}

#ifdef HAVE_REPLICATION
void
get_server_startup_prerequirements(Trans_context_info& requirements,
                                   bool has_lock)
{
  requirements.binlog_enabled= opt_bin_log;
  requirements.binlog_format= global_system_variables.binlog_format;
  requirements.binlog_checksum_options= binlog_checksum_options;
  requirements.gtid_mode=
    get_gtid_mode(has_lock ? GTID_MODE_LOCK_GTID_MODE :
                  GTID_MODE_LOCK_NONE);
  requirements.transaction_write_set_extraction=
    global_system_variables.transaction_write_set_extraction;
  requirements.mi_repository_type= opt_mi_repository_id;
  requirements.rli_repository_type= opt_rli_repository_id;
}
#endif //HAVE_REPLICATION

bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      uint *length)
{
  global_sid_lock->wrlock();

  DBUG_ASSERT(get_gtid_mode(GTID_MODE_LOCK_SID) > 0);

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


void global_thd_manager_add_thd(THD *thd)
{
  Global_THD_manager::get_instance()->add_thd(thd);
}


void global_thd_manager_remove_thd(THD *thd)
{
  Global_THD_manager::get_instance()->remove_thd(thd);
}
