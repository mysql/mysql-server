/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>

#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif /* _WIN32 */

#include "openssl/ssl.h"      /* SSL_* */
#include "openssl/x509_vfy.h" /* X509_* */

#include "mysql/components/services/log_builtins.h" /* LogErr */
#include "mysqld_error.h"                           /* Error/Warning macros */

#include "sql/ssl_acceptor_context_data.h"

/* Helpers */
static const char *verify_store_cert(SSL_CTX *ctx, SSL *ssl) {
  const char *result = nullptr;
  X509 *cert = SSL_get_certificate(ssl);
  X509_STORE_CTX *sctx = X509_STORE_CTX_new();

  if (nullptr != sctx &&
      0 != X509_STORE_CTX_init(sctx, SSL_CTX_get_cert_store(ctx), cert,
                               nullptr) &&
      !X509_verify_cert(sctx)) {
    result = X509_verify_cert_error_string(X509_STORE_CTX_get_error(sctx));
  }
  if (sctx != nullptr) X509_STORE_CTX_free(sctx);
  return result;
}

static char *my_asn1_time_to_string(ASN1_TIME *time, char *buf, int len) {
  int n_read;
  char *res = nullptr;
  BIO *bio = BIO_new(BIO_s_mem());

  if (bio == nullptr) return nullptr;

  if (!ASN1_TIME_print(bio, time)) goto end;

  n_read = BIO_read(bio, buf, len - 1);

  if (n_read > 0) {
    buf[n_read] = 0;
    res = buf;
  }

end:
  BIO_free(bio);
  return res;
}
/* Helpers end */

static std::string Ssl_acceptor_context_propert_type_names[] = {
    "Ssl_accept_renegotiates",
    "Ssl_accepts",
    "Ssl_callback_cache_hits",
    "Ssl_client_connects",
    "Ssl_connect_renegotiates",
    "Ssl_ctx_verify_depth",
    "Ssl_ctx_verify_mode",
    "Current_tls_ca",
    "Current_tls_capath",
    "Current_tls_cert",
    "Current_tls_cipher",
    "Current_tls_ciphersuites",
    "Current_tls_crl",
    "Current_tls_crlpath",
    "Current_tls_key",
    "Current_tls_version",
    "Ssl_finished_accepts",
    "Ssl_finished_connects",
    "Ssl_server_not_after",
    "Ssl_server_not_before",
    "Ssl_session_cache_hits",
    "Ssl_session_cache_misses",
    "Ssl_session_cache_mode",
    "Ssl_session_cache_overflows",
    "Ssl_session_cache_size",
    "Ssl_session_cache_timeouts",
    "Ssl_used_session_cache_entries",
    "Ssl_session_cache_timeout",
    ""};

std::string Ssl_ctx_property_name(
    Ssl_acceptor_context_property_type property_type) {
  return Ssl_acceptor_context_propert_type_names[static_cast<unsigned int>(
      property_type)];
}

Ssl_acceptor_context_property_type &operator++(
    Ssl_acceptor_context_property_type &property_type) {
  property_type = static_cast<Ssl_acceptor_context_property_type>(
      static_cast<
          std::underlying_type<Ssl_acceptor_context_property_type>::type>(
          property_type) +
      1);
  return property_type;
}

Ssl_acceptor_context_data::Ssl_acceptor_context_data(
    std::string channel, bool use_ssl_arg, Ssl_init_callback *callbacks,
    bool report_ssl_error /* = true */,
    enum enum_ssl_init_error *out_error /* = nullptr */)
    : channel_(channel), ssl_acceptor_fd_(nullptr), acceptor_(nullptr) {
  enum enum_ssl_init_error error_num = SSL_INITERR_NOERROR;
  {
    callbacks->read_parameters(
        &current_ca_, &current_capath_, &current_version_, &current_cert_,
        &current_cipher_, &current_ciphersuites_, &current_key_, &current_crl_,
        &current_crlpath_, &current_tls_session_cache_mode_,
        &current_tls_session_cache_timeout_);
  }

  if (use_ssl_arg) {
    long ssl_flags = process_tls_version(current_version_.c_str());

    /* Turn off server's ticket sending for TLS 1.2 if requested */
    if (!current_tls_session_cache_mode_) ssl_flags |= SSL_OP_NO_TICKET;

    ssl_acceptor_fd_ = new_VioSSLAcceptorFd(
        current_key_.c_str(), current_cert_.c_str(), current_ca_.c_str(),
        current_capath_.c_str(), current_cipher_.c_str(),
        current_ciphersuites_.c_str(), &error_num, current_crl_.c_str(),
        current_crlpath_.c_str(), ssl_flags);

    if (!ssl_acceptor_fd_ && report_ssl_error) {
      LogErr(WARNING_LEVEL, ER_WARN_TLS_CHANNEL_INITIALIZATION_ERROR,
             channel_.c_str());
      LogErr(WARNING_LEVEL, ER_SSL_LIBRARY_ERROR, sslGetErrString(error_num));
    }

    if (ssl_acceptor_fd_) acceptor_ = SSL_new(ssl_acceptor_fd_->ssl_context);

    if (ssl_acceptor_fd_ && acceptor_) {
      const char *error =
          verify_store_cert(ssl_acceptor_fd_->ssl_context, acceptor_);

      if (error && report_ssl_error)
        LogErr(WARNING_LEVEL, ER_SSL_SERVER_CERT_VERIFY_FAILED, error);

      SSL_CTX_set_session_cache_mode(ssl_acceptor_fd_->ssl_context,
                                     current_tls_session_cache_mode_
                                         ? SSL_SESS_CACHE_SERVER
                                         : SSL_SESS_CACHE_OFF);
      SSL_CTX_set_timeout(ssl_acceptor_fd_->ssl_context,
                          current_tls_session_cache_timeout_);
#ifdef HAVE_TLSv13
      /* Turn off server's ticket sending for TLS 1.3 if requested */
      if (!current_tls_session_cache_mode_ && !(ssl_flags & SSL_OP_NO_TLSv1_3))
        SSL_CTX_set_num_tickets(ssl_acceptor_fd_->ssl_context, 0);
#endif
    }
  }
  if (out_error) *out_error = error_num;
}

Ssl_acceptor_context_data::~Ssl_acceptor_context_data() {
  if (acceptor_) SSL_free(acceptor_);
  if (ssl_acceptor_fd_) free_vio_ssl_acceptor_fd(ssl_acceptor_fd_);
}

std::string Ssl_acceptor_context_data::show_property(
    Ssl_acceptor_context_property_type property_type) const {
  auto c =
      (ssl_acceptor_fd_ == nullptr) ? nullptr : ssl_acceptor_fd_->ssl_context;
  auto s = acceptor_;
  std::string output;
  switch (property_type) {
    case Ssl_acceptor_context_property_type::accept_renegotiates: {
      output +=
          std::to_string(c == nullptr ? 0 : SSL_CTX_sess_accept_renegotiate(c));
      break;
    }
    case Ssl_acceptor_context_property_type::accepts: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_accept(c));
      break;
    }
    case Ssl_acceptor_context_property_type::callback_cache_hits: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_cb_hits(c));
      break;
    }
    case Ssl_acceptor_context_property_type::client_connects: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_connect(c));
      break;
    }
    case Ssl_acceptor_context_property_type::connect_renegotiates: {
      output += std::to_string(
          c == nullptr ? 0 : SSL_CTX_sess_connect_renegotiate(c));
      break;
    }
    case Ssl_acceptor_context_property_type::ctx_verify_depth: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_get_verify_depth(c));
      break;
    }
    case Ssl_acceptor_context_property_type::ctx_verify_mode: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_get_verify_mode(c));
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_ca: {
      const char *cert = current_ca();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_capath: {
      const char *cert = current_capath();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_cert: {
      const char *cert = current_cert();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_cipher: {
      const char *cert = current_cipher();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_ciphersuites: {
      const char *cert = current_ciphersuites();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_crl: {
      const char *cert = current_crl();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_crlpath: {
      const char *cert = current_crlpath();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_key: {
      const char *cert = current_key();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::current_tls_version: {
      const char *cert = current_version();
      output.assign((cert == nullptr) ? "" : cert);
      break;
    }
    case Ssl_acceptor_context_property_type::finished_accepts: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_accept_good(c));
      break;
    }
    case Ssl_acceptor_context_property_type::finished_connects: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_connect_good(c));
      break;
    }
    case Ssl_acceptor_context_property_type::server_not_after: {
      if (s != nullptr) {
        X509 *cert = SSL_get_certificate(s);
        ASN1_TIME *not_after = X509_get_notAfter(cert);
        if (not_after != nullptr) {
          char buffer[1024] = {};
          (void)my_asn1_time_to_string(not_after, buffer, 1024);
          output.assign(buffer);
        }
      } else
        output.assign("");
      break;
    }
    case Ssl_acceptor_context_property_type::server_not_before: {
      if (s != nullptr) {
        X509 *cert = SSL_get_certificate(s);
        ASN1_TIME *not_before = X509_get_notBefore(cert);
        if (not_before != nullptr) {
          char buffer[1024] = {};
          (void)my_asn1_time_to_string(not_before, buffer, 1024);
          output.assign(buffer);
        }
      } else
        output.assign("");
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_hits: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_hits(c));
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_misses: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_misses(c));
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_mode: {
      if (c == nullptr)
        output.assign("NONE");
      else {
        switch (SSL_CTX_get_session_cache_mode(c)) {
          case SSL_SESS_CACHE_OFF:
            output.assign("OFF");
            break;
          case SSL_SESS_CACHE_CLIENT:
            output.assign("CLIENT");
            break;
          case SSL_SESS_CACHE_SERVER:
            output.assign("SERVER");
            break;
          case SSL_SESS_CACHE_BOTH:
            output.assign("BOTH");
            break;
          case SSL_SESS_CACHE_NO_AUTO_CLEAR:
            output.assign("NO_AUTO_CLEAR");
            break;
          case SSL_SESS_CACHE_NO_INTERNAL_LOOKUP:
            output.assign("NO_INTERNAL_LOOKUP");
            break;
          default:
            output.assign("UNKNOWN");
            break;
        }
      }
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_overflows: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_cache_full(c));
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_size: {
      output +=
          std::to_string(c == nullptr ? 0 : SSL_CTX_sess_get_cache_size(c));
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_timeouts: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_timeouts(c));
      break;
    }
    case Ssl_acceptor_context_property_type::used_session_cache_entries: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_sess_number(c));
      break;
    }
    case Ssl_acceptor_context_property_type::session_cache_timeout: {
      output += std::to_string(c == nullptr ? 0 : SSL_CTX_get_timeout(c));
      break;
    }
    case Ssl_acceptor_context_property_type::last:
      [[fallthrough]];
    default:
      output.assign("");
  }
  return output;
}
