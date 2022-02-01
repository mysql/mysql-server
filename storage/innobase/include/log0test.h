/*****************************************************************************

Copyright (c) 2017, 2021, Oracle and/or its affiliates.

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

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

*****************************************************************************/

/**************************************************/ /**
 @file include/log0test.h

 Redo log - helper for unit tests.

 *******************************************************/

#ifndef log0test_h
#define log0test_h

#include "log0types.h"

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

    virtual ~Sync_point() {}
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

#endif /* !UNIV_HOTBACKUP */

#endif /* log0test_h */
