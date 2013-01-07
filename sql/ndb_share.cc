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

#include "ndb_share.h"
#include "ndb_event_data.h"
#include "ndb_dist_priv_util.h"
#include "ha_ndbcluster_tables.h"

#include <ndbapi/NdbEventOperation.hpp>

#include <my_sys.h>

extern Ndb* g_ndb;

void
NDB_SHARE::destroy(NDB_SHARE* share)
{
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->mutex);

#ifdef HAVE_NDB_BINLOG
  if (share->m_cfn_share && 
      share->m_cfn_share->m_ex_tab_writer.hasTable() && 
      g_ndb)
  {
    share->m_cfn_share->m_ex_tab_writer.free(g_ndb);
  }
#endif
  share->new_op= 0;
  Ndb_event_data* event_data = share->event_data;
  if (event_data)
  {
    delete event_data;
    event_data= 0;
  }
  free_root(&share->mem_root, MYF(0));
  my_free(share);
}


bool
NDB_SHARE::need_events(bool default_on) const
{
  DBUG_ENTER("NDB_SHARE::need_events");
  DBUG_PRINT("enter", ("db: %s, table_name: %s",
                        db, table_name));

  if (default_on)
  {
    // Events are on by default, check if it should be turned off

    if (Ndb_dist_priv_util::is_distributed_priv_table(db, table_name))
    {
      /*
        The distributed privilege tables are distributed by writing
        the CREATE USER, GRANT, REVOKE etc. to ndb_schema -> no need
        to listen to events from those table
      */
      DBUG_PRINT("exit", ("no events for dist priv table"));
      DBUG_RETURN(false);
    }

    DBUG_PRINT("exit", ("need events(the default for this mysqld)"));
    DBUG_RETURN(true);
  }

  // Events are off by default, check if it should be turned on
  if (strcmp(db, NDB_REP_DB) == 0)
  {
    // The table is in "mysql" database
    if (strcmp(table_name, NDB_SCHEMA_TABLE) == 0)
    {
      DBUG_PRINT("exit", ("need events for " NDB_SCHEMA_TABLE));
      DBUG_RETURN(true);
    }

    if (strcmp(table_name, NDB_APPLY_TABLE) == 0)
    {
      DBUG_PRINT("exit", ("need events for " NDB_APPLY_TABLE));
      DBUG_RETURN(true);
    }
  }

  DBUG_PRINT("exit", ("no events(the default for this mysqld)"));
  DBUG_RETURN(false);
}


Ndb_event_data* NDB_SHARE::get_event_data_ptr() const
{
  if (event_data)
  {
    // The event_data pointer is only used before
    // creating the NdbEventoperation -> check no op yet
    assert(!op);

    return event_data;
  }

  if (op)
  {
    // The event_data should now be empty since it's been moved to
    // op's custom data
    assert(!event_data);

    // Check that op has custom data
    assert(op->getCustomData());

    return (Ndb_event_data*)op->getCustomData();
  }

  return NULL;
}


void NDB_SHARE::print(const char* where, FILE* file) const
{
  fprintf(file, "%s %s.%s: use_count: %u\n",
          where, db, table_name, use_count);
  fprintf(file, "  - key: '%s', key_length: %d\n", key, key_length);
  fprintf(file, "  - commit_count: %llu\n", commit_count);
  if (new_key)
    fprintf(file, "  - new_key: %p, '%s'\n",
            new_key, new_key);
  if (event_data)
    fprintf(file, "  - event_data: %p\n", event_data);
  if (op)
    fprintf(file, "  - op: %p\n", op);
  if (new_op)
    fprintf(file, "  - new_op: %p\n", new_op);

  Ndb_event_data *event_data_ptr= get_event_data_ptr();
  if (event_data_ptr)
    event_data_ptr->print("  -", file);
}
