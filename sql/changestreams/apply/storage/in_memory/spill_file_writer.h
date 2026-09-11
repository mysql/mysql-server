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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_FILE_WRITER_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_FILE_WRITER_H

#include <cstddef>
#include <memory>
#include <string>

#include "my_inttypes.h"        // my_off_t
#include "sql/basic_ostream.h"  // IO_CACHE_ostream

class Format_description_log_event;

namespace mysql::csa {

/// @brief Private on-disk spill file for one large transaction, written in
/// relay-log format.
///
/// A transaction whose length exceeds the spill threshold is streamed to a
/// private file on disk instead of RAM. This helper owns that file.
class Spill_file_writer {
 public:
  /// @brief Shared pointer to the Format_description_log_event written as the
  /// file's prefix and used by the reader to decode subsequent events.
  using Fde_ptr = std::shared_ptr<Format_description_log_event>;

  /// @brief Name of the per-channel subdirectory that holds spill files.
  static constexpr const char *kTempSubdirName = "in_memory_relaylog_temp_files";

  /// @brief Prefix of every spill file name: "imr_sp_<unique_id>".
  static constexpr const char *kFileNamePrefix = "imr_sp_";

  /// @brief Constructs a writer. Does not touch the filesystem until open().
  ///
  /// @param fde The FDE serialized into the file prefix. Must be non-null when
  ///        open() runs.
  /// @param relay_log_dir The channel's relay log directory. open() creates the
  ///        @c in_memory_relaylog_temp_files subdirectory under it and places
  ///        the spill file there. Must be non-empty when open() runs.
  Spill_file_writer(Fde_ptr fde, std::string relay_log_dir);

  /// @brief Closes the IO_CACHE and deletes the temp file, if any.
  ~Spill_file_writer();

  Spill_file_writer(const Spill_file_writer &) = delete;
  Spill_file_writer &operator=(const Spill_file_writer &) = delete;
  Spill_file_writer(Spill_file_writer &&) = delete;
  Spill_file_writer &operator=(Spill_file_writer &&) = delete;

  /// @brief Ensures the temp-files subdirectory exists, creates a uniquely
  /// named @c imr_sp_<unique_id> file in it, and writes the relay-log prefix
  /// (BINLOG_MAGIC + serialized FDE).
  ///
  /// @retval false Success.
  /// @retval true  Error (see get_error_str()).
  bool open();

  /// @brief Appends @p len raw bytes to the open file and advances the end
  /// position by @p len. The bytes are copied into the IO_CACHE synchronously,
  /// so @p buf may be transient. A zero-length append is a no-op.
  ///
  /// @param buf Bytes to append.
  /// @param len Number of bytes at @p buf.
  /// @retval false Success.
  /// @retval true  Error (see get_error_str()).
  bool append_raw(const char *buf, std::size_t len);

  /// @brief Flushes the IO_CACHE buffer to the file so a concurrent reader can
  /// read up to end_position(). Does not fsync.
  ///
  /// @retval false Success.
  /// @retval true  Error (see get_error_str()).
  bool flush();

  /// @brief Current logical end position: the number of bytes written so far
  /// (prefix + all appended bytes). Advances monotonically.
  my_off_t end_position() const { return m_end_pos; }

  /// @brief Full path of the backing spill file (empty until open() succeeds).
  const std::string &file_name() const { return m_file_name; }

  /// @brief Full path of the temp-files subdirectory (empty until open()).
  const std::string &temp_dir() const { return m_temp_dir; }

  /// @brief True once open() has laid down the prefix successfully.
  bool is_open() const { return m_is_open; }

  /// @brief Human-readable message for the last error, empty if none.
  const std::string &get_error_str() const { return m_error; }

 private:
  /// @brief Creates a uniquely named, O_EXCL spill file under m_temp_dir and
  /// stores its path in m_file_name. Returns true on error.
  bool create_unique_file();

  /// @brief FDE serialized into the file prefix.
  Fde_ptr m_fde;
  /// @brief The channel's relay log directory (parent of the temp subdir).
  std::string m_relay_log_dir;
  /// @brief Buffered write stream over the temp file.
  IO_CACHE_ostream m_ostream;
  /// @brief Path to the temp-files subdirectory (set in open()).
  std::string m_temp_dir;
  /// @brief Path to the backing spill file (empty until open() succeeds).
  std::string m_file_name;
  /// @brief Last error message, empty if none.
  std::string m_error;
  /// @brief Logical bytes written so far (prefix + appended bytes).
  my_off_t m_end_pos{0};
  /// @brief True while the IO_CACHE stream is open (guards close()).
  bool m_stream_open{false};
  /// @brief True once the prefix is fully written.
  bool m_is_open{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_FILE_WRITER_H
