/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "log_client.h"

#include <assert.h>

#include <iostream>

namespace auth_ldap_sasl_client {

Ldap_logger *Ldap_logger::m_logger = nullptr;

void Ldap_logger::create_logger(ldap_log_level log_level) {
  if (!m_logger) m_logger = new Ldap_logger(log_level);
}
void Ldap_logger::destroy_logger() {
  if (m_logger) {
    delete m_logger;
    m_logger = nullptr;
  }
}

Ldap_logger::Ldap_logger(ldap_log_level log_level)
    : m_log_writer(nullptr), m_log_level(log_level) {
  m_log_writer = new Ldap_log_writer_error();
}

Ldap_logger::~Ldap_logger() {
  if (m_log_writer) {
    delete m_log_writer;
  }
}

const char dbg_prefix[] = "[DBG]";
const char info_prefix[] = "[Note]";
const char warning_prefix[] = "[Warning]";
const char error_prefix[] = "[Error]";

void Ldap_logger::log_dbg_msg(Message msg) {
  assert(m_logger);
  m_logger->log<ldap_log_level::LDAP_LOG_LEVEL_ALL, dbg_prefix>(msg);
}
void Ldap_logger::log_info_msg(Message msg) {
  assert(m_logger);
  m_logger->log<ldap_log_level::LDAP_LOG_LEVEL_ERROR_WARNING_INFO, info_prefix>(
      msg);
}
void Ldap_logger::log_warning_msg(Message msg) {
  assert(m_logger);
  m_logger->log<ldap_log_level::LDAP_LOG_LEVEL_ERROR_WARNING, warning_prefix>(
      msg);
}
void Ldap_logger::log_error_msg(Message msg) {
  assert(m_logger);
  m_logger->log<ldap_log_level::LDAP_LOG_LEVEL_ERROR_WARNING, error_prefix>(
      msg);
}

void Ldap_log_writer_error::write(const std::string &data) {
  std::cerr << data << "\n";
  std::cerr.flush();
}

/**
  This class writes error into default error streams.
  We needed this constructor because of template class usage.
*/
Ldap_log_writer_error::Ldap_log_writer_error() = default;

/**
 */
Ldap_log_writer_error::~Ldap_log_writer_error() = default;

}  // namespace auth_ldap_sasl_client
