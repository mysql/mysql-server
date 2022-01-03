/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_ESGS_BY_USER_BY_EVENT_NAME_H
#define TABLE_ESGS_BY_USER_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_esgs_by_user_by_event_name.h
  Table EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_instr_class;
struct PFS_stage_class;
struct PFS_user;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esgs_by_user_by_event_name : public PFS_engine_index {
 public:
  PFS_index_esgs_by_user_by_event_name()
      : PFS_engine_index(&m_key_1, &m_key_2),
        m_key_1("USER"),
        m_key_2("EVENT_NAME") {}

  ~PFS_index_esgs_by_user_by_event_name() override = default;

  virtual bool match(PFS_user *pfs);
  virtual bool match(PFS_instr_class *instr_class);

 private:
  PFS_key_user m_key_1;
  PFS_key_event_name m_key_2;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME.
*/
struct row_esgs_by_user_by_event_name {
  /** Column USER. */
  PFS_user_row m_user;
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stage_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME.
  Index 1 on user (0 based)
  Index 2 on stage class (1 based)
*/
struct pos_esgs_by_user_by_event_name : public PFS_double_index {
  pos_esgs_by_user_by_event_name() : PFS_double_index(0, 1) {}

  inline void reset(void) {
    m_index_1 = 0;
    m_index_2 = 1;
  }

  inline void next_user(void) {
    m_index_1++;
    m_index_2 = 1;
  }

  inline void next_stage(void) { m_index_2++; }
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME. */
class table_esgs_by_user_by_event_name : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position(void) override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_esgs_by_user_by_event_name();

 public:
  ~table_esgs_by_user_by_event_name() override = default;

 protected:
  int make_row(PFS_user *user, PFS_stage_class *klass);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_esgs_by_user_by_event_name m_row;
  /** Current position. */
  pos_esgs_by_user_by_event_name m_pos;
  /** Next position. */
  pos_esgs_by_user_by_event_name m_next_pos;

  PFS_index_esgs_by_user_by_event_name *m_opened_index;
};

/** @} */
#endif
