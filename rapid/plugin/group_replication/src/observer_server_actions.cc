/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include "plugin/group_replication/include/observer_server_actions.h"

#include "plugin/group_replication/include/observer_trans.h"

int group_replication_reset_master_logs(Binlog_transmit_param*)
{
  register_server_reset_master();
  return 0;
}

int group_replication_transmit_start(Binlog_transmit_param*,
                                     const char*, my_off_t)
{
  return 0;
}

int group_replication_transmit_stop(Binlog_transmit_param*)
{
  return 0;
}

int group_replication_reserve_header(Binlog_transmit_param*,
                                     unsigned char*,
                                     unsigned long,
                                     unsigned long*)
{
  return 0;
}

int group_replication_before_send_event(Binlog_transmit_param*,
                                        unsigned char*,
                                        unsigned long,
                                        const char*,
                                        my_off_t)
{
  return 0;
}

int group_replication_after_send_event(Binlog_transmit_param*,
                                       const char*,
                                       unsigned long,
                                       const char*,
                                       my_off_t)
{
  return 0;
}

Binlog_transmit_observer binlog_transmit_observer = {
  sizeof(Binlog_transmit_observer),

  group_replication_transmit_start,     // transmit_start,
  group_replication_transmit_stop,      // transmit_stop,
  group_replication_reserve_header,     // reserve_header,
  group_replication_before_send_event,  // before_send_event,
  group_replication_after_send_event,   // after_send_event,
  group_replication_reset_master_logs   // reset_master
};
