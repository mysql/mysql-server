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

#ifndef MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_IFILE_H
#define MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_IFILE_H

#include <atomic>
#include <cstring>
#include <memory>
#include <vector>

#include "sql/basic_istream.h"   // Basic_istream
#include "sql/binlog_istream.h"  // Basic_binlog_ifile
#include "sql/binlog_reader.h"
#include "sql/changestreams/apply/storage/relay_log/istream_prefetched.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

/// Implementation of the Basic_binlog_ifile, which works on the
/// prefetched stream (Istream_prefetched)
/// Exposes additional function to set prefetcher (since legacy design does
/// not allow to use a custom constructor - see Basic_binlog_file_reader).
class Prefetched_ifile : public Basic_binlog_ifile {
 public:
  using Basic_binlog_ifile::Basic_binlog_ifile;

  /// Sets the prefetcher. Prefetcher must be set before opening the first file.
  /// Later on, same prefetcher object is reused to initialize
  /// Istream_prefetched object, which uses asynchronous prefetcher to get
  /// data.
  /// @param prefetcher Shared prefetcher object.
  void set_prefetcher(Log_prefetcher_sptr prefetcher);

 protected:
  /// Creates stream object "from the file". Data will be obtained
  /// via prefetcher
  std::unique_ptr<Basic_seekable_istream> open_file(
      const char *file_name) override;

  /// File currently opened
  std::string m_current_file;
  /// Prefetcher object we need to pass into created stream
  Log_prefetcher_sptr m_prefetcher;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_IFILE_H
