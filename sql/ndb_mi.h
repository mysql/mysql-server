/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MI_H
#define NDB_MI_H

#include "my_inttypes.h"

/*
   This file defines methods for interacting with the
   Master Info structure on a Slave MySQLD.
   These methods are only valid when running in an
   active slave thread.
*/

/*
  Accessors
*/
uint32 ndb_mi_get_master_server_id();
const char* ndb_mi_get_group_master_log_name();
uint64 ndb_mi_get_group_master_log_pos();
uint64 ndb_mi_get_future_event_relay_log_pos();
uint64 ndb_mi_get_group_relay_log_pos();
bool ndb_mi_get_ignore_server_id(uint32 server_id);
uint32 ndb_mi_get_slave_run_id();
bool ndb_mi_get_slave_sql_running();
ulong ndb_mi_get_slave_parallel_workers();
uint32 ndb_get_number_of_channels();

/*
   Relay log info related functions
*/
ulong ndb_mi_get_relay_log_trans_retries();
void ndb_mi_set_relay_log_trans_retries(ulong number);

// #ifndef NDB_MI_H
#endif
