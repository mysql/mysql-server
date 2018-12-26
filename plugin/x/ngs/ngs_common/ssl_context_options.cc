/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/ngs/include/ngs_common/ssl_context_options.h"
#include "mysql/service_ssl_wrapper.h"

namespace ngs {

long Ssl_context_options::ssl_ctx_verify_depth() {
  if (nullptr == m_vio_ssl) return 0;

  return ssl_wrapper_ctx_verify_depth(m_vio_ssl);
}

long Ssl_context_options::ssl_ctx_verify_mode() {
  if (nullptr == m_vio_ssl) return 0;

  return ssl_wrapper_ctx_verify_mode(m_vio_ssl);
}

std::string Ssl_context_options::ssl_server_not_after() {
  char result[200];

  if (nullptr == m_vio_ssl) return "";

  ssl_wrapper_ctx_server_not_after(m_vio_ssl, result, sizeof(result));

  return result;
}

std::string Ssl_context_options::ssl_server_not_before() {
  char result[200];

  if (nullptr == m_vio_ssl) return "";

  ssl_wrapper_ctx_server_not_before(m_vio_ssl, result, sizeof(result));

  return result;
}

long Ssl_context_options::ssl_sess_accept_good() {
  if (nullptr == m_vio_ssl) return 0;

  return ssl_wrapper_sess_accept_good(m_vio_ssl);
}

long Ssl_context_options::ssl_sess_accept() {
  if (nullptr == m_vio_ssl) return 0;

  return ssl_wrapper_sess_accept(m_vio_ssl);
}

long Ssl_context_options::ssl_accept_renegotiates() { return 0; }

long Ssl_context_options::ssl_session_cache_hits() { return 0; }

long Ssl_context_options::ssl_session_cache_misses() { return 0; }

std::string Ssl_context_options::ssl_session_cache_mode() { return "OFF"; }

long Ssl_context_options::ssl_session_cache_overflows() { return 0; }

long Ssl_context_options::ssl_session_cache_size() { return 0; }

long Ssl_context_options::ssl_session_cache_timeouts() { return 0; }

long Ssl_context_options::ssl_used_session_cache_entries() { return 0; }

}  // namespace ngs
