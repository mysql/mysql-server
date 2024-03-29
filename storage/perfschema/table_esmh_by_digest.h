/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef TABLE_ESMH_BY_DIGEST_H
#define TABLE_ESMH_BY_DIGEST_H

/**
  @file storage/perfschema/table_esmh_by_digest.h
  Table EVENTS_STATEMENTS_HISTOGRAM_BY_DIGEST (declarations).
*/

#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTOGRAM_BY_DIGEST.
  Index 1 on digest array (0 based).
  Index 2 on buckets (0 based).
*/
struct pos_esmh_by_digest : public PFS_double_index {
  pos_esmh_by_digest() : PFS_double_index(0, 0) {}

  inline void reset() {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline bool has_more_digest() { return (m_index_1 < digest_max); }

  inline void next_digest() {
    m_index_1++;
    m_index_2 = 0;
  }

  inline bool has_more_buckets() { return (m_index_2 < NUMBER_OF_BUCKETS); }

  inline void next_bucket() { m_index_2++; }
};

class PFS_index_esmh_by_digest : public PFS_engine_index {
 public:
  PFS_index_esmh_by_digest()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("SCHEMA_NAME"),
        m_key_2("DIGEST"),
        m_key_3("BUCKET_NUMBER") {}

  ~PFS_index_esmh_by_digest() override = default;

  bool match_digest(PFS_statements_digest_stat *pfs);
  bool match_bucket(ulong bucket_index);

 private:
  PFS_key_schema m_key_1;
  PFS_key_digest m_key_2;
  PFS_key_bucket_number m_key_3;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTOGRAM_BY_DIGEST.
*/

struct PFS_esmh_by_digest_bucket {
  /** Column COUNT_BUCKET. */
  ulonglong m_count_bucket;
  /** Column COUNT_BUCKET_AND_LOWER. */
  ulonglong m_count_bucket_and_lower;
};

struct PFS_esmh_by_digest_histogram {
  /** Columns SCHEMA_NAME, DIGEST. */
  PFS_digest_row m_digest;

  /** Statistics for all buckets. */
  PFS_esmh_by_digest_bucket m_buckets[NUMBER_OF_BUCKETS];
};

struct row_esmh_by_digest {
  /*
    No need to repeat SCHEMA_NAME, DIGEST here,
    only materialize the parts of the row that changes per bucket.
  */
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

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTOGRAM_BY_DIGEST. */
class table_esmh_by_digest : public PFS_engine_table {
  typedef pos_esmh_by_digest pos_t;

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

  table_esmh_by_digest();

 public:
  ~table_esmh_by_digest() override = default;

 protected:
  void materialize(PFS_statements_digest_stat *stat);
  int make_row(PFS_statements_digest_stat *stat, ulong bucket_index);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  PFS_statements_digest_stat *m_materialized_digest;
  PFS_esmh_by_digest_histogram m_materialized_histogram;
  row_esmh_by_digest m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_esmh_by_digest *m_opened_index;
};

/** @} */
#endif
