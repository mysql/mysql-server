/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_OPTIONS_SSL_H_
#define _NGS_OPTIONS_SSL_H_

#include "plugin/x/ngs/include/ngs_common/options.h"
#include "violite.h"

namespace ngs
{

  class Options_session_ssl : public IOptions_session
  {
  public:
    Options_session_ssl(Vio *vio)
    : m_vio(vio)
    {
    }

    bool supports_tls() const override { return true; };
    bool active_tls() const override { return true; };

    std::string ssl_cipher() const override;
    std::string ssl_version() const override;
    std::vector<std::string> ssl_cipher_list() const override;

    long ssl_verify_depth() const override;
    long ssl_verify_mode() const override;

    long ssl_sessions_reused() const override;
    long ssl_get_verify_result_and_cert() const override;

    std::string ssl_get_peer_certificate_issuer() const override;

    std::string ssl_get_peer_certificate_subject() const override;

  private:
    Vio *m_vio;
  };

  class Options_context_ssl : public IOptions_context
  {
  public:
    Options_context_ssl(st_VioSSLFd *vio_ssl)
    : m_vio_ssl(vio_ssl)
    {
    }

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

} // namespace ngs

#endif // _NGS_OPTIONS_SSL_H_
