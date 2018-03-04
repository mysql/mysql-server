/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CHANNEL_OBSERVATION_MANAGER_INCLUDE
#define	CHANNEL_OBSERVATION_MANAGER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <list>

#include "my_inttypes.h"

/**
  A interface class to code channel state response methods
*/
class Channel_state_observer
{
public:
  virtual ~Channel_state_observer()= 0;
  virtual int thread_start(Binlog_relay_IO_param *param)= 0;
  virtual int thread_stop(Binlog_relay_IO_param *param)= 0;
  virtual int applier_start(Binlog_relay_IO_param *param)= 0;
  virtual int applier_stop(Binlog_relay_IO_param *param, bool aborted)= 0;
  virtual int before_request_transmit(Binlog_relay_IO_param *param,
                                      uint32 flags)= 0;
  virtual int after_read_event(Binlog_relay_IO_param *param,
                               const char *packet, unsigned long len,
                               const char **event_buf,
                               unsigned long *event_len)= 0;
  virtual int after_queue_event(Binlog_relay_IO_param *param,
                                const char *event_buf,
                                unsigned long event_len,
                                uint32 flags)= 0;
  virtual int after_reset_slave(Binlog_relay_IO_param *param)= 0;
  virtual int applier_log_event(Binlog_relay_IO_param *param,
                                Trans_param *trans_param,
                                int& out)= 0;
};

/*
  A class to register observers for channel state events.
*/
class Channel_observation_manager
{
public:
  /*
    Initialize the class and register an observer.

    @param plugin_info The plugin info to register the hooks
  */
  Channel_observation_manager(MYSQL_PLUGIN plugin_info);

  /*
    Destructor.
    Deletes all observers and unregisters the observer
  */
  ~Channel_observation_manager();

  /*
    A method to register observers to the events that come from the server.

    @param observer A channel state observer implementation.
  */
  void register_channel_observer(Channel_state_observer* observer);

  /*
    A method to remove a channel state observer.

    @param observer A channel state observer implementation.
  */
  void unregister_channel_observer(Channel_state_observer* observer);

  /**
    Get all registered observers

    @note to get the list and while using it, you should take a read lock from
    channel_list_lock (you can use the read_lock_channel_list method)

    @return The list of all registered observers
  */
  std::list<Channel_state_observer*>* get_channel_state_observers();

  /** Locks the observer list for reads */
  void read_lock_channel_list();
  /** Locks the observer list for writes */
  void write_lock_channel_list();
  /** Unlocks the observer list */
  void unlock_channel_list();

private:
  Binlog_relay_IO_observer server_channel_state_observers;
  std::list<Channel_state_observer*> channel_observers;
  MYSQL_PLUGIN group_replication_plugin_info;

  //run conditions and locks
  Checkable_rwlock *channel_list_lock;
};

#endif /* CHANNEL_OBSERVATION_MANAGER_INCLUDE */
