/* Copyright (c) 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_SETUP_LOGGERS_H
#define TABLE_SETUP_LOGGERS_H

/**
  @file storage/perfschema/table_setup_loggers.h
  Table SETUP_LOGGERS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_instr_class;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_LOGGERS. */
struct row_setup_loggers {
  PFS_logger_class *m_instr_class;
  /** Column NAME. */
  char m_logger_name[MAX_LOGGER_NAME_LEN + 1];
  uint m_logger_name_length;
  /** Column LEVEL. */
  OTELLogLevel m_level;
  /** Column DESCRIPTION. */
  char m_description[COL_INFO_SIZE];
  uint m_description_length;
};

#define LOGGERS_PREALLOC (size_t)50

/** Table PERFORMANCE_SCHEMA.SETUP_LOGGERS. */
class table_setup_loggers : public PFS_engine_table {
  typedef PFS_simple_index pos_t;
  typedef Prealloced_array<row_setup_loggers, LOGGERS_PREALLOC> Loggers_array;

 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  int update_row_values(TABLE *table, const unsigned char *old_buf,
                        unsigned char *new_buf, Field **fields) override;

  table_setup_loggers();

 public:
  ~table_setup_loggers() override = default;

 private:
  int make_row(PFS_logger_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_loggers m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;
};

/** @} */
#endif
