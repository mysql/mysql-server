/*****************************************************************************

Copyright (c) 2014, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0stage.h
 Supplementary code to performance schema stage instrumentation.

 Created Nov 12, 2014 Vasil Dimov
 *******************************************************/

#ifndef ut0stage_h
#define ut0stage_h

#include <math.h>
#include <algorithm>

#include "dict0mem.h"              /* dict_index_t */
#include "mysql/psi/mysql_stage.h" /* mysql_stage_inc_work_completed */
#include "row0log.h"               /* row_log_estimate_work() */
#include "srv0srv.h"               /* Alter_stage */

// Forward declaration.
class Alter_stage;
using Alter_stages = std::vector<Alter_stage *, ut::allocator<Alter_stage *>>;

#ifdef HAVE_PSI_STAGE_INTERFACE

/** Class used to report ALTER TABLE progress via performance_schema.
The only user of this class is the ALTER TABLE code and it calls the methods
in the following order
constructor
begin_phase_read_pk()
  multiple times:
    n_pk_recs_inc() // once per record read
    inc(1) // once per page read
end_phase_read_pk()
if any new indexes are being added, for each one:
  begin_phase_sort()
    multiple times:
      inc(1) // once per every m_recs_req_for_prog records sorted
  begin_phase_insert()
    multiple times:
      inc(1) // once per every m_recs_req_for_prog records inserted
  being_phase_log_index()
    multiple times:
      inc() // once per log-block applied
begin_phase_flush()
    multiple times:
      inc() // once per page flushed
begin_phase_log_table()
    multiple times:
      inc() // once per log-block applied
begin_phase_end()
destructor

This class knows the specifics of each phase and tries to increment the
progress in an even manner across the entire ALTER TABLE lifetime. */
class Alter_stage {
 public:
  /** Constructor.
  @param[in]    pk      primary key of the old table */
  explicit Alter_stage(const dict_index_t *pk) noexcept
      : m_pk(pk), m_recs_req_for_prog(1), m_cur_phase(NOT_STARTED) {}

  /** Copy constructor. "Inherits" the current state of rhs.
  @param[in] rhs                Instance to copy current state from. */
  explicit Alter_stage(const Alter_stage &rhs) noexcept;

  /** Destructor. */
  ~Alter_stage();

  /** Flag an ALTER TABLE start (read primary key phase).
  @param[in]    n_sort_indexes  number of indexes that will be sorted
  during ALTER TABLE, used for estimating the total work to be done */
  void begin_phase_read_pk(size_t n_sort_indexes);

  /** Increment the number of records in PK (table) with 1.
  This is used to get more accurate estimate about the number of
  records per page which is needed because some phases work on
  per-page basis while some work on per-record basis and we want
  to get the progress as even as possible.
  @param[in] n Number of new records to report. */
  void n_pk_recs_inc(uint64_t n = 1);

  /** Flag either one record or one page processed, depending on the
  current phase.
  @param[in]    inc_val flag this many units processed at once */
  void inc(uint64_t inc_val);

  /** Increment the progress if we have crossed the threshold
  for unreported records, or if it is the last report.
  @param[in, out]    unreported_recs  Number of records not updated to
                                      Alter_stage up till now.
                                      After reporting sets itself to 0.
  @param[in]         is_last_report   if true, update the status irrespective
                                      of number of unreported_recs.
  */
  void inc_progress_if_needed(uint64_t &unreported_recs,
                              bool is_last_report = false);

  /** Flag the end of reading of the primary key.
  Here we know the exact number of pages and records and calculate
  the number of records per page and refresh the estimate. */
  void end_phase_read_pk();

  /** Flag the beginning of the sort phase.
  @param[in]    sort_multi_factor       since merge sort processes
  one page more than once we only update the estimate once per this
  many pages processed. */
  void begin_phase_sort(double sort_multi_factor);

  /** Flag the beginning of the insert phase. */
  void begin_phase_insert();

  /** Flag the beginning of the flush phase.
  @param[in]    n_flush_pages   this many pages are going to be
  flushed */
  void begin_phase_flush(page_no_t n_flush_pages);

  /** Flag the beginning of the log index phase. */
  void begin_phase_log_index();

  /** Flag the beginning of the log table phase. */
  void begin_phase_log_table();

  /** Flag the beginning of the end phase. */
  void begin_phase_end();

  /** Aggregate the results of the build from the sub builds.
  @param[in] alter_stages       Sub stages to aggregate. */
  void aggregate(const Alter_stages &alter_stages) noexcept;

 private:
  /** Check whether we have seen enough records to increment the progress.
  @param[in]    inc_val this many units processed up til now
  @return true, if we have seen records as much to increment the progress. */
  bool should_inc_progress(uint64_t inc_val) const;

  /** Update the estimate of total work to be done. */
  void reestimate();

  /** Change the current phase.
  @param[in]    new_stage       pointer to the new stage to change to */
  void change_phase(const PSI_stage_info *new_stage);

 private:
  using Counter = std::pair<uint64_t, uint64_t>;
  using Progress = std::pair<PSI_stage_progress *, Counter>;
  using Stage = std::pair<const PSI_stage_info *, Progress>;
  using Stages = std::vector<Stage, ut::allocator<Stage>>;

  /** Progress counters for the various stages. */
  Stage m_stage{};

  /** Collection of all [previous + current] stages.
  Can only be modified between stages. */
  Stages m_stages{};

  /** Mutex required to update values for m_n_pk_pages
  and m_n_flush_pages.
  We also use this mutex for reestimating & updating the progress,
  as it is done parallelly by multiple threads.*/
  IB_mutex m_mutex{LATCH_ID_ALTER_STAGE};

  /** Old table PK. Used for calculating the estimate. */
  const dict_index_t *m_pk{};

  /** Number of records in the primary key (table), including delete
  marked records. */
  std::atomic<uint64_t> m_n_pk_recs{};

  /** Number of leaf pages in the primary key. Protected by m_mutex. */
  page_no_t m_n_pk_pages{};

  /** Estimated number of records per page in the primary key.
  Can only be modified between stages. */
  double m_n_recs_per_page{};

  /** Number of records for which we increment the progress.
  For READ_PK phase 1
  For SORT phase m_n_recs_per_page * m_sort_multi_factor
  For INSERT phase m_n_recs_per_page
  Can only be modified between stages. */
  uint64_t m_recs_req_for_prog{};

  /** Number of indexes that are being added.
  Is only set during begin_phase_read_pk(). */
  size_t m_n_sort_indexes{};

  /** During the sort phase, increment the counter once per this
  many pages processed. This is because sort processes one page more
  than once. Can only be modified between stages. */
  uint64_t m_sort_multi_factor{};

  /** Number of pages to flush. Protected by m_mutex. */
  page_no_t m_n_flush_pages{};

  /** Current phase. */
  enum {
    /** Init phase. */
    NOT_STARTED = 0,

    /** Scan phase. */
    READ_PK = 1,

    /** Sort phase. */
    SORT = 2,

    /** Bulk load/insert phase. */
    INSERT = 3,

    /** Flush non-redo logged pages phase. */
    FLUSH = 4,

    /** Apply entries from the row log to the index after creation. */
    LOG_INDEX = 5,

    /** Apply entries from the row log to the table after the build. */
    LOG_TABLE = 6,

    /** End/Stop. */
    END = 7,
  } m_cur_phase{NOT_STARTED};
};

inline Alter_stage::Alter_stage(const Alter_stage &rhs) noexcept
    : m_pk(rhs.m_pk), m_recs_req_for_prog(1), m_cur_phase(NOT_STARTED) {}

inline Alter_stage::~Alter_stage() {
  auto progress = m_stage.second.first;

  if (progress == nullptr) {
    return;
  }

  /* Set completed = estimated before we quit. */
  mysql_stage_set_work_completed(progress,
                                 mysql_stage_get_work_estimated(progress));

  mysql_end_stage();
}

inline void Alter_stage::n_pk_recs_inc(uint64_t n) {
  m_n_pk_recs.fetch_add(n, std::memory_order_relaxed);
}

inline void Alter_stage::inc(uint64_t inc_val) {
  if (m_stages.empty()) {
    return;
  }

  ut_a(m_cur_phase != NOT_STARTED);
  {
    IB_mutex_guard guard(&m_mutex, UT_LOCATION_HERE);
    if (m_cur_phase == READ_PK) {
      ut_ad(inc_val == 1);
      ++m_n_pk_pages;

      /* Overall the read pk phase will read all the pages from the
      PK and will do work, proportional to the number of added
      indexes, thus when this is called once per read page we
      increment with 1 + m_n_sort_indexes */
      inc_val = 1 + m_n_sort_indexes;
    }

    auto progress = m_stages.back().second.first;
    mysql_stage_inc_work_completed(progress, inc_val);
  }
  reestimate();
}

inline void Alter_stage::inc_progress_if_needed(uint64_t &unreported_recs,
                                                bool is_last_report) {
  if (m_stages.empty()) {
    return;
  }

  if (should_inc_progress(unreported_recs) || is_last_report) {
    /* Total estimate is based on pages. See reestimate()
    Now that we have done task equivalent to 1 page,
    increment progress by 1 */
    inc(1);
    /* All the rows have been updated to the progress. */
    unreported_recs = 0;
  }
}

inline void Alter_stage::begin_phase_read_pk(size_t n_sort_indexes) {
  m_cur_phase = READ_PK;

  m_n_sort_indexes = n_sort_indexes;

  Stage stage{};

  stage.first = &srv_stage_alter_table_read_pk_internal_sort;
  stage.second.first = mysql_set_stage(stage.first->m_key);

  if (stage.second.first != nullptr) {
    m_stages.push_back(stage);

    auto progress = stage.second.first;
    mysql_stage_set_work_completed(progress, 0);

    reestimate();
  }
}

inline void Alter_stage::end_phase_read_pk() {
  reestimate();

  /* PK reading is complete, and we have the final value for
  m_n_pk_recs & m_n_pk_pages to calculate m_n_recs_per_page. */
  if (m_n_pk_pages == 0) {
    /* The number of pages in the PK could be 0 if the tree is
    empty. In this case we set m_n_recs_per_page to 1 to avoid
    division by zero later. */
    m_n_recs_per_page = 1.0;
  } else {
    m_n_recs_per_page = std::max(
        static_cast<double>(m_n_pk_recs.load(std::memory_order_relaxed)) /
            m_n_pk_pages,
        1.0);
  }
}

inline void Alter_stage::begin_phase_sort(double sort_multi_factor) {
  if (sort_multi_factor <= 1.0) {
    m_sort_multi_factor = 1;
  } else {
    m_sort_multi_factor = static_cast<uint64_t>(round(sort_multi_factor));
  }
  m_recs_req_for_prog =
      static_cast<uint64_t>(m_sort_multi_factor * m_n_recs_per_page);
  change_phase(&srv_stage_alter_table_merge_sort);
}

inline void Alter_stage::begin_phase_insert() {
  change_phase(&srv_stage_alter_table_insert);
  m_recs_req_for_prog = static_cast<uint64_t>(m_n_recs_per_page);
}

inline void Alter_stage::begin_phase_flush(page_no_t n_flush_pages) {
  {
    IB_mutex_guard guard(&m_mutex, UT_LOCATION_HERE);
    m_n_flush_pages = n_flush_pages;
  }

  reestimate();
  change_phase(&srv_stage_alter_table_flush);
}

inline void Alter_stage::begin_phase_log_index() {
  change_phase(&srv_stage_alter_table_log_index);
}

inline void Alter_stage::begin_phase_log_table() {
  change_phase(&srv_stage_alter_table_log_table);
}

inline void Alter_stage::begin_phase_end() {
  change_phase(&srv_stage_alter_table_end);
}

inline bool Alter_stage::should_inc_progress(uint64_t inc_val) const {
  /* We only need to increment the progress when we have seen
     m_recs_req_for_prog. */
  return inc_val >= m_recs_req_for_prog;
}

inline void Alter_stage::reestimate() {
  if (m_stages.empty()) {
    return;
  }

  IB_mutex_guard guard(&m_mutex, UT_LOCATION_HERE);
  /* During the log table phase we calculate the estimate as
  work done so far + log size remaining. */
  if (m_cur_phase == LOG_TABLE) {
    auto progress = m_stages.back().second.first;

    mysql_stage_set_work_estimated(
        progress,
        mysql_stage_get_work_completed(progress) + row_log_estimate_work(m_pk));

    return;
  }

  /* During the other phases we use a formula, regardless of
  how much work has been done so far. */

  /* For number of pages in the PK - if the PK has not been
  read yet, use stat_n_leaf_pages (approximate), otherwise
  use the exact number we gathered. */
  const page_no_t n_pk_pages =
      m_cur_phase != READ_PK ? m_n_pk_pages : m_pk->stat_n_leaf_pages;

  /* If flush phase has not started yet and we do not know how
  many pages are to be flushed, then use a wild guess - the
  number of pages in the PK / 2. */
  if (m_n_flush_pages == 0) {
    m_n_flush_pages = n_pk_pages / 2;
  }

  uint64_t estimate =
      n_pk_pages *
          (1                  /* read PK */
           + m_n_sort_indexes /* row_merge_buf_sort() inside the
                              read PK per created index */
           + m_n_sort_indexes * 2 /* sort & insert per created index */) +
      m_n_flush_pages + row_log_estimate_work(m_pk);

  auto progress = m_stages.back().second.first;
  const auto completed = (uint64_t)mysql_stage_get_work_completed(progress);

  /* Prevent estimate < completed */
  mysql_stage_set_work_estimated(progress, std::max(estimate, completed));
}

inline void Alter_stage::change_phase(const PSI_stage_info *new_stage) {
  if (m_stages.empty()) {
    return;
  }

  ut_a(new_stage != &srv_stage_alter_table_read_pk_internal_sort);

  if (new_stage == &srv_stage_alter_table_merge_sort) {
    m_cur_phase = SORT;
  } else if (new_stage == &srv_stage_alter_table_insert) {
    m_cur_phase = INSERT;
  } else if (new_stage == &srv_stage_alter_table_flush) {
    m_cur_phase = FLUSH;
  } else if (new_stage == &srv_stage_alter_table_log_index) {
    m_cur_phase = LOG_INDEX;
  } else if (new_stage == &srv_stage_alter_table_log_table) {
    m_cur_phase = LOG_TABLE;
  } else if (new_stage == &srv_stage_alter_table_end) {
    m_cur_phase = END;
  } else {
    ut_error;
  }

  auto progress = m_stages.back().second.first;

  const auto c = mysql_stage_get_work_completed(progress);
  const auto e = mysql_stage_get_work_estimated(progress);

  Stage stage{new_stage, {mysql_set_stage(new_stage->m_key), {}}};

  if (stage.second.first != nullptr) {
    m_stages.push_back(stage);

    auto &counter = m_stages.back().second.second;

    counter.first = c;
    counter.second = e;

    mysql_stage_set_work_completed(stage.second.first, c);
    mysql_stage_set_work_estimated(stage.second.first, e);
  }
}

inline void Alter_stage::aggregate(const Alter_stages &alter_stages) noexcept {
  if (alter_stages.empty()) {
    return;
  }

  ut_a(m_cur_phase == NOT_STARTED);

  Stage cur_stage{};

  for (auto alter_stage : alter_stages) {
    alter_stage->begin_phase_end();

    for (auto stage : alter_stage->m_stages) {
      if (stage.first == &srv_stage_alter_table_end) {
        continue;
      }
      auto progress = mysql_set_stage(stage.first->m_key);

      if (progress == nullptr) {
        /* The user can disable the instrument clas and that can return
        nullptr. That forces us to skip and will break the state transitions
        and counts.*/
        return;
      }

      auto c = mysql_stage_get_work_completed(progress);
      auto e = mysql_stage_get_work_estimated(progress);

      const auto &counter = stage.second.second;

      c += counter.first;
      e += counter.second;

      mysql_stage_set_work_completed(progress, c);
      mysql_stage_set_work_estimated(progress, e);

      if (stage.first == &srv_stage_alter_table_read_pk_internal_sort) {
        if ((int)m_cur_phase < (int)READ_PK) {
          m_cur_phase = READ_PK;
          cur_stage.first = stage.first;
          cur_stage.second.first = progress;
        }
      } else if (stage.first == &srv_stage_alter_table_merge_sort) {
        if ((int)m_cur_phase < (int)NOT_STARTED) {
          m_cur_phase = SORT;
          cur_stage.first = stage.first;
          cur_stage.second.first = progress;
        }
      } else if (stage.first == &srv_stage_alter_table_insert) {
        if ((int)m_cur_phase < (int)SORT) {
          m_cur_phase = INSERT;
          cur_stage.first = stage.first;
          cur_stage.second.first = progress;
        }
      } else if (stage.first == &srv_stage_alter_table_log_index) {
        if ((int)m_cur_phase < (int)LOG_INDEX) {
          m_cur_phase = LOG_INDEX;
          cur_stage.first = stage.first;
          cur_stage.second.first = progress;
        }
      }

      ut_a(stage.first != &srv_stage_alter_table_flush);
      ut_a(stage.first != &srv_stage_alter_table_log_table);
    }
  }

  if (cur_stage.first != nullptr) {
    ut_a(cur_stage.second.first != nullptr);
    m_stages.push_back(cur_stage);
  }
}

/** class to monitor the progress of 'ALTER TABLESPACE ENCRYPTION' in terms
of number of pages operated upon. */
class Alter_stage_ts {
 public:
  /** Constructor. */
  Alter_stage_ts()
      : m_progress(nullptr),
        m_work_estimated(0),
        m_work_done(0),
        m_cur_phase(NOT_STARTED) {}

  /** Destructor. */
  inline ~Alter_stage_ts() {
    if (m_progress == nullptr) {
      return;
    }
    mysql_end_stage();
  }

  /** Initialize.
  @param[in] key                PFS key. */
  void init(int key) {
    ut_ad(key != -1);
    ut_ad(m_cur_phase == NOT_STARTED);

    m_progress = nullptr;
    m_work_estimated = 0;
    m_work_done = 0;

    m_progress = mysql_set_stage(key);
    /* Change phase to INITIATED */
    change_phase();
  }

  /** Set estimate.
  @param[in] units              Units. */
  void set_estimate(uint64_t units) {
    if (m_progress == nullptr) {
      return;
    }

    ut_ad(m_cur_phase == INITIATED);
    m_work_estimated = units;
    mysql_stage_set_work_estimated(m_progress, m_work_estimated);
    /* Change phase to WORK_ESTIMATED */
    change_phase();
  }

  /** Update the progress.
  @param[in] units              Update delta. */
  void update_work(uint64_t units) {
    if (m_progress == nullptr) {
      return;
    }

    ut_ad(m_cur_phase == WORK_ESTIMATED);

    m_work_done += units;
    ut_ad(m_work_done <= m_work_estimated);
    mysql_stage_set_work_completed(m_progress, m_work_done);

    if (m_work_done == m_work_estimated) {
      /* Change phase to WORK_COMPLETED */
      change_phase();
    }
  }

  /** Change phase. */
  void change_phase() {
    if (m_progress == nullptr) {
      ut_ad(m_cur_phase == NOT_STARTED);
      return;
    }

    switch (m_cur_phase) {
      case NOT_STARTED:
        m_cur_phase = INITIATED;
        break;
      case INITIATED:
        m_cur_phase = WORK_ESTIMATED;
        break;
      case WORK_ESTIMATED:
        m_cur_phase = WORK_COMPLETED;
        break;
      case WORK_COMPLETED:
      default:
        ut_error;
    }
  }

  bool is_completed() {
    if (m_progress == nullptr) {
      return true;
    } else {
      return (m_cur_phase == WORK_COMPLETED);
    }
  }

 private:
  /** Performance schema accounting object. */
  PSI_stage_progress *m_progress;

  /** Number of pages to be (un)encrypted . */
  page_no_t m_work_estimated;

  /** Number of pages already (un)encrypted . */
  page_no_t m_work_done;

  /** Current phase. */
  enum {
    /** Not open phase. */
    NOT_STARTED = 0,

    /** Initialised. */
    INITIATED = 1,

    /** Work estimated phase. */
    WORK_ESTIMATED = 2,

    /** Work completed phase. */
    WORK_COMPLETED = 3,
  } m_cur_phase;
};

#else  /* HAVE_PSI_STAGE_INTERFACE */

/** Dummy alter stage. */
class Alter_stage {
 public:
  /** Constructor. */
  explicit Alter_stage(const dict_index_t *pk) {}

  /** Setup the number of indexes to read.
  @param[in] n_sort_indexes     Number of indexe.s */
  void begin_phase_read_pk(size_t n_sort_indexes) {}

  /** Increments the number of rows read so far. */
  void n_pk_recs_inc() {}

  /** Increments the number of rows read so far. */
  void n_pk_recs_inc(uint64_t) {}

  /** Increment depending on stage. */
  void inc(uint64_t inc_val = 1) {}

  /** Increment the progress if we have crossed the threshold
  for unreported records. */
  void inc_progress_if_needed(uint64_t &unreported_recs,
                              bool is_last_report = false) {}

  /** End scan phase. */
  void end_phase_read_pk() {}

  /** Begin merge sort phase. */
  void begin_phase_sort(double sort_multi_factor) {}

  /** Begin insert phase. */
  void begin_phase_insert() {}

  /** Begin flushing of non-redo logged pages.
  @param[in] n_flush_pages      Number of pages to flush. */
  void begin_phase_flush(page_no_t n_flush_pages) {}

  /** Begin row log apply phase to the index. */
  void begin_phase_log_index() {}

  /** Begin row log apply phase to the table. */
  void begin_phase_log_table() {}

  /** Build end phase. */
  void begin_phase_end() {}

  /** Aggregate the sub stages..
  @param[in] stages             Stages to aggregate. */
  void aggregate(const Alter_stages &alter_stages) noexcept {}
};

class Alter_stage_ts {
 public:
  /** Constructor. */
  Alter_stage_ts() {}

  /** Destructor. */
  inline ~Alter_stage_ts() {}

  /** Initialize.
  @param[in] key                PFS key. */
  void init(int key) {}

  /** Set estimate.
  @param[in] units              Units. */
  void set_estimate(uint units) {}

  /** Update the progress.
  @param[in] units              Update delta. */
  void update_work(uint units) {}

  /** Change phase. */
  void change_phase() {}
};
#endif /* HAVE_PSI_STAGE_INTERFACE */

#endif /* ut0stage_h */
