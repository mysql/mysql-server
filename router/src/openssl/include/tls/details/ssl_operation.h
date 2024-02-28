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

#ifndef ROUTER_SRC_HTTP_SRC_TLS_SSL_OPERATION_H_
#define ROUTER_SRC_HTTP_SRC_TLS_SSL_OPERATION_H_

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// OpenSSL version hex format:                  0xMNN00PPSL
#define NET_TLS_USE_BACKWARD_COMPATIBLE_OPENSSL 0x10100000L

namespace net {
namespace tls {

class Operation {
 public:
  enum Result { ok, close, fatal, want_read, want_write };

 public:
  virtual ~Operation() = default;

 protected:
  class AnalyzeOperation {
   public:
    AnalyzeOperation(BIO *bio, SSL *ssl)
        : bio_{bio}, ssl_{ssl}, pending_{BIO_pending(bio)} {
      ERR_clear_error();
    }

    Result check_ssl_result(int ssl_result) {
      auto pending = BIO_pending(bio_);

      auto error_cause = SSL_get_error(ssl_, ssl_result);
      Result op_result = ok;

      if (ssl_result <= 0) {
        switch (error_cause) {
          case SSL_ERROR_WANT_READ:
            op_result = want_read;
            break;

          case SSL_ERROR_WANT_WRITE:
            op_result = want_write;
            break;

          case SSL_ERROR_ZERO_RETURN:
            return close;

          case SSL_ERROR_SYSCALL:
            if (pending > pending_) return want_write;
            return fatal;

          case SSL_ERROR_SSL:
            return fatal;

          default:
            return fatal;
        }
      }

      if (pending) {
        op_result = want_write;
      }

      return op_result;
    }

    BIO *bio_;
    SSL *ssl_;
    int pending_;
  };
};

class SslReadOperation : public Operation {
 public:
  static bool is_read_operation() { return true; }

#if OPENSSL_VERSION_NUMBER >= NET_TLS_USE_BACKWARD_COMPATIBLE_OPENSSL
  static int read_ex(SSL *ssl, void *buf, size_t num,
                     size_t *out_number_of_bytes_io) {
    return SSL_read_ex(ssl, buf, num, out_number_of_bytes_io);
  }
#else
  static int read_ex(SSL *ssl, void *buf, size_t num,
                     size_t *out_number_of_bytes_io) {
    *out_number_of_bytes_io = 0;
    auto result = SSL_read(ssl, buf, num);
    if (result > 0) *out_number_of_bytes_io = result;
    return result;
  }
#endif

  static Result op(BIO *bio, SSL *ssl, void *buffer, const size_t buffer_size,
                   size_t *out_number_of_bytes_io) {
    AnalyzeOperation op{
        bio,
        ssl,
    };

    if (!buffer_size) return ok;

    return op.check_ssl_result(
        read_ex(ssl, buffer, buffer_size, out_number_of_bytes_io));
  }
};

class SslWriteOperation : public Operation {
 public:
#if OPENSSL_VERSION_NUMBER >= NET_TLS_USE_BACKWARD_COMPATIBLE_OPENSSL
  static int write_ex(SSL *ssl, const void *buf, size_t num,
                      size_t *out_number_of_bytes_io) {
    return SSL_write_ex(ssl, buf, num, out_number_of_bytes_io);
  }
#else
  static int write_ex(SSL *ssl, const void *buf, size_t num,
                      size_t *out_number_of_bytes_io) {
    *out_number_of_bytes_io = 0;
    auto result = SSL_write(ssl, buf, num);
    if (result > 0) *out_number_of_bytes_io = result;
    return result;
  }
#endif

  static bool is_read_operation() { return false; }
  static Result op(BIO *bio, SSL *ssl, const void *buffer,
                   const size_t buffer_size, size_t *out_number_of_bytes_io) {
    AnalyzeOperation op{
        bio,
        ssl,
    };

    if (!buffer_size) return ok;

    return op.check_ssl_result(
        write_ex(ssl, buffer, buffer_size, out_number_of_bytes_io));
  }
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_HTTP_SRC_TLS_SSL_OPERATION_H_
