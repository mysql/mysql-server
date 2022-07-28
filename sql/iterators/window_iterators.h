#ifndef SQL_ITERATORS_WINDOW_ITERATORS_H_
#define SQL_ITERATORS_WINDOW_ITERATORS_H_

/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
#include "sql/iterators/row_iterator.h"

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
  when materializing a temporary table for a window function; if the
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

  If we don't need to buffer rows to evaluate the window functions, execution
  is simple; see WindowIterator for details. In that case, we can just evaluate
  the window functions as we go here, similar to the non-windowing flow.

  If we do need buffering, though, we buffer the row in Read(). Next, we enter a
  loop calling process_buffered_windowing_record, and conditionally return
  the row. That is, if process_buffered_windowing_record was able to complete
  evaluation of a row (cf. output_row_ready), including its window functions
  given how much has already been buffered, we return a row, else we read more
  rows, and postpone evaluation and returning till we have enough rows in the
  buffer.

  When we have read a full partition (or reach EOF), we evaluate any remaining
  rows. Note that since we have to read one row past the current partition to
  detect that that previous row was indeed the last row in a partition, we
  need to re-establish the first row of the next partition when we are done
  processing the current one. This is because the record will be overwritten
  (many times) during evaluation of window functions in the current partition.

  Usually [1], for window execution we have two or three tmp tables per
  windowing step involved (although not all are always materialized;
  they may be just streaming through StreamingIterator):

  - The input table, corresponding to the parent iterator. Holds (possibly
    sorted) records ready for windowing, sorted on expressions concatenated from
    any PARTITION BY and ORDER BY clauses.

  - The output table, as given by temp_table_param: where we write the evaluated
    records from this step. Note that we may optimize away this last write if
    we have no final ORDER BY or DISTINCT.

  - If we have buffering, the frame buffer, held by
    Window::m_frame_buffer[_param].

  [1] This is not always the case. For the first window, if we have no
  PARTITION BY or ORDER BY in the window, and there is more than one table
  in the join, the logical input can consist of more than one table
  (e.g. a NestedLoopIterator).

  The first thing we do in Read() is:
  We copy fields from IN to OUT (copy_fields), and evaluate non-WF functions
  (copy_funcs): those functions then read their arguments from IN and store
  their result into their result_field which is a field in OUT.

  Then, let's take SUM(A+FLOOR(B)) OVER (ROWS 2 FOLLOWING) as example. Above,
  we have stored A and the result of FLOOR in OUT. Now we buffer (save) the row
  from OUT into the FB: For that, we copy both field A and FLOOR's result_field
  from OUT to FB; a single copy_fields() call handles both copy jobs. Then we
  look at the rows we have buffered and may realize that we have enough of the
  frame to calculate SUM for a certain row (not necessarily the one we just
  buffered; might be an earlier row, in our example it is the row which is 2
  rows above the buffered row). If we do, to calculate WFs, we bring back the
  frame's rows; which is done by: first copying field A and FLOOR's result_field
  back from FB to OUT, thus getting in OUT all that SUM needs (A and FLOOR),
  then giving that OUT row to SUM (SUM will then add the row's value to its
  total; that happens in copy_funcs). After we have done that on all rows of the
  frame, we have the values of SUM ready in OUT, we also restore the row which
  owns this SUM value, in the same way as we restored the frame's rows, and we
  return from Read() - we're done for this row. However, on the next Read()
  call, we loop to check if we can calculate one more row with the frame
  we have, and if so, we do, until we can't calculate any more rows -- in which
  case we're back to just buffering.
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

#endif  // SQL_ITERATORS_WINDOW_ITERATORS_H_
