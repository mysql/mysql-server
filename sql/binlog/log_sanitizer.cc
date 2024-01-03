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

#include "sql/binlog/log_sanitizer.h"
#include "sql/binlog.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/psi_memory_key.h"
#include "sql/psi_memory_resource.h"
#include "sql/raii/sentry.h"     // raii::Sentry<>
#include "sql/xa/xid_extract.h"  // xa::XID_extractor

namespace binlog {

Log_sanitizer::Log_sanitizer()
    : m_mem_root{key_memory_binlog_recover_exec,
                 static_cast<size_t>(my_getpagesize())},
      m_set_alloc{&m_mem_root},
      m_map_alloc{&m_mem_root},
      m_internal_xids{m_set_alloc},
      m_external_xids{m_map_alloc} {}

my_off_t Log_sanitizer::get_valid_pos() const { return this->m_valid_pos; }

std::pair<my_off_t, bool> Log_sanitizer::get_valid_source_pos() const {
  return std::make_pair(this->m_valid_source_pos, this->m_has_valid_source_pos);
}

std::pair<std::string, bool> Log_sanitizer::get_valid_source_file() const {
  return std::make_pair(this->m_valid_source_file,
                        !this->m_valid_source_file.empty());
}

bool Log_sanitizer::is_log_malformed() const { return this->m_is_malformed; }

bool Log_sanitizer::is_fatal_error() const { return this->m_fatal_error; }

std::string const &Log_sanitizer::get_failure_message() const {
  return this->m_failure_message;
}

bool Log_sanitizer::is_log_truncation_needed() const {
  return m_is_log_truncation_needed;
}

void Log_sanitizer::process_query_event(Query_log_event const &ev) {
  std::string query{ev.query};

  if (!m_validation_started) {
    if (is_atomic_ddl_event(&ev)) {
      m_validation_started = true;
    } else if (query != "BEGIN" && query.find("XA START") != 0) {
      m_validation_started = true;
      return;
    }
  }

  if (query == "BEGIN" || query.find("XA START") == 0)
    this->process_start();

  else if (is_atomic_ddl_event(&ev))
    this->process_atomic_ddl(ev);

  else if (query == "COMMIT")
    this->process_commit();

  else if (query == "ROLLBACK")
    this->process_rollback();

  else if (query.find("XA COMMIT") == 0)
    this->process_xa_commit(query);

  else if (query.find("XA ROLLBACK") == 0)
    this->process_xa_rollback(query);
}

void Log_sanitizer::process_xid_event(Xid_log_event const &ev) {
  if (!m_validation_started) m_validation_started = true;
  this->m_is_malformed = !this->m_in_transaction;
  if (this->m_is_malformed) {
    this->m_failure_message.assign(
        "Xid_log_event outside the boundary of a sequence of events "
        "representing an active transaction");
    return;
  }
  this->m_in_transaction = false;
  if (!this->m_internal_xids.insert(ev.xid).second) {
    this->m_is_malformed = true;
    this->m_failure_message.assign("Xid_log_event holds an invalid XID");
  }
}

void Log_sanitizer::process_xa_prepare_event(XA_prepare_log_event const &ev) {
  if (!m_validation_started) return;
  this->m_is_malformed = !this->m_in_transaction;
  if (this->m_is_malformed) {
    this->m_failure_message.assign(
        "XA_prepare_log_event outside the boundary of a sequence of events "
        "representing an active transaction");
    return;
  }

  this->m_in_transaction = false;

  XID xid;
  xid = ev.get_xid();
  auto found = this->m_external_xids.find(xid);
  if (found != this->m_external_xids.end()) {
    assert(found->second != enum_ha_recover_xa_state::PREPARED_IN_SE);
    if (found->second == enum_ha_recover_xa_state::PREPARED_IN_TC) {
      // If it was found already, must have been committed or rolled back, it
      // can't be in prepared state
      this->m_is_malformed = true;
      this->m_failure_message.assign(
          "XA_prepare_log_event holds an invalid XID");
      return;
    }
  }

  this->m_external_xids[xid] =
      ev.is_one_phase() ? enum_ha_recover_xa_state::COMMITTED_WITH_ONEPHASE
                        : enum_ha_recover_xa_state::PREPARED_IN_TC;
}

void Log_sanitizer::process_start() {
  this->m_is_malformed = this->m_in_transaction;
  if (this->m_is_malformed)
    this->m_failure_message.assign(
        "Query_log_event containing `BEGIN/XA START` inside the boundary of a "
        "sequence of events representing an active transaction");
  this->m_in_transaction = true;
}

void Log_sanitizer::process_commit() {
  this->m_is_malformed = !this->m_in_transaction;
  if (this->m_is_malformed)
    this->m_failure_message.assign(
        "Query_log_event containing `COMMIT` outside the boundary of a "
        "sequence of events representing an active transaction");
  this->m_in_transaction = false;
}

void Log_sanitizer::process_rollback() {
  this->m_is_malformed = !this->m_in_transaction;
  if (this->m_is_malformed)
    this->m_failure_message.assign(
        "Query_log_event containing `ROLLBACK` outside the boundary of a "
        "sequence of events representing an active transaction");
  this->m_in_transaction = false;
}

void Log_sanitizer::process_atomic_ddl(Query_log_event const &ev) {
  this->m_is_malformed = this->m_in_transaction;
  if (this->m_is_malformed) {
    this->m_failure_message.assign(
        "Query_log event containing a DDL inside the boundary of a sequence of "
        "events representing an active transaction");
    return;
  }
  if (!this->m_internal_xids.insert(ev.ddl_xid).second) {
    this->m_is_malformed = true;
    this->m_failure_message.assign(
        "Query_log_event containing a DDL holds an invalid XID");
  }
}

void Log_sanitizer::process_xa_commit(std::string const &query) {
  this->m_is_malformed = this->m_in_transaction;
  this->m_in_transaction = false;
  if (this->m_is_malformed) {
    this->m_failure_message.assign(
        "Query_log_event containing `XA COMMIT` inside the boundary of a "
        "sequence of events representing a transaction not yet in prepared "
        "state");
    return;
  }
  this->add_external_xid(query, enum_ha_recover_xa_state::COMMITTED);
  if (this->m_is_malformed)
    this->m_failure_message.assign(
        "Query_log_event containing `XA COMMIT` holds an invalid XID");
}

void Log_sanitizer::process_xa_rollback(std::string const &query) {
  this->m_is_malformed = this->m_in_transaction;
  this->m_in_transaction = false;
  if (this->m_is_malformed) {
    this->m_failure_message.assign(
        "Query_log_event containing `XA ROLLBACK` inside the boundary of a "
        "sequence of events representing a transaction not yet in prepared "
        "state");
    return;
  }
  this->add_external_xid(query, enum_ha_recover_xa_state::ROLLEDBACK);
  if (this->m_is_malformed)
    this->m_failure_message.assign(
        "Query_log_event containing `XA ROLLBACK` holds an invalid XID");
}

void Log_sanitizer::add_external_xid(std::string const &query,
                                     enum_ha_recover_xa_state state) {
  xa::XID_extractor tokenizer{query, 1};
  if (tokenizer.size() == 0) {
    this->m_is_malformed = true;
    return;
  }

  auto found = this->m_external_xids.find(tokenizer[0]);
  if (found != this->m_external_xids.end()) {
    assert(found->second != enum_ha_recover_xa_state::PREPARED_IN_SE);
    if (found->second != enum_ha_recover_xa_state::PREPARED_IN_TC) {
      // If it was found already, it needs to be in prepared in TC state
      this->m_is_malformed = true;
      return;
    }
  }

  this->m_external_xids[tokenizer[0]] = state;
}

}  // namespace binlog
