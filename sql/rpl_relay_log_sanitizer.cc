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

#include "sql/rpl_relay_log_sanitizer.h"
#include "sql/binlog.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/binlog_reader.h"  // Relay_log_file_reader
#include "sql/rpl_mi.h"         // Master_info

namespace rpl {

void Relay_log_sanitizer::analyze_logs(MYSQL_BIN_LOG &log,
                                       bool checksum_validation) {
  Relaylog_file_reader reader(checksum_validation);
  this->process_logs(reader, log);
}

bool Relay_log_sanitizer::sanitize_log(MYSQL_BIN_LOG &log) {
  std::stringstream ss;
  if (!is_fatal_error() && is_log_truncation_needed()) {
    ss << "Truncating " << get_valid_file()
       << " to log position: " << get_valid_pos();
    LogErr(INFORMATION_LEVEL, ER_LOG_SANITIZATION, ss.str().c_str());
    return log.truncate_update_log_file(
        get_valid_file().c_str(), get_valid_pos(), m_last_file_size, false);
  }
  if (is_fatal_error()) {
    ss << "Skipping log sanitization due to: " << this->m_failure_message;
    LogErr(INFORMATION_LEVEL, ER_LOG_SANITIZATION, ss.str().c_str());
  }
  return false;
}

void Relay_log_sanitizer::update_source_position(Master_info *mi) {
  if (is_fatal_error()) return;
  std::string new_source_file{""};
  my_off_t new_source_pos{0};
  if (!this->m_valid_source_file.empty()) {
    // we are sure that we were able to obtain source position
    assert(this->m_has_valid_source_pos);
    new_source_file = m_valid_source_file;
    new_source_pos = m_valid_source_pos;

  } else if (this->m_has_valid_source_pos) {
    // update only postion
    new_source_pos = m_valid_source_pos;
    new_source_file = mi->get_master_log_name();
  } else {
    // no valid position could have been recovered from the source file,
    // setting to applier source position
    new_source_pos = std::max<ulonglong>(BIN_LOG_HEADER_SIZE,
                                         mi->rli->get_group_master_log_pos());
    new_source_file = mi->rli->get_group_master_log_name();
  }

  if (mi->get_master_log_name() != new_source_file ||
      mi->get_master_log_pos() != new_source_pos) {
    std::stringstream ss;
    std::string new_source_file_str = new_source_file.empty()
                                          ? Master_info::first_source_log_name
                                          : new_source_file;
    ss << "Changing source log coordinates from: " << mi->get_io_rpl_log_name()
       << "; " << mi->get_master_log_pos() << " to: " << new_source_file_str
       << "; " << new_source_pos;
    LogErr(INFORMATION_LEVEL, ER_LOG_SANITIZATION, ss.str().c_str());
    // set position and filename
    mi->set_master_log_pos(new_source_pos);
    mi->set_master_log_name(new_source_file.c_str());
  }
}

}  // namespace rpl
