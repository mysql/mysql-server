/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <algorithm>
#include <iterator>

#include "mysql/service_ssl_wrapper.h"
#include "plugin/x/src/ssl_session_options.h"

namespace xpl {

bool Ssl_session_options::active_tls() const {
  return m_vio->get_type() == Connection_type::Connection_tls;
}

std::string Ssl_session_options::ssl_cipher() const {
  char result[1024];

  if (!active_tls()) return "";

  ssl_wrapper_cipher(m_vio->get_vio(), result, sizeof(result) - 1);
  result[sizeof(result) - 1] = '\0';

  return result;
}

std::string Ssl_session_options::ssl_version() const {
  char result[256];

  if (!active_tls()) return "";

  ssl_wrapper_version(m_vio->get_vio(), result, sizeof(result));

  return result;
}

std::vector<std::string> Ssl_session_options::ssl_cipher_list() const {
  std::vector<std::string> result;
  const int64_t num_of_elements = 1024;
  const char *versions[num_of_elements];

  if (active_tls()) {
    int64_t number_of_items =
        ssl_wrapper_cipher_list(m_vio->get_vio(), versions, num_of_elements);

    std::copy(versions, versions + number_of_items, std::back_inserter(result));
  }

  return result;
}

int64_t Ssl_session_options::ssl_verify_depth() const {
  if (!active_tls()) return 0;

  return ssl_wrapper_verify_depth(m_vio->get_vio());
}

int64_t Ssl_session_options::ssl_verify_mode() const {
  if (!active_tls()) return 0;

  return ssl_wrapper_verify_mode(m_vio->get_vio());
}

int64_t Ssl_session_options::ssl_sessions_reused() const { return 0; }

int64_t Ssl_session_options::ssl_get_verify_result_and_cert() const {
  if (!active_tls()) return 0;

  return ssl_wrapper_get_verify_result_and_cert(m_vio->get_vio());
}

std::string Ssl_session_options::ssl_get_peer_certificate_issuer() const {
  char issuer[1024];

  if (!active_tls()) return "";

  ssl_wrapper_get_peer_certificate_issuer(m_vio->get_vio(), issuer,
                                          sizeof(issuer));

  return issuer;
}

std::string Ssl_session_options::ssl_get_peer_certificate_subject() const {
  char subject[1024];

  if (!active_tls()) return "";

  ssl_wrapper_get_peer_certificate_subject(m_vio->get_vio(), subject,
                                           sizeof(subject));

  return subject;
}

}  // namespace xpl
