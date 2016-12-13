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

#ifndef _NGS_ASIO_OPTIONS_H_
#define _NGS_ASIO_OPTIONS_H_

#include <string>
#include <vector>
#include "ngs_common/smart_ptr.h"


namespace ngs
{

  class IOptions_session
  {
  public:
    virtual ~IOptions_session() {};

    virtual bool supports_tls() = 0;
    virtual bool active_tls() = 0;

    virtual std::string ssl_cipher() = 0;
    virtual std::vector<std::string> ssl_cipher_list() = 0;

    virtual std::string ssl_version() = 0;

    virtual long ssl_verify_depth() = 0;
    virtual long ssl_verify_mode() = 0;
    virtual long ssl_sessions_reused() = 0;

    virtual long ssl_get_verify_result_and_cert() = 0;
    virtual std::string ssl_get_peer_certificate_issuer() = 0;
    virtual std::string ssl_get_peer_certificate_subject() = 0;
  };

  class Options_session_default : public IOptions_session
  {
  public:
    bool supports_tls() { return false; };
    bool active_tls() { return false; };
    std::string ssl_cipher() { return ""; };
    std::vector<std::string> ssl_cipher_list() { return std::vector<std::string>(); };
    std::string ssl_version() { return ""; };

    long ssl_ctx_verify_depth() { return 0; }
    long ssl_ctx_verify_mode() { return 0; }
    long ssl_verify_depth() { return 0; }
    long ssl_verify_mode() { return 0; }

    std::string ssl_server_not_after() { return ""; }
    std::string ssl_server_not_before() { return ""; }
    long ssl_sessions_reused() { return 0; }
    long ssl_get_verify_result_and_cert() { return 0; }
    std::string ssl_get_peer_certificate_issuer()  { return ""; }
    std::string ssl_get_peer_certificate_subject() { return ""; }
  };

  class IOptions_context
  {
  public:
    virtual ~IOptions_context() {};

    virtual long ssl_ctx_verify_depth() = 0;
    virtual long ssl_ctx_verify_mode() = 0;

    virtual std::string ssl_server_not_after() = 0;
    virtual std::string ssl_server_not_before() = 0;

    virtual long ssl_sess_accept_good() = 0;
    virtual long ssl_sess_accept() = 0;
    virtual long ssl_accept_renegotiates() = 0;

    virtual std::string ssl_session_cache_mode() = 0;

    virtual long ssl_session_cache_hits() = 0;
    virtual long ssl_session_cache_misses() = 0;
    virtual long ssl_session_cache_overflows() = 0;
    virtual long ssl_session_cache_size() = 0;
    virtual long ssl_session_cache_timeouts() = 0;
    virtual long ssl_used_session_cache_entries() = 0;
  };

  class Options_context_default : public IOptions_context
  {
  public:
    virtual long ssl_ctx_verify_depth() { return 0; }
    virtual long ssl_ctx_verify_mode() { return 0; }

    virtual std::string ssl_server_not_after() { return ""; }
    virtual std::string ssl_server_not_before() { return ""; }

    virtual long ssl_sess_accept_good() { return 0; }
    virtual long ssl_sess_accept() { return 0; }
    virtual long ssl_accept_renegotiates() { return 0; }

    virtual std::string ssl_session_cache_mode() { return ""; }

    virtual long ssl_session_cache_hits() { return 0; }
    virtual long ssl_session_cache_misses() { return 0; }
    virtual long ssl_session_cache_overflows() { return 0; }
    virtual long ssl_session_cache_size() { return 0; }
    virtual long ssl_session_cache_timeouts() { return 0; }
    virtual long ssl_used_session_cache_entries() { return 0; }
  };

  typedef ngs::shared_ptr<IOptions_session> IOptions_session_ptr;
  typedef ngs::shared_ptr<IOptions_context> IOptions_context_ptr;

} // namespace ngs

#endif // _NGS_ASIO_OPTIONS_H_
