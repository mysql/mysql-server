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
#include "sql/basic_ostream.h"  // Truncatable_ostream

class Format_description_log_event;

namespace mysql::csa {

class Trx_envelope_queue;

/// @brief Owns the private relay-log-format file of one spilled transaction.
///
/// The file is encrypted when @c binlog_encryption is ON at open(), and keeps
/// that state for its whole life. An encrypted file has the format of an
/// encrypted relay log: the 512-byte encryption header, then the AES-CTR
/// encrypted data (BINLOG_MAGIC, FDE, events). end_position() is logical and
/// excludes the header.
class Spill_file_writer {
 public:
  /// @brief Shared pointer to the Format_description_log_event written as the
  /// file's prefix and used by the reader to decode subsequent events.
  using Fde_ptr = std::shared_ptr<Format_description_log_event>;

  /// @brief Name of the server's single subdirectory that holds spill files.
  static constexpr const char *kTempSubdirName =
      "#in_memory_relaylog_temp_files";

  /// @brief Common prefix of every spill file name:
  /// "imr_sp_<channel_name>_<unique_id>", or "imr_sp_<unique_id>" for the
  /// default channel. The channel segment (filename-charset encoded) keeps
  /// channels from colliding, because the unique-id counter is process-global
  /// and restarts at zero. The shared prefix lets the purge match all files.
  static constexpr const char *kFileNamePrefix = "imr_sp_";

  /// @brief Constructs a writer. Does not touch the filesystem until open().
  ///
  /// @param fde The FDE serialized into the file prefix; non-null at open().
  /// @param relay_log_dir The server's relay log directory; non-empty at
  ///        open(). Its @c \#in_memory_relaylog_temp_files subdirectory must
  ///        exist (see ensure_spill_directory()).
  /// @param channel_name Embedded in the file name (see kFileNamePrefix).
  /// @param owner_queue Queue whose spill gauges (SPILL_FILE_COUNT /
  ///        SPILL_BYTES_USED) this writer keeps; nullptr disables accounting.
  /// @param reserved_disk_bytes Admission's spill disk reservation, owned by
  ///        the writer. The destructor or discard_after_setup_failure()
  ///        releases it after the delete attempt, whether or not open()
  ///        succeeded. 0 when none was taken.
  Spill_file_writer(Fde_ptr fde, std::string relay_log_dir,
                    std::string channel_name = {},
                    Trx_envelope_queue *owner_queue = nullptr,
                    std::size_t reserved_disk_bytes = 0);

  /// @brief Closes the IO_CACHE and deletes the temp file, if any. A failed
  /// delete is logged as a warning and leaves the file for the startup purge.
  ~Spill_file_writer();

  Spill_file_writer(const Spill_file_writer &) = delete;
  Spill_file_writer &operator=(const Spill_file_writer &) = delete;
  Spill_file_writer(Spill_file_writer &&) = delete;
  Spill_file_writer &operator=(Spill_file_writer &&) = delete;

  /// @brief Creates a uniquely named @c imr_sp_<channel_name>_<unique_id> file
  /// in the (already-provisioned) temp-files subdirectory and writes the
  /// relay-log prefix (BINLOG_MAGIC + serialized FDE).
  ///
  /// When @c binlog_encryption is ON, the 512-byte encryption header is
  /// written and flushed first, and the prefix is encrypted. Must run under
  /// the channel's relay-log LOCK_log (see spill_encryption.h).
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

  /// @brief Closes the output stream; the file, its gauges and reservation
  /// stay until the destructor. Idempotent. No append_raw()/flush() after it.
  void close_stream();

  /// @brief Current logical end position: the number of bytes written so far
  /// (prefix + all appended bytes). Advances monotonically. Excludes the
  /// encryption header of an encrypted file.
  my_off_t end_position() const { return m_end_pos; }

  /// @brief Full path of the backing spill file (empty until open() succeeds).
  const std::string &file_name() const { return m_file_name; }

  /// @brief Full path of the temp-files subdirectory (empty until open()).
  const std::string &temp_dir() const { return m_temp_dir; }

  /// @brief True once open() has laid down the prefix successfully.
  bool is_open() const { return m_is_open; }

  /// @brief Human-readable message for the last error, empty if none.
  const std::string &get_error_str() const { return m_error; }

  /// @brief After a setup failure, deletes the file without a queue record,
  /// and returns the spill gauges and the spill disk reservation at once.
  void discard_after_setup_failure();

 private:
  /// @brief Creates a uniquely named, O_EXCL spill file under m_temp_dir and
  /// stores its path in m_file_name. Returns true on error.
  bool create_unique_file();

  /// @brief Puts a Binlog_encryption_ostream on top of m_pipeline_head, which
  /// writes the encryption header, and flushes the header to the file. On
  /// failure closes and deletes the file. Returns true on error.
  bool open_encryption_stream();

  /// @brief Deletes m_file_name and clears it. Logs a warning with the path
  /// and errno if the delete fails. Returns true if the delete failed.
  bool delete_file();

  /// @brief FDE serialized into the file prefix.
  Fde_ptr m_fde;
  /// @brief The server's relay log directory (parent of the temp subdir).
  std::string m_relay_log_dir;
  /// @brief The owning channel's name, embedded in the spill file name. Empty
  /// for the default channel.
  std::string m_channel_name;
  /// @brief Non-owning queue whose live spill-disk gauges this writer keeps
  /// current. nullptr disables accounting (unit tests).
  Trx_envelope_queue *m_owner_queue;
  /// @brief Spill disk reservation owned by this writer, returned to
  /// m_owner_queue after the delete attempt (see the constructor).
  std::size_t m_reserved_disk_bytes;
  /// @brief Head of the output pipeline: the IO_CACHE_ostream over the temp
  /// file, or a Binlog_encryption_ostream that owns it. Null while no stream
  /// is open.
  std::unique_ptr<Truncatable_ostream> m_pipeline_head;
  /// @brief Path to the temp-files subdirectory (set in open()).
  std::string m_temp_dir;
  /// @brief Path to the backing spill file (empty until open() succeeds).
  std::string m_file_name;
  /// @brief Last error message, empty if none.
  std::string m_error;
  /// @brief Logical bytes written so far (prefix + appended bytes).
  my_off_t m_end_pos{0};
  /// @brief Bytes on disk in front of the logical stream: 512 (the encryption
  /// header) when the file is encrypted, else 0. Set once in open(); counted
  /// in SPILL_BYTES_USED but not in m_end_pos.
  my_off_t m_physical_overhead{0};
  /// @brief True once the prefix is fully written.
  bool m_is_open{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_FILE_WRITER_H
