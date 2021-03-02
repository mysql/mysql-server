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

/**
  WindowingIterator is similar to AggregateIterator, but deals with windowed
  aggregates (i.e., OVER expressions). It deals specifically with aggregates
  that don't need to buffer rows.

  If we are outputting to a temporary table -- we take over responsibility
  for storing the fields from MaterializeIterator, which would otherwise do it.
  Otherwise, we do a fair amount of slice switching back and forth to be sure
  to present the right output row to the user. Longer-term, we should probably
  do as AggregateIterator does -- it used to do the same, but now instead saves
  and restores rows, making for a more uniform data flow.
 */
class WindowingIterator final : public RowIterator {
 public:
  WindowingIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
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
  BufferingWindowingIterator is like WindowingIterator, but deals with window
  functions that need to buffer rows.
 */
class BufferingWindowingIterator final : public RowIterator {
 public:
  BufferingWindowingIterator(
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
