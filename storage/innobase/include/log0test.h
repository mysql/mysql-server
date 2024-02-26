/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

*****************************************************************************/

/**************************************************/ /**
 @file include/log0test.h

 Redo log - helper for unit tests.

 *******************************************************/

#ifndef log0test_h
#define log0test_h

#include <memory>

/* lsn_t */
#include "log0types.h"

#ifdef UNIV_DEBUG

/* DBUG_EXECUTE_IF */
#include "my_dbug.h"

/* DEBUG_SYNC_C */
#include "my_sys.h"

/* current_thd */
#include "sql/current_thd.h"

/* debug_sync_set_action */
#include "sql/debug_sync.h"

#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP

/** It is a environment for tests of redo log. It contains a mock, which
replaces real buffer pool during the test. */
class Log_test {
 public:
  typedef page_no_t Key;
  typedef int64_t Value;

  struct Page {
    Key key;
    Value value;
    lsn_t oldest_modification;
    lsn_t newest_modification;
  };

  typedef std::map<Key, Page> Pages;

  class Sync_point {
   public:
    virtual void sync() = 0;

    virtual ~Sync_point() = default;
  };

  enum class Options {
    VALIDATE_RECENT_CLOSED = 1,
    VALIDATE_RECENT_WRITTEN = 2
  };

  typedef std::map<std::string, std::unique_ptr<Sync_point>> Sync_points;

  /** Calculates oldest_modification of the earliest added dirty page
  during the test in log0log-t. It is basically a replacement for the
  log_buf_get_oldest_modification_approx() during the test.
  @return oldest_modification lsn */
  lsn_t oldest_modification_approx() const;

  void add_dirty_page(const Page &page);

  void fsync_written_pages();

  void purge(lsn_t max_dirty_page_age);

  static byte *create_mlog_rec(byte *rec, Key key, Value value, size_t payload);

  static byte *create_mlog_rec(byte *rec, Key key, Value value);

  static byte *parse_mlog_rec(byte *begin, byte *end, Key &key, Value &value,
                              lsn_t &start_lsn, lsn_t &end_lsn);

  byte *parse_mlog_rec(byte *begin, byte *end);

  const Pages &flushed() const;

  const Pages &recovered() const;

  void sync_point(const std::string &sync_point_name);

  void register_sync_point_handler(
      const std::string &sync_point_name,
      std::unique_ptr<Sync_point> &&sync_point_handler);

  bool enabled(Options option) const;

  void set_enabled(Options option, bool enabled);

  int flush_every() const;

  void set_flush_every(int flush_every);

  int verbosity() const;

  void set_verbosity(int level);

 private:
  void recovered_reset(Key key, lsn_t oldest_modification,
                       lsn_t newest_modification);

  void recovered_add(Key key, Value value, lsn_t oldest_modification,
                     lsn_t newest_modification);

  mutable std::mutex m_mutex;
  mutable std::mutex m_purge_mutex;
  std::map<lsn_t, Page> m_buf;
  Pages m_written;
  Pages m_flushed;
  Pages m_recovered;
  Sync_points m_sync_points;
  uint64_t m_options_enabled = 0;
  int m_verbosity = 0;
  int m_flush_every = 10;
};

/** Represents currently running test of redo log, nullptr otherwise. */
extern std::unique_ptr<Log_test> log_test;

/** This function is responsible for three actions:

1. Defines a conditional sync point with name = sync_point_name
   (@see CONDITIONAL_SYNC_POINT).

2. Crashes MySQL if debug variable with name = "crash_" + sync_poit_name is
   defined. You could use following approach to crash it:
      SET GLOBAL DEBUG = '+d,crash_foo' (if sync_point_name='foo')

3. Notifies log_test (unless it's nullptr) about the sync point.

@param[in]  sync_point_name  name of syncpoint, must be a string literal */
template <size_t N>
static void log_sync_point(const char (&sync_point_name)[N]) {
#ifdef UNIV_DEBUG
  CONDITIONAL_SYNC_POINT(sync_point_name);
  const std::string crash_var_name = std::string{"crash_"} + sync_point_name;
  DBUG_EXECUTE_IF(crash_var_name.c_str(), DBUG_SUICIDE(););
#endif /* UNIV_DEBUG */
  if (log_test != nullptr) {
    log_test->sync_point(sync_point_name);
  }
}

#endif /* !UNIV_HOTBACKUP */

#endif /* log0test_h */
