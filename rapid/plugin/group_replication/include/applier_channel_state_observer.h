/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef APPLIER_CHANNEL_STATE_OBSERVER_INCLUDE
#define	APPLIER_CHANNEL_STATE_OBSERVER_INCLUDE

#include "channel_observation_manager.h"

class Applier_channel_state_observer : public Channel_state_observer
{
public:
  Applier_channel_state_observer();

  /** Observer for receiver thread starts */
  int thread_start(Binlog_relay_IO_param *param);

  /** Observer for receiver thread stops */
  int thread_stop(Binlog_relay_IO_param *param);

  /** Observer for applier thread starts */
  int applier_start(Binlog_relay_IO_param *param);

  /** Observer for applier thread stops */
  int applier_stop(Binlog_relay_IO_param *param, bool aborted);

  /**  Observer for when a new transmission from a another server is requested*/
  int before_request_transmit(Binlog_relay_IO_param *param,
                            uint32 flags);

  /** Observer for whenever a event is read by the receiver thread*/
  int after_read_event(Binlog_relay_IO_param *param,
                       const char *packet, unsigned long len,
                       const char **event_buf,
                       unsigned long *event_len);

  /** Observer for whenever a event is queued by the receiver thread*/
  int after_queue_event(Binlog_relay_IO_param *param,
                        const char *event_buf,
                        unsigned long event_len,
                        uint32 flags);

  /** Observer for whenever a reset slave is executed */
  int after_reset_slave(Binlog_relay_IO_param *param);

};

#endif /* APPLIER_CHANNEL_STATE_OBSERVER_INCLUDE */


