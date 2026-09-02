/* Copyright (c) 2000, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <openssl/crypto.h>  // for CRYPTO_cleanup_all_ex_data
#include <openssl/err.h>     // for ERR_print_errors_fp, ERR_erro...
#include <openssl/evp.h>
#include <openssl/opensslv.h>  // for OPENSSL_VERSION_NUMBER
#include <openssl/rand.h>      // for RAND_bytes
#include <openssl/ssl.h>       // for SSL_CTX_free, SSL_CTX_set_verify
#include <openssl/x509.h>      // for X509_STORE_load_locations
#include <stdio.h>
#include <string.h>

#include <string>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread.h"  // IWYU pragma: keep my_thread_self
#include "mysql/my_loglevel.h"
#include "mysql/psi/mysql_rwlock.h"  // IWYU pragma: keep PSI_rwlock_key
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysys_err.h"
#include "template_utils.h"
#include "vio/vio_priv.h"

#include <dh_ecdh_config.h>
#include <tls_ciphers.h>

#include "my_openssl_fips.h"
#define TLS_VERSION_OPTION_SIZE 256

static bool ssl_initialized = false;

/* Helper functions */

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
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l, buf),
                         file, line, (flags & ERR_TXT_STRING) ? data : ""));
#endif
  }
}

static const char *ssl_error_string[] = {
    "No error",
    "Unable to get certificate",
    "Unable to get private key",
    "Private key does not match the certificate public key",
    "SSL_CTX_set_default_verify_paths failed",
    "Failed to set ciphers to use",
    "SSL_CTX_new failed",
    "SSL context is not usable without certificate and private key",
    "SSL_CTX_set_tmp_dh failed",
    "TLS version is invalid",
    "FIPS mode invalid",
    "FIPS mode failed",
    "Failed to set ecdh information",
    "Failed to set TLS key exchange groups",
    "force_pqc requires TLSv1.3 with OpenSSL 3.5.0 or newer",
    "Failed to set X509 verification parameter",
    "Invalid certificates",
    "Failed to set TLS signature algorithms",
    "Failed to set TLS session id context"};

const char *sslGetErrString(enum enum_ssl_init_error e) {
  assert(SSL_INITERR_NOERROR < e && e < SSL_INITERR_LASTERR);
  return ssl_error_string[e];
}

#define TLS_SIGALGS_COMMON_LIST                                         \
  "ECDSA+SHA256:ECDSA+SHA384:ECDSA+SHA512:rsa_pss_pss_sha256:rsa_"      \
  "pss_pss_sha384:rsa_pss_pss_sha512:rsa_pss_rsae_sha256:rsa_pss_rsae_" \
  "sha384:rsa_pss_rsae_sha512:RSA+SHA256:RSA+SHA384:RSA+SHA512"
#define TLS_SIGALGS_NON_FIPS_EXTRA ":ed25519:ECDSA+SHA224:RSA+SHA224"
#define TLS_SIGALGS_PQC_EXTRA "ML-DSA-44:ML-DSA-65:ML-DSA-87:"

static const char tls_sigalgs_fips[] = TLS_SIGALGS_COMMON_LIST;
static const char tls_sigalgs_non_fips[] =
    TLS_SIGALGS_COMMON_LIST TLS_SIGALGS_NON_FIPS_EXTRA;
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
static const char tls_sigalgs_non_fips_pqc[] =
    TLS_SIGALGS_PQC_EXTRA TLS_SIGALGS_COMMON_LIST TLS_SIGALGS_NON_FIPS_EXTRA;
#endif
#undef TLS_SIGALGS_PQC_EXTRA
#undef TLS_SIGALGS_NON_FIPS_EXTRA
#undef TLS_SIGALGS_COMMON_LIST

static const char *get_sigalgs_list(bool tls_use_pqc_sign [[maybe_unused]]) {
  if (get_fips_mode()) return tls_sigalgs_fips;

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  if (tls_use_pqc_sign) return tls_sigalgs_non_fips_pqc;
#endif

  return tls_sigalgs_non_fips;
}

static int vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file,
                              const char *key_file,
                              enum enum_ssl_init_error *error) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("ctx: %p  cert_file: %s  key_file: %s", ctx, cert_file,
                       key_file));

  if (!cert_file && key_file) cert_file = key_file;

  if (!key_file && cert_file) key_file = cert_file;

  if (cert_file && SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
    *error = SSL_INITERR_CERT;
    DBUG_PRINT("error",
               ("%s from file '%s'", sslGetErrString(*error), cert_file));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    my_message_local(ERROR_LEVEL, EE_SSL_ERROR_FROM_FILE,
                     sslGetErrString(*error), cert_file);
    return 1;
  }

  if (key_file &&
      SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
    *error = SSL_INITERR_KEY;
    DBUG_PRINT("error",
               ("%s from file '%s'", sslGetErrString(*error), key_file));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    my_message_local(ERROR_LEVEL, EE_SSL_ERROR_FROM_FILE,
                     sslGetErrString(*error), key_file);
    return 1;
  }

  /*
    If we are using DSA, we can copy the parameters from the private key
    Now we know that a key and cert have been set against the SSL context
  */
  if (cert_file && !SSL_CTX_check_private_key(ctx)) {
    *error = SSL_INITERR_NOMATCH;
    DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    my_message_local(ERROR_LEVEL, EE_SSL_ERROR, sslGetErrString(*error));
    return 1;
  }

  return 0;
}

void vio_ssl_end() {
  if (ssl_initialized) {
    fips_deinit();
    ERR_free_strings();

    CRYPTO_cleanup_all_ex_data();

    ssl_initialized = false;
  }
}

void ssl_start() {
  if (!ssl_initialized) {
    ssl_initialized = true;

    fips_init();
    SSL_library_init();
    SSL_load_error_strings();
  }
}

long process_tls_version(const char *tls_version) {
  const char *separator = ",";
  char *token, *lasts = nullptr;

  const char *tls_version_name_list[] = {"TLSv1.2", "TLSv1.3"};
  const char ctx_flag_default[] = "TLSv1.2,TLSv1.3";
  const long tls_ctx_list[] = {SSL_OP_NO_TLSv1_2, SSL_OP_NO_TLSv1_3};
  long tls_ctx_flag = SSL_OP_NO_TLSv1_2 | SSL_OP_NO_TLSv1_3;
  const unsigned int tls_versions_count = array_elements(tls_version_name_list);
  char tls_version_option[TLS_VERSION_OPTION_SIZE] = "";
  int tls_found = 0;

  if (!tls_version ||
      !my_strcasecmp(&my_charset_latin1, tls_version, ctx_flag_default))
    return 0;

  if (strlen(tls_version) + 1 > sizeof(tls_version_option)) return -1;

  snprintf(tls_version_option, sizeof(tls_version_option), "%s", tls_version);
  token = my_strtok_r(tls_version_option, separator, &lasts);
  while (token) {
    for (unsigned int i = 0; i < tls_versions_count; i++) {
      if (!my_strcasecmp(&my_charset_latin1, tls_version_name_list[i], token)) {
        tls_found = 1;
        tls_ctx_flag &= ~tls_ctx_list[i];
        break;
      }
    }
    token = my_strtok_r(nullptr, separator, &lasts);
  }

  if (!tls_found) return -1;
  return tls_ctx_flag;
}

static int get_min_tls_version(long ssl_ctx_flags) {
  return (ssl_ctx_flags & SSL_OP_NO_TLSv1_2) ? TLS1_3_VERSION : TLS1_2_VERSION;
}

static int get_max_tls_version(long ssl_ctx_flags) {
  return (ssl_ctx_flags & SSL_OP_NO_TLSv1_3) ? TLS1_2_VERSION : TLS1_3_VERSION;
}

/************************ VioSSLFd **********************************/
static struct st_VioSSLFd *new_VioSSLFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher,
    const char *ciphersuites [[maybe_unused]], bool is_client,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags, bool tls_force_pqc, bool tls_use_pqc_sign,
    const char *tls_kex, const char *server_host [[maybe_unused]]) {
  struct st_VioSSLFd *ssl_fd;
  long ssl_ctx_options = ssl_ctx_flags & SSL_OP_NO_TICKET;
  std::string tls12_cipher_list, tls13_cipher_list;
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("key_file: '%s'  cert_file: '%s'  ca_file: '%s'  ca_path: '%s'  "
              "cipher: '%s' crl_file: '%s' crl_path: '%s' ssl_ctx_flags: '%ld' "
              "tls_force_pqc: '%d' tls_use_pqc_sign: '%d' tls_kex: '%s' ",
              key_file ? key_file : "NULL", cert_file ? cert_file : "NULL",
              ca_file ? ca_file : "NULL", ca_path ? ca_path : "NULL",
              cipher ? cipher : "NULL", crl_file ? crl_file : "NULL",
              crl_path ? crl_path : "NULL", ssl_ctx_flags, tls_force_pqc,
              tls_use_pqc_sign, tls_kex ? tls_kex : "NULL"));

  if (ssl_ctx_flags < 0) {
    *error = SSL_TLS_VERSION_INVALID;
    DBUG_PRINT("error", ("TLS version invalid : %s", sslGetErrString(*error)));
    report_errors();
    return nullptr;
  }

  if (!is_client) {
    ssl_ctx_options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
  }

  if (!(ssl_fd = ((struct st_VioSSLFd *)my_malloc(
            key_memory_vio_ssl_fd, sizeof(struct st_VioSSLFd), MYF(0)))))
    return nullptr;

  ssl_fd->tls_force_pqc = false;
  ssl_fd->tls_session_cache_pqc_only = false;
  ssl_fd->tls_use_pqc_sign = false;
  ssl_fd->tls_kex = nullptr;

  if (!(ssl_fd->ssl_context = SSL_CTX_new(is_client ? TLS_client_method()
                                                    : TLS_server_method()))) {
    *error = SSL_INITERR_MEMFAIL;
    DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
    report_errors();
    my_free(ssl_fd);
    return nullptr;
  }

  const int min_tls_version = get_min_tls_version(ssl_ctx_flags);
  const int max_tls_version = get_max_tls_version(ssl_ctx_flags);
  if (SSL_CTX_set_min_proto_version(ssl_fd->ssl_context, min_tls_version) !=
          1 ||
      SSL_CTX_set_max_proto_version(ssl_fd->ssl_context, max_tls_version) !=
          1) {
    *error = SSL_TLS_VERSION_INVALID;
    goto error;
  }

  if (tls_kex != nullptr) {
    ssl_fd->tls_kex = my_strdup(key_memory_vio_ssl_fd, tls_kex, MYF(0));
    if (ssl_fd->tls_kex == nullptr) {
      *error = SSL_INITERR_MEMFAIL;
      DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
      report_errors();
      SSL_CTX_free(ssl_fd->ssl_context);
      my_free(ssl_fd);
      return nullptr;
    }
  }

  /*
    Set OpenSSL TLS v1.3 ciphersuites.
    Note that an empty list is permissible.
  */
  if (nullptr != ciphersuites)
    tls13_cipher_list = ciphersuites;
  else
    tls13_cipher_list = default_tls13_ciphers;

  /*
    Note: if TLSv1.3 is enabled but TLSv1.3 ciphersuite list is empty
    (that's permissible and mentioned in the documentation),
    the connection will fail with "no ciphers available" error.
  */
  if (0 == SSL_CTX_set_ciphersuites(ssl_fd->ssl_context,
                                    tls13_cipher_list.c_str())) {
    *error = SSL_INITERR_CIPHERS;
    goto error;
  }

  {
    /*
      Set supported signature algorithms for OpenSSL TLS v1.3
      with preference towards more performant ones (ECDSA).
      If tls_use_pqc_sign is enabled and OpenSSL >= 3.5 outside FIPS mode,
      also advertise PQC-capable signature algorithms before the classical
      fallback list. FIPS mode uses only provider-accepted classical
      algorithms.
    */
    const char *sig_algs = get_sigalgs_list(tls_use_pqc_sign);
    if (0 == SSL_CTX_set1_sigalgs_list(ssl_fd->ssl_context,
                                       const_cast<char *>(sig_algs))) {
      *error = SSL_INITERR_SIGALGS;
      goto error;
    }
  }

  /*
    We explicitly prohibit weak ciphers.
    NOTE: SSL_CTX_set_cipher_list will return 0 if
    none of the provided ciphers could be selected
  */
  tls12_cipher_list += blocked_tls12_ciphers;
  tls12_cipher_list += ":";

  /*
    If ciphers are specified explicitly by caller, use them.
    Otherwise, fallback to the default list.
  */
  if (cipher == nullptr) {
    tls12_cipher_list.append(default_tls12_ciphers);
    if (is_client) {
      tls12_cipher_list.append(":");
      tls12_cipher_list.append(additional_client_ciphers);
    }
  } else
    tls12_cipher_list.append(cipher);

  if (0 ==
      SSL_CTX_set_cipher_list(ssl_fd->ssl_context, tls12_cipher_list.c_str())) {
    *error = SSL_INITERR_CIPHERS;
    goto error;
  }

  /* Load certs from the trusted ca */
  if (SSL_CTX_load_verify_locations(ssl_fd->ssl_context, ca_file, ca_path) <=
      0) {
    DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
    if (ca_file || ca_path) {
      /* fail only if ca file or ca path were supplied and looking into
         them fails. */
      DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
      *error = SSL_INITERR_BAD_PATHS;
      goto error;
    }

    /* otherwise go use the defaults */
    if (SSL_CTX_set_default_verify_paths(ssl_fd->ssl_context) == 0) {
      *error = SSL_INITERR_BAD_PATHS;
      goto error;
    }
  }

  if (crl_file || crl_path) {
    X509_STORE *store = SSL_CTX_get_cert_store(ssl_fd->ssl_context);
    /* Load crls from the trusted ca */
    if (X509_STORE_load_locations(store, crl_file, crl_path) == 0 ||
        X509_STORE_set_flags(
            store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) == 0) {
      DBUG_PRINT("warning", ("X509_STORE_load_locations for CRL failed"));
      *error = SSL_INITERR_BAD_PATHS;
      goto error;
    }
  }

  if (vio_set_cert_stuff(ssl_fd->ssl_context, cert_file, key_file, error)) {
    DBUG_PRINT("warning", ("vio_set_cert_stuff failed"));
    goto error;
  }

  /* Server specific check : Must have certificate and key file */
  if (!is_client && !key_file && !cert_file) {
    *error = SSL_INITERR_NO_USABLE_CTX;
    goto error;
  }

  /* DH stuff */
  if (!is_client && set_dh(ssl_fd->ssl_context)) {
    printf("%s\n", ERR_error_string(ERR_get_error(), nullptr));
    *error = SSL_INITERR_DHFAIL;
    goto error;
  }

  /* ECDH / TLS key exchange group setup */
  if (set_ecdh(ssl_fd->ssl_context, tls_force_pqc, tls_kex)) {
    *error = (tls_kex != nullptr && tls_kex[0] != '\0')
                 ? SSL_INITERR_KEX_GROUPS
                 : (tls_force_pqc ? SSL_INITERR_PQC_UNSUPPORTED
                                  : SSL_INITERR_ECDHFAIL);
    goto error;
  }

  /*
    If server_host parameter is set it contains either IP address or server's
    hostname. Pass it to the lib to perform automatic checks.
  */
  if (server_host) {
    X509_VERIFY_PARAM *param = SSL_CTX_get0_param(ssl_fd->ssl_context);
    assert(is_client);
    /*
      As we don't know if the server_host contains IP addr or hostname
      call X509_VERIFY_PARAM_set1_ip_asc() first and if it returns an error
      (not valid IP address), call X509_VERIFY_PARAM_set1_host().
    */
    if (1 != X509_VERIFY_PARAM_set1_ip_asc(param, server_host)) {
      if (1 != X509_VERIFY_PARAM_set1_host(param, server_host, 0)) {
        *error = SSL_INITERR_X509_VERIFY_PARAM;
        goto error;
      }
    }
  }

  SSL_CTX_set_options(ssl_fd->ssl_context, ssl_ctx_options);

  DBUG_PRINT("exit", ("OK 1"));

  return ssl_fd;

error:
  DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
  report_errors();
  SSL_CTX_free(ssl_fd->ssl_context);
  my_free(ssl_fd->tls_kex);
  my_free(ssl_fd);
  return nullptr;
}

/************************ VioSSLConnectorFd
 * **********************************/
struct st_VioSSLFd *new_VioSSLConnectorFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher, const char *ciphersuites,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags, bool tls_force_pqc, bool tls_use_pqc_sign,
    const char *tls_kex, const char *server_host) {
  struct st_VioSSLFd *ssl_fd;
  int verify = SSL_VERIFY_PEER;

  /*
    Turn off verification of servers certificate if both
    ca_file and ca_path is set to nullptr
  */
  if (ca_file == nullptr && ca_path == nullptr) verify = SSL_VERIFY_NONE;

  if (!(ssl_fd = new_VioSSLFd(key_file, cert_file, ca_file, ca_path, cipher,
                              ciphersuites, true, error, crl_file, crl_path,
                              ssl_ctx_flags, tls_force_pqc, tls_use_pqc_sign,
                              tls_kex, server_host))) {
    return nullptr;
  }
  ssl_fd->tls_force_pqc = tls_force_pqc;
  ssl_fd->tls_session_cache_pqc_only = false;
  ssl_fd->tls_use_pqc_sign = tls_use_pqc_sign;

  /* Init the VioSSLFd as a "connector" ie. the client side */

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, nullptr);

  return ssl_fd;
}

/************************ VioSSLAcceptorFd **********************************/
struct st_VioSSLFd *new_VioSSLAcceptorFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher, const char *ciphersuites,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags, bool tls_force_pqc, bool tls_use_pqc_sign,
    const char *tls_kex) {
  struct st_VioSSLFd *ssl_fd;
  int const verify = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  if (!(ssl_fd = new_VioSSLFd(key_file, cert_file, ca_file, ca_path, cipher,
                              ciphersuites, false, error, crl_file, crl_path,
                              ssl_ctx_flags, tls_force_pqc, tls_use_pqc_sign,
                              tls_kex, nullptr))) {
    return nullptr;
  }
  ssl_fd->tls_force_pqc = tls_force_pqc;
  /*
    new_VioSSLAcceptorFd() always creates a fresh SSL_CTX. Combined with the
    random session id context below and OpenSSL's per-SSL_CTX ticket keys, this
    means a force_pqc acceptor can resume only sessions created by this
    PQC-only context.
  */
  ssl_fd->tls_session_cache_pqc_only = tls_force_pqc;
  ssl_fd->tls_use_pqc_sign = tls_use_pqc_sign;

  /* Init the the VioSSLFd as a "acceptor" ie. the server side */

  /* Set max number of cached sessions, returns the previous size */
  SSL_CTX_sess_set_cache_size(ssl_fd->ssl_context, 128);

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, nullptr);

  {
    unsigned char session_id_context[SSL_MAX_SSL_SESSION_ID_LENGTH];
    if (RAND_bytes(session_id_context, sizeof(session_id_context)) != 1 ||
        SSL_CTX_set_session_id_context(ssl_fd->ssl_context, session_id_context,
                                       sizeof(session_id_context)) != 1) {
      *error = SSL_INITERR_SESSION_ID_CONTEXT;
      SSL_CTX_free(ssl_fd->ssl_context);
      my_free(ssl_fd->tls_kex);
      my_free(ssl_fd);
      return nullptr;
    }
  }

  return ssl_fd;
}

void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd) {
  SSL_CTX_free(fd->ssl_context);
  my_free(fd->tls_kex);
  my_free(fd);
}
