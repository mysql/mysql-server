// Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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

#ifndef BINLOG_LOG_SANITIZER_IMPL_HPP
#define BINLOG_LOG_SANITIZER_IMPL_HPP

#include "include/scope_guard.h"
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysqld_error.h"
#include "sql/binlog.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/binlog/log_sanitizer.h"
#include "sql/psi_memory_key.h"
#include "sql/psi_memory_resource.h"
#include "sql/raii/sentry.h"     // raii::Sentry<>
#include "sql/xa/xid_extract.h"  // xa::XID_extractor

namespace binlog {

template <class Type_reader>
void Log_sanitizer::process_logs(Type_reader &reader,
                                 const std::list<std::string> &list_of_files,
                                 MYSQL_BIN_LOG &log) {
  // function we run for relay logs
  for (auto rit = list_of_files.rbegin(); rit != list_of_files.rend(); ++rit) {
    this->m_validation_started = false;
    if (process_one_log(reader, *rit) || rit == --list_of_files.rend()) {
      // valid log file found or no valid position was found in relay logs
      // remove relay logs containing no valid positions in case a valid
      // position has been found in one of the files
      if (rit != list_of_files.rbegin() && this->m_has_valid_pos) {
        // keeps files between first and last; removes the rest
        log.remove_logs_outside_range_from_index(*(list_of_files.begin()),
                                                 *(rit));
        // remove logs containing partially written transaction
        auto rem_it = rit;
        do {
          --rem_it;
          std::stringstream ss;
          ss << "Removed " << *rem_it
             << " from index file: " << log.get_index_fname()
             << " ; removing file from disk";
          LogErr(INFORMATION_LEVEL, ER_LOG_SANITIZATION, ss.str().c_str());
          my_delete_allow_opened(rem_it->c_str(), MYF(0));
        } while (rem_it != list_of_files.rbegin());
      }
      return;
    }
  }
}

template <class Type_reader>
void Log_sanitizer::process_logs(Type_reader &reader, MYSQL_BIN_LOG &log) {
  auto [list_of_files, status] = log.get_filename_list();
  if (status.is_error()) {
    this->m_fatal_error = true;
    this->m_is_malformed = true;
    this->m_failure_message.assign("Could not process index file");
    return;
  }
  process_logs(reader, list_of_files, log);
}

template <class Type_reader>
void Log_sanitizer::process_logs(Type_reader &reader) {
  if (!reader.is_open()) {
    this->m_fatal_error = true;
    this->m_is_malformed = true;
    this->m_failure_message.assign("Reader is not initialized");
    return;
  }
  process_one_log(reader, reader.get_file_name());
}

template <class Type_reader>
bool Log_sanitizer::process_one_log(Type_reader &reader,
                                    const std::string &filename) {
  Scope_guard close_at_end([&reader]() { reader.close(); });

  if (!reader.is_open()) {
    if (reader.open(filename.c_str())) {
      this->m_is_malformed = true;
      this->m_fatal_error = true;
      this->m_failure_message.assign("Could not open relay log file");
      close_at_end.commit();
      return false;
    }
  } else {
    close_at_end.commit();
  }

  m_last_file_size = reader.ifile()->length();
  Decompressing_event_object_istream istream{
      reader, psi_memory_resource(get_memory_key())};
  std::shared_ptr<Log_event> ev;
  this->m_valid_pos = reader.position();
  this->m_valid_file = filename;
  this->m_valid_source_pos = BIN_LOG_HEADER_SIZE;
  bool contains_finished_transaction = false;
  this->m_in_transaction = false;
  this->m_is_malformed = false;

  while (istream >> ev) {
    bool is_source_event = !ev->is_relay_log_event() ||
                           (ev->server_id && ::server_id != ev->server_id);
    switch (ev->get_type_code()) {
      case mysql::binlog::event::QUERY_EVENT: {
        this->process_query_event(dynamic_cast<Query_log_event &>(*ev));
        break;
      }
      case mysql::binlog::event::XID_EVENT: {
        this->process_xid_event(dynamic_cast<Xid_log_event &>(*ev));
        break;
      }
      case mysql::binlog::event::XA_PREPARE_LOG_EVENT: {
        this->process_xa_prepare_event(
            dynamic_cast<XA_prepare_log_event &>(*ev));
        break;
      }
      case mysql::binlog::event::ROTATE_EVENT: {
        if (is_source_event) {
          m_validation_started = true;
        }
        break;
      }
      default: {
        break;
      }
    }

    // Whenever the current position is at a transaction boundary, save it
    // to m_valid_pos
    if (!this->m_is_malformed && !this->m_in_transaction &&
        !is_any_gtid_event(ev.get()) && !is_session_control_event(ev.get()) &&
        m_validation_started) {
      this->m_valid_pos = reader.position();
      if (ev->get_type_code() != mysql::binlog::event::STOP_EVENT &&
          ev->get_type_code() !=
              mysql::binlog::event::FORMAT_DESCRIPTION_EVENT &&
          ev->get_type_code() != mysql::binlog::event::ROTATE_EVENT) {
        this->m_valid_source_pos = ev->common_header->log_pos;
        this->m_has_valid_source_pos = true;
      }
      if (ev->get_type_code() == mysql::binlog::event::ROTATE_EVENT &&
          is_source_event) {
        auto rev = dynamic_cast<mysql::binlog::event::Rotate_event *>(ev.get());
        if (rev->new_log_ident != nullptr) {
          this->m_valid_source_file.resize(rev->ident_len);
          memcpy(this->m_valid_source_file.data(), rev->new_log_ident,
                 rev->ident_len);
          this->m_has_valid_source_pos = true;
          this->m_valid_source_pos = rev->pos;
        }
      }
      this->m_has_valid_pos = true;
      contains_finished_transaction = true;
    }
    if (this->m_is_malformed) break;
  }
  if (istream.has_error()) {
    using Status_t = Decompressing_event_object_istream::Status_t;
    switch (istream.get_status()) {
      case Status_t::out_of_memory:
        this->m_is_malformed = true;
        this->m_fatal_error = true;
        this->m_failure_message.assign("Out of memory");
        break;
      case Status_t::exceeds_max_size:
        this->m_is_malformed = true;
        this->m_fatal_error = true;
        this->m_failure_message.assign(istream.get_error_str().c_str());
        break;
      case Status_t::corrupted:  // even if decoding failed somehow, we will
                                 // trim
      case Status_t::success:
      case Status_t::end:
      case Status_t::truncated:
        break;
    }
  }

  if ((reader.position() != this->m_valid_pos || istream.has_error()) &&
      contains_finished_transaction && !is_fatal_error()) {
    m_is_log_truncation_needed = true;
    std::stringstream ss;
    ss << "The following log needs truncation:" << filename;
    ss << " ; read up to: " << reader.position();
    LogErr(INFORMATION_LEVEL, ER_LOG_SANITIZATION, ss.str().c_str());
  }
  return contains_finished_transaction;
}

}  // namespace binlog

#endif  // BINLOG_LOG_SANITIZER_IMPL_HPP
