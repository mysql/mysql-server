/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_mi.h"
#include "ha_ndbcluster_glue.h"
#include "my_sys.h"
#include "hash.h"
#include "rpl_mi.h"

#ifdef HAVE_NDB_BINLOG

extern Master_info *active_mi;


uint32 ndb_mi_get_master_server_id()
{
  return (uint32) active_mi->master_id;
}

const char* ndb_mi_get_group_master_log_name()
{
#if MYSQL_VERSION_ID < 50600
  return active_mi->rli.group_master_log_name;
#else
  return active_mi->rli->get_group_master_log_name();
#endif
}

uint64 ndb_mi_get_group_master_log_pos()
{
#if MYSQL_VERSION_ID < 50600
  return (uint64) active_mi->rli.group_master_log_pos;
#else
  return (uint64) active_mi->rli->get_group_master_log_pos();
#endif
}

uint64 ndb_mi_get_future_event_relay_log_pos()
{
#if MYSQL_VERSION_ID < 50600
  return (uint64) active_mi->rli.future_event_relay_log_pos;
#else
  return (uint64) active_mi->rli->get_future_event_relay_log_pos();
#endif
}

uint64 ndb_mi_get_group_relay_log_pos()
{
#if MYSQL_VERSION_ID < 50600
  return (uint64) active_mi->rli.group_relay_log_pos;
#else
  return (uint64) active_mi->rli->get_group_relay_log_pos();
#endif
}

bool ndb_mi_get_ignore_server_id(uint32 server_id)
{
  return (active_mi->shall_ignore_server_id(server_id) != 0);
}

uint32 ndb_mi_get_slave_run_id()
{
  return active_mi->rli->slave_run_id;
}

bool ndb_mi_get_in_relay_log_statement(Relay_log_info* rli)
{
  return (rli->get_flag(Relay_log_info::IN_STMT) != 0);
}

/* #ifdef HAVE_NDB_BINLOG */

#endif
