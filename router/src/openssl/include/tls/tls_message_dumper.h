/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_MESSAGE_DUMPER_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_MESSAGE_DUMPER_H_

#include <openssl/ssl.h>

#include <string>
#include <utility>

namespace tls {

class TlsMessageDumper {
 public:
  TlsMessageDumper(SSL_CTX *ctx, std::string &&name)
      : ctx_{ctx}, name_{std::move(name)} {
    SSL_CTX_set_msg_callback(ctx_, TlsMessageDumper::message_callback);

    if (!name_.empty()) SSL_CTX_set_msg_callback_arg(ctx_, &name_[0]);
  }

  TlsMessageDumper(SSL *ssl, std::string &&name)
      : ssl_{ssl}, name_{std::move(name)} {
    SSL_set_msg_callback(ssl_, TlsMessageDumper::message_callback);

    if (!name_.empty()) SSL_set_msg_callback_arg(ssl_, &name_[0]);
  }

  ~TlsMessageDumper() {
    if (ctx_) {
      SSL_CTX_set_msg_callback(ctx_, nullptr);

      if (!name_.empty()) SSL_CTX_set_msg_callback_arg(ctx_, nullptr);
    }

    if (ssl_) {
      SSL_set_msg_callback(ssl_, nullptr);

      if (!name_.empty()) SSL_set_msg_callback_arg(ssl_, nullptr);
    }
  }

 private:
  static std::string to_string_write_p(int write_p) {
    if (0 == write_p) return "RECV";
    return "SEND";
  }

  static std::string to_string_version(int version) {
    switch (version) {
      case SSL2_VERSION:
        return "SSL2";
      case SSL3_VERSION:
        return "SSL3";

      case TLS1_VERSION:
        return "TLS1";
      case TLS1_1_VERSION:
        return "TLS1.1";
      case TLS1_2_VERSION:
        return "TLS1.2";
      case TLS1_3_VERSION:
        return "TLS1.3";

      default:
        return std::string("unknown-") + std::to_string(version);
    }
  }

  static std::string to_string_content(int content) {
    switch (content) {
      case 0:
        return "UNDEFINED";
      case SSL3_RT_HANDSHAKE:
        return "SSL3_RT_HANDSHAKE";
      case SSL3_RT_CHANGE_CIPHER_SPEC:
        return "SSL3_RT_CHANGE_CIPHER_SPEC";
      case SSL3_RT_HEADER:
        return "SSL3_RT_HEADER";
      case SSL3_RT_INNER_CONTENT_TYPE:
        return "SSL3_RT_INNER_CONTENT_TYPE";
      case SSL3_RT_ALERT:
        return "SSL3_RT_ALERT";

      default:
        return "UNKNOWN";
    }
  }

  static std::string to_string_name(void *arg) {
    if (nullptr == arg) return "";

    std::string result{reinterpret_cast<char *>(arg)};
    result += "/";
    return result;
  }

  static void message_callback(int write_p, int version, int content_type,
                               const void *buf, size_t len, SSL *, void *arg) {
    std::cout << to_string_name(arg) << "OpenSSL-" << to_string_write_p(write_p)
              << ", VERSION:" << to_string_version(version)
              << ", content:" << to_string_content(content_type)
              << ", buffer: " << buf << ", len:" << len << std::endl;
  }

  SSL *ssl_{nullptr};
  SSL_CTX *ctx_{nullptr};
  std::string name_;
};

}  // namespace tls

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_MESSAGE_DUMPER_H_
