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
#include "my_dbug.h"
#include "my_table_map.h"
#include "sql/item.h"
#include "sql/row_iterator.h"
#include "sql/table.h"

class JOIN;
class SELECT_LEX;
class THD;
class Temp_table_param;
template <class T>
class List;

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

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override { m_source->UnlockRow(); }

  std::vector<Child> children() const override;

  std::vector<std::string> DebugString() const override {
    return {"Filter: " + ItemToString(m_condition)};
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

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override { m_source->UnlockRow(); }

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_source.get(), ""}};
  }

  std::vector<std::string> DebugString() const override {
    char buf[256];
    if (m_offset == 0) {
      snprintf(buf, sizeof(buf), "Limit: %llu row(s)", m_limit);
    } else if (m_limit == HA_POS_ERROR) {
      snprintf(buf, sizeof(buf), "Offset: %llu row(s)", m_offset);
    } else {
      snprintf(buf, sizeof(buf), "Limit/Offset: %llu/%llu row(s)",
               m_limit - m_offset, m_offset);
    }
    return {std::string(buf)};
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
  The caller knows the context about where our output goes, and thus also picks
  the appropriate output slice for us.

  This isn't very pretty. What should be done is probably a more abstract
  concept of sending a row around and taking copies of it if needed, as opposed
  to it implicitly staying in the table's buffer. (This would also solve some
  issues in EQRefIterator and when synthesizing NULL rows for outer joins.)
  However, that's a large refactoring.
 */
class AggregateIterator final : public RowIterator {
 public:
  AggregateIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                    JOIN *join, Temp_table_param *temp_table_param,
                    int output_slice)
      : RowIterator(thd),
        m_source(move(source)),
        m_join(join),
        m_output_slice(output_slice),
        m_temp_table_param(temp_table_param) {}

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }
  void UnlockRow() override;

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_source.get(), ""}};
  }

  std::vector<std::string> DebugString() const override;

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

  /// The slice of the fields we are outputting to. See the class comment.
  int m_output_slice;

  /// Whether we are about to read the very first row.
  bool m_first_row;

  /// Whether we have seen the last input row.
  bool m_eof;

  /**
    Used to save NULL information in the specific case where we have
    zero input rows.
   */
  table_map m_save_nullinfo;

  /// The parameters for the temporary table we are materializing into, if any.
  Temp_table_param *m_temp_table_param;
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
                               JOIN *join, Temp_table_param *temp_table_param,
                               int output_slice)
      : RowIterator(thd),
        m_source(move(source)),
        m_join(join),
        m_temp_table_param(temp_table_param),
        m_output_slice(output_slice) {}

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }
  void UnlockRow() override;

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_source.get(), ""}};
  }

  std::vector<std::string> DebugString() const override;

 private:
  unique_ptr_destroy_only<RowIterator> m_source;

  /**
    The join we are part of. It would be nicer not to rely on this,
    but we need a large number of members from there, like which
    aggregate functions we have, the THD, temporary table parameters
    and so on.
   */
  JOIN *m_join = nullptr;

  /// The parameters for the temporary table we are materializing into, if any.
  Temp_table_param *m_temp_table_param;

  /// The slice of the fields we are outputting to.
  int m_output_slice;
};

enum class JoinType { INNER, OUTER, ANTI };

/**
  A simple nested loop join, taking in two iterators (left/outer and
  right/inner) and joining them together. This may, of course, scan the inner
  iterator many times. It is currently the only form of join we have.

  The iterator works as a state machine, where the state records whether we need
  to read a new outer row or not, and whether we've seen any rows from the inner
  iterator at all (if not, an outer join need to synthesize a new NULL row).

  The iterator takes care of activating performance schema batch mode on the
  right iterator if needed; this is typically only used if it is the innermost
  table in the entire join (where the gains from turning on batch mode is the
  largest, and the accuracy loss from turning it off are the least critical).
 */
class NestedLoopIterator final : public RowIterator {
 public:
  NestedLoopIterator(THD *thd,
                     unique_ptr_destroy_only<RowIterator> source_outer,
                     unique_ptr_destroy_only<RowIterator> source_inner,
                     JoinType join_type, bool pfs_batch_mode)
      : RowIterator(thd),
        m_source_outer(move(source_outer)),
        m_source_inner(move(source_inner)),
        m_join_type(join_type),
        m_pfs_batch_mode(pfs_batch_mode) {
    DBUG_ASSERT(m_source_outer != nullptr);
    DBUG_ASSERT(m_source_inner != nullptr);

    // Batch mode makes no sense for anti-joins, since they should only
    // be reading one row.
    DBUG_ASSERT(!(pfs_batch_mode && join_type == JoinType::ANTI));
  }

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    // TODO: write something here about why we can't do this lazily.
    m_source_outer->SetNullRowFlag(is_null_row);
    m_source_inner->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override {
    // Since we don't know which condition that caused the row to be rejected,
    // we can't know whether we could also unlock the outer row
    // (it may still be used as parts of other joined rows).
    if (m_state == READING_FIRST_INNER_ROW || m_state == READING_INNER_ROWS) {
      m_source_inner->UnlockRow();
    }
  }

  std::vector<std::string> DebugString() const override;

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_source_outer.get(), ""},
                              {m_source_inner.get(), ""}};
  }

 private:
  enum {
    NEEDS_OUTER_ROW,
    READING_FIRST_INNER_ROW,
    READING_INNER_ROWS,
    END_OF_ROWS
  } m_state;

  unique_ptr_destroy_only<RowIterator> const m_source_outer;
  unique_ptr_destroy_only<RowIterator> const m_source_inner;
  const JoinType m_join_type;

  /** Whether to use batch mode when scanning the inner iterator. */
  const bool m_pfs_batch_mode;
};

/**
  An iterator that helps invalidating caches. Every time a row passes through it
  or it changes state in any other way, it increments its “generation” counter.
  This allows MaterializeIterator to see whether any of its dependencies has
  changed, and then force a rematerialization -- this is typically used for
  LATERAL tables, where we're joining in a derived table that depends on
  something earlier in the join.
 */
class CacheInvalidatorIterator final : public RowIterator {
 public:
  CacheInvalidatorIterator(THD *thd,
                           unique_ptr_destroy_only<RowIterator> source_iterator,
                           const std::string &name)
      : RowIterator(thd),
        m_source_iterator(move(source_iterator)),
        m_name(name) {}

  bool Init() override {
    ++m_generation;
    return m_source_iterator->Init();
  }

  int Read() override {
    ++m_generation;
    return m_source_iterator->Read();
  }

  void SetNullRowFlag(bool is_null_row) override {
    ++m_generation;
    m_source_iterator->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override { m_source_iterator->UnlockRow(); }
  std::vector<std::string> DebugString() const override;
  std::vector<Child> children() const override {
    return {Child{m_source_iterator.get(), ""}};
  }

  int64_t generation() const { return m_generation; }
  std::string name() const { return m_name; }

 private:
  unique_ptr_destroy_only<RowIterator> m_source_iterator;
  int64_t m_generation = 0;
  std::string m_name;
};

/**
  Handles materialization; the first call to Init() will scan the given iterator
  to the end, store the results in a temporary table (optionally with
  deduplication), and then Read() will allow you to read that table repeatedly
  without the cost of executing the given subquery many times (unless you ask
  for rematerialization).

  When materializing, MaterializeIterator takes care of evaluating any items
  that need so, and storing the results in the fields of the outgoing table --
  which items is governed by the temporary table parameters.

  Conceptually (although not performance-wise!), the MaterializeIterator is a
  no-op if you don't ask for deduplication, and in some cases, we probably
  should elide it (e.g. when scanning a table only once). However, it's not
  necessarily straightforward to do so by just not inserting the iterator,
  as the optimizer will have set up everything (e.g., read sets, or what table
  upstream items will read from) assuming the materialization will happen.
 */
class MaterializeIterator final : public TableRowIterator {
 public:
  // SELECT_LEX is used only to get active options.
  //
  // If “copy_fields_and_items” is set to false, the Field objects in the
  // output row is presumed already to be filled out. This is the case
  // iff there's an AggregateIterator earlier in the chain.
  //
  // The “limit_rows” parameter does the same job as a LimitOffsetIterator
  // right before the MaterializeIterator would have done, except that it
  // works _after_ deduplication (if that is active). It is used for when
  // pushing LIMIT down to MaterializeIterator, so that we can stop
  // materializing when there are enough rows. The deduplication is the
  // reason why this specific limit has to be handled in MaterializeIterator
  // and not using a regular LimitOffsetIterator. Set to HA_POS_ERROR
  // for no limit.
  MaterializeIterator(THD *thd,
                      unique_ptr_destroy_only<RowIterator> subquery_iterator,
                      Temp_table_param *temp_table_param, TABLE *table,
                      unique_ptr_destroy_only<RowIterator> table_iterator,
                      const Common_table_expr *cte, SELECT_LEX *select_lex,
                      JOIN *join, int ref_slice, bool copy_fields_and_items,
                      bool rematerialize, ha_rows limit_rows);

  bool Init() override;
  int Read() override;
  std::vector<std::string> DebugString() const override;

  std::vector<Child> children() const override;

  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }

  // The temporary table is private to us, so there's no need to worry about
  // locks to other transactions.
  void UnlockRow() override {}

  /**
    Add a cache invalidator that must be checked on every Init().
    If its generation has increased since last materialize, we need to
    rematerialize even if m_rematerialize is false.
   */
  void AddInvalidator(const CacheInvalidatorIterator *invalidator);

 private:
  unique_ptr_destroy_only<RowIterator> m_subquery_iterator;
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  /// If we are materializing a CTE, points to it. Otherwise nullptr.
  const Common_table_expr *m_cte;

  Temp_table_param *m_tmp_table_param;
  SELECT_LEX *m_select_lex;

  /// The join we are materializing.
  JOIN *const m_join;

  /// The slice to set when accessing temporary table; used if anything upstream
  /// (e.g. WHERE, HAVING) wants to evaluate values based on its contents.
  const int m_ref_slice;

  /// See constructor.
  const bool m_copy_fields_and_items;

  /// If true, we need to materialize anew for each Init() (because the contents
  /// of the table will depend on some outer non-constant value).
  const bool m_rematerialize;

  /// See constructor.
  const ha_rows m_limit_rows;

  struct Invalidator {
    const CacheInvalidatorIterator *iterator;
    int64_t generation_at_last_materialize;
  };
  Mem_root_array<Invalidator> m_invalidators;

  /// Whether we are deduplicating using a hash field on the temporary
  /// table. (This condition mirrors check_unique_constraint().)
  /// If so, we compute a hash value for every row, look up all rows with
  /// the same hash and manually compare them to the row we are trying to
  /// insert.
  ///
  /// Note that this is _not_ the common way of deduplicating as we go.
  /// The common method is to have a regular index on the table
  /// over the right columns, and in that case, ha_write_row() will fail
  /// with an ignorable error, so that the row is ignored even though
  /// check_unique_constraint() is not called. However, B-tree indexes
  /// have limitations, in particular on length, that sometimes require us
  /// to do this instead. See create_tmp_table() for details.
  bool doing_hash_deduplication() const {
    return table()->hash_field && !table()->no_keyread;
  }
};

/**
  Aggregates unsorted data into a temporary table, using update operations
  to keep running aggregates. After that, works as a MaterializeIterator
  in that it allows the temporary table to be scanned.
 */
class TemptableAggregateIterator final : public TableRowIterator {
 public:
  TemptableAggregateIterator(
      THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
      Temp_table_param *temp_table_param, TABLE *table,
      unique_ptr_destroy_only<RowIterator> table_iterator,
      SELECT_LEX *select_lex, JOIN *join, int ref_slice);

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }
  void UnlockRow() override {}
  std::vector<std::string> DebugString() const override;

  std::vector<Child> children() const override;

 private:
  /// The iterator we are reading rows from.
  unique_ptr_destroy_only<RowIterator> m_subquery_iterator;

  /// The iterator used to scan the resulting temporary table.
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  Temp_table_param *m_tmp_table_param;
  SELECT_LEX *m_select_lex;
  JOIN *const m_join;
  const int m_ref_slice;

  // See MaterializeIterator::doing_hash_deduplication().
  bool using_hash_key() const { return table()->hash_field; }
};

/**
  An iterator that wraps a Table_function (e.g. JSON_TABLE) and allows you to
  iterate over the materialized temporary table. The table is materialized anew
  for every Init().

  TODO: Just wrapping it is probably not the optimal thing to do;
  Table_function is highly oriented around materialization, but never caches.
  Thus, perhaps we should rewrite Table_function to return a RowIterator
  instead of going through a temporary table.
 */
class MaterializedTableFunctionIterator final : public TableRowIterator {
 public:
  MaterializedTableFunctionIterator(
      THD *thd, Table_function *table_function, TABLE *table,
      unique_ptr_destroy_only<RowIterator> table_iterator);

  bool Init() override;
  int Read() override { return m_table_iterator->Read(); }
  std::vector<std::string> DebugString() const override {
    return {{"Materialize table function"}};
  }
  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }

  // The temporary table is private to us, so there's no need to worry about
  // locks to other transactions.
  void UnlockRow() override {}

 private:
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  Table_function *m_table_function;
};

#endif  // SQL_COMPOSITE_ITERATORS_INCLUDED
