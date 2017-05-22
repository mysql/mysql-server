/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "member_info.h"
#include "plugin.h"
#include "asynchronous_channels_state_observer.h"

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
      group_member_mgr)
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
      group_member_mgr)
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
applier_stop(Binlog_relay_IO_param *param, bool aborted)
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
