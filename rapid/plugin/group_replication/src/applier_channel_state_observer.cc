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

#include "applier_channel_state_observer.h"
#include "plugin.h"

Applier_channel_state_observer::
Applier_channel_state_observer()
{}

int Applier_channel_state_observer::
thread_start(Binlog_relay_IO_param *param)
{
  return 0;
}

int Applier_channel_state_observer::thread_stop(Binlog_relay_IO_param *param)
{
  return 0;
}

int Applier_channel_state_observer::
applier_start(Binlog_relay_IO_param *param)
{
  return 0;
}

int Applier_channel_state_observer::
applier_stop(Binlog_relay_IO_param *param, bool aborted)
{
  applier_module->inform_of_applier_stop(param->channel_name, aborted);
  return 0;
}

int Applier_channel_state_observer::
before_request_transmit(Binlog_relay_IO_param *param,
                        uint32 flags)
{
  return 0;
}

int Applier_channel_state_observer::
after_read_event(Binlog_relay_IO_param *param,
                 const char *packet, unsigned long len,
                 const char **event_buf,
                 unsigned long *event_len)
{
  return 0;
}

int Applier_channel_state_observer::
after_queue_event(Binlog_relay_IO_param *param,
                  const char *event_buf,
                  unsigned long event_len,
                  uint32 flags)
{
  return 0;
}

int Applier_channel_state_observer::
after_reset_slave(Binlog_relay_IO_param *param)
{
  return 0;
}
