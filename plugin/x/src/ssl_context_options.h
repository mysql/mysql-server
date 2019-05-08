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

#ifndef PLUGIN_X_SRC_SSL_CONTEXT_OPTIONS_H_
#define PLUGIN_X_SRC_SSL_CONTEXT_OPTIONS_H_

#include "violite.h"

#include "plugin/x/ngs/include/ngs/interface/ssl_context_options_interface.h"

namespace xpl {

class Ssl_context_options : public ngs::Ssl_context_options_interface {
 public:
  Ssl_context_options(st_VioSSLFd *vio_ssl = nullptr) : m_vio_ssl(vio_ssl) {}

  long ssl_ctx_verify_depth() override;
  long ssl_ctx_verify_mode() override;

  std::string ssl_server_not_after() override;
  std::string ssl_server_not_before() override;

  long ssl_sess_accept_good() override;
  long ssl_sess_accept() override;
  long ssl_accept_renegotiates() override;

  std::string ssl_session_cache_mode() override;

  long ssl_session_cache_hits() override;
  long ssl_session_cache_misses() override;
  long ssl_session_cache_overflows() override;
  long ssl_session_cache_size() override;
  long ssl_session_cache_timeouts() override;
  long ssl_used_session_cache_entries() override;

 private:
  st_VioSSLFd *m_vio_ssl;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SSL_CONTEXT_OPTIONS_H_
