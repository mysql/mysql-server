/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
 @file log/log0files_dict.cc

 Redo log - in-memory dictionary of log files (their meta data).

 *******************************************************/

/* std::find_if, std::count_if */
#include <algorithm>

/* std::distance */
#include <iterator>

/* std::accumulate */
#include <numeric>

#include "log0files_dict.h"

/**************************************************/ /**

 @name Log_files_dict methods

 *******************************************************/

/** @{ */

Log_files_dict::Log_files_dict(Log_files_context &files_ctx)
    : m_files_ctx{files_ctx} {}

Log_files_dict::Log_files_dict(Log_files_dict &&other)
    : m_files_ctx{other.m_files_ctx},
      m_files_by_id{std::move(other.m_files_by_id)} {}

Log_files_dict &Log_files_dict::operator=(Log_files_dict &&other) {
  m_files_by_id = std::move(other.m_files_by_id);
  ut_a(&m_files_ctx == &other.m_files_ctx);
  return *this;
}

const Log_files_context &Log_files_dict::ctx() const { return m_files_ctx; }

void Log_files_dict::clear() { m_files_by_id.clear(); }

void Log_files_dict::erase(Log_file_id file_id) {
  auto it = m_files_by_id.find(file_id);
  ut_a(it != m_files_by_id.end());

  m_files_by_id.erase(it);

  ut_a(m_files_by_id.find(file_id) == m_files_by_id.end());
}

Log_files_dict::Const_iterator Log_files_dict::find(lsn_t lsn) const {
  /* The performance of Log_files_dict::find is not important.
  If it became important, one could add another dictionary to
  this class: m_files_by_start_lsn. */
  const Const_iterator it =
      std::find_if(begin(), end(),
                   [lsn](const Log_file &file) { return file.contains(lsn); });

  if (it != end()) {
    DBUG_PRINT("ib_log",
               ("found file for lsn=" LSN_PF ": file_id=%zu "
                "[" LSN_PF "," LSN_PF "), ",
                lsn, size_t{it->m_id}, it->m_start_lsn, it->m_end_lsn));
  } else {
    DBUG_PRINT("ib_log", ("found no file for lsn=" LSN_PF, lsn));
  }
  return it;
}

Log_files_dict::Const_iterator Log_files_dict::file(Log_file_id file_id) const {
  return Const_iterator{m_files_by_id.find(file_id)};
}

void Log_files_dict::add(Log_file_id file_id, os_offset_t size_in_bytes,
                         lsn_t start_lsn, bool full,
                         Encryption_metadata &encryption_metadata) {
  add(file_id, size_in_bytes, start_lsn, full, false, encryption_metadata);
}

void Log_files_dict::add(Log_file_id file_id, os_offset_t size_in_bytes,
                         lsn_t start_lsn, bool full, bool consumed,
                         Encryption_metadata &encryption_metadata) {
  ut_a(start_lsn == 0 || LOG_START_LSN <= start_lsn);
  ut_a(start_lsn < LSN_MAX);
  ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(size_in_bytes > 0);
  ut_a(m_files_by_id.find(file_id) == m_files_by_id.end());

  lsn_t end_lsn;
  if (start_lsn > 0) {
    const bool success =
        log_file_compute_end_lsn(start_lsn, size_in_bytes, end_lsn);
    ut_a(success);
  } else {
    end_lsn = 0;
  }
  ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  Log_file meta_info{m_files_ctx,   file_id,   consumed, full,
                     size_in_bytes, start_lsn, end_lsn,  encryption_metadata};

  m_files_by_id.emplace(file_id, meta_info);
}

void Log_files_dict::set_consumed(Log_file_id file_id) {
  const auto it = m_files_by_id.find(file_id);
  ut_a(it != m_files_by_id.end());
  it->second.m_consumed = true;
}

void Log_files_dict::set_full(Log_file_id file_id) {
  const auto it = m_files_by_id.find(file_id);
  ut_a(it != m_files_by_id.end());
  it->second.m_full = true;
}

void Log_files_dict::set_incomplete(Log_file_id file_id) {
  const auto it = m_files_by_id.find(file_id);
  ut_a(it != m_files_by_id.end());
  it->second.m_full = false;
}

void Log_files_dict::set_size(Log_file_id file_id, os_offset_t new_size) {
  const auto it = m_files_by_id.find(file_id);
  ut_a(it != m_files_by_id.end());
  auto &meta_info = it->second;

  meta_info.m_size_in_bytes = new_size;

  /* meta_info.m_start_lsn == 0 holds only for redo files in legacy format,
  but for them Log_files_dict::set_size must not be called */
  ut_a(meta_info.m_start_lsn > 0);
  const bool success = log_file_compute_end_lsn(
      meta_info.m_start_lsn, meta_info.m_size_in_bytes, meta_info.m_end_lsn);
  ut_a(success);
}

bool Log_files_dict::empty() const { return m_files_by_id.empty(); }

const Log_file &Log_files_dict::front() const {
  ut_a(!m_files_by_id.empty());
  return m_files_by_id.begin()->second;
}

const Log_file &Log_files_dict::back() const {
  ut_a(!m_files_by_id.empty());
  return m_files_by_id.rbegin()->second;
}

/** @} */

/**************************************************/ /**

 @name Log_files_dict::Const_iterator and related

 *******************************************************/

/** @{ */

Log_files_dict::Const_iterator::Const_iterator(
    Log_files_dict::Log_files_map_iterator it)
    : m_it(it) {}

const Log_file &Log_files_dict::Const_iterator::operator*() const {
  return m_it->second;
}

const Log_file *Log_files_dict::Const_iterator::operator->() const {
  return &(m_it->second);
}

Log_files_dict::Const_iterator &Log_files_dict::Const_iterator::operator++() {
  ++m_it;
  return *this;
}

Log_files_dict::Const_iterator Log_files_dict::Const_iterator::operator++(int) {
  Const_iterator tmp = *this;
  ++*this;
  return tmp;
}

Log_files_dict::Const_iterator &Log_files_dict::Const_iterator::operator--() {
  --m_it;
  return *this;
}

Log_files_dict::Const_iterator Log_files_dict::Const_iterator::operator--(int) {
  Const_iterator tmp = *this;
  --*this;
  return tmp;
}

bool Log_files_dict::Const_iterator::operator==(
    const Const_iterator &rhs) const {
  return m_it == rhs.m_it;
}

bool Log_files_dict::Const_iterator::operator!=(
    const Const_iterator &rhs) const {
  return m_it != rhs.m_it;
}

Log_files_dict::Const_iterator Log_files_dict::begin() const {
  return Const_iterator{m_files_by_id.cbegin()};
}

Log_files_dict::Const_iterator Log_files_dict::end() const {
  return Const_iterator{m_files_by_id.cend()};
}

/** @} */

/**************************************************/ /**

 @name Helpers

 *******************************************************/

/** @{ */

bool log_file_compute_logical_capacity(os_offset_t file_size_in_bytes,
                                       lsn_t &lsn_capacity) {
  if (file_size_in_bytes < LOG_FILE_HDR_SIZE ||
      file_size_in_bytes < UNIV_PAGE_SIZE) {
    return false;
  }
  lsn_capacity = file_size_in_bytes - LOG_FILE_HDR_SIZE;
  return true;
}

bool log_file_compute_end_lsn(lsn_t start_lsn, os_offset_t file_size_in_bytes,
                              lsn_t &end_lsn) {
  const lsn_t MAX_FILE_END_LSN = LSN_MAX - 1;

  lsn_t lsn_capacity;
  if (!log_file_compute_logical_capacity(file_size_in_bytes, lsn_capacity)) {
    return false;
  }

  if (start_lsn < LOG_START_LSN || MAX_FILE_END_LSN <= start_lsn ||
      MAX_FILE_END_LSN - start_lsn <= lsn_capacity) {
    return false;
  }

  end_lsn = start_lsn + lsn_capacity;
  return true;
}

size_t log_files_number_of_existing_files(const Log_files_dict &files) {
  return std::distance(files.begin(), files.end());
}

size_t log_files_number_of_consumed_files(const Log_files_dict &files) {
  return std::count_if(files.begin(), files.end(),
                       [](const Log_file &file) { return file.m_consumed; });
}

os_offset_t log_files_size_of_existing_files(const Log_files_dict &files) {
  return std::accumulate(files.begin(), files.end(), os_offset_t{0},
                         [](os_offset_t total_size, const Log_file &file) {
                           return total_size + file.m_size_in_bytes;
                         });
}

lsn_t log_files_capacity_of_existing_files(const Log_files_dict &files) {
  return std::accumulate(files.begin(), files.end(), lsn_t{0},
                         [](lsn_t total_capacity, const Log_file &file) {
                           lsn_t logical_capacity;
                           const bool ret = log_file_compute_logical_capacity(
                               file.m_size_in_bytes, logical_capacity);
                           ut_a(ret);
                           return total_capacity + logical_capacity;
                         });
}

Log_files_dict::Const_iterator log_files_find_largest(
    const Log_files_dict &files) {
  return std::max_element(files.begin(), files.end(),
                          [](const Log_file &a, const Log_file &b) {
                            return a.m_size_in_bytes < b.m_size_in_bytes;
                          });
}

/** @} */
