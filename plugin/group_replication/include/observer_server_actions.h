/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef OBSERVER_SERVER_ACTIONS_INCLUDE
#define OBSERVER_SERVER_ACTIONS_INCLUDE

#include <mysql/group_replication_priv.h>

#include "my_inttypes.h"

int group_replication_reset_master_logs(Binlog_transmit_param *param);

// Remaining binlog transmit observer methods, not used as of now

int group_replication_transmit_start(Binlog_transmit_param *param,
                                     const char *log_file, my_off_t log_pos);

int group_replication_transmit_stop(Binlog_transmit_param *param);

int group_replication_reserve_header(Binlog_transmit_param *param,
                                     unsigned char *header, unsigned long size,
                                     unsigned long *len);

int group_replication_before_send_event(Binlog_transmit_param *param,
                                        unsigned char *packet,
                                        unsigned long len, const char *log_file,
                                        my_off_t log_pos);

int group_replication_after_send_event(Binlog_transmit_param *param,
                                       const char *event_buf, unsigned long len,
                                       const char *skipped_log_file,
                                       my_off_t skipped_log_pos);

// Binlog_transmit observer struct
extern Binlog_transmit_observer binlog_transmit_observer;

#endif /* OBSERVER_SERVER_ACTIONS_INCLUDE */
