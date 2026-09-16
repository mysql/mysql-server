// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/storage/relay_log/istream_prefetched.h"

namespace mysql::csa {

Istream_prefetched::Istream_prefetched(Log_prefetcher_sptr prefetcher,
                                       const std::string &file_name)
    : m_prefetcher(prefetcher), m_allowed_file_name(file_name) {}

ssize_t Istream_prefetched::skip(std::size_t length) {
  return read(nullptr, length);
}

void Istream_prefetched::copy_to(unsigned char *buffer,
                                 std::size_t &buffer_offset,
                                 std::size_t bytes) {
  if (buffer) {
    memcpy(buffer + buffer_offset,
           m_current_batch->data_raw() + m_current_offset, bytes);
    buffer_offset += bytes;
  }
}

ssize_t Istream_prefetched::read(unsigned char *buffer, std::size_t length) {
  std::size_t bytes_read_left = length;
  std::size_t buffer_offset = 0;
  bool error = m_prefetcher_error;
  if (!error &&
      (!m_current_batch || m_current_offset == m_current_batch->size())) {
    error = read_next_batch();
  }
  while (bytes_read_left != 0 && !error && !m_is_stopped) {
    auto bytes_left = m_current_batch->size() - m_current_offset;
    if (bytes_read_left <= bytes_left) {
      copy_to(buffer, buffer_offset, bytes_read_left);
      m_current_offset += bytes_read_left;
      m_file_offset += bytes_read_left;
      return length;
    }
    copy_to(buffer, buffer_offset, bytes_left);
    m_current_offset += bytes_left;
    m_file_offset += bytes_left;
    bytes_read_left -= bytes_left;
    error = read_next_batch();
  }
  if (m_prefetcher_error) {
    return -1;  // error case
  }
  return length - bytes_read_left;
}

bool Istream_prefetched::read_next_batch() {
  if (m_prefetcher->is_error()) {
    m_prefetcher_error = true;
    return true;
  }
  if (m_current_batch && m_current_batch->ends_file()) {
    return true;
  }
  auto result =
      m_prefetcher->dequeue([this]() -> bool { return m_is_stopped; });
  if (result.has_value()) {
    m_current_batch = result.value();
    m_current_offset = 0;
    return false;  // success
  }
  return true;  // stopped in the process
}

bool Istream_prefetched::seek(my_off_t offset) {
  if (offset < m_file_offset) {
    return true;
  }
  ssize_t diff = offset - m_file_offset;
  auto bytes_skipped = skip(diff);
  return bytes_skipped != diff;
}

my_off_t Istream_prefetched::length() {
  if (!m_current_batch) {  // for the first batch, read data
    std::ignore = read_next_batch();
  }
  if (m_current_batch) {
    m_current_batch->get_file_length();
  }
  return 0;
}

}  // namespace mysql::csa
