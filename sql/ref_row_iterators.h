#ifndef SQL_REF_ROW_ITERATORS_H
#define SQL_REF_ROW_ITERATORS_H

/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>
#include <memory>

#include "my_alloc.h"
#include "my_inttypes.h"
#include "sql/basic_row_iterators.h"
#include "sql/row_iterator.h"
#include "sql/sql_sort.h"

class QEP_TAB;
class THD;
struct TABLE;
struct TABLE_REF;

/**
  For each record on the left side of a join (given in Init()), returns one or
  more matching rows from the given table, i.e., WHERE column=\<ref\>.
 */
template <bool Reverse>
class RefIterator final : public RowIterator {
 public:
  RefIterator(THD *thd, TABLE *table, TABLE_REF *ref, bool use_order,
              QEP_TAB *qep_tab);

  bool Init() override;
  int Read() override;

 private:
  TABLE_REF *const m_ref;
  const bool m_use_order;
  QEP_TAB *const m_qep_tab;
  bool m_first_record_since_init;
};

/**
  Like RefIterator, but after it's returned all its rows, will also search for
  rows that match NULL, i.e., WHERE column=\<ref\> OR column IS NULL.
 */
class RefOrNullIterator final : public RowIterator {
 public:
  RefOrNullIterator(THD *thd, TABLE *table, TABLE_REF *ref, bool use_order,
                    QEP_TAB *qep_tab);

  bool Init() override;
  int Read() override;

 private:
  TABLE_REF *const m_ref;
  const bool m_use_order;
  bool m_reading_first_row;
  QEP_TAB *const m_qep_tab;
};

/**
  Like RefIterator, but used in situations where we're guaranteed to have
  exactly zero or one rows for each reference (due to e.g. unique constraints).
  It adds extra buffering to reduce the number of calls to the storage engine in
  the case where many consecutive rows on the left side contain the same value.
 */
class EQRefIterator final : public RowIterator {
 public:
  EQRefIterator(THD *thd, TABLE *table, TABLE_REF *ref, bool use_order);

  bool Init() override;
  int Read() override;
  void UnlockRow() override;

 private:
  TABLE_REF *const m_ref;
  const bool m_use_order;
  bool m_first_record_since_init;
};

/**
  An iterator that reads from a table where only a single row is known to be
  matching, no matter what's on the left side, i.e., WHERE column=\<const\>.
 */
class ConstIterator final : public RowIterator {
 public:
  ConstIterator(THD *thd, TABLE *table, TABLE_REF *table_ref);

  bool Init() override;
  int Read() override;

 private:
  TABLE_REF *const m_ref;
  bool m_first_record_since_init;
};

/** An iterator that does a search through a full-text index. */
class FullTextSearchIterator final : public RowIterator {
 public:
  FullTextSearchIterator(THD *thd, TABLE *table, TABLE_REF *ref,
                         bool use_order);
  ~FullTextSearchIterator();

  bool Init() override;
  int Read() override;

 private:
  TABLE_REF *const m_ref;
  const bool m_use_order;
};

/*
  This is for QS_DYNAMIC_RANGE, i.e., "Range checked for each
  record". The trace for the range analysis below this point will
  be printed with different ranges for every record to the left of
  this table in the join; the range optimizer can either select any
  QUICK_SELECT_I (aka IndexRangeScanIterator) or a full table
  scan, and any Read() is just proxied over to that.

  Note in particular that this means the range optimizer will be
  executed anew on every single call to Init(), and modify the
  query plan accordingly! It is not clear whether this is an actual
  win in a typical query.
 */
class DynamicRangeIterator final : public RowIterator {
 public:
  DynamicRangeIterator(THD *thd, TABLE *table, QEP_TAB *qep_tab);

  bool Init() override;
  int Read() override;

 private:
  QEP_TAB *m_qep_tab;

  // See IteratorHolder in records.h; this is the same pattern,
  // just with fewer candidates.
  unique_ptr_destroy_only<RowIterator> m_iterator;
  union MiniIteratorHolder {
    MiniIteratorHolder() {}
    ~MiniIteratorHolder() {}

    TableScanIterator table_scan;
    IndexRangeScanIterator index_range_scan;
  } m_iterator_holder;

  /**
    Used by optimizer tracing to decide whether or not dynamic range
    analysis of this select has been traced already. If optimizer
    trace option DYNAMIC_RANGE is enabled, range analysis will be
    traced with different ranges for every record to the left of this
    table in the join. If disabled, range analysis will only be traced
    for the first range.
  */
  bool m_quick_traced_before = false;
};

/**
   Read a table *assumed* to be included in execution of a pushed join.
   This is the counterpart of RefIterator / EQRefIterator for child
   tables in a pushed join.

   When the table access is performed as part of the pushed join,
   all 'linked' child colums are prefetched together with the parent row.
   The handler will then only format the row as required by MySQL and set
   table status accordingly.

   However, there may be situations where the prepared pushed join was not
   executed as assumed. It is the responsibility of the handler to handle
   these situation by letting @c ha_index_read_pushed() then effectively do a
   plain old' index_read_map(..., HA_READ_KEY_EXACT);
 */
class PushedJoinRefIterator final : public RowIterator {
 public:
  PushedJoinRefIterator(THD *thd, TABLE *table, TABLE_REF *ref, bool use_order);

  bool Init() override;
  int Read() override;

 private:
  TABLE_REF *const m_ref;
  const bool m_use_order;
  bool m_first_record_since_init;
};

#endif  // SQL_REF_ROW_ITERATORS_H
