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

#ifndef OBSERVER_SERVER_CHANNELS
#define OBSERVER_SERVER_CHANNELS

#include <mysql/group_replication_priv.h>

#include "my_inttypes.h"

int group_replication_thread_start(Binlog_relay_IO_param *param);
int group_replication_thread_stop(Binlog_relay_IO_param *param);
int group_replication_applier_start(Binlog_relay_IO_param *param);
int group_replication_applier_stop(Binlog_relay_IO_param *param, bool aborted);
int group_replication_before_request_transmit(Binlog_relay_IO_param *param,
                                              uint32 flags);
int group_replication_after_read_event(Binlog_relay_IO_param *param,
                                       const char *packet, unsigned long len,
                                       const char **event_buf,
                                       unsigned long *event_len);
int group_replication_after_queue_event(Binlog_relay_IO_param *param,
                                        const char *event_buf,
                                        unsigned long event_len, uint32 flags);
int group_replication_after_reset_slave(Binlog_relay_IO_param *param);

extern Binlog_relay_IO_observer binlog_IO_observer;

#endif /* OBSERVER_SERVER_CHANNELS */
