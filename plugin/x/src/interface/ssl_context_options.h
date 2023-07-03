/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_OPTIONS_H_
#define PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_OPTIONS_H_

#include <string>

namespace xpl {
namespace iface {

class Ssl_context_options {
 public:
  virtual ~Ssl_context_options() = default;

  virtual int64_t ssl_ctx_verify_depth() = 0;
  virtual int64_t ssl_ctx_verify_mode() = 0;

  virtual std::string ssl_server_not_after() = 0;
  virtual std::string ssl_server_not_before() = 0;

  virtual int64_t ssl_sess_accept_good() = 0;
  virtual int64_t ssl_sess_accept() = 0;
  virtual int64_t ssl_accept_renegotiates() = 0;

  virtual std::string ssl_session_cache_mode() = 0;

  virtual int64_t ssl_session_cache_hits() = 0;
  virtual int64_t ssl_session_cache_misses() = 0;
  virtual int64_t ssl_session_cache_overflows() = 0;
  virtual int64_t ssl_session_cache_size() = 0;
  virtual int64_t ssl_session_cache_timeouts() = 0;
  virtual int64_t ssl_used_session_cache_entries() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_OPTIONS_H_
