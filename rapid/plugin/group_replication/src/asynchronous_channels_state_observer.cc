/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/asynchronous_channels_state_observer.h"

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"

Asynchronous_channels_state_observer::
Asynchronous_channels_state_observer()
{}

int Asynchronous_channels_state_observer::
thread_start(Binlog_relay_IO_param* param)
{
  /* Can't start slave relay io thread when group replication is running on
     single primary-mode on secondary */
  if (plugin_is_group_replication_running() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0 &&
      group_member_mgr &&
      local_member_info->in_primary_mode())
  {
    std::string m_uuid;
    group_member_mgr->get_primary_member_uuid(m_uuid);

    if (m_uuid == "UNDEFINED")
    {
      log_message(MY_ERROR_LEVEL, "Can't start slave IO THREAD when group"
                                  " replication is running with single"
                                  " primary-mode and the primary member is"
                                  " not known.");
      return 1;
    }

    if (m_uuid != local_member_info->get_uuid())
    {
      log_message(MY_ERROR_LEVEL, "Can't start slave IO THREAD when group"
                                  " replication is running with single"
                                  " primary-mode on a secondary member.");
      return 1;
    }
  }

  return 0;
}

int Asynchronous_channels_state_observer::thread_stop(Binlog_relay_IO_param*)
{
  return 0;
}

int Asynchronous_channels_state_observer::
applier_start(Binlog_relay_IO_param *param)
{
  /* Can't start slave relay sql thread when group replication is running on
     single primary-mode on secondary */
  if (plugin_is_group_replication_running() &&
      strcmp(param->channel_name, "group_replication_recovery") != 0 &&
      strcmp(param->channel_name, "group_replication_applier") != 0 &&
      group_member_mgr &&
      local_member_info->in_primary_mode())
  {
    std::string m_uuid;
    group_member_mgr->get_primary_member_uuid(m_uuid);

    if (m_uuid == "UNDEFINED")
    {
      log_message(MY_ERROR_LEVEL, "Can't start slave SQL THREAD when group"
                                  " replication is running with single"
                                  " primary-mode and the primary member is"
                                  " not known.");
      return 1;
    }

    if (m_uuid != local_member_info->get_uuid())
    {
      log_message(MY_ERROR_LEVEL, "Can't start slave SQL THREAD when group"
                                  " replication is running with single"
                                  " primary-mode on a secondary member.");
      return 1;
    }
  }

  return 0;
}

int Asynchronous_channels_state_observer::
applier_stop(Binlog_relay_IO_param*, bool)
{
  return 0;
}

int Asynchronous_channels_state_observer::
before_request_transmit(Binlog_relay_IO_param*,
                        uint32)
{
  return 0;
}

int Asynchronous_channels_state_observer::
after_read_event(Binlog_relay_IO_param*,
                 const char*, unsigned long,
                 const char**,
                 unsigned long*)
{
  return 0;
}

int Asynchronous_channels_state_observer::
after_queue_event(Binlog_relay_IO_param*,
                  const char*,
                  unsigned long,
                  uint32)
{
  return 0;
}

int Asynchronous_channels_state_observer::
after_reset_slave(Binlog_relay_IO_param*)
{
  return 0;
}

int Asynchronous_channels_state_observer::
applier_log_event(Binlog_relay_IO_param*,
                  Trans_param *trans_param,
                  int& out)
{
  out= 0;

  /*
    Cycle through all involved tables to assess if they all
    comply with the plugin runtime requirements. For now:
    - The table must be from a transactional engine
    - It must contain at least one primary key
    - It should not contain 'ON DELETE/UPDATE CASCADE' referential action
  */
  for (uint table=0; table < trans_param->number_of_tables; table++)
  {
    if (trans_param->tables_info[table].db_type != DB_TYPE_INNODB)
    {
      log_message(MY_ERROR_LEVEL, "Table %s does not use the InnoDB storage "
                                  "engine. This is not compatible with Group "
                                  "Replication.",
                                  trans_param->tables_info[table].table_name);
      out++;
    }

    if (trans_param->tables_info[table].number_of_primary_keys == 0)
    {
      log_message(MY_ERROR_LEVEL, "Table %s does not have any PRIMARY KEY."
                                  " This is not compatible with Group "
                                  "Replication.",
                                  trans_param->tables_info[table].table_name);
      out++;
    }

    if (local_member_info->has_enforces_update_everywhere_checks() &&
        trans_param->tables_info[table].has_cascade_foreign_key)
    {
      log_message(MY_ERROR_LEVEL, "Table %s has a foreign key with 'CASCADE'"
                                  " clause. This is not compatible with "
                                  "Group Replication.",
                                  trans_param->tables_info[table].table_name);
      out++;
    }
  }

  return 0;
}
