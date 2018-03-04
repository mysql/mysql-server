/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_ESGS_GLOBAL_BY_EVENT_NAME_H
#define TABLE_ESGS_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_esgs_global_by_event_name.h
  Table EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME (declarations).
*/

#include <sys/types.h>

#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esgs_global_by_event_name : public PFS_engine_index
{
public:
  PFS_index_esgs_global_by_event_name()
    : PFS_engine_index(&m_key), m_key("EVENT_NAME")
  {
  }

  ~PFS_index_esgs_global_by_event_name()
  {
  }

  virtual bool match(PFS_instr_class *instr_class);

private:
  PFS_key_event_name m_key;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME.
*/
struct row_esgs_global_by_event_name
{
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stage_stat_row m_stat;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_esgs_global_by_event_name : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_esgs_global_by_event_name();

public:
  ~table_esgs_global_by_event_name()
  {
  }

protected:
  int make_row(PFS_stage_class *klass);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_esgs_global_by_event_name m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_esgs_global_by_event_name *m_opened_index;
};

/** @} */
#endif
