/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/recovery_channel_state_observer.h"

Recovery_channel_state_observer::Recovery_channel_state_observer(
    Recovery_state_transfer *recovery_state_transfer)
    : recovery_state_transfer(recovery_state_transfer) {}

int Recovery_channel_state_observer::thread_start(Binlog_relay_IO_param *) {
  return 0;
}

int Recovery_channel_state_observer::thread_stop(Binlog_relay_IO_param *param) {
  recovery_state_transfer->inform_of_receiver_stop(param->thread_id);
  return 0;
}

int Recovery_channel_state_observer::applier_start(Binlog_relay_IO_param *) {
  return 0;
}

int Recovery_channel_state_observer::applier_stop(Binlog_relay_IO_param *param,
                                                  bool aborted) {
  recovery_state_transfer->inform_of_applier_stop(param->thread_id, aborted);
  return 0;
}

int Recovery_channel_state_observer::before_request_transmit(
    Binlog_relay_IO_param *, uint32) {
  return 0;
}

int Recovery_channel_state_observer::after_read_event(Binlog_relay_IO_param *,
                                                      const char *,
                                                      unsigned long,
                                                      const char **,
                                                      unsigned long *) {
  return 0;
}

int Recovery_channel_state_observer::after_queue_event(Binlog_relay_IO_param *,
                                                       const char *,
                                                       unsigned long, uint32) {
  return 0;
}

int Recovery_channel_state_observer::after_reset_slave(
    Binlog_relay_IO_param *) {
  return 0;
}

int Recovery_channel_state_observer::applier_log_event(Binlog_relay_IO_param *,
                                                       Trans_param *, int &) {
  return 0;
}
