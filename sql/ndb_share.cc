/*
   Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "ndb_conflict.h"
#include "ndb_name_util.h"

#include <ndbapi/NdbEventOperation.hpp>

#include <my_sys.h>

extern Ndb* g_ndb;

void
NDB_SHARE::destroy(NDB_SHARE* share)
{
  thr_lock_delete(&share->lock);
  native_mutex_destroy(&share->mutex);

#ifdef HAVE_NDB_BINLOG
  teardown_conflict_fn(g_ndb, share->m_cfn_share);
#endif
  share->new_op= 0;
  Ndb_event_data* event_data = share->event_data;
  if (event_data)
  {
    delete event_data;
    event_data= 0;
  }
  // Release memory for the variable length strings held by
  // key but also referenced by db, table_name and shadow_table->db etc.
  free_key(share->key);
  my_free(share);
}

/*
  Struct holding dynamic length strings for NDB_SHARE. The type is
  opaque to the user of NDB_SHARE and should
  only be accessed using NDB_SHARE accessor functions.

  All the strings are zero terminated.

  Layout:
  size_t key_length
  "key"\0
  "db\0"
  "table_name\0"
*/
struct NDB_SHARE_KEY {
  size_t m_key_length;
  char m_buffer[1];
};

NDB_SHARE_KEY*
NDB_SHARE::create_key(const char *new_key)
{
  const size_t new_key_length = strlen(new_key);

  char db_name_buf[FN_HEADLEN];
  ndb_set_dbname(new_key, db_name_buf);
  const size_t db_name_len = strlen(db_name_buf);

  char table_name_buf[FN_HEADLEN];
  ndb_set_tabname(new_key, table_name_buf);
  const size_t table_name_len = strlen(table_name_buf);

  // Calculate total size needed for the variable length strings
  const size_t size=
      sizeof(NDB_SHARE_KEY) +
      new_key_length +
      db_name_len + 1 +
      table_name_len + 1;

  NDB_SHARE_KEY* allocated_key=
      (NDB_SHARE_KEY*) my_malloc(PSI_INSTRUMENT_ME,
                                 size,
                                 MYF(MY_WME | ME_FATALERROR));

  allocated_key->m_key_length = new_key_length;

  // Copy key into the buffer
  char* buf_ptr = allocated_key->m_buffer;
  my_stpcpy(buf_ptr, new_key);
  buf_ptr += new_key_length + 1;

  // Copy db_name into the buffer
  my_stpcpy(buf_ptr, db_name_buf);
  buf_ptr += db_name_len + 1;

  // Copy table_name into the buffer
  my_stpcpy(buf_ptr, table_name_buf);
  buf_ptr += table_name_len;

  // Check that writing has not occured beyond end of allocated memory
  assert(buf_ptr < reinterpret_cast<char*>(allocated_key) + size);

  DBUG_PRINT("info", ("size: %lu, sizeof(NDB_SHARE_KEY): %lu",
                      size, sizeof(NDB_SHARE_KEY)));
  DBUG_PRINT("info", ("new_key: '%s', %lu", new_key, new_key_length));
  DBUG_PRINT("info", ("db_name: '%s', %lu", db_name_buf, db_name_len));
  DBUG_PRINT("info", ("table_name: '%s', %lu", table_name_buf, table_name_len));
  DBUG_DUMP("NDB_SHARE_KEY: ", (const uchar*)allocated_key->m_buffer, size);

  return allocated_key;
}


void NDB_SHARE::free_key(NDB_SHARE_KEY* key)
{
  my_free(key);
}


const uchar* NDB_SHARE::key_get_key(NDB_SHARE_KEY* key)
{
  assert(key->m_key_length == strlen(key->m_buffer));
  return (const uchar*)key->m_buffer;
}


size_t NDB_SHARE::key_get_length(NDB_SHARE_KEY* key)
{
  assert(key->m_key_length == strlen(key->m_buffer));
  return key->m_key_length;
}


char* NDB_SHARE::key_get_db_name(NDB_SHARE_KEY* key)
{
  char* buf_ptr = key->m_buffer;
  // Step past the key string and it's zero terminator
  buf_ptr += key->m_key_length + 1;
  return buf_ptr;
}


char* NDB_SHARE::key_get_table_name(NDB_SHARE_KEY* key)
{
  char* buf_ptr = key_get_db_name(key);
  const size_t db_name_len = strlen(buf_ptr);
  // Step past the db name string and it's zero terminator
  buf_ptr += db_name_len + 1;
  return buf_ptr;
}


size_t NDB_SHARE::key_length() const
{
  assert(key->m_key_length == strlen(key->m_buffer));
  return key->m_key_length;
}


const char* NDB_SHARE::key_string() const
{
  assert(strlen(key->m_buffer) == key->m_key_length);
  return key->m_buffer;
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
  fprintf(file, "  - key: '%s', key_length: %lu\n",
          key_string(), key_length());
  fprintf(file, "  - commit_count: %llu\n", commit_count);
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
