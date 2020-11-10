/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTER_DESTINATION_TLS_CONTEXT_INCLUDED
#define MYSQL_ROUTER_DESTINATION_TLS_CONTEXT_INCLUDED

#include <map>
#include <memory>  // make_unique
#include <string>

#include "mysql/harness/tls_client_context.h"
#include "ssl_mode.h"  // SslVerify

/**
 * TlsClientContext per destination.
 */
class DestinationTlsContext {
 public:
  void verify(SslVerify ssl_verify) { ssl_verify_ = ssl_verify; }
  void ca_file(const std::string &file) { ca_file_ = file; }
  void ca_path(const std::string &path) { ca_path_ = path; }
  void crl_file(const std::string &file) { crl_file_ = file; }
  void crl_path(const std::string &path) { crl_path_ = path; }
  void curves(const std::string &curves) { curves_ = curves; }
  void ciphers(const std::string &ciphers) { ciphers_ = ciphers; }

  TlsClientContext *get(const std::string &dest_id) {
    auto it = tls_contexts_.find(dest_id);
    if (it == tls_contexts_.end()) {
      // not found
      auto res =
          tls_contexts_.emplace(dest_id, std::make_unique<TlsClientContext>());
      auto *tls_ctx = res.first->second.get();

      if (!ciphers_.empty()) tls_ctx->cipher_list(ciphers_);
      if (!curves_.empty()) tls_ctx->curves_list(curves_);

      switch (ssl_verify_) {
        case SslVerify::kDisabled:
          tls_ctx->verify(TlsVerify::NONE);
          break;
        case SslVerify::kVerifyIdentity:
          tls_ctx->verify_hostname(dest_id);
          // fallthrough
        case SslVerify::kVerifyCa:
          tls_ctx->ssl_ca(ca_file_, ca_path_);
          tls_ctx->crl(crl_file_, crl_path_);
          tls_ctx->verify(TlsVerify::PEER);
          break;
      }

      return tls_ctx;
    } else {
      return it->second.get();
    }
  }

 private:
  SslVerify ssl_verify_{SslVerify::kDisabled};
  std::string ca_file_;
  std::string ca_path_;
  std::string crl_file_;
  std::string crl_path_;
  std::string curves_;
  std::string ciphers_;

  std::map<std::string, std::unique_ptr<TlsClientContext>> tls_contexts_;
};

#endif
