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

#ifndef _NGS_OPTIONS_H_
#define _NGS_OPTIONS_H_

#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"


namespace ngs
{

  class IOptions_session
  {
  public:
    virtual ~IOptions_session() {};

    virtual bool supports_tls() const = 0;
    virtual bool active_tls() const = 0;

    virtual std::string ssl_cipher() const = 0;
    virtual std::vector<std::string> ssl_cipher_list() const = 0;

    virtual std::string ssl_version() const = 0;

    virtual long ssl_verify_depth() const = 0;
    virtual long ssl_verify_mode() const = 0;
    virtual long ssl_sessions_reused() const = 0;

    virtual long ssl_get_verify_result_and_cert() const = 0;
    virtual std::string ssl_get_peer_certificate_issuer() const = 0;
    virtual std::string ssl_get_peer_certificate_subject() const = 0;
  };

  class Options_session_default : public IOptions_session
  {
  public:
    bool supports_tls() const override { return false; };
    bool active_tls() const override { return false; };
    std::string ssl_cipher() const override { return ""; };
    std::vector<std::string> ssl_cipher_list() const override {
      return std::vector<std::string>();
    };
    std::string ssl_version() const override { return ""; };

    long ssl_ctx_verify_depth() const { return 0; }
    long ssl_ctx_verify_mode() const { return 0; }
    long ssl_verify_depth() const override { return 0; }
    long ssl_verify_mode() const override { return 0; }

    std::string ssl_server_not_after() const { return ""; }
    std::string ssl_server_not_before() const { return ""; }
    long ssl_sessions_reused() const override { return 0; }
    long ssl_get_verify_result_and_cert() const override { return 0; }
    std::string ssl_get_peer_certificate_issuer() const override { return ""; }
    std::string ssl_get_peer_certificate_subject() const override { return ""; }
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
    long ssl_ctx_verify_depth() override { return 0; }
    long ssl_ctx_verify_mode() override { return 0; }

    std::string ssl_server_not_after() override { return ""; }
    std::string ssl_server_not_before() override { return ""; }

    long ssl_sess_accept_good() override { return 0; }
    long ssl_sess_accept() override { return 0; }
    long ssl_accept_renegotiates() override { return 0; }

    std::string ssl_session_cache_mode() override { return ""; }

    long ssl_session_cache_hits() override { return 0; }
    long ssl_session_cache_misses() override { return 0; }
    long ssl_session_cache_overflows() override { return 0; }
    long ssl_session_cache_size() override { return 0; }
    long ssl_session_cache_timeouts() override { return 0; }
    long ssl_used_session_cache_entries() override { return 0; }
  };

  typedef ngs::shared_ptr<IOptions_session> IOptions_session_ptr;
  typedef ngs::shared_ptr<IOptions_context> IOptions_context_ptr;

} // namespace ngs

#endif // _NGS_OPTIONS_H_
