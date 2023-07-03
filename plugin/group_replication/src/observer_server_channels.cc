/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/observer_server_channels.h"

#include <stddef.h>

#include "plugin/group_replication/include/plugin.h"

int group_replication_thread_start(Binlog_relay_IO_param *param) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->thread_start(param);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_thread_stop(Binlog_relay_IO_param *param) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->thread_stop(param);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_applier_start(Binlog_relay_IO_param *param) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->applier_start(param);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_applier_stop(Binlog_relay_IO_param *param, bool aborted) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->applier_stop(param, aborted);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_before_request_transmit(Binlog_relay_IO_param *param,
                                              uint32 flags) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->before_request_transmit(param, flags);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_after_read_event(Binlog_relay_IO_param *param,
                                       const char *packet, unsigned long len,
                                       const char **event_buf,
                                       unsigned long *event_len) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->after_read_event(param, packet, len, event_buf,
                                                event_len);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_after_queue_event(Binlog_relay_IO_param *param,
                                        const char *event_buf,
                                        unsigned long event_len, uint32 flags) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error +=
          state_observer->after_queue_event(param, event_buf, event_len, flags);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_after_reset_slave(Binlog_relay_IO_param *param) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->after_reset_slave(param);

    observation_manager->unlock_channel_list();
  }

  return error;
}

int group_replication_applier_log_event(Binlog_relay_IO_param *param,
                                        Trans_param *trans_param, int &out) {
  int error = 0;

  for (Channel_observation_manager *observation_manager :
       channel_observation_manager_list
           ->get_channel_observation_manager_list()) {
    observation_manager->read_lock_channel_list();

    for (Channel_state_observer *state_observer :
         observation_manager->get_channel_state_observers())
      error += state_observer->applier_log_event(param, trans_param, out);

    observation_manager->unlock_channel_list();
  }

  return error;
}

Binlog_relay_IO_observer binlog_IO_observer = {
    sizeof(Binlog_relay_IO_observer),

    group_replication_thread_start,
    group_replication_thread_stop,
    group_replication_applier_start,
    group_replication_applier_stop,
    group_replication_before_request_transmit,
    group_replication_after_read_event,
    group_replication_after_queue_event,
    group_replication_after_reset_slave,
    group_replication_applier_log_event};
