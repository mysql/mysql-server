/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_instance_log_status.cc
  Table instance_log_status (implementation).
*/

#include "storage/perfschema/table_instance_log_status.h"

#include "sql/debug_sync.h"
#include "sql/rpl_msr.h"                       // channel_map
#include "sql/instance_log_resource.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_instance_log_status::m_table_lock;

Plugin_table table_instance_log_status::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "instance_log_status",
  /* Definition */
  "  SERVER_UUID CHAR(36) collate utf8_bin not null,\n"
  "  MASTER JSON not null,\n"
  "  CHANNELS JSON not null,\n"
  "  STORAGE_ENGINES JSON not null\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_instance_log_status::m_share = {
  &pfs_readonly_acl,
  table_instance_log_status::create,
  NULL,                                               /* write_row */
  NULL,                                               /* delete_all_rows */
  table_instance_log_status::get_row_count,           /* records */
  sizeof(PFS_simple_index),                           /* ref length */
  &m_table_lock,
  &m_table_def,
  true, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};


PFS_engine_table *
table_instance_log_status::create(PFS_engine_table_share *)
{
  return new table_instance_log_status();
}


table_instance_log_status::table_instance_log_status()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}


table_instance_log_status::~table_instance_log_status()
{
}


void
table_instance_log_status::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}


ha_rows
table_instance_log_status::get_row_count()
{
  return 1;
}


int
table_instance_log_status::rnd_next(void)
{
  int res = HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < 1 && res != 0;
       m_pos.next())
  {
    res = make_row();
    m_next_pos.set_after(&m_pos);
  }

  return res;
}


int table_instance_log_status::rnd_pos(const void *pos MY_ATTRIBUTE((unused)))
{
  int res = HA_ERR_RECORD_DELETED;

  set_position(pos);

  if (m_pos.m_index == 1)
  {
    res = make_row();
  }

  return res;
}


int
table_instance_log_status::make_row()
{
  DBUG_ENTER("table_instance_log_status::make_row");
  THD *thd= current_thd;

  /* Report an error if THD has no BACKUP_ADMIN privilege */
  Security_context *sctx= thd->security_context();
  if (!sctx->has_global_grant(STRING_WITH_LEN("BACKUP_ADMIN")).first)
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "BACKUP_ADMIN");
    DBUG_RETURN(HA_ERR_RECORD_DELETED);
  }

  /* Lock instance to collect log information */
  mysql_mutex_lock(&LOCK_collect_instance_log);
  bool error= false;

  Json_object json_master;                     // MASTER field
  Json_object json_channels;                   // CHANNELS field
  Json_array json_channels_array;              // JSON array for CHANNELS field
  Json_object json_storage_engines;            // STORAGE_ENGINES field

  /* To block replication channels creation/removal/admin */
  channel_map.wrlock();

  /* List of resources to be locked/collected/unlocked */
  std::list<Instance_log_resource *> resources;
  std::list<Instance_log_resource *>::iterator it;
  std::list<Instance_log_resource *>::reverse_iterator rit;

  /*
    Add resources to lock/collect/unlock list.

    Each resource will to be added to the list will be wrapped with a new
    object that will be deleted once finishing this process.

    Note: the order the resources are added to the resource list is also the
    order they will lock theirs resources.
  */
  if (error)
    goto end;

  /* Lock all resources */
  for (it=resources.begin(); it != resources.end(); ++it)
    (*it)->lock();

  /* Collect all resources information (up to hitting some error) */
  for (it=resources.begin(); it != resources.end(); ++it)
    if ((error= (*it)->collect_info()))
    {
      my_error(ER_UNABLE_TO_COLLECT_INSTANCE_LOG_STATUS, MYF(0),
               (*it)->get_json() == &json_storage_engines ?
                 "STORAGE_ENGINES" :
                 (*it)->get_json() == &json_master ?
                 "MASTER" : "CHANNELS",
               "failed to allocate memory to collect information");
      goto err_unlock;
    }

  DBUG_SIGNAL_WAIT_FOR(thd,
                       "pause_collecting_instance_logs_info",
                       "reached_collecting_instance_logs_info",
                       "continue_collecting_instance_logs_info");

err_unlock:
  /* Unlock all resources */
  for (rit=resources.rbegin(); rit != resources.rend(); ++rit)
    (*rit)->unlock();

end:
  /* Delete all wrappers */
  while (!resources.empty())
  {
    Instance_log_resource *wrapper= resources.back();
    resources.pop_back();
    delete wrapper;
  }

  /* To allow replication channels creation/removal/admin */
  channel_map.unlock();

  /* Unlock instance after collecting log information */
  mysql_mutex_unlock(&LOCK_collect_instance_log);

  if (!error)
  {
    /* Populate m_row */
    if ((error= json_channels.add_clone("channels", &json_channels_array)))
    {
      my_error(ER_UNABLE_TO_COLLECT_INSTANCE_LOG_STATUS, MYF(0), "CHANNELS",
               "failed to allocate memory to collect information");
    }
    else
    {
      memcpy(m_row.server_uuid, server_uuid, UUID_LENGTH);
      m_row.w_master= Json_wrapper(json_master.clone());
      m_row.w_channels= Json_wrapper(json_channels.clone());
      m_row.w_storage_engines= Json_wrapper(json_storage_engines.clone());
    }
  }

  DBUG_RETURN(error ? HA_ERR_RECORD_DELETED : 0);
}


int
table_instance_log_status::read_row_values(
  TABLE *table MY_ATTRIBUTE((unused)),
  unsigned char *buf MY_ATTRIBUTE((unused)),
  Field **fields MY_ATTRIBUTE((unused)),
  bool read_all MY_ATTRIBUTE((unused)))
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /*server_uuid*/
        set_field_char_utf8(f, m_row.server_uuid, UUID_LENGTH);
        break;
      case 1: /*master*/
        set_field_json(f, &m_row.w_master);
        break;
      case 2: /*channels*/
        set_field_json(f, &m_row.w_channels);
        break;
      case 3: /*storage_engines*/
        set_field_json(f, &m_row.w_storage_engines);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  m_row.cleanup();

  return 0;
}
