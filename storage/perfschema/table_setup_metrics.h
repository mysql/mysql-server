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

#ifndef TABLE_SETUP_METRICS_H
#define TABLE_SETUP_METRICS_H

/**
  @file storage/perfschema/table_setup_metrics.h
  Table SETUP_METRICS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_instr_class;
struct PFS_metric_class;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_METRICS. */
struct row_setup_metrics {
  /** Columns NAME, METER, ENABLED, METRIC_TYPE, NUM_TYPE, UNIT, DESCRIPTION. */
  PFS_metric_class *m_instr_class;

  // materialized from PFS_metric_class
  const char *m_metric;
  uint m_metric_length;
  const char *m_group;
  uint m_group_length;
  const char *m_unit;
  uint m_unit_length;
  const char *m_description;
  uint m_description_length;
  MetricNumType m_num_type;
  MetricOTELType m_metric_type;
};

class PFS_index_setup_metrics : public PFS_engine_index {
 public:
  explicit PFS_index_setup_metrics(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  ~PFS_index_setup_metrics() override = default;

  virtual bool match(PFS_metric_class *pfs) = 0;
};

class PFS_index_setup_metrics_by_name : public PFS_index_setup_metrics {
 public:
  PFS_index_setup_metrics_by_name()
      : PFS_index_setup_metrics(&m_key), m_key("NAME") {}

  ~PFS_index_setup_metrics_by_name() override = default;

  bool match(PFS_metric_class *pfs) override;

 private:
  PFS_key_metric_name m_key;
};

/** Position of a cursor, for metric iterations. */
struct PFS_metric_index {
  /** Current row index. */
  uint m_index;

  /**
    Constructor.
    @param index the index initial value.
  */
  explicit PFS_metric_index(uint index) : m_index(index) {}

  /**
    Set this index at a given position.
    @param index an index
  */
  void set_at(uint index) { m_index = index; }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const PFS_metric_index *other) { m_index = other->m_index; }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const PFS_metric_index *other) {
    m_index = other->m_index;
    next();
  }

  /** Set this index to the next record. */
  void next() {
    do {
      m_index++;
    } while (m_index < metric_class_max &&
             metric_class_array[m_index - 1].m_key == 0);
  }
};

/** Table PERFORMANCE_SCHEMA.SETUP_METRICS. */
class table_setup_metrics : public PFS_engine_table {
  typedef PFS_metric_index pos_t;

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

  table_setup_metrics();

 public:
  ~table_setup_metrics() override = default;

 private:
  int make_row(PFS_metric_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_metrics m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_setup_metrics *m_opened_index;
};

/** @} */
#endif
