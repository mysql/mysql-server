/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "rpl_slave.h"
#include "tc_log.h"
#include "mysqld_thd_manager.h"
#include "log.h"


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
get_connection_status_info(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks)
{
  if (plugin_handle)
    return plugin_handle->get_connection_status_info(callbacks);
  return true;
}

bool
Group_replication_handler::
get_group_members_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks)
{
  if (plugin_handle)
    return plugin_handle->get_group_members_info(index, callbacks);
  return true;
}

bool
Group_replication_handler::
get_group_member_stats_info(
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks)
{
  if (plugin_handle)
    return plugin_handle->get_group_member_stats_info(callbacks);
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
#ifdef HAVE_REPLICATION
int group_replication_init(const char* plugin_name)
{
  if (initialize_channel_service_interface())
  {
    return 1;
  }

  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (group_replication_handler == NULL)
  {
    group_replication_handler= new Group_replication_handler(plugin_name);

    if (group_replication_handler)
    {
      int ret= group_replication_handler->init();
      mysql_mutex_unlock(&LOCK_group_replication_handler);
      return ret;
    }
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return 1;
}
#endif

int group_replication_cleanup()
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (group_replication_handler != NULL)
  {
    delete group_replication_handler;
    group_replication_handler= NULL;
  }
  else
  {
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return 1;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

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
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
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
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);
  sql_print_error("Group Replication plugin is not installed.");
  return 1;
}

int group_replication_stop()
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    int ret= group_replication_handler->stop();
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  sql_print_error("Group Replication plugin is not installed.");
  return 1;
}

bool is_group_replication_running()
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    bool ret= group_replication_handler->is_running();
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return false;
}

int set_group_replication_retrieved_certification_info(View_change_log_event *view_change_event)
{
  if (is_group_replication_plugin_loaded())
    return group_replication_handler->set_retrieved_certification_info(view_change_event);
  return 1;
}

bool get_group_replication_connection_status_info(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks)
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    int ret= group_replication_handler->get_connection_status_info(callbacks);
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return true;
}

bool get_group_replication_group_members_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks)
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    bool ret=
      group_replication_handler->get_group_members_info(index, callbacks);
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return true;
}

bool get_group_replication_group_member_stats_info(
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks)
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    bool ret= group_replication_handler->get_group_member_stats_info(callbacks);
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return true;
}

unsigned int get_group_replication_members_number_info()
{
  mysql_mutex_lock(&LOCK_group_replication_handler);
  if (is_group_replication_plugin_loaded())
  {
    unsigned int ret= group_replication_handler->get_members_number_info();
    mysql_mutex_unlock(&LOCK_group_replication_handler);
    return ret;
  }
  mysql_mutex_unlock(&LOCK_group_replication_handler);

  return 0;
}



/*
  Server methods exported to plugin through
  include/mysql/group_replication_priv.h
*/
#ifdef HAVE_REPLICATION
void get_server_parameters(char **hostname, uint *port, char** uuid,
                           unsigned int *out_server_version,
                           st_server_ssl_variables* server_ssl_variables)
  {
  /*
    use startup option report-host and report-port when provided,
    as value provided by glob_hostname, which used gethostname() function
    internally to determine hostname, will not always provide correct
    network interface, especially in case of multiple network interfaces.
  */
  if (report_host)
    *hostname= report_host;
  else
    *hostname= glob_hostname;

  if (report_port)
    *port= report_port;
  else
    *port= mysqld_port;

  *uuid= server_uuid;

  //Convert server version to hex

  ulong major= 0, minor= 0, patch= 0;
  char *pos= server_version, *end_pos;
  //extract each server decimal number, e.g., for 5.9.30 -> 5, 9 and 30
  major= strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  minor= strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  patch= strtoul(pos, &end_pos, 10);

  /*
    Convert to a equivalent hex representation.
    5.9.30 -> 0x050930
    version= 0 x 16^5 + 5 x 16^4 + 0 x 16^3 + 9 x 16^2 + 3 x 16^1 + 0 x 16^0
  */
  int v1= patch / 10;
  int v0= patch - v1 * 10;
  int v3= minor / 10;
  int v2= minor - v3 * 10;
  int v5= major / 10;
  int v4= major - v5 * 10;

  *out_server_version= v0 + v1 * 16 + v2 * 256 + v3 * 4096 + v4 * 65536 + v5 * 1048576;

  server_ssl_variables->have_ssl_opt= (have_ssl == SHOW_OPTION_YES);
  server_ssl_variables->ssl_ca= opt_ssl_ca;
  server_ssl_variables->ssl_capath= opt_ssl_capath;
  server_ssl_variables->tls_version= opt_tls_version;
  server_ssl_variables->ssl_cert= opt_ssl_cert;
  server_ssl_variables->ssl_cipher= opt_ssl_cipher;
  server_ssl_variables->ssl_key= opt_ssl_key;
  server_ssl_variables->ssl_crl= opt_ssl_crl;
  server_ssl_variables->ssl_crlpath= opt_ssl_crlpath;

  return;
}
#endif

ulong get_server_id()
{
  return server_id;
}

ulong get_auto_increment_increment()
{
  return global_system_variables.auto_increment_increment;
}

ulong get_auto_increment_offset()
{
  return global_system_variables.auto_increment_offset;
}

void set_auto_increment_increment(ulong auto_increment_increment)
{
  global_system_variables.auto_increment_increment= auto_increment_increment;
}

void set_auto_increment_offset(ulong auto_increment_offset)
{
  global_system_variables.auto_increment_offset= auto_increment_offset;
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
  requirements.log_slave_updates= opt_log_slave_updates;
  requirements.transaction_write_set_extraction=
    global_system_variables.transaction_write_set_extraction;
  requirements.mi_repository_type= opt_mi_repository_id;
  requirements.rli_repository_type= opt_rli_repository_id;
  requirements.parallel_applier_type= mts_parallel_option;
  requirements.parallel_applier_workers= opt_mts_slave_parallel_workers;
  requirements.parallel_applier_preserve_commit_order= opt_slave_preserve_commit_order;
  requirements.lower_case_table_names = lower_case_table_names;
}
#endif //HAVE_REPLICATION

bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      size_t *length)
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
                                 size_t length)
{
  /* No sid_lock because this is a completely local object. */
  Sid_map sid_map(NULL);
  Gtid_set set(&sid_map);

  if (set.add_gtid_encoding(encoded_gtid_set, length) !=
      RETURN_STATUS_OK)
    return NULL;

  char *buf;
  set.to_string(&buf);
  return buf;
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
