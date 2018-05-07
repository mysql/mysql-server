#ifndef SQL_COMPOSITE_ITERATORS_INCLUDED
#define SQL_COMPOSITE_ITERATORS_INCLUDED

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

/**
  @file composite_iterators.h

  A composite row iterator is one that takes in one or more existing iterators
  and processes their rows in some interesting way. They are usually not bound
  to a single table or similar, but are the inner (non-leaf) nodes of the
  iterator execution tree. They consistently own their source iterator, although
  not its memory (since we never allocate row iterators on the heap--usually on
  a MEM_ROOT or as static parts of READ_RECORD). This means that in the end,
  you'll end up with a single root iterator which then owns everything else
  recursively.

  SortingIterator is also a composite iterator, but is defined in its own file.
 */

#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "sql/item.h"
#include "sql/row_iterator.h"

class JOIN;
class THD;

/**
  An iterator that takes in a stream of rows and passes through only those that
  meet some criteria (i.e., a condition evaluates to true). This is typically
  used for WHERE/HAVING.
 */
class FilterIterator final : public RowIterator {
 public:
  FilterIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                 Item *condition)
      : RowIterator(thd), m_source(move(source)), m_condition(condition) {}

  bool Init() override { return m_source->Init(); }

  int Read() override;

  void UnlockRow() override { m_source->UnlockRow(); }

  std::vector<RowIterator *> children() const override {
    return std::vector<RowIterator *>{m_source.get()};
  }
  std::string DebugString() const override {
    return "Filter: " + ItemToString(m_condition);
  }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  Item *m_condition;
};

/**
  Handles LIMIT and/or OFFSET; Init() eats the first "offset" rows, and Read()
  stops as soon as it's seen "limit" rows (including any skipped by offset).
 */
class LimitOffsetIterator final : public RowIterator {
 public:
  /**
    @param thd Thread context
    @param source Row source
    @param limit Maximum number of rows to read, including the ones skipped by
      offset. Can be HA_POS_ERROR for no limit.
    @param offset Number of initial rows to skip. Can be 0 for no offset.
    @param skipped_rows If not nullptr, is incremented for each row skipped by
      offset.
   */
  LimitOffsetIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                      ha_rows limit, ha_rows offset, ha_rows *skipped_rows)
      : RowIterator(thd),
        m_source(move(source)),
        m_limit(limit),
        m_offset(offset),
        m_skipped_rows(skipped_rows) {}

  bool Init() override;

  int Read() override;

  void UnlockRow() override { m_source->UnlockRow(); }

  std::vector<RowIterator *> children() const override {
    return std::vector<RowIterator *>{m_source.get()};
  }

  std::string DebugString() const override {
    char buf[256];
    if (m_offset == 0) {
      snprintf(buf, sizeof(buf), "Limit: %llu row(s)", m_limit);
    } else if (m_limit == HA_POS_ERROR) {
      snprintf(buf, sizeof(buf), "Offset: %llu row(s)", m_offset);
    } else {
      snprintf(buf, sizeof(buf), "Limit/Offset: %llu/%llu row(s)",
               m_limit - m_offset, m_offset);
    }
    return buf;
  }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  ha_rows m_seen_rows;
  const ha_rows m_limit, m_offset;
  ha_rows *m_skipped_rows;
};

/**
  Handles aggregation (typically used for GROUP BY) for the case where the rows
  are already properly grouped coming in, ie., all rows that are supposed to be
  part of the same group are adjacent in the input stream. (This could be
  because they were sorted earlier, because we are scanning an index that
  already gives us the rows in a group-compatible order, or because there is no
  grouping.)

  AggregateIterator is special in that it's one of the very few row iterators
  that actually change the shape of the rows; some columns are dropped as part
  of aggregation, others (the aggregates) are added. For this reason (and also
  because we need to make copies of the group expressions -- see Read()), it
  conceptually always outputs to a temporary table. If we _are_ outputting to a
  temporary table, that's not a problem -- we take over responsibility for
  copying the group expressions from MaterializeIterator, which would otherwise
  do it.

  However, if we are outputting directly to the user, we need somewhere to store
  the output. This is solved by abusing the slice system; since we only need to
  buffer a single row, we can set up just enough items in the
  REF_SLICE_ORDERED_GROUP_BY slice, so that it can hold a single row. This row
  is then used for our output, and we then switch to it just before the end of
  Read() so that anyone reading from the buffers will get that output.

  This isn't very pretty. What should be done is probably a more abstract
  concept of sending a row around and taking copies of it if needed, as opposed
  to it implicitly staying in the table's buffer. (This would also solve some
  issues in EQRefIterator and when synthesizing NULL rows for outer joins.)
  However, that's a large refactoring.
 */
class AggregateIterator final : public RowIterator {
 public:
  AggregateIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                    JOIN *join)
      : RowIterator(thd), m_source(move(source)), m_join(join) {}

  bool Init() override;
  int Read() override;
  void UnlockRow() override;

  std::vector<RowIterator *> children() const override {
    return std::vector<RowIterator *>{m_source.get()};
  }

  std::string DebugString() const override;

 private:
  unique_ptr_destroy_only<RowIterator> m_source;

  /**
    The join we are part of. It would be nicer not to rely on this,
    but we need a large number of members from there, like which
    aggregate functions we have, the THD, temporary table parameters
    and so on.
   */
  JOIN *m_join = nullptr;

  /// The slice of the fields we are reading from (see the class comment).
  int m_input_slice;

  /// Whether we are about to read the very first row.
  bool m_first_row;

  /// Whether we have seen the last input row.
  bool m_eof;

  /**
    Used to save NULL information in the specific case where we have
    zero input rows.
   */
  table_map m_save_nullinfo;
};

/**
  Similar to AggregateIterator, but asusmes that the actual aggregates are
  already have been filled out (typically by QUICK_RANGE_MIN_MAX), and all the
  iterator needs to do is copy over the non-aggregated fields.
 */
class PrecomputedAggregateIterator final : public RowIterator {
 public:
  PrecomputedAggregateIterator(THD *thd,
                               unique_ptr_destroy_only<RowIterator> source,
                               JOIN *join)
      : RowIterator(thd), m_source(move(source)), m_join(join) {}

  bool Init() override;
  int Read() override;
  void UnlockRow() override;

  std::vector<RowIterator *> children() const override {
    return std::vector<RowIterator *>{m_source.get()};
  }

  std::string DebugString() const override;

 private:
  unique_ptr_destroy_only<RowIterator> m_source;

  /**
    The join we are part of. It would be nicer not to rely on this,
    but we need a large number of members from there, like which
    aggregate functions we have, the THD, temporary table parameters
    and so on.
   */
  JOIN *m_join = nullptr;
};

#endif  // SQL_COMPOSITE_ITERATORS_INCLUDED
