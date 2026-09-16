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

#ifndef MYSQL_CSA_STORAGE_RELAY_LOG_DATA_SOURCE_H
#define MYSQL_CSA_STORAGE_RELAY_LOG_DATA_SOURCE_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "mysql/allocators/allocator.h"

namespace mysql::csa {

class Data_source;
using Data_source_sptr = std::shared_ptr<Data_source>;

/// @brief Represents a cached chunk of data that comes from the specific relay
/// log file (events / headers may cross boundaries of Data_source, but
/// one Data_source does not spread across multiple relay log files)
/// Keeps track of the number of bytes currently in use
class Data_source {
 public:
  using Memory_allocator = mysql::allocators::Allocator<uint8_t>;
  using Data_type = std::vector<uint8_t, Memory_allocator>;
  /// @brief Construct from a vector of bytes and metadata of the relay log file
  /// this data was fetched from
  /// @param data Data_source gets ownership of this data
  /// @param file_name The relay log file path and name (full path)
  /// @param file_offset The relay log file offset starting from which data
  /// was fetched
  /// @param file_length The number of bytes in the `file_name` file
  /// @param ends_file True if this data batch ends the file
  Data_source(Data_type &&data, const std::string &file_name,
              std::size_t file_offset, std::size_t file_length, bool ends_file);
  /// @brief Accesses fetched data
  /// @return Const reference to bytes fetched
  const Data_type &data() const;
  /// @brief Accesses raw data (reading)
  uint8_t *data_raw();
  /// @brief Obtains data size
  /// @return data size
  std::size_t size() const;
  /// @brief Get source file name
  /// @return Source file name
  const std::string &get_file_name() const;
  /// @brief Get source file length
  /// @return Source file length (number of bytes in the file)
  std::size_t get_file_length() const;
  /// @brief Return current batch file offset
  /// @return This batch file offset
  std::size_t get_file_offset() const;
  /// Checks if this data chunk ends the file
  /// @return True if this data chunk ends the file, false otherwise
  bool ends_file() const;
  /// @brief Destructs object
  virtual ~Data_source();

 private:
  /// Cached chunk of data
  Data_type m_data;
  /// Variable to track the number of cached bytes used
  static std::atomic<std::size_t> m_used_bytes;
  /// Source relay log name
  std::string m_file_name{""};
  /// Source relay log offset
  std::size_t m_file_offset{0};
  /// Source file length
  std::size_t m_file_length{0};
  /// True if EOF appears after this data chunk
  bool m_is_eof{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_RELAY_LOG_DATA_SOURCE_H
