// Copyright (c) 2024, Oracle and/or its affiliates.
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

#ifndef RPL_RELAY_LOG_SANITIZER_H
#define RPL_RELAY_LOG_SANITIZER_H

#include <functional>
#include "mysql/binlog/event/binlog_event.h"
#include "sql/binlog.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // binlog::Decompressing_event_object_istream
#include "sql/binlog/log_sanitizer.h"
#include "sql/binlog_ostream.h"  // binlog::tools::Iterator
#include "sql/binlog_reader.h"   // Binlog_file_reader
#include "sql/log_event.h"       // Log_event
#include "sql/xa.h"              // XID

namespace rpl {

/// @brief Class used to recover relay log files
/// @details Recovery of the relay log files is:
/// - finding the last valid position outside of a transaction boundary
///   (analyze_logs)
/// - removing relay logs appearing after the relay log with the last valid
///   position (analyze_logs)
/// - truncation of the relay log file containing the last valid position
///   to remove partially written transaction from the log (sanitize log)
class Relay_log_sanitizer : public binlog::Log_sanitizer {
 public:
  /// @brief Ctor
  Relay_log_sanitizer() { this->m_validation_started = false; }

  /// @brief Dtor
  ~Relay_log_sanitizer() override = default;

  /// @brief Given specific log, performs sanitization. Reads log list obtained
  /// from the MYSQL_BIN_LOG object and searches for last, fully written
  /// transaction. Removes log files that are created after last finished
  /// transaction
  /// @param log Handle to MYSQL_BIN_LOG object, which does not need to be
  /// open. We need specific functions from the MYSQL_BIN_LOG, e.g.
  /// reading of the index file
  /// @param checksum_validation True if we need to perform relay log file
  /// checksum validation
  void analyze_logs(MYSQL_BIN_LOG &log, bool checksum_validation);

  /// @brief Sanitize opened log
  /// @param log Handle to MYSQL_BIN_LOG object, which we will truncate
  /// if needed
  /// @return false on no error or when no truncation was done, true otherwise
  bool sanitize_log(MYSQL_BIN_LOG &log);

  /// @brief Updates source position if a valid source position has been
  /// found whilst reading the relay log files
  /// @param mi Master_info for the receiver thread.
  void update_source_position(Master_info *mi);

 protected:
  /// @brief Function used to obtain memory key for derived classes
  /// @returns Reference to a memory key
  PSI_memory_key &get_memory_key() const override {
    return key_memory_relaylog_recovery;
  }
};

}  // namespace rpl

#endif  // RPL_RELAY_LOG_SANITIZER_H
