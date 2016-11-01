/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef OBSERVER_SERVER_ACTIONS_INCLUDE
#define	OBSERVER_SERVER_ACTIONS_INCLUDE

#include <mysql/group_replication_priv.h>

int group_replication_reset_master_logs(Binlog_transmit_param *param);

// Remaining binlog transmit observer methods, not used as of now

int group_replication_transmit_start(Binlog_transmit_param *param,
                                     const char *log_file, my_off_t log_pos);

int group_replication_transmit_stop(Binlog_transmit_param *param);

int group_replication_reserve_header(Binlog_transmit_param *param,
                                     unsigned char *header,
                                     unsigned long size,
                                     unsigned long *len);

int group_replication_before_send_event(Binlog_transmit_param *param,
                                        unsigned char *packet,
                                        unsigned long len,
                                        const char *log_file, my_off_t log_pos);

int group_replication_after_send_event(Binlog_transmit_param *param,
                                       const char *event_buf,
                                       unsigned long len,
                                       const char *skipped_log_file,
                                       my_off_t skipped_log_pos);

//Binlog_transmit observer struct
extern Binlog_transmit_observer binlog_transmit_observer;

#endif	/* OBSERVER_SERVER_ACTIONS_INCLUDE */

