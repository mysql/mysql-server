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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE_H
#define TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE_H

/**
  @file storage/perfschema/table_tiws_by_index_usage.h
  Table TABLE_IO_WAIT_SUMMARY_BY_INDEX_USAGE (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_table_share;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX.
*/
struct row_tiws_by_index_usage {
  /** Column OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, INDEX_NAME. */
  PFS_index_row m_index;
  /** Columns COUNT/SUM/MIN/AVG/MAX (+_READ, +WRITE). */
  PFS_table_io_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX.
  Index 1 on global_table_share_container (0 based)
  Index 2 on index (0 based)
*/
struct pos_tiws_by_index_usage : public PFS_double_index {
  pos_tiws_by_index_usage() : PFS_double_index(0, 0) {}

  inline void reset(void) {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void next_table(void) {
    m_index_1++;
    m_index_2 = 0;
  }
};

class PFS_index_tiws_by_index_usage : public PFS_engine_index {
 public:
  PFS_index_tiws_by_index_usage()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3, &m_key_4),
        m_key_1("OBJECT_TYPE"),
        m_key_2("OBJECT_SCHEMA"),
        m_key_3("OBJECT_NAME"),
        m_key_4("INDEX_NAME") {}

  ~PFS_index_tiws_by_index_usage() override = default;

  virtual bool match(PFS_table_share *table);
  virtual bool match(PFS_table_share *share, uint index);

 private:
  PFS_key_object_type m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
  PFS_key_object_name m_key_4; /* index name */
};

/** Table PERFORMANCE_SCHEMA.TABLE_IO_WAIT_SUMMARY_BY_INDEX. */
class table_tiws_by_index_usage : public PFS_engine_table {
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
  table_tiws_by_index_usage();

 public:
  ~table_tiws_by_index_usage() override = default;

 protected:
  int make_row(PFS_table_share *table_share, uint index);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_tiws_by_index_usage m_row;
  /** Current position. */
  pos_tiws_by_index_usage m_pos;
  /** Next position. */
  pos_tiws_by_index_usage m_next_pos;

 protected:
  PFS_index_tiws_by_index_usage *m_opened_index;
};

/** @} */
#endif
