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

#include "sql/changestreams/apply/storage/relay_log/relay_log_deleter.h"
#include <iostream>

namespace mysql::csa {

Relay_log_deleter::Relay_log_deleter(const std::string &rl_file_name,
                                     Log_purge_controller_sptr purge_sptr)
    : m_rl_file_name(rl_file_name), m_purger(purge_sptr) {}

Relay_log_deleter::~Relay_log_deleter() {
  // remove relay log
  if (m_successfull_subscribers.load() == m_subscribers.load()) {
    remove_handled_relay_log();
  }
}

void Relay_log_deleter::add_subscriber() { ++m_subscribers; }

void Relay_log_deleter::set_subscriber_success() {
  ++m_successfull_subscribers;
}

void Relay_log_deleter::remove_handled_relay_log() const {
  m_purger->concurrent_purge(m_rl_file_name.c_str());
}

const std::string &Relay_log_deleter::get_file_name() const {
  return m_rl_file_name;
}

}  // namespace mysql::csa
