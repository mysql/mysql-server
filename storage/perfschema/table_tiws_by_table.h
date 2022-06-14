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

#ifndef TABLE_IO_WAITS_SUMMARY_BY_TABLE_H
#define TABLE_IO_WAITS_SUMMARY_BY_TABLE_H

/**
  @file storage/perfschema/table_tiws_by_table.h
  Table TABLE_IO_WAITS_SUMMARY_BY_TABLE (declarations).
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
  PERFORMANCE_SCHEMA.TABLE_IO_WAITS_SUMMARY_BY_TABLE.
*/
struct row_tiws_by_table {
  /** Column OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
  PFS_object_row m_object;
  /** Columns COUNT/SUM/MIN/AVG/MAX (+_READ, +WRITE). */
  PFS_table_io_stat_row m_stat;
};

class PFS_index_tiws_by_table : public PFS_engine_index {
 public:
  PFS_index_tiws_by_table()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("OBJECT_TYPE"),
        m_key_2("OBJECT_SCHEMA"),
        m_key_3("OBJECT_NAME") {}

  ~PFS_index_tiws_by_table() override = default;

  virtual bool match(const PFS_table_share *table);

 private:
  PFS_key_object_type m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
};

/** Table PERFORMANCE_SCHEMA.TABLE_IO_WAITS_SUMMARY_BY_TABLE. */
class table_tiws_by_table : public PFS_engine_table {
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
  table_tiws_by_table();

 public:
  ~table_tiws_by_table() override = default;

 protected:
  int make_row(PFS_table_share *table_share);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_tiws_by_table m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

 protected:
  PFS_index_tiws_by_table *m_opened_index;
};

/** @} */
#endif
