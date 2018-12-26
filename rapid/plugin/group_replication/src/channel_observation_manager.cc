/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "channel_observation_manager.h"
#include "observer_server_channels.h"
#include "plugin_psi.h"


Channel_state_observer::~Channel_state_observer() {}

Channel_observation_manager::
Channel_observation_manager(MYSQL_PLUGIN plugin_info)
  :group_replication_plugin_info(plugin_info)
{
  channel_list_lock= new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
                                          key_GR_LOCK_channel_observation_list
#endif
                                         );

  server_channel_state_observers= binlog_IO_observer;
  register_binlog_relay_io_observer(&server_channel_state_observers,
                                    group_replication_plugin_info);
}

Channel_observation_manager::~Channel_observation_manager()
{
  if(!channel_observers.empty())
  {
    /* purecov: begin inspected */
    std::list<Channel_state_observer*>::const_iterator obs_iterator;
    for (obs_iterator = channel_observers.begin();
         obs_iterator != channel_observers.end();
         ++obs_iterator)
    {
      delete (*obs_iterator);
    }
    channel_observers.clear();
    /* purecov: end */
  }
  unregister_binlog_relay_io_observer(&server_channel_state_observers,
                                      group_replication_plugin_info);

  delete channel_list_lock;
}

std::list<Channel_state_observer*>*
Channel_observation_manager::get_channel_state_observers()
{
  DBUG_ENTER("Channel_observation_manager::get_channel_state_observers");
#ifndef DBUG_OFF
  channel_list_lock->assert_some_lock();
#endif
  DBUG_RETURN(&channel_observers);
}

void
Channel_observation_manager::
register_channel_observer(Channel_state_observer* observer)
{
  DBUG_ENTER("Channel_observation_manager::register_channel_observer");
  write_lock_channel_list();
  channel_observers.push_back(observer);
  unlock_channel_list();
  DBUG_VOID_RETURN;
}

void
Channel_observation_manager::
unregister_channel_observer(Channel_state_observer* observer)
{
  DBUG_ENTER("Channel_observation_manager::unregister_channel_observer");
  write_lock_channel_list();
  channel_observers.remove(observer);
  unlock_channel_list();
  DBUG_VOID_RETURN;
}

void Channel_observation_manager::read_lock_channel_list()
{
  channel_list_lock->rdlock();
}

void Channel_observation_manager::write_lock_channel_list()
{
  channel_list_lock->wrlock();
}

void Channel_observation_manager::unlock_channel_list()
{
  channel_list_lock->unlock();
}

