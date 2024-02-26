/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_observers/channel_observation_manager.h"

#include "my_dbug.h"
#include "plugin/group_replication/include/observer_server_channels.h"
#include "plugin/group_replication/include/plugin_psi.h"

Channel_state_observer::~Channel_state_observer() = default;

Channel_observation_manager_list::Channel_observation_manager_list(
    MYSQL_PLUGIN plugin_info, uint num_managers)
    : group_replication_plugin_info(plugin_info) {
  for (uint i = 0; i < num_managers; ++i) {
    Channel_observation_manager *channel_manager =
        new Channel_observation_manager();
    add_channel_observation_manager(channel_manager);
  }

  server_channel_state_observers = binlog_IO_observer;
  register_binlog_relay_io_observer(&server_channel_state_observers,
                                    group_replication_plugin_info);
}

Channel_observation_manager_list::~Channel_observation_manager_list() {
  unregister_binlog_relay_io_observer(&server_channel_state_observers,
                                      group_replication_plugin_info);

  if (!channel_observation_manager.empty()) {
    /* purecov: begin inspected */
    std::list<Channel_observation_manager *>::const_iterator obm_iterator;
    for (obm_iterator = channel_observation_manager.begin();
         obm_iterator != channel_observation_manager.end(); ++obm_iterator) {
      delete (*obm_iterator);
    }
    channel_observation_manager.clear();
    /* purecov: end */
  }
}

void Channel_observation_manager_list::add_channel_observation_manager(
    Channel_observation_manager *manager) {
  channel_observation_manager.push_back(manager);
}

void Channel_observation_manager_list::remove_channel_observation_manager(
    Channel_observation_manager *manager) {
  channel_observation_manager.remove(manager);
}

std::list<Channel_observation_manager *>
    &Channel_observation_manager_list::get_channel_observation_manager_list() {
  DBUG_TRACE;
  return channel_observation_manager;
}

Channel_observation_manager *
Channel_observation_manager_list::get_channel_observation_manager(
    uint position) {
  DBUG_TRACE;
  assert(position < channel_observation_manager.size());
  std::list<Channel_observation_manager *>::const_iterator cit =
      channel_observation_manager.begin();
  std::advance(cit, position);

  return *cit;
}

Channel_observation_manager::Channel_observation_manager() {
  channel_list_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_channel_observation_list
#endif
  );
}

Channel_observation_manager::~Channel_observation_manager() {
  if (!channel_observers.empty()) {
    /* purecov: begin inspected */
    std::list<Channel_state_observer *>::const_iterator obs_iterator;
    for (obs_iterator = channel_observers.begin();
         obs_iterator != channel_observers.end(); ++obs_iterator) {
      delete (*obs_iterator);
    }
    channel_observers.clear();
    /* purecov: end */
  }

  delete channel_list_lock;
}

std::list<Channel_state_observer *>
    &Channel_observation_manager::get_channel_state_observers() {
  DBUG_TRACE;
#ifndef NDEBUG
  channel_list_lock->assert_some_lock();
#endif
  return channel_observers;
}

void Channel_observation_manager::register_channel_observer(
    Channel_state_observer *observer) {
  DBUG_TRACE;
  write_lock_channel_list();
  channel_observers.push_back(observer);
  unlock_channel_list();
}

void Channel_observation_manager::unregister_channel_observer(
    Channel_state_observer *observer) {
  DBUG_TRACE;
  write_lock_channel_list();
  channel_observers.remove(observer);
  unlock_channel_list();
}

void Channel_observation_manager::read_lock_channel_list() {
  channel_list_lock->rdlock();
}

void Channel_observation_manager::write_lock_channel_list() {
  channel_list_lock->wrlock();
}

void Channel_observation_manager::unlock_channel_list() {
  channel_list_lock->unlock();
}
