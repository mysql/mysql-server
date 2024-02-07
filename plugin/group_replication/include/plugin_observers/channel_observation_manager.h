/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CHANNEL_OBSERVATION_MANAGER_INCLUDE
#define CHANNEL_OBSERVATION_MANAGER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <list>

#include "my_inttypes.h"
#include "mysql/plugin.h"

class Channel_observation_manager;

/**
  A interface class to code channel state response methods
*/
class Channel_state_observer {
 public:
  virtual ~Channel_state_observer() = 0;
  virtual int thread_start(Binlog_relay_IO_param *param) = 0;
  virtual int thread_stop(Binlog_relay_IO_param *param) = 0;
  virtual int applier_start(Binlog_relay_IO_param *param) = 0;
  virtual int applier_stop(Binlog_relay_IO_param *param, bool aborted) = 0;
  virtual int before_request_transmit(Binlog_relay_IO_param *param,
                                      uint32 flags) = 0;
  virtual int after_read_event(Binlog_relay_IO_param *param, const char *packet,
                               unsigned long len, const char **event_buf,
                               unsigned long *event_len) = 0;
  virtual int after_queue_event(Binlog_relay_IO_param *param,
                                const char *event_buf, unsigned long event_len,
                                uint32 flags) = 0;
  virtual int after_reset_slave(Binlog_relay_IO_param *param) = 0;
  virtual int applier_log_event(Binlog_relay_IO_param *param,
                                Trans_param *trans_param, int &out) = 0;
};

/**
  A class to hold different channel observation manager.

  @note Slave channels observation and group replication channel observation
        serves different purposes and can interfere with one another.
        For that reason they are separated here.
*/
class Channel_observation_manager_list {
 public:
  /**
    Constructor.
    Initializes the given number of channel observation manager
    and register an observer in the server.

    @param plugin_info  The plugin info to register the hooks
    @param num_managers The number of channel observation manager instantiated
  */
  Channel_observation_manager_list(MYSQL_PLUGIN plugin_info, uint num_managers);

  /**
    Destructor.
    Unregister the server observer
    and deletes all the channel observation manager.
  */
  ~Channel_observation_manager_list();

  /**
    A method to add channel observation manager to the
    channel_observation_manager list.

    @param manager A channel observation manager implementation.
  */
  void add_channel_observation_manager(Channel_observation_manager *manager);

  /**
    A method to remove a channel observation manager from
    channel_observation_manager list.

    @param manager A channel observation manager implementation.
  */
  void remove_channel_observation_manager(Channel_observation_manager *manager);

  /**
    Get all the channel observation manager

    @return The list of all channel observation manager
  */
  std::list<Channel_observation_manager *>
      &get_channel_observation_manager_list();

  /**
    Get particular channel observation manager

    @param  position get iterator value at position
    @return The channel observation manager
  */
  Channel_observation_manager *get_channel_observation_manager(
      uint position = 0);

 private:
  /** Server relay log observer struct */
  Binlog_relay_IO_observer server_channel_state_observers;

  /** server plugin handle */
  MYSQL_PLUGIN group_replication_plugin_info;

  /** list of channel observation manager */
  std::list<Channel_observation_manager *> channel_observation_manager;
};

/**
  A class to register observers for channel state events.
*/
class Channel_observation_manager {
 public:
  /**
    Initialize the class.
  */
  Channel_observation_manager();

  /**
    Destructor.
    Deletes all the channel state observers.
  */
  ~Channel_observation_manager();

  /**
    A method to register observers to the events that come from the server.

    @param observer A channel state observer implementation.
  */
  void register_channel_observer(Channel_state_observer *observer);

  /**
    A method to remove a channel state observer.

    @param observer A channel state observer implementation.
  */
  void unregister_channel_observer(Channel_state_observer *observer);

  /**
    Get all registered observers

    @note to get the list and while using it, you should take a read lock from
    channel_list_lock (you can use the read_lock_channel_list method)

    @return The list of all registered observers
  */
  std::list<Channel_state_observer *> &get_channel_state_observers();

  /** Locks the observer list for reads */
  void read_lock_channel_list();
  /** Locks the observer list for writes */
  void write_lock_channel_list();
  /** Unlocks the observer list */
  void unlock_channel_list();

 private:
  /** list of channel state observer */
  std::list<Channel_state_observer *> channel_observers;

  // run conditions and locks
  Checkable_rwlock *channel_list_lock;
};

#endif /* CHANNEL_OBSERVATION_MANAGER_INCLUDE */
