/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_ASIO_OPTIONS_SSL_H_
#define _NGS_ASIO_OPTIONS_SSL_H_

#include "ngs_common/options.h"
#include <violite.h>

namespace ngs
{

  class Options_session_ssl : public IOptions_session
  {
  public:
    Options_session_ssl(Vio *vio)
    : m_vio(vio)
    {
    }

    bool supports_tls() { return true; };
    bool active_tls() { return true; };

    std::string ssl_cipher();
    std::string ssl_version();
    std::vector<std::string> ssl_cipher_list();

    long ssl_verify_depth();
    long ssl_verify_mode();

    long ssl_sessions_reused();
    long ssl_get_verify_result_and_cert();

    std::string ssl_get_peer_certificate_issuer();

    std::string ssl_get_peer_certificate_subject();

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

    long ssl_ctx_verify_depth();
    long ssl_ctx_verify_mode();

    std::string ssl_server_not_after();
    std::string ssl_server_not_before();

    long ssl_sess_accept_good();
    long ssl_sess_accept();
    long ssl_accept_renegotiates();

    std::string ssl_session_cache_mode();

    long ssl_session_cache_hits();
    long ssl_session_cache_misses();
    long ssl_session_cache_overflows();
    long ssl_session_cache_size();
    long ssl_session_cache_timeouts();
    long ssl_used_session_cache_entries();
  private:
    st_VioSSLFd *m_vio_ssl;
  };

} // namespace ngs

#endif // _NGS_ASIO_OPTIONS_SSL_H_
