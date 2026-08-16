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

#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"

#include <cassert>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "my_io.h"   // File, FN_LIBCHAR
#include "my_sys.h"  // my_create, my_mkdir, my_close, my_delete
#include "sql/log_event.h"  // Format_description_log_event, BINLOG_MAGIC, BIN_LOG_HEADER_SIZE

namespace mysql::csa {

namespace {

std::atomic<std::uint64_t> g_spill_seq{0};

/// Joins @p dir and @p leaf with the platform directory separator, avoiding a
/// double separator when @p dir already ends with one.
std::string join_path(const std::string &dir, const std::string &leaf) {
  if (dir.empty()) return leaf;
  if (dir.back() == FN_LIBCHAR) return dir + leaf;
  return dir + FN_LIBCHAR + leaf;
}

}

Spill_file_writer::Spill_file_writer(Fde_ptr fde, std::string relay_log_dir)
    : m_fde(std::move(fde)), m_relay_log_dir(std::move(relay_log_dir)) {}

Spill_file_writer::~Spill_file_writer() {
  // Close the IO_CACHE, then delete the temp file.
  if (m_stream_open) {
    m_ostream.close();
    m_stream_open = false;
  }
  if (!m_file_name.empty()) {
    my_delete(m_file_name.c_str(), MYF(0));
    m_file_name.clear();
  }
}

bool Spill_file_writer::create_unique_file() {
  constexpr int kMaxAttempts = 1024;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const std::uint64_t id = g_spill_seq.fetch_add(1);
    char id_buf[32];
    // Lowercase hex id.
    std::snprintf(id_buf, sizeof(id_buf), "%llx",
                  static_cast<unsigned long long>(id));
    const std::string candidate =
        join_path(m_temp_dir, std::string(kFileNamePrefix) + id_buf);

    const File fd =
        my_create(candidate.c_str(), 0, O_CREAT | O_EXCL | O_WRONLY, MYF(0));
    if (fd >= 0) {
      // Reserved the name exclusively; IO_CACHE_ostream reopens it by name.
      my_close(fd, MYF(0));
      m_file_name = candidate;
      return false;
    }
    if (my_errno() != EEXIST) {
      m_error.assign("Spill_file_writer: could not create spill file");
      return true;
    }
  }
  m_error.assign("Spill_file_writer: exhausted unique spill file names");
  return true;
}

bool Spill_file_writer::open() {
  assert(!m_is_open);
  assert(m_fde != nullptr);

  if (m_relay_log_dir.empty()) {
    m_error.assign("Spill_file_writer: empty relay log directory");
    return true;
  }

  // Place spill files in the "in_memory_relaylog_temp_files" subdirectory under
  // the channel's relay log directory, creating it on demand.
  // TODO: Move the folder creation to server start with proper error check.
  m_temp_dir = join_path(m_relay_log_dir, kTempSubdirName);
  if (my_mkdir(m_temp_dir.c_str(), 0777, MYF(0)) != 0 && my_errno() != EEXIST) {
    m_error.assign(
        "Spill_file_writer: could not create temp-files subdirectory");
    m_temp_dir.clear();
    return true;
  }

  // Create a uniquely named "imr_sp_<unique_id>" file in that directory.
  if (create_unique_file()) {
    return true;
  }

  if (m_ostream.open(
#ifdef HAVE_PSI_INTERFACE
          PSI_NOT_INSTRUMENTED,
#endif
          m_file_name.c_str(), MYF(MY_WME))) {
    m_error.assign("Spill_file_writer: could not open IO_CACHE over spill file");
    my_delete(m_file_name.c_str(), MYF(0));
    m_file_name.clear();
    return true;
  }
  m_stream_open = true;

  // Relay-log prefix: the 4-byte magic.
  if (m_ostream.write(reinterpret_cast<const unsigned char *>(BINLOG_MAGIC),
                      BIN_LOG_HEADER_SIZE)) {
    m_error.assign("Spill_file_writer: failed to write BINLOG_MAGIC");
    return true;
  }
  m_end_pos = BIN_LOG_HEADER_SIZE;

  // Relay-log prefix: the serialized FDE.
  // Serialize a PRIVATE copy.
  Format_description_log_event fde_copy;
  static_cast<mysql::binlog::event::Format_description_event &>(fde_copy) =
      static_cast<const mysql::binlog::event::Format_description_event &>(
          *m_fde);

  // Mark it a relay-log FDE and PRESERVE the checksum algorithm the copy
  // carries.
  fde_copy.set_relay_log_event();
  if (fde_copy.common_footer->checksum_alg ==
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF) {
    fde_copy.common_footer->checksum_alg =
        mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  }
  fde_copy.common_header->log_pos = m_end_pos;
  if (fde_copy.write(&m_ostream)) {
    m_error.assign("Spill_file_writer: failed to write FDE");
    return true;
  }
  m_end_pos += fde_copy.common_header->data_written;

  m_is_open = true;
  return false;
}

bool Spill_file_writer::append_raw(const char *buf, std::size_t len) {
  assert(m_is_open);
  if (!m_is_open) {
    m_error.assign("Spill_file_writer: append_raw before open");
    return true;
  }
  if (len == 0) return false;
  if (m_ostream.write(reinterpret_cast<const unsigned char *>(buf), len)) {
    m_error.assign("Spill_file_writer: failed to append event bytes");
    return true;
  }
  m_end_pos += len;
  return false;
}

bool Spill_file_writer::flush() {
  assert(m_is_open);
  if (!m_is_open) {
    m_error.assign("Spill_file_writer: flush before open");
    return true;
  }
  if (m_ostream.flush()) {
    m_error.assign("Spill_file_writer: failed to flush spill file");
    return true;
  }
  return false;
}

}  // namespace mysql::csa
