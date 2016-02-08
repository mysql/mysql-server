/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_msr.h"
#include "rpl_mi.h"
#include "rpl_rli.h"

#ifdef HAVE_NDB_BINLOG

/*
  Utility class for interacting with the global structure which
  holds information about the current multi source replication setup.

  The global structure requires locking to prevent that channels
  are added or removed by concurrent replication setup commands while
  accesing it.

  So far the cluster replication only works with the default channel.
*/
class Multisource_info_guard {
  Multisource_info_guard(const Multisource_info_guard&);
  Multisource_info_guard& operator=(const Multisource_info_guard&);
public:
  Multisource_info_guard()
  {
    channel_map.rdlock();
  }

  // Return the default channels Master_info*
  Master_info* get_default_mi() const
  {
    Master_info* default_mi = channel_map.get_default_channel_mi();
    // There should always be a default Master_info at this point
    DBUG_ASSERT(default_mi);
    return default_mi;
  }

  ~Multisource_info_guard()
  {
    // Unlock channel map
    channel_map.unlock();
  }
};


uint32 ndb_mi_get_master_server_id()
{
  Multisource_info_guard msi;
  return (uint32) msi.get_default_mi()->master_id;
}

const char* ndb_mi_get_group_master_log_name()
{
  Multisource_info_guard msi;
  return msi.get_default_mi()->rli->get_group_master_log_name();
}

uint64 ndb_mi_get_group_master_log_pos()
{
  Multisource_info_guard msi;
  return (uint64) msi.get_default_mi()->rli->get_group_master_log_pos();
}

uint64 ndb_mi_get_future_event_relay_log_pos()
{
  Multisource_info_guard msi;
  return (uint64) msi.get_default_mi()->rli->get_future_event_relay_log_pos();
}

uint64 ndb_mi_get_group_relay_log_pos()
{
  Multisource_info_guard msi;
  return (uint64) msi.get_default_mi()->rli->get_group_relay_log_pos();
}

bool ndb_mi_get_ignore_server_id(uint32 server_id)
{
  Multisource_info_guard msi;
  return (msi.get_default_mi()->shall_ignore_server_id(server_id) != 0);
}

uint32 ndb_mi_get_slave_run_id()
{
  Multisource_info_guard msi;
  return msi.get_default_mi()->rli->slave_run_id;
}

ulong ndb_mi_get_relay_log_trans_retries()
{
  Multisource_info_guard msi;
  return msi.get_default_mi()->rli->trans_retries;
}

void ndb_mi_set_relay_log_trans_retries(ulong number)
{
  Multisource_info_guard msi;
  msi.get_default_mi()->rli->trans_retries = number;
}

bool ndb_mi_get_slave_sql_running()
{
  Multisource_info_guard msi;
  return msi.get_default_mi()->rli->slave_running;
}

#endif
