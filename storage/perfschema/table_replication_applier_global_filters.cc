/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_applier_global_filters.cc
  Table replication_applier_global_filters (implementation).
*/

#include "storage/perfschema/table_replication_applier_global_filters.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"
#include "rpl_info.h"
#include "rpl_mi.h"
#include "rpl_msr.h" /* Multisource replication */
#include "rpl_rli.h"
#include "rpl_slave.h"
#include "sql_parse.h"

THR_LOCK table_replication_applier_global_filters::m_table_lock;

/*
  numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("FILTER_NAME")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL,0}
  },

  {
    {C_STRING_WITH_LEN("FILTER_RULE")},
    {C_STRING_WITH_LEN("longtext")},
    {NULL,0}
  },

  {
    {C_STRING_WITH_LEN("CONFIGURED_BY")},
    {C_STRING_WITH_LEN("enum('STARTUP_OPTIONS','CHANGE_REPLICATION_FILTER')")},
    {NULL, 0}
  },

  {
    { C_STRING_WITH_LEN("ACTIVE_SINCE") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_applier_global_filters::m_field_def=
{ 4, field_types };

PFS_engine_table_share
table_replication_applier_global_filters::m_share=
{
  { C_STRING_WITH_LEN("replication_applier_global_filters") },
  &pfs_readonly_acl,
  table_replication_applier_global_filters::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_applier_global_filters::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_replication_applier_global_filters::create(void)
{
  return new table_replication_applier_global_filters();
}

table_replication_applier_global_filters
  ::table_replication_applier_global_filters()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{
}

table_replication_applier_global_filters
  ::~table_replication_applier_global_filters()
{
}

void table_replication_applier_global_filters::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}


ha_rows table_replication_applier_global_filters::get_row_count()
{
  global_rpl_filter->rdlock();
  uint count= global_rpl_filter->get_filter_count();
  global_rpl_filter->unlock();

  return count;
}


int table_replication_applier_global_filters::rnd_next(void)
{
  int res= HA_ERR_END_OF_FILE;
  Rpl_pfs_filter* rpl_pfs_filter= NULL;

  global_rpl_filter->wrlock();
  for (m_pos.set_at(&m_next_pos); res != 0; m_pos.next())
  {
    /* Get ith rpl_pfs_filter from global replication filters. */
    rpl_pfs_filter=
      global_rpl_filter->get_global_filter_at_pos(m_pos.m_index);

    if (rpl_pfs_filter == NULL)
    {
      break;
    }
    else
    {
      make_row(rpl_pfs_filter);
      m_next_pos.set_after(&m_pos);
      res= 0;
    }
  }
  global_rpl_filter->unlock();

  return res;
}


int table_replication_applier_global_filters::rnd_pos(const void *pos)
{
  int res= HA_ERR_RECORD_DELETED;
  Rpl_pfs_filter* rpl_pfs_filter= NULL;
  set_position(pos);

  global_rpl_filter->wrlock();
  /* Get ith rpl_pfs_filter from global replication filters. */
  rpl_pfs_filter=
    global_rpl_filter->get_global_filter_at_pos(m_pos.m_index - 1);
  if (rpl_pfs_filter)
  {
    make_row(rpl_pfs_filter);
    res= 0;
  }
  global_rpl_filter->unlock();

  return res;
}


void table_replication_applier_global_filters::make_row(
  Rpl_pfs_filter* rpl_pfs_filter)
{
  m_row_exists= false;

  m_row.filter_name_length= strlen(rpl_pfs_filter->get_filter_name());
  memcpy(m_row.filter_name, rpl_pfs_filter->get_filter_name(),
         m_row.filter_name_length);

  m_row.filter_rule.copy(rpl_pfs_filter->get_filter_rule());

  m_row.configured_by=
    rpl_pfs_filter->m_rpl_filter_statistics.get_configured_by();

  m_row.active_since=
    rpl_pfs_filter->m_rpl_filter_statistics.get_active_since();

  m_row_exists= true;
}


int table_replication_applier_global_filters::read_row_values(
  TABLE *table, unsigned char *buf, Field **fields, bool read_all)
{
  Field *f;

  if (unlikely(!m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* filter_name */
        set_field_char_utf8(f, m_row.filter_name, m_row.filter_name_length);
        break;
      case 1: /* filter_rule */
        set_field_longtext_utf8(f, m_row.filter_rule.ptr(),
                                m_row.filter_rule.length());
        break;
      case 2: /* configured_by */
        set_field_enum(f, m_row.configured_by);
        break;
      case 3: /* active_since */
        set_field_timestamp(f, m_row.active_since);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
