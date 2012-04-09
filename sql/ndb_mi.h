/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MI_H
#define NDB_MI_H

#include <my_global.h>

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

/*
   Relay log info related functions
*/
bool ndb_mi_get_in_relay_log_statement(class Relay_log_info* rli);

// #ifndef NDB_MI_H
#endif
