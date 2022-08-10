#ifndef SQL_ITERATORS_TIMING_ITERATOR_H_
#define SQL_ITERATORS_TIMING_ITERATOR_H_

/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <inttypes.h>
#include <stdio.h>
#include <chrono>

#include "my_alloc.h"
#include "sql/iterators/row_iterator.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"

/**
   This class is used in implementing the 'EXPLAIN ANALYZE' command.
   It maintains a set of profiling data.
*/
class IteratorProfilerImpl final : public IteratorProfiler {
 public:
  // To avoid a lot of repetitive writing.
  using steady_clock = std::chrono::steady_clock;
  using duration = steady_clock::time_point::duration;
  using TimeStamp = steady_clock::time_point;

  /** Return current time.*/
  static TimeStamp Now() {
#if defined(__linux__)
    // Work around very slow libstdc++ implementations of std::chrono
    // (those compiled with _GLIBCXX_USE_CLOCK_GETTIME_SYSCALL).
    timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return steady_clock::time_point(
        steady_clock::duration(std::chrono::seconds(tp.tv_sec) +
                               std::chrono::nanoseconds(tp.tv_nsec)));
#else
    return steady_clock::now();
#endif
  }

  double GetFirstRowMs() const override {
    return DurationToMs(m_elapsed_first_row);
  }
  double GetLastRowMs() const override {
    return DurationToMs(m_elapsed_first_row + m_elapsed_other_rows);
  }

  uint64_t GetNumInitCalls() const override { return m_num_init_calls; }
  uint64_t GetNumRows() const override { return m_num_rows; }

  /** Mark the end of an iterator->Init() call.*/
  void StopInit(TimeStamp start_time) {
    m_elapsed_first_row += Now() - start_time;
    m_num_init_calls++;
    m_first_row = true;
  }

  /**
     Update the number of rows read. Note that this function is only called
     for iterator where we read all rows during iterator->Init()
     (@see MaterializeIterator and @see TemptableAggregateIterator).
  */
  void IncrementNumRows(uint64_t materialized_rows) {
    m_num_rows += materialized_rows;
  }

  /**
      Mark the end of an iterator->Read() call.
      @param start_time time when Read() started.
      @param read_ok 'true' if Read() was successful.
  */
  void StopRead(TimeStamp start_time, bool read_ok) {
    if (m_first_row) {
      m_elapsed_first_row += Now() - start_time;
      m_first_row = false;
    } else {
      m_elapsed_other_rows += Now() - start_time;
    }
    if (read_ok) {
      m_num_rows++;
    }
  }

 private:
  static double DurationToMs(duration dur) {
    return std::chrono::duration<double>(dur).count() * 1e3;
  }

  /** The number of loops.*/
  uint64_t m_num_init_calls{0};

  /** The number of rows fetched. (Sum for all loops.)*/
  uint64_t m_num_rows{0};

  /** True if we are about to read the first row.*/
  bool m_first_row;

  /**
     Elapsed time in all calls to m_iterator.Init() and Read() for the
     first row.
  */
  duration m_elapsed_first_row{0};

  /**
      Elapsed time in all calls to m_iterator.Read() for all but the first
      row.
  */
  duration m_elapsed_other_rows{0};
};

/**
  An iterator template that wraps a RowIterator, such that all calls to Init()
  and Read() are timed (all others are passed through unchanged, and possibly
  even inlined, since all RowIterator implementations are final). This is used
  for EXPLAIN ANALYZE.

  Note that MaterializeIterator does not use this class. Doing so
  would give misleading measurements. MaterializeIterator has an
  internal member iterator (m_table_iterator) that iterates over the
  materialized result. Calls to Init()/Read() on that iterator goes
  via Init()/Read() on the MaterializeIterator. And the internal
  iterator is listed above MaterializeIterator in 'EXPLAIN ANALYZE'
  output. Its elapsed time values should thus include both the cost of
  materialization and iterating over the result, while the entry for
  MaterializeIterator should only show the time spent on
  materialization. But if we used TimingIterator, the entry for
  MaterializeIterator would give the sum of time spent on both
  materialization and iteration, and the entry for the internal
  iterator would only show the time spent on iterating over the
  materialized result. (See also Bug #33834146 "'EXPLAIN ANALYZE' cost
  estimates and elapsed time values are not cumulative"). This also
  applies to TemptableAggregateIterator. These classes therefore have
  other mechanisms for obtaining profiling data.

  See also NewIterator, below.
 */
template <class RealIterator>
class TimingIterator final : public RowIterator {
 public:
  template <class... Args>
  TimingIterator(THD *thd, Args &&... args)
      : RowIterator(thd), m_iterator(thd, std::forward<Args>(args)...) {}

  bool Init() override {
    const IteratorProfilerImpl::TimeStamp start_time =
        IteratorProfilerImpl::Now();
    bool err = m_iterator.Init();
    m_profiler.StopInit(start_time);
    return err;
  }

  int Read() override {
    const IteratorProfilerImpl::TimeStamp start_time =
        IteratorProfilerImpl::Now();
    int err = m_iterator.Read();
    m_profiler.StopRead(start_time, err == 0);
    return err;
  }

  void SetNullRowFlag(bool is_null_row) override {
    m_iterator.SetNullRowFlag(is_null_row);
  }
  void UnlockRow() override { m_iterator.UnlockRow(); }
  void StartPSIBatchMode() override { m_iterator.StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_iterator.EndPSIBatchModeIfStarted();
  }

  void SetOverrideProfiler(const IteratorProfiler *profiler) override {
    m_override_profiler = profiler;
  }

  const IteratorProfiler *GetProfiler() const override {
    return m_override_profiler == nullptr ? &m_profiler : m_override_profiler;
  }

  RealIterator *real_iterator() override { return &m_iterator; }
  const RealIterator *real_iterator() const override { return &m_iterator; }

 private:
  /** This maintains the profiling measurements.*/
  IteratorProfilerImpl m_profiler;

  /**
     For iterators over materialized tables we must make profiling
     measurements in a different way. This field keeps those measurements.
  */
  const IteratorProfiler *m_override_profiler{nullptr};

  RealIterator m_iterator;
};

// Allocates a new iterator on the given MEM_ROOT. The MEM_ROOT must live
// for at least as long as the iterator does.
//
// If we are in EXPLAIN ANALYZE, the iterator is wrapped in a TimingIterator<T>,
// so that it collects timing information. For this reason, nearly all
// instantiations of iterators should go through this function.

template <class RealIterator, class... Args>
unique_ptr_destroy_only<RowIterator> NewIterator(THD *thd, MEM_ROOT *mem_root,
                                                 Args &&... args) {
  if (thd->lex->is_explain_analyze) {
    return unique_ptr_destroy_only<RowIterator>(
        new (mem_root)
            TimingIterator<RealIterator>(thd, std::forward<Args>(args)...));
  } else {
    return unique_ptr_destroy_only<RowIterator>(
        new (mem_root) RealIterator(thd, std::forward<Args>(args)...));
  }
}

#endif  // SQL_ITERATORS_TIMING_ITERATOR_H_
