/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0test.cc

 Tools used to test redo log in gtests (log0log-t.cc).

 *******************************************************/

#ifndef UNIV_HOTBACKUP

#include "log0log.h"

/** Maximum size of payload put inside each MLOG_TEST record. */
static constexpr size_t MLOG_TEST_PAYLOAD_MAX_LEN = 50;

/** Space id used for pages modified during tests of redo log. */
static constexpr size_t MLOG_TEST_PAGE_SPACE_ID = 1;

/** Represents currently running test of redo log, nullptr otherwise. */
std::unique_ptr<Log_test> log_test;

lsn_t Log_test::oldest_modification_approx() const {
  std::lock_guard<std::mutex> lock{m_mutex};

  return (m_buf.empty() ? 0 : m_buf.begin()->first);
}

void Log_test::add_dirty_page(const Page &page) {
#ifndef UNIV_HOTBACKUP
  ut_a(log_lsn_validate(page.oldest_modification));
  ut_a(log_lsn_validate(page.newest_modification));
#endif /* !UNIV_HOTBACKUP */

  std::lock_guard<std::mutex> lock{m_mutex};

  m_buf.insert(std::make_pair(page.oldest_modification, page));
}

void Log_test::fsync_written_pages() {
  std::lock_guard<std::mutex> lock{m_mutex};

  for (auto written : m_written) {
    m_flushed[written.first] = written.second;
  }

  m_written.clear();
}

void Log_test::purge(lsn_t max_dirty_page_age) {
  std::lock_guard<std::mutex> purge_lock{m_purge_mutex};

  std::unique_lock<std::mutex> lock{m_mutex};

  if (m_buf.empty()) {
    return;
  }

  const lsn_t max_lsn = m_buf.rbegin()->first;

  while (!m_buf.empty() &&
         max_lsn - m_buf.begin()->first > max_dirty_page_age) {
    auto it = m_buf.begin();

    const auto &page = it->second;

    /* Fragment below would make it more similar to real env.
    However there is some issue now. */
#if 0
		/* We need to avoid deadlock when resizing log
		buffer in background ... (because of m_mutex). */
		if (page.newest_modification > log_sys->write_lsn.load()) {

			lock.unlock();

			log_write_up_to(
				*log_sys,
				page.newest_modification,
				true);

			lock.lock();
			continue;
		}
#endif

    m_written[page.key] = page;

    m_buf.erase(it);
  }
}

byte *Log_test::create_mlog_rec(byte *rec, Key key, Value value) {
  const space_id_t space_id = MLOG_TEST_PAGE_SPACE_ID;
  const page_no_t page_no = key;

  uchar *ptr;
  size_t payload;

  ptr = rec;

  payload = ut_rnd_interval(0, MLOG_TEST_PAYLOAD_MAX_LEN);

  mach_write_to_1(ptr, MLOG_TEST);
  ptr++;

  ptr += mach_write_compressed(ptr, space_id);

  ptr += mach_write_compressed(ptr, page_no);

  mach_write_to_8(ptr, key);
  ptr += 8;

  mach_write_to_8(ptr, value);
  ptr += 8;

  mach_write_to_1(ptr, payload);
  ptr++;

  std::memset(ptr, 0x00, payload);
  ptr += payload;

  /* Place for oldest_modification */
  mach_write_to_8(ptr, 0);
  ptr += 8;

  /* Place for newest_modification */
  mach_write_to_8(ptr, 0);
  ptr += 8;

  return (ptr);
}

byte *Log_test::parse_mlog_rec(byte *begin, byte *end) {
  if (begin + 2 * 8 + 1 > end) {
    return (nullptr);
  }

  const Key key = static_cast<Key>(mach_read_from_8(begin));
  begin += 8;

  const Value value = static_cast<Value>(mach_read_from_8(begin));
  begin += 8;

  const size_t payload = mach_read_from_1(begin);
  begin++;

  if (begin + payload + 2 * 8 > end) {
    return (nullptr);
  }

  begin += payload;

  const lsn_t start_lsn = mach_read_from_8(begin);
  begin += 8;

  const lsn_t end_lsn = mach_read_from_8(begin);
  begin += 8;

  if (value == MLOG_TEST_VALUE) {
    recovered_reset(key, start_lsn, end_lsn);

    recovered_add(key, value, start_lsn, end_lsn);

  } else if (value == 0) {
    recovered_reset(key, start_lsn, end_lsn);

  } else {
    recovered_add(key, value, start_lsn, end_lsn);
  }

  return (begin);
}

void Log_test::recovered_reset(Key key, lsn_t oldest_modification,
                               lsn_t newest_modification) {
  Page page;
  page.key = key;
  page.value = 0;
  page.oldest_modification = oldest_modification;
  page.newest_modification = newest_modification;

  m_recovered[key] = page;
}

void Log_test::recovered_add(Key key, Value value, lsn_t oldest_modification,
                             lsn_t newest_modification) {
  auto it = m_recovered.find(key);
  ut_a(it != m_recovered.end());

  ut_a(it->second.oldest_modification == oldest_modification);
  ut_a(it->second.newest_modification == newest_modification);

  it->second.value += value;
  ut_a(it->second.value <= MLOG_TEST_VALUE);
}

const Log_test::Pages &Log_test::flushed() const { return (m_flushed); }

const Log_test::Pages &Log_test::recovered() const { return (m_recovered); }

void Log_test::sync_point(const std::string &sync_point_name) {
  auto it = m_sync_points.find(sync_point_name);

  if (it == m_sync_points.end()) {
    return;
  }

  it->second->sync();
}

void Log_test::register_sync_point_handler(
    const std::string &sync_point_name,
    std::unique_ptr<Sync_point> &&sync_point_handler) {
  m_sync_points[sync_point_name] = std::move(sync_point_handler);
}

bool Log_test::enabled(Options option) const {
  return ((m_options_enabled & uint64_t(option)) != 0);
}

void Log_test::set_enabled(Options option, bool enabled) {
  if (enabled) {
    m_options_enabled |= uint64_t(option);
  } else {
    m_options_enabled &= ~uint64_t(option);
  }
}

int Log_test::flush_every() const { return (m_flush_every); }

void Log_test::set_flush_every(int flush_every) { m_flush_every = flush_every; }

int Log_test::verbosity() const { return (m_verbosity); }

void Log_test::set_verbosity(int level) {
  ut_a(level >= 0);
  m_verbosity = level;
}

#endif /* !UNIV_HOTBACKUP */
