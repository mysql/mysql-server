/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

#ifndef TABLE_ESMS_BY_DIGEST_H
#define TABLE_ESMS_BY_DIGEST_H

/**
  @file storage/perfschema/table_esms_by_digest.h
  Table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST (declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "pfs_digest.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esms_by_digest : public PFS_engine_index
{
public:
  PFS_index_esms_by_digest()
    : PFS_engine_index(&m_key_1, &m_key_2),
      m_key_1("SCHEMA_NAME"),
      m_key_2("DIGEST")
  {
  }

  ~PFS_index_esms_by_digest()
  {
  }

  virtual bool match(PFS_statements_digest_stat *pfs);

private:
  PFS_key_schema m_key_1;
  PFS_key_digest m_key_2;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
*/
struct row_esms_by_digest
{
  /** Columns DIGEST/DIGEST_TEXT. */
  PFS_digest_row m_digest;

  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_statement_stat_row m_stat;

  /** Column FIRST_SEEN. */
  ulonglong m_first_seen;
  /** Column LAST_SEEN. */
  ulonglong m_last_seen;

  /** Column QUANTILE_95. */
  ulonglong m_p95;
  /** Column QUANTILE_99. */
  ulonglong m_p99;
  /** Column QUANTILE_999. */
  ulonglong m_p999;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
class table_esms_by_digest : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_esms_by_digest();

public:
  ~table_esms_by_digest()
  {
  }

protected:
  int make_row(PFS_statements_digest_stat *);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_esms_by_digest m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_esms_by_digest *m_opened_index;
};

/** @} */
#endif
