/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_SETUP_METERS_H
#define TABLE_SETUP_METERS_H

/**
  @file storage/perfschema/table_setup_meters.h
  Table SETUP_METERS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_instr_class;
struct PFS_meter_class;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_METERS. */
struct row_setup_meters {
  /** Columns NAME, ENABLED. */
  PFS_meter_class *m_instr_class;

  // materialized from PFS_meter_class
  const char *m_meter;
  uint m_meter_length;
  uint m_frequency;
  const char *m_description;
  uint m_description_length;
  bool m_enabled;
};

class PFS_index_setup_meters : public PFS_engine_index {
 public:
  explicit PFS_index_setup_meters(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  ~PFS_index_setup_meters() override = default;

  virtual bool match(PFS_meter_class *pfs) = 0;
};

class PFS_index_setup_meters_by_name : public PFS_index_setup_meters {
 public:
  PFS_index_setup_meters_by_name()
      : PFS_index_setup_meters(&m_key), m_key("NAME") {}

  ~PFS_index_setup_meters_by_name() override = default;

  bool match(PFS_meter_class *pfs) override;

 private:
  PFS_key_meter_name m_key;
};

/** Position of a cursor, for meter iterations. */
struct PFS_meter_index {
  /** Current row index. */
  uint m_index;

  /**
    Constructor.
    @param index the index initial value.
  */
  explicit PFS_meter_index(uint index) : m_index(index) {}

  /**
    Set this index at a given position.
    @param index an index
  */
  void set_at(uint index) { m_index = index; }

  /**
    Set this index at a given position.
    @param other a position
   */
  void set_at(const PFS_meter_index *other) { m_index = other->m_index; }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const PFS_meter_index *other) {
    m_index = other->m_index;
    next();
  }

  /** Set this index to the next record. */
  void next() {
    do {
      m_index++;
    } while (m_index < meter_class_max &&
             meter_class_array[m_index - 1].m_key == 0);
  }
};

/** Table PERFORMANCE_SCHEMA.SETUP_METERS. */
class table_setup_meters : public PFS_engine_table {
  typedef PFS_meter_index pos_t;

 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  int update_row_values(TABLE *table, const unsigned char *old_buf,
                        unsigned char *new_buf, Field **fields) override;

  table_setup_meters();

 public:
  ~table_setup_meters() override = default;

 private:
  int make_row(PFS_meter_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_meters m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_setup_meters *m_opened_index;
};

/** @} */
#endif
