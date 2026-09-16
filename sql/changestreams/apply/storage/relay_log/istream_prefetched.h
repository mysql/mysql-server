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

#ifndef MYSQL_CSA_STORAGE_RELAY_LOG_ISTREAM_PREFETCHED_H
#define MYSQL_CSA_STORAGE_RELAY_LOG_ISTREAM_PREFETCHED_H

#include <atomic>
#include <cstring>
#include <memory>
#include <vector>

#include "sql/basic_istream.h"  // Basic_istream
#include "sql/changestreams/apply/storage/relay_log/log_prefetcher.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

/// @brief Implementation of the Basic_istream, which reads data from the
/// prefetched relay log. Reads cannot go outside a single relay log file
/// boundary (legacy design). Note that prefetcher is able to move across relay
/// log files. Therefore, to read from the next file, it is sufficient to
/// create a new Istream_prefetched object with the same shared prefetcher
/// object.
/// Istream_prefetched allows for:
/// - reading requested number of bytes into the preallocated buffer (read)
/// - skipping requested number of bytes
/// - seeking to a given file position (forward only)
/// - length Function that will return the size of the file; length function
///   is implemented as required by
///   the `Basic_seekable_istream`, however, it is probably not required
///   (note Stdin_binlog_istream has the 'length' function disabled)
/// Istream_prefetched does not add new errors to error model, however, it needs
/// to propagate prefetcher error. In case of prefetcher error, read, skip and
/// seek functions will return an error value.
class Istream_prefetched : public Basic_seekable_istream {
 public:
  /// Construct from prefetcher
  /// @param prefetcher Prefetcher object used to obtain data
  /// @param file_name The file we are allowed to read from
  Istream_prefetched(Log_prefetcher_sptr prefetcher,
                     const std::string &file_name);
  /// @brief Read requested number of bytes from the input stream.
  /// It will block when reaching the end of the open stream that is not yet
  /// prefetched. This function will read beyond the one file boundary if called
  /// many times, unless the requested read would be split into several files.
  /// In that case, it will return the number of bytes read till reaching the
  /// end of the file.
  /// @param buffer Preallocated buffer to >= length or nullptr
  /// @param length The requested number of bytes to read.
  /// @retval length Bytes read
  /// @retval >=0 Reached the end of the file or was stopped in the
  /// process. EOF : Since one operation cannot go beyond file boundary,
  /// operation has been stopped
  /// @retval -1 Error, i.e. prefetcher errored out and cannot read more data
  ssize_t read(unsigned char *buffer, std::size_t length) override;

  /// If possible, seeks to a given file offset
  /// @retval False Success
  /// @retval True Cannot seek to requested postion or prefetcher has errored
  /// out
  bool seek(my_off_t offset) override;

  /// Returns currently opened file length...
  /// @return file length
  my_off_t length() override;

  virtual ~Istream_prefetched() override = default;

 private:
  /// @brief Helper function that copies the requested number of bytes from
  /// the current batch into buffer, using the current buffer offset. Buffer
  /// offset is updated after copy
  /// @param buffer Data will be copied into the following address:
  /// buffer + buffer_offset
  /// @param buffer_offset Current buffer offset, which will be advanced after
  /// copy
  /// @param bytes Copies this amount of bytes
  void copy_to(unsigned char *buffer, std::size_t &buffer_offset,
               std::size_t bytes);

  /// Skips requested number of bytes. Applies the same rules as for read
  /// fuction
  /// @retval length The requested number of bytes have been skipped
  /// @retval >=0 Reached the end of the file or was stopped in the
  /// process. EOF : Since one operation cannot go beyond file boundary,
  /// operation has been stopped
  /// @retval -1 Error, i.e. prefetcher errored out and cannot read more data
  ssize_t skip(std::size_t length);

  /// @brief Reads the next batch
  /// @retval true Stopped in the process or source filename changed
  /// @retval false Data read successfully without file change
  [[nodiscard]] bool read_next_batch();

  using Batch_type = Log_prefetcher::Elem_type;

  /// Prefetcher that asynchronously fetches chunks of data from the relay log
  Log_prefetcher_sptr m_prefetcher;
  /// Prefetcher works asynchronously on the whole relay log. To comply with
  /// implemented structure of Binlog readers, we need to restrict fetching
  /// the data to the file that was requested to open. After reaching end of
  /// this file, instead of fetching next batches we report EOF.
  std::string m_allowed_file_name{""};
  /// Batch currently in use
  Batch_type m_current_batch;
  /// Current batch offset
  std::size_t m_current_offset{0};
  /// Variable used to stop waiting for data (read may block)
  std::atomic<bool> m_is_stopped{false};
  /// True in case there has been prefetcher error which needs to be
  /// propagated
  bool m_prefetcher_error{false};
  /// Offset from start of the file
  std::size_t m_file_offset{0};
};

}  // namespace mysql::csa
#endif  // MYSQL_CSA_STORAGE_RELAY_LOG_ISTREAM_PREFETCHED_H
