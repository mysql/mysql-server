#ifndef SQL_WINDOW_ITERATORS_INCLUDED
#define SQL_WINDOW_ITERATORS_INCLUDED

/* Copyright (c) 2018, 2021, Oracle and/or its affiliates.

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

#include "my_alloc.h"
#include "sql/row_iterator.h"

class JOIN;
class THD;
class Temp_table_param;
class Window;

/*
  WindowIterator is similar to AggregateIterator, but deals with windowed
  aggregates (i.e., OVER expressions). It deals specifically with aggregates
  that don't need to buffer rows.

  Window function execution is centered around temporary table materialization;
  every window corresponds to exactly one materialization (although the
  “materialization” can often be shortcut to streaming). For every window,
  we must materialize/evaluate exactly the aggregates that belong to that
  window, and no others (earlier ones are just copied from the temporary table
  fields, later ones are ignored). Thus, create_tmp_table() has special logic
  when materializing a temporary table for a window functon; if the
  Temp_table_param has m_window set (non-nullptr), we ignore all aggregates that
  don't belong to that window. E.g., assume we have foo() OVER w1, bar() OVER
  w2, baz() OVER w2, quux() OVER w3, the temporary tables and field lists will
  look like:

                     Temp table       |     SELECT list
               foo()   bar()   baz()  |
   before wnd:                        | foo()        bar()        baz()
   window 1:   value   -----   -----  | temp_w1.foo  bar()        baz()
   window 2:   value   value   value  | temp_w2.foo  temp_w2.bar  temp_w2.baz

  In e.g. step 2, w2.foo is simply copied from w1.foo (through
  temp_table_param->copy_fields), while w2.bar and w2.baz are evaluated
  from bar() and baz() (through temp_table_param->copy_func).

  WindowIterator only takes responsibility for resetting the window functions
  on a window boundary; the rest is handled by correct input ordering (typically
  through sorting) and delicate ordering of copy_funcs() calls.
  (BufferingWindowIterator, below, has more intricate logic for feeding rows
  into the window functions, and only stopping to output new rows whenever
  process_buffered_windowing_record() signals it is time to do that -- but apart
  from that, the separation of concerns is much the same.)

  In particular, ordering of copies gets complicated when we have expressions
  that depend on window functions, or even window functions from multiple
  windows. Say we have something like foo() OVER w1 + bar() OVER w2.
  split_sum_funcs() will have made slices for us so that we have separate items
  for foo() and bar():

                           base slice    window 1 output   window 2 output
    0: <ref1> + <ref2>     +             +                 temp_w2.+
    1: foo() OVER w1       foo()         temp_w1.foo       temp_w2.foo
    2: bar() OVER w2       bar()         N/A               temp_w2.bar

  We first copy fields and non-WF-related functions into the output table,
  from the previous slice (e.g., for window 2, we copy temp_w1.foo to
  temp_w2.foo); these are always safe. Then, we copy/evaluate the window
  functions themselves (#1 or #2, depending on which window we are evaluating).
  Finally, we get to the composite item (#0); in order not to evaluate the
  window functions anew, the references in the add expression must refer to the
  temporary table fields that we just populated, so we need to be in the
  _output_ slice. When buffering is active (BufferingWindowIterator), we have
  more phases to deal with; it would be good to have this documented as well.

  If we are outputting to a temporary table, we take over responsibility
  for storing the fields from MaterializeIterator, which would otherwise do it.
 */
class WindowIterator final : public RowIterator {
 public:
  WindowIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                 Temp_table_param *temp_table_param,  // Includes the window.
                 JOIN *join, int output_slice);

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    // There's nothing we can do here.
  }

 private:
  /// The iterator we are reading from.
  unique_ptr_destroy_only<RowIterator> const m_source;

  /// Parameters for the temporary table we are outputting to.
  Temp_table_param *m_temp_table_param;

  /// The window function itself.
  Window *m_window;

  /// The join we are a part of.
  JOIN *m_join;

  /// The slice we will be using when reading rows.
  int m_input_slice;

  /// The slice we will be using when outputting rows.
  int m_output_slice;
};

/**
  BufferingWindowIterator is like WindowIterator, but deals with window
  functions that need to buffer rows.
 */
class BufferingWindowIterator final : public RowIterator {
 public:
  BufferingWindowIterator(
      THD *thd, unique_ptr_destroy_only<RowIterator> source,
      Temp_table_param *temp_table_param,  // Includes the window.
      JOIN *join, int output_slice);

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    // There's nothing we can do here.
  }

 private:
  int ReadBufferedRow(bool new_partition_or_eof);

  /// The iterator we are reading from.
  unique_ptr_destroy_only<RowIterator> const m_source;

  /// Parameters for the temporary table we are outputting to.
  Temp_table_param *m_temp_table_param;

  /// The window function itself.
  Window *m_window;

  /// The join we are a part of.
  JOIN *m_join;

  /// The slice we will be using when reading rows.
  int m_input_slice;

  /// The slice we will be using when outputting rows.
  int m_output_slice;

  /// If true, we may have more buffered rows to process that need to be
  /// checked for before reading more rows from the source.
  bool m_possibly_buffered_rows;

  /// Whether the last input row started a new partition, and was tucked away
  /// to finalize the previous partition; if so, we need to bring it back
  /// for processing before we read more rows.
  bool m_last_input_row_started_new_partition;

  /// Whether we have seen the last input row.
  bool m_eof;
};

#endif  // SQL_WINDOW_ITERATORS_INCLUDED
