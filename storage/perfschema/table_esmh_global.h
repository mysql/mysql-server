/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef TABLE_ESMH_GLOBAL_H
#define TABLE_ESMH_GLOBAL_H

/**
  @file storage/perfschema/table_esmh_global.h
  Table EVENTS_STATEMENTS_HISTOGRAM_GLOBAL (declarations).
*/

#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esmh_global : public PFS_engine_index {
 public:
  PFS_index_esmh_global()
      : PFS_engine_index(&m_key_1), m_key_1("BUCKET_NUMBER") {}

  ~PFS_index_esmh_global() override = default;

  bool match_bucket(ulong bucket_index);

 private:
  PFS_key_bucket_number m_key_1;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTOGRAM_GLOBAL.
*/

struct PFS_esmh_global_bucket {
  /** Column BUCKET_NUMBER. */
  ulonglong m_count_bucket;
  /** Column COUNT_BUCKET_AND_LOWER. */
  ulonglong m_count_bucket_and_lower;
};

struct PFS_esmh_global_histogram {
  /** Statistics for all buckets. */
  PFS_esmh_global_bucket m_buckets[NUMBER_OF_BUCKETS];
};

struct row_esmh_global {
  /** Column BUCKET_NUMBER. */
  ulong m_bucket_number;
  /** Column BUCKET_TIMER_LOW. */
  ulonglong m_bucket_timer_low;
  /** Column BUCKET_TIMER_HIGH. */
  ulonglong m_bucket_timer_high;
  /** Column COUNT_BUCKET. */
  ulonglong m_count_bucket;
  /** Column COUNT_BUCKET_AND_LOWER. */
  ulonglong m_count_bucket_and_lower;
  /** Column BUCKET_QUANTILE. */
  double m_percentile;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTOGRAM_GLOBAL. */
class table_esmh_global : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_esmh_global();

 public:
  ~table_esmh_global() override = default;

 protected:
  void materialize();
  int make_row(ulong bucket_index);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  PFS_esmh_global_histogram m_materialized_histogram;
  row_esmh_global m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_esmh_global *m_opened_index;

  bool m_materialized;
};

/** @} */
#endif
