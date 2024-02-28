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

/**
  @file storage/perfschema/table_setup_metrics.cc
  Table SETUP_METRICS (implementation).
*/

#include "storage/perfschema/table_setup_metrics.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_setup_metrics::m_table_lock;

Plugin_table table_setup_metrics::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_metrics",
    /* Definition */
    "  NAME VARCHAR(63) not null,\n"
    "  METER VARCHAR(63) not null,\n"
    "  METRIC_TYPE ENUM ('ASYNC COUNTER', 'ASYNC UPDOWN COUNTER', 'ASYNC GAUGE "
    "COUNTER') not null,\n"
    "  NUM_TYPE ENUM ('INTEGER', 'DOUBLE') not null,\n"
    "  UNIT VARCHAR(63),\n"
    "  DESCRIPTION VARCHAR(1023),\n"
    "  PRIMARY KEY (NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_metrics::m_share = {
    &pfs_readonly_acl,
    table_setup_metrics::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_setup_metrics::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_setup_metrics_by_name::match(PFS_metric_class *pfs) {
  if (m_fields >= 1) {
    return m_key.match(pfs);
  }

  return true;
}

PFS_engine_table *table_setup_metrics::create(PFS_engine_table_share *) {
  return new table_setup_metrics();
}

ha_rows table_setup_metrics::get_row_count() { return metric_class_count(); }

table_setup_metrics::table_setup_metrics()
    : PFS_engine_table(&m_share, &m_pos),
      m_pos(1),
      m_next_pos(1),
      m_opened_index(nullptr) {}

void table_setup_metrics::reset_position() {
  m_pos.m_index = 1;
  m_next_pos.m_index = 1;
}

int table_setup_metrics::rnd_next() {
  /* Do not advertise metrics when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos);; m_pos.next()) {
    PFS_metric_class *instr_class = find_metric_class(m_pos.m_index);

    if (instr_class) {
      m_next_pos.set_after(&m_pos);
      return make_row(instr_class);
    }

    return HA_ERR_END_OF_FILE;
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_metrics::rnd_pos(const void *pos) {
  /* Do not advertise metrics when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);

  PFS_metric_class *instr_class = find_metric_class(m_pos.m_index);

  if (instr_class) {
    return make_row(instr_class);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_setup_metrics::index_init(uint idx [[maybe_unused]], bool) {
  PFS_index_setup_metrics_by_name *result;

  assert(idx == 0);
  result = PFS_NEW(PFS_index_setup_metrics_by_name);

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_metrics::index_next() {
  /* Do not advertise metrics when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos);; m_pos.next()) {
    PFS_metric_class *instr_class = find_metric_class(m_pos.m_index);

    if (instr_class) {
      if (m_opened_index->match(instr_class)) {
        if (!make_row(instr_class)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    } else {
      return HA_ERR_END_OF_FILE;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_metrics::make_row(PFS_metric_class *klass) {
  /* Protect this reader against an instrument delete. */
  pfs_optimistic_state lock = pfs_optimistic_state();
  klass->m_lock.begin_optimistic_lock(&lock);

  m_row.m_instr_class = klass;

  // materialize the row
  m_row.m_metric = klass->m_metric;
  m_row.m_metric_length = klass->m_metric_length;
  m_row.m_group = klass->m_group;
  m_row.m_group_length = klass->m_group_length;
  m_row.m_unit = klass->m_unit;
  m_row.m_unit_length = klass->m_unit_length;
  m_row.m_description = klass->m_description;
  m_row.m_description_length = klass->m_description_length;
  m_row.m_num_type = klass->m_num_type;
  m_row.m_metric_type = klass->m_metric_type;

  if (!klass->m_lock.end_optimistic_lock(&lock)) {
    return 1;
  }

  return 0;
}

int table_setup_metrics::read_row_values(TABLE *table, unsigned char *buf,
                                         Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  /*
    The row may not exist, the metric instrument classes
    are not static and may disappear.
  */
  assert(m_row.m_instr_class->m_key > 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* NAME */
          set_field_varchar_utf8mb4(f, m_row.m_metric, m_row.m_metric_length);
          break;
        case 1: /* METER */
          set_field_varchar_utf8mb4(f, m_row.m_group, m_row.m_group_length);
          break;
        case 2: /* METRIC_TYPE */
          set_field_enum(f, static_cast<ulonglong>(m_row.m_metric_type) + 1);
          break;
        case 3: /* NUM_TYPE */
          set_field_enum(f, static_cast<ulonglong>(m_row.m_num_type) + 1);
          break;
        case 4: /* UNIT */
          set_field_varchar_utf8mb4(f, m_row.m_unit, m_row.m_unit_length);
          break;
        case 5: /* DESCRIPTION */
          set_field_varchar_utf8mb4(f, m_row.m_description,
                                    m_row.m_description_length);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
