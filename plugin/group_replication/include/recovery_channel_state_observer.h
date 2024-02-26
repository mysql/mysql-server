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

#ifndef RECOVERY_CHANNEL_STATE_OBSERVER_INCLUDE
#define RECOVERY_CHANNEL_STATE_OBSERVER_INCLUDE

#include "my_inttypes.h"
#include "plugin/group_replication/include/plugin_observers/channel_observation_manager.h"
#include "plugin/group_replication/include/recovery_state_transfer.h"

class Recovery_channel_state_observer : public Channel_state_observer {
 public:
  Recovery_channel_state_observer(
      Recovery_state_transfer *recovery_state_transfer);

  /** Observer for receiver thread starts */
  int thread_start(Binlog_relay_IO_param *param) override;

  /** Observer for receiver thread stops */
  int thread_stop(Binlog_relay_IO_param *param) override;

  /** Observer for applier thread starts */
  int applier_start(Binlog_relay_IO_param *param) override;

  /** Observer for applier thread stops */
  int applier_stop(Binlog_relay_IO_param *param, bool aborted) override;

  /**  Observer for when a new transmission from a another server is requested*/
  int before_request_transmit(Binlog_relay_IO_param *param,
                              uint32 flags) override;

  /** Observer for whenever a event is read by the receiver thread*/
  int after_read_event(Binlog_relay_IO_param *param, const char *packet,
                       unsigned long len, const char **event_buf,
                       unsigned long *event_len) override;

  /** Observer for whenever a event is queued by the receiver thread*/
  int after_queue_event(Binlog_relay_IO_param *param, const char *event_buf,
                        unsigned long event_len, uint32 flags) override;

  /** Observer for whenever a reset slave is executed */
  int after_reset_slave(Binlog_relay_IO_param *param) override;

  /** Observer for applier skip event */
  int applier_log_event(Binlog_relay_IO_param *param, Trans_param *trans_param,
                        int &out) override;

 private:
  Recovery_state_transfer *recovery_state_transfer;
};

#endif /* RECOVERY_CHANNEL_STATE_OBSERVER_INCLUDE */
