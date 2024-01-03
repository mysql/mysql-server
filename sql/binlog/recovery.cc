/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/binlog/recovery.h"

#include "sql/binlog/decompressing_event_object_istream.h"  // binlog::Decompressing_event_object_istream
#include "sql/psi_memory_key.h"
#include "sql/psi_memory_resource.h"
#include "sql/raii/sentry.h"     // raii::Sentry<>
#include "sql/xa/xid_extract.h"  // xa::XID_extractor

binlog::Binlog_recovery::Binlog_recovery(Binlog_file_reader &binlog_file_reader)
    : m_reader{binlog_file_reader} {
  this->m_validation_started = true;
}

bool binlog::Binlog_recovery::has_failures() const {
  return this->is_log_malformed() || this->m_no_engine_recovery;
}

bool binlog::Binlog_recovery::is_binlog_malformed() const {
  return this->is_log_malformed();
}

bool binlog::Binlog_recovery::has_engine_recovery_failed() const {
  return this->m_no_engine_recovery;
}

std::string const &binlog::Binlog_recovery::get_failure_message() const {
  return this->m_failure_message;
}

binlog::Binlog_recovery &binlog::Binlog_recovery::recover() {
  process_logs(m_reader);
  if (!this->is_log_malformed() && total_ha_2pc > 1) {
    Xa_state_list xa_list{this->m_external_xids};
    this->m_no_engine_recovery = ha_recover(&this->m_internal_xids, &xa_list);
    if (this->m_no_engine_recovery) {
      this->m_failure_message.assign("Recovery failed in storage engines");
    }
  }
  return (*this);
}
