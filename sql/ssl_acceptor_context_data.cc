/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
/* Function to report SSL library errors
 */
static void report_errors() {
  unsigned long l;
  const char *file;
  const char *data;
  int line, flags;

  DBUG_TRACE;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  while ((l = ERR_get_error_all(&file, &line, nullptr, &data, &flags))) {
#else          /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  while ((l = ERR_get_error_line_data(&file, &line, &data, &flags)) > 0) {
#endif         /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
#ifndef NDEBUG /* Avoid warning */
    char buf[512];
    LogErr(ERROR_LEVEL, ER_WARN_FAILED_TO_SETUP_TLS, ERR_error_string(l, buf),
           file, line, (flags & ERR_TXT_STRING) ? data : "");
#endif
  }
}

static bool verify_individual_certificate(const char *ssl_cert,
                                          const char *ssl_ca,
                                          const char *ssl_capath,
                                          const char *crl,
                                          const char *crl_path) {
  if (!ssl_cert || my_access(ssl_cert, F_OK) == -1) {
    /* Cert not present */
    return false;
  }
  using raii_bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
  using raii_server_cert = std::unique_ptr<X509, decltype(&X509_free)>;
  using raii_store = std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)>;
  using raii_store_ctx =
      std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)>;
  auto deleter = [&](FILE *ptr) { my_fclose(ptr, MYF(0)); };
  std::unique_ptr<FILE, decltype(deleter)> fp(
      my_fopen(ssl_cert, O_RDONLY | MY_FOPEN_BINARY, MYF(MY_WME)), deleter);

  if (!fp) {
    LogErr(ERROR_LEVEL, ER_WARN_CANT_OPEN_CERTIFICATE, ssl_cert);
    return true;
  }

  raii_bio bio(BIO_new(BIO_s_file()), &BIO_free);
  if (!bio) {
    /* purecov: begin inspected */
    LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
    report_errors();
    return true;
    /* purecov: end */
  }

  BIO_set_fp(bio.get(), fp.get(), BIO_NOCLOSE);
  raii_server_cert server_cert(
      PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), &X509_free);
  if (!server_cert) {
    /* We are not interested in anything other than X509 certificates */
    return false;
  }

  raii_store store(X509_STORE_new(), &X509_STORE_free);
  if (!store) {
    /* purecov: begin inspected */
    LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
    report_errors();
    return true;
    /* purecov: end */
  }

  if (ssl_ca || ssl_capath) {
    if (!X509_STORE_load_locations(store.get(), ssl_ca, ssl_capath)) {
      LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
      report_errors();
      return true;
    }
  }

  if (crl || crl_path) {
    if (X509_STORE_load_locations(store.get(), crl, crl_path) == 0 ||
        X509_STORE_set_flags(store.get(), X509_V_FLAG_CRL_CHECK |
                                              X509_V_FLAG_CRL_CHECK_ALL) == 0) {
      LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
      report_errors();
      return true;
    }
  }

  raii_store_ctx store_ctx(X509_STORE_CTX_new(), &X509_STORE_CTX_free);
  if (!store_ctx) {
    /* purecov: begin inspected */
    LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
    report_errors();
    return true;
    /* purecov: end */
  }

  if (!X509_STORE_CTX_init(store_ctx.get(), store.get(), server_cert.get(),
                           nullptr)) {
    /* purecov: begin inspected */
    LogErr(ERROR_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
    report_errors();
    return true;
    /* purecov: end */
  }

  if (X509_STORE_add_cert(store.get(), server_cert.get()) <= 0) {
    /* purecov: begin inspected */
    LogErr(WARNING_LEVEL, ER_TLS_LIBRARY_ERROR_INTERNAL);
    report_errors();
    return true;
    /* purecov: end */
  }
  if (!X509_verify_cert(store_ctx.get())) {
    const char *result = X509_verify_cert_error_string(
        X509_STORE_CTX_get_error(store_ctx.get()));
    LogErr(WARNING_LEVEL, ER_WARN_CERTIFICATE_ERROR_STRING, ssl_cert, result);
    return true;
  }
  return false;
}

/*
 *   Function to Validate CA Certificate/Certificates.
 */

static bool verify_ca_certificates(const char *ssl_ca, const char *ssl_capath,
                                   const char *ssl_crl,
                                   const char *ssl_crl_path) {
  bool r_value = false;
  if (ssl_ca && ssl_ca[0]) {
    if (verify_individual_certificate(ssl_ca, nullptr, nullptr, ssl_crl,
                                      ssl_crl_path))
      r_value = true;
  }
  if (ssl_capath && ssl_capath[0]) {
    /* We have ssl-capath. So search all files in the dir */
    MY_DIR *ca_dir;
    uint file_count;
    DYNAMIC_STRING file_path;
    char dir_separator[FN_REFLEN];
    size_t dir_path_length;

    init_dynamic_string(&file_path, ssl_capath, FN_REFLEN);
    dir_separator[0] = FN_LIBCHAR;
    dir_separator[1] = 0;
    dynstr_append(&file_path, dir_separator);
    dir_path_length = file_path.length;

    if (!(ca_dir = my_dir(ssl_capath, MY_WANT_STAT | MY_DONT_SORT | MY_WME))) {
      LogErr(ERROR_LEVEL, ER_CANT_ACCESS_CAPATH);
      return true;
    }

    for (file_count = 0; file_count < ca_dir->number_off_files; file_count++) {
      if (!MY_S_ISDIR(ca_dir->dir_entry[file_count].mystat->st_mode)) {
        file_path.length = dir_path_length;
        dynstr_append(&file_path, ca_dir->dir_entry[file_count].name);
        if ((r_value = verify_individual_certificate(
                 file_path.str, nullptr, nullptr, ssl_crl, ssl_crl_path)))
          r_value = true;
      }
    }
    my_dirend(ca_dir);
    dynstr_free(&file_path);

    ca_dir = nullptr;
    memset(&file_path, 0, sizeof(file_path));
  }
  return r_value;
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
    std::string channel, Ssl_init_callback *callbacks,
    bool report_ssl_error /* = true */, enum enum_ssl_init_error *out_error)
    : channel_(channel), ssl_acceptor_fd_(nullptr), acceptor_(nullptr) {
  enum enum_ssl_init_error error_num = SSL_INITERR_NOERROR;
  {
    callbacks->read_parameters(
        &current_ca_, &current_capath_, &current_version_, &current_cert_,
        &current_cipher_, &current_ciphersuites_, &current_key_, &current_crl_,
        &current_crlpath_, &current_tls_session_cache_mode_,
        &current_tls_session_cache_timeout_);
  }

  /* Verify server certificate */
  if (verify_individual_certificate(
          current_cert_.c_str(), current_ca_.c_str(), current_capath_.c_str(),
          current_crl_.c_str(), current_crlpath_.c_str())) {
    LogErr(WARNING_LEVEL, ER_SERVER_CERT_VERIFY_FAILED, current_cert_.c_str());
    /* Verify possible issues in CA certificates*/
    if (verify_ca_certificates(current_ca_.c_str(), current_capath_.c_str(),
                               current_crl_.c_str(),
                               current_crlpath_.c_str())) {
      LogErr(WARNING_LEVEL, ER_WARN_CA_CERT_VERIFY_FAILED);
    }
    error_num = SSL_INITERR_INVALID_CERTIFICATES;
    if (opt_tls_certificates_enforced_validation) {
      if (out_error) *out_error = error_num;
      assert(ssl_acceptor_fd_ == nullptr);
      return;
    }
  }
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
