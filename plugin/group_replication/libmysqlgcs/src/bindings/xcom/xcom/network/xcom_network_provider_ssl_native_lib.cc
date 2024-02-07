/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "xcom/network/xcom_network_provider_native_lib.h"

#include "xcom/network/include/network_provider.h"
#include "xcom/network/network_provider_manager.h"

#ifdef WIN32
// In OpenSSL before 1.1.0, we need this first.
#include <winsock2.h>
#endif  // WIN32

#ifndef _WIN32
#include <poll.h>
#endif

#ifndef XCOM_WITHOUT_OPENSSL
#include <assert.h>
#include <stdlib.h>

#include <openssl/dh.h>
#include <openssl/opensslv.h>
#include <openssl/x509v3.h>

#include <my_openssl_fips.h>

#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif

#include "openssl/engine.h"

#include "xcom/retry.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"

#define TLS_VERSION_OPTION_SIZE 256
#define SSL_CIPHER_LIST_SIZE 4096

static const char *tls_ciphers_list =
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-ECDSA-AES256-GCM-SHA384:"
    "ECDHE-RSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES256-GCM-SHA384:"
    "ECDHE-ECDSA-AES128-SHA256:"
    "ECDHE-RSA-AES128-SHA256:"
    "ECDHE-ECDSA-AES256-SHA384:"
    "ECDHE-RSA-AES256-SHA384:"
    "DHE-RSA-AES128-GCM-SHA256:"
    "DHE-DSS-AES128-GCM-SHA256:"
    "DHE-RSA-AES128-SHA256:"
    "DHE-DSS-AES128-SHA256:"
    "DHE-DSS-AES256-GCM-SHA384:"
    "DHE-RSA-AES256-SHA256:"
    "DHE-DSS-AES256-SHA256:"
    "ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:"
    "ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:"
    "DHE-DSS-AES128-SHA:DHE-RSA-AES128-SHA:"
    "TLS_DHE_DSS_WITH_AES_256_CBC_SHA:DHE-RSA-AES256-SHA:"
    "AES128-GCM-SHA256:DH-DSS-AES128-GCM-SHA256:"
    "ECDH-ECDSA-AES128-GCM-SHA256:AES256-GCM-SHA384:"
    "DH-DSS-AES256-GCM-SHA384:ECDH-ECDSA-AES256-GCM-SHA384:"
    "AES128-SHA256:DH-DSS-AES128-SHA256:ECDH-ECDSA-AES128-SHA256:AES256-SHA256:"
    "DH-DSS-AES256-SHA256:ECDH-ECDSA-AES256-SHA384:AES128-SHA:"
    "DH-DSS-AES128-SHA:ECDH-ECDSA-AES128-SHA:AES256-SHA:"
    "DH-DSS-AES256-SHA:ECDH-ECDSA-AES256-SHA:DHE-RSA-AES256-GCM-SHA384:"
    "DH-RSA-AES128-GCM-SHA256:ECDH-RSA-AES128-GCM-SHA256:DH-RSA-AES256-GCM-"
    "SHA384:"
    "ECDH-RSA-AES256-GCM-SHA384:DH-RSA-AES128-SHA256:"
    "ECDH-RSA-AES128-SHA256:DH-RSA-AES256-SHA256:"
    "ECDH-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:"
    "ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA:"
    "ECDHE-ECDSA-AES256-SHA:DHE-DSS-AES128-SHA:DHE-RSA-AES128-SHA:"
    "TLS_DHE_DSS_WITH_AES_256_CBC_SHA:DHE-RSA-AES256-SHA:"
    "AES128-SHA:DH-DSS-AES128-SHA:ECDH-ECDSA-AES128-SHA:AES256-SHA:"
    "DH-DSS-AES256-SHA:ECDH-ECDSA-AES256-SHA:DH-RSA-AES128-SHA:"
    "ECDH-RSA-AES128-SHA:DH-RSA-AES256-SHA:ECDH-RSA-AES256-SHA:DES-CBC3-SHA";
static const char *tls_cipher_blocked =
    "!aNULL:!eNULL:!EXPORT:!LOW:!MD5:!DES:!RC2:!RC4:!PSK:"
    "!DHE-DSS-DES-CBC3-SHA:!DHE-RSA-DES-CBC3-SHA:"
    "!ECDH-RSA-DES-CBC3-SHA:!ECDH-ECDSA-DES-CBC3-SHA:"
    "!ECDHE-RSA-DES-CBC3-SHA:!ECDHE-ECDSA-DES-CBC3-SHA:";

#if OPENSSL_VERSION_NUMBER < 0x30000000L
/*
  Diffie-Hellman key.
  Generated using: >openssl dhparam -5 -C 2048

  -----BEGIN DH PARAMETERS-----
  MIIBCAKCAQEAil36wGZ2TmH6ysA3V1xtP4MKofXx5n88xq/aiybmGnReZMviCPEJ
  46+7VCktl/RZ5iaDH1XNG1dVQmznt9pu2G3usU+k1/VB4bQL4ZgW4u0Wzxh9PyXD
  glm99I9Xyj4Z5PVE4MyAsxCRGA1kWQpD9/zKAegUBPLNqSo886Uqg9hmn8ksyU9E
  BV5eAEciCuawh6V0O+Sj/C3cSfLhgA0GcXp3OqlmcDu6jS5gWjn3LdP1U0duVxMB
  h/neTSCSvtce4CAMYMjKNVh9P1nu+2d9ZH2Od2xhRIqMTfAS1KTqF3VmSWzPFCjG
  mjxx/bg6bOOjpgZapvB6ABWlWmRmAAWFtwIBBQ==
  -----END DH PARAMETERS-----
 */
static unsigned char dh2048_p[] = {
    0x8A, 0x5D, 0xFA, 0xC0, 0x66, 0x76, 0x4E, 0x61, 0xFA, 0xCA, 0xC0, 0x37,
    0x57, 0x5C, 0x6D, 0x3F, 0x83, 0x0A, 0xA1, 0xF5, 0xF1, 0xE6, 0x7F, 0x3C,
    0xC6, 0xAF, 0xDA, 0x8B, 0x26, 0xE6, 0x1A, 0x74, 0x5E, 0x64, 0xCB, 0xE2,
    0x08, 0xF1, 0x09, 0xE3, 0xAF, 0xBB, 0x54, 0x29, 0x2D, 0x97, 0xF4, 0x59,
    0xE6, 0x26, 0x83, 0x1F, 0x55, 0xCD, 0x1B, 0x57, 0x55, 0x42, 0x6C, 0xE7,
    0xB7, 0xDA, 0x6E, 0xD8, 0x6D, 0xEE, 0xB1, 0x4F, 0xA4, 0xD7, 0xF5, 0x41,
    0xE1, 0xB4, 0x0B, 0xE1, 0x98, 0x16, 0xE2, 0xED, 0x16, 0xCF, 0x18, 0x7D,
    0x3F, 0x25, 0xC3, 0x82, 0x59, 0xBD, 0xF4, 0x8F, 0x57, 0xCA, 0x3E, 0x19,
    0xE4, 0xF5, 0x44, 0xE0, 0xCC, 0x80, 0xB3, 0x10, 0x91, 0x18, 0x0D, 0x64,
    0x59, 0x0A, 0x43, 0xF7, 0xFC, 0xCA, 0x01, 0xE8, 0x14, 0x04, 0xF2, 0xCD,
    0xA9, 0x2A, 0x3C, 0xF3, 0xA5, 0x2A, 0x83, 0xD8, 0x66, 0x9F, 0xC9, 0x2C,
    0xC9, 0x4F, 0x44, 0x05, 0x5E, 0x5E, 0x00, 0x47, 0x22, 0x0A, 0xE6, 0xB0,
    0x87, 0xA5, 0x74, 0x3B, 0xE4, 0xA3, 0xFC, 0x2D, 0xDC, 0x49, 0xF2, 0xE1,
    0x80, 0x0D, 0x06, 0x71, 0x7A, 0x77, 0x3A, 0xA9, 0x66, 0x70, 0x3B, 0xBA,
    0x8D, 0x2E, 0x60, 0x5A, 0x39, 0xF7, 0x2D, 0xD3, 0xF5, 0x53, 0x47, 0x6E,
    0x57, 0x13, 0x01, 0x87, 0xF9, 0xDE, 0x4D, 0x20, 0x92, 0xBE, 0xD7, 0x1E,
    0xE0, 0x20, 0x0C, 0x60, 0xC8, 0xCA, 0x35, 0x58, 0x7D, 0x3F, 0x59, 0xEE,
    0xFB, 0x67, 0x7D, 0x64, 0x7D, 0x8E, 0x77, 0x6C, 0x61, 0x44, 0x8A, 0x8C,
    0x4D, 0xF0, 0x12, 0xD4, 0xA4, 0xEA, 0x17, 0x75, 0x66, 0x49, 0x6C, 0xCF,
    0x14, 0x28, 0xC6, 0x9A, 0x3C, 0x71, 0xFD, 0xB8, 0x3A, 0x6C, 0xE3, 0xA3,
    0xA6, 0x06, 0x5A, 0xA6, 0xF0, 0x7A, 0x00, 0x15, 0xA5, 0x5A, 0x64, 0x66,
    0x00, 0x05, 0x85, 0xB7};

static unsigned char dh2048_g[] = {0x05};

static DH *get_dh2048(void) {
  DH *dh;
  if ((dh = DH_new())) {
    BIGNUM *p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), nullptr);
    BIGNUM *g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), nullptr);
    if (!p || !g
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        || !DH_set0_pqg(dh, p, nullptr, g)
#endif /* OPENSSL_VERSION_NUMBER >= 0x10100000L */
    ) {
      /* DH_free() will free 'p' and 'g' at once. */
      DH_free(dh);
      return nullptr;
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    dh->p = p;
    dh->g = g;
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  }
  return (dh);
}
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

static char *ssl_pw = nullptr;
static int ssl_init_done = 0;

SSL_CTX *server_ctx = nullptr;
SSL_CTX *client_ctx = nullptr;

/*
  Note that the functions, i.e. strtok and strcasecmp, are not
  considering multiple-byte characters as the original server
  code does.
*/
static long process_tls_version(const char *tls_version) {
  const char *separator = ", ";
  char *token = nullptr;
#ifdef HAVE_TLSv13
  const char *tls_version_name_list[] = {"TLSv1.2", "TLSv1.3"};
#else
  const char *tls_version_name_list[] = {"TLSv1.2"};
#endif /* HAVE_TLSv13 */
#define TLS_VERSIONS_COUNTS \
  (sizeof(tls_version_name_list) / sizeof(*tls_version_name_list))
  unsigned int tls_versions_count = TLS_VERSIONS_COUNTS;
#ifdef HAVE_TLSv13
  const long tls_ctx_list[TLS_VERSIONS_COUNTS] = {SSL_OP_NO_TLSv1_2,
                                                  SSL_OP_NO_TLSv1_3};
  const char *ctx_flag_default = "TLSv1.2,TLSv1.3";
  long tls_ctx_flag = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 |
                      SSL_OP_NO_TLSv1_3 | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
#else
  const long tls_ctx_list[TLS_VERSIONS_COUNTS] = {SSL_OP_NO_TLSv1_2};
  const char *ctx_flag_default = "TLSv1.2";
  long tls_ctx_flag = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 |
                      SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
#endif /* HAVE_TLSv13 */
  unsigned int index = 0;
  char tls_version_option[TLS_VERSION_OPTION_SIZE] = "";
  int tls_found = 0;
  char *saved_ctx = nullptr;

  if (!tls_version || !xcom_strcasecmp(tls_version, ctx_flag_default)) return 0;

  if (strlen(tls_version) + 1 > sizeof(tls_version_option)) return -1;

  snprintf(tls_version_option, sizeof(tls_version_option), "%s", tls_version);
  token = xcom_strtok(tls_version_option, separator, &saved_ctx);
  while (token) {
    for (index = 0; index < tls_versions_count; index++) {
      if (!xcom_strcasecmp(tls_version_name_list[index], token)) {
        tls_found = 1;
        tls_ctx_flag = tls_ctx_flag & (~tls_ctx_list[index]);
        break;
      }
    }
    token = xcom_strtok(nullptr, separator, &saved_ctx);
  }

  if (!tls_found)
    return -1;
  else
    return tls_ctx_flag;
}

/* purecov: begin deadcode */
static int PasswordCallBack(char *passwd, int sz, int rw [[maybe_unused]],
                            void *userdata [[maybe_unused]]) {
  const char *pw = ssl_pw ? ssl_pw : "yassl123";
  strncpy(passwd, pw, (size_t)sz);
  return (int)strlen(pw);
}
/* purecov: end */

static int configure_ssl_algorithms(SSL_CTX *ssl_ctx, const char *cipher,
                                    const char *tls_version,
                                    const char *tls_ciphersuites
                                    [[maybe_unused]]) {
  long ssl_ctx_options =
      SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
  char cipher_list[SSL_CIPHER_LIST_SIZE] = {0};
  long ssl_ctx_flags = -1;
#ifdef HAVE_TLSv13
  int tlsv1_3_enabled = 0;
#endif /* HAVE_TLSv13 */

  SSL_CTX_set_default_passwd_cb(ssl_ctx, PasswordCallBack);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

  ssl_ctx_flags = process_tls_version(tls_version);
  if (ssl_ctx_flags < 0) {
    G_ERROR("TLS version is invalid: %s", tls_version);
    return 1;
  }

#ifdef HAVE_TLSv13
  ssl_ctx_options = (ssl_ctx_options | ssl_ctx_flags) &
                    (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                     SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 | SSL_OP_NO_TLSv1_3);
#else
  ssl_ctx_options = (ssl_ctx_options | ssl_ctx_flags) &
                    (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                     SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2);
#endif /* HAVE_TLSv13 */

  SSL_CTX_set_options(ssl_ctx, ssl_ctx_options);

#ifdef HAVE_TLSv13
  tlsv1_3_enabled = ((ssl_ctx_options & SSL_OP_NO_TLSv1_3) == 0);
  if (tlsv1_3_enabled) {
    /* Set OpenSSL TLS v1.3 ciphersuites.
       If the ciphersuites are unspecified, i.e. tls_ciphersuites == NULL, then
       we use whatever OpenSSL uses by default. Note that an empty list is
       permissible; it disallows all ciphersuites. */
    if (tls_ciphersuites != nullptr) {
      /*
        Note: if TLSv1.3 is enabled but TLSv1.3 ciphersuite list is empty
        (that's permissible and mentioned in the documentation),
        the connection will fail with "no ciphers available" error.
      */
      if (SSL_CTX_set_ciphersuites(ssl_ctx, tls_ciphersuites) == 0) {
        G_ERROR(
            "Failed to set the list of ciphersuites. Check if the values "
            "configured for ciphersuites are correct and valid and if the list "
            "is not empty");
        return 1;
      }
    }
  } else {
    /* Disable OpenSSL TLS v1.3 ciphersuites. */
    if (SSL_CTX_set_ciphersuites(ssl_ctx, "") == 0) {
      G_DEBUG("Failed to set empty ciphersuites with TLS v1.3 disabled.");
      return 1;
    }
  }
#endif /* HAVE_TLSv13 */

  /*
    Set the ciphers that can be used. Note, however, that the
    SSL_CTX_set_cipher_list will return 0 if none of the provided
    ciphers could be selected
  */
  strncat(cipher_list, tls_cipher_blocked, SSL_CIPHER_LIST_SIZE - 1);
  if (cipher && strlen(cipher) != 0)
    strncat(cipher_list, cipher, SSL_CIPHER_LIST_SIZE - 1);
  else
    strncat(cipher_list, tls_ciphers_list, SSL_CIPHER_LIST_SIZE - 1);

  if (SSL_CTX_set_cipher_list(ssl_ctx, cipher_list) == 0) {
    G_ERROR("Failed to set the list of chipers.");
    return 1;
  }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (SSL_CTX_set_dh_auto(ssl_ctx, 1) != 1) return true;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  DH *dh = get_dh2048();
  if (SSL_CTX_set_tmp_dh(ssl_ctx, dh) == 0) {
    G_ERROR("Error setting up Diffie-Hellman key exchange");
    DH_free(dh);
    return 1;
  }
  DH_free(dh);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

  return 0;
}

/**
  @retval true     for error
  @retval false    for success
*/
static bool configure_ssl_fips_mode(const int fips_mode) {
  bool rc = false;
  char err_string[OPENSSL_ERROR_LENGTH] = {'\0'};
  if (set_fips_mode(fips_mode, err_string)) {
    G_ERROR("openssl fips mode set failed: %s", err_string);
    rc = true;
  }
  return rc;
}

static int configure_ssl_ca(SSL_CTX *ssl_ctx, const char *ca_file,
                            const char *ca_path) {
  /* Load certs from the trusted ca. */
  if (SSL_CTX_load_verify_locations(ssl_ctx, ca_file, ca_path) == 0) {
    std::string out_ca_file(ca_file ? ca_file : "NULL");
    std::string out_ca_path(ca_path ? ca_path : "NULL");
    G_WARNING("Failed to locate and verify ca_file: %s ca_path: %s",
              out_ca_file.c_str(), out_ca_path.c_str());
    if (ca_file || ca_path) {
      G_ERROR(
          "Cannot use default locations because ca_file or "
          "ca_path has been specified");
      goto error;
    }

    /* Otherwise go use the defaults. */
    if (SSL_CTX_set_default_verify_paths(ssl_ctx) == 0) {
      G_ERROR("Failed to use defaults for ca_file and ca_path");
      goto error;
    }
  }

  return 0;

error:
  return 1;
}

static int configure_ssl_revocation(SSL_CTX *ssl_ctx [[maybe_unused]],
                                    const char *crl_file,
                                    const char *crl_path) {
  int retval = 0;
  if (crl_file || crl_path) {
    X509_STORE *store = SSL_CTX_get_cert_store(ssl_ctx);
    /* Load crls from the trusted ca */
    if (X509_STORE_load_locations(store, crl_file, crl_path) == 0 ||
        X509_STORE_set_flags(
            store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) == 0) {
      G_ERROR("X509_STORE_load_locations for CRL error");
      retval = 1;
    }
  }
  return retval;
}

static int configure_ssl_keys(SSL_CTX *ssl_ctx, const char *key_file,
                              const char *cert_file) {
  if (!cert_file && !key_file) {
    G_ERROR("Both the certification file and the key file cannot be None");
    goto error;
  }

  if (!cert_file && key_file) {
    G_WARNING("Using the key file also as a certification file: %s", key_file);
    cert_file = key_file;
  }

  if (!key_file && cert_file) {
    G_WARNING("Using the certification file also as a key file: %s", cert_file);
    key_file = cert_file;
  }

  if (cert_file &&
      SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) <= 0) {
    G_ERROR("Error loading certification file %s", cert_file);
    goto error;
  }

  if (key_file &&
      SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
    G_ERROR("Error loading key file %s", key_file);
    goto error;
  }

  /*
    If we are using DSA, we can copy the parameters from the private key
    Now we know that a key and cert have been set against the SSL context
  */
  if (cert_file && !SSL_CTX_check_private_key(ssl_ctx)) {
    G_ERROR("Private key is not properly loaded");
    goto error;
  }

  return 0;

error:
  return 1;
}

/* Initialize SSL. Return 1 f success, 0 if error */
static int init_ssl(const char *key_file, const char *cert_file,
                    const char *ca_file, const char *ca_path,
                    const char *crl_file, const char *crl_path,
                    const char *cipher, const char *tls_version,
                    const char *tls_ciphersuites, SSL_CTX *ssl_ctx) {
  G_DEBUG(
      "Initializing SSL with key_file: '%s'  cert_file: '%s'  "
      "ca_file: '%s'  ca_path: '%s'",
      key_file ? key_file : "NULL", cert_file ? cert_file : "NULL",
      ca_file ? ca_file : "NULL", ca_path ? ca_path : "NULL");

  G_DEBUG(
      "Additional SSL configuration is "
      "cipher: '%s' crl_file: '%s' crl_path: '%s'",
      cipher ? cipher : "NULL", crl_file ? crl_file : "NULL",
      crl_path ? crl_path : "NULL");

  G_DEBUG("TLS configuration is version: '%s', ciphersuites: '%s'",
          tls_version ? tls_version : "NULL",
          tls_ciphersuites ? tls_ciphersuites : "NULL");

  if (configure_ssl_algorithms(ssl_ctx, cipher, tls_version, tls_ciphersuites))
    goto error;

  if (configure_ssl_ca(ssl_ctx, ca_file, ca_path)) goto error;

  if (configure_ssl_revocation(ssl_ctx, crl_file, crl_path)) goto error;

  if (configure_ssl_keys(ssl_ctx, key_file, cert_file)) goto error;

  G_DEBUG("Success initializing SSL");

  return 0;

error:
  G_MESSAGE("Error initializing SSL");
  return 1;
}

int Xcom_network_provider_ssl_library::xcom_init_ssl(
    const char *server_key_file, const char *server_cert_file,
    const char *client_key_file, const char *client_cert_file,
    const char *ca_file, const char *ca_path, const char *crl_file,
    const char *crl_path, const char *cipher, const char *tls_version,
    const char *tls_ciphersuites) {
  int verify_server = SSL_VERIFY_NONE;
  int verify_client = SSL_VERIFY_NONE;

  if (configure_ssl_fips_mode(
          Network_provider_manager::getInstance().xcom_get_ssl_fips_mode())) {
    G_ERROR("Error setting the ssl fips mode");
    goto error;
  }

  SSL_library_init();
  SSL_load_error_strings();

  if (!Network_provider_manager::getInstance().is_xcom_using_ssl()) {
    G_WARNING("SSL is not enabled");
    return !ssl_init_done;
  }

  if (ssl_init_done) {
    G_DEBUG("SSL already initialized");
    return !ssl_init_done;
  }

  G_DEBUG("Configuring SSL for the server")
#ifdef HAVE_TLSv13
  server_ctx = SSL_CTX_new(TLS_server_method());
#else
  server_ctx = SSL_CTX_new(SSLv23_server_method());
#endif /* HAVE_TLSv13 */

  if (!server_ctx) {
    G_ERROR("Error allocating SSL Context object for the server");
    goto error;
  }
  if (init_ssl(server_key_file, server_cert_file, ca_file, ca_path, crl_file,
               crl_path, cipher, tls_version, tls_ciphersuites, server_ctx))
    goto error;

  if (Network_provider_manager::getInstance().xcom_get_ssl_mode() !=
      SSL_REQUIRED)
    verify_server = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  SSL_CTX_set_verify(server_ctx, verify_server, nullptr);

  G_DEBUG("Configuring SSL for the client")
#ifdef HAVE_TLSv13
  client_ctx = SSL_CTX_new(TLS_client_method());
#else
  client_ctx = SSL_CTX_new(SSLv23_client_method());
#endif /* HAVE_TLSv13 */
  if (!client_ctx) {
    G_ERROR("Error allocating SSL Context object for the client");
    goto error;
  }
  if (init_ssl(client_key_file, client_cert_file, ca_file, ca_path, crl_file,
               crl_path, cipher, tls_version, tls_ciphersuites, client_ctx))
    goto error;

  if (Network_provider_manager::getInstance().xcom_get_ssl_mode() !=
      SSL_REQUIRED) {
    verify_client = SSL_VERIFY_PEER;
  }
  SSL_CTX_set_verify(client_ctx, verify_client, nullptr);

  ssl_init_done = 1;

  return !ssl_init_done;

error:
  xcom_destroy_ssl();

  return !ssl_init_done;
}

void Xcom_network_provider_ssl_library::xcom_cleanup_ssl() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(nullptr);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
}

void Xcom_network_provider_ssl_library::xcom_destroy_ssl() {
  G_DEBUG("Destroying SSL");

  ssl_init_done = 0;

  if (server_ctx) {
    SSL_CTX_free(server_ctx);
    server_ctx = nullptr;
  }

  if (client_ctx) {
    SSL_CTX_free(client_ctx);
    client_ctx = nullptr;
  }

#if defined(WITH_SSL_STANDALONE)
  ENGINE_cleanup();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
#endif

  Xcom_network_provider_ssl_library::xcom_cleanup_ssl();

  G_DEBUG("Success destroying SSL");
}

int Xcom_network_provider_ssl_library::ssl_verify_server_cert(
    SSL *ssl, const char *server_hostname) {
  X509 *server_cert = nullptr;
  int ret_validation = 1;

#if !(OPENSSL_VERSION_NUMBER >= 0x10002000L || defined(HAVE_WOLFSSL))
  int cn_loc = -1;
  char *cn = NULL;
  ASN1_STRING *cn_asn1 = NULL;
  X509_NAME_ENTRY *cn_entry = NULL;
  X509_NAME *subject = NULL;
#endif

  G_DEBUG("Verifying server certificate and expected host name: %s",
          server_hostname);

  if (Network_provider_manager::getInstance().xcom_get_ssl_mode() !=
      SSL_VERIFY_IDENTITY)
    return 0;

  if (!server_hostname) {
    G_ERROR("No server hostname supplied to verify server certificate");
    goto error;
  }

  if (!(server_cert = SSL_get_peer_certificate(ssl))) {
    G_ERROR("Could not get server certificate to be verified");
    goto error;
  }

  if (X509_V_OK != SSL_get_verify_result(ssl)) {
    G_ERROR("Failed to verify the server certificate");
    goto error;
  }
  /*
    We already know that the certificate exchanged was valid; the SSL library
    handled that. Now we need to verify that the contents of the certificate
    are what we expect.
  */

  /* Use OpenSSL certificate matching functions instead of our own if we
     have OpenSSL. The X509_check_* functions return 1 on success.
  */
#if OPENSSL_VERSION_NUMBER >= 0x10002000L || defined(HAVE_WOLFSSL)
  if ((X509_check_host(server_cert, server_hostname, strlen(server_hostname), 0,
                       nullptr) != 1) &&
      (X509_check_ip_asc(server_cert, server_hostname, 0) != 1)) {
    G_ERROR(
        "Failed to verify the server certificate via X509 certificate "
        "matching functions");
    goto error;

  } else {
    /* Success */
    ret_validation = 0;
  }
#else  /* OPENSSL_VERSION_NUMBER < 0x10002000L */
  /*
     OpenSSL prior to 1.0.2 do not support X509_check_host() function.
     Use deprecated X509_get_subject_name() instead.
  */
  subject = X509_get_subject_name((X509 *)server_cert);
  /* Find the CN location in the subject */
  cn_loc = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
  if (cn_loc < 0) {
    G_ERROR("Failed to get CN location in the server certificate subject");
    goto error;
  }

  /* Get the CN entry for given location */
  cn_entry = X509_NAME_get_entry(subject, cn_loc);
  if (cn_entry == NULL) {
    G_ERROR(
        "Failed to get CN entry using CN location in the server "
        "certificate");
    goto error;
  }

  /* Get CN from common name entry */
  cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
  if (cn_asn1 == NULL) {
    G_ERROR("Failed to get CN from CN entry in the server certificate");
    goto error;
  }

  cn = (char *)ASN1_STRING_data(cn_asn1);

  /* There should not be any NULL embedded in the CN */
  if ((size_t)ASN1_STRING_length(cn_asn1) != strlen(cn)) {
    G_ERROR("NULL embedded in the server certificate CN");
    goto error;
  }

  G_DEBUG("Server hostname in cert: %s", cn);

  if (!strcmp(cn, server_hostname)) {
    /* Success */
    ret_validation = 0;
  } else {
    G_ERROR(
        "Expected hostname is '%s' but found the name '%s' in the "
        "server certificate",
        cn, server_hostname);
  }
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

error:
  if (server_cert) X509_free(server_cert);

  return ret_validation;
}
#else
int avoid_compile_warning = 1;
#endif

std::pair<SSL *, int> Xcom_network_provider_ssl_library::timed_connect_ssl_msec(
    int fd, SSL_CTX *outgoing_ctx, const std::string &hostname, int timeout) {
  result ret;
  int return_value_error = 0;

  /* Set non-blocking */
  if (unblock_fd(fd) < 0) {
    return std::make_pair(nullptr, 1);
  }

  SSL *ssl_fd = SSL_new(outgoing_ctx);
  G_DEBUG("Trying to connect using SSL.")

  SSL_set_fd(ssl_fd, fd);
  ERR_clear_error();
  ret.val = SSL_connect(ssl_fd);
  ret.funerr = to_ssl_err(SSL_get_error(ssl_fd, ret.val));

  // Start the timer for the global timeout verification
  auto start = std::chrono::steady_clock::now();

  // Lambda function to verify the timeout.
  auto has_timed_out = [&]() {
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
               .count() > timeout;
  };

  bool poll_error = false;
  while (ret.val != SSL_SUCCESS && can_retry(ret.funerr) && !has_timed_out()) {
    if (poll_error =
            Xcom_network_provider_library::poll_for_timed_connects(fd, timeout);
        poll_error) {
      break;
    }

    SET_OS_ERR(0);
    ERR_clear_error();
    ret.val = SSL_connect(ssl_fd);
    ret.funerr = to_ssl_err(SSL_get_error(ssl_fd, ret.val));
  }

  if (ret.val != SSL_SUCCESS || poll_error) {
    if (!can_retry(ret.funerr)) {  // Do not emit an error, if it is a WANT_READ
                                   // or a WANT_WRITE, because it is not an
                                   // actionable item
      G_INFO("Error connecting using SSL %d %d", ret.funerr,
             SSL_get_error(ssl_fd, ret.val));
    }
    task_dump_err(ret.funerr);

    return_value_error = 1;
  } else if (ssl_verify_server_cert(ssl_fd, hostname.c_str())) {
    G_MESSAGE("Error validating certificate and peer from %s.",
              hostname.c_str());
    task_dump_err(ret.funerr);

    return_value_error = 1;
  }

  /* Set blocking */
  SET_OS_ERR(0);
  if (block_fd(fd) < 0) {
    G_ERROR(
        "Unable to set socket back to blocking state. "
        "(socket=%d, error=%d).",
        fd, GET_OS_ERR);
    return_value_error = 1;
  }

  return std::make_pair(ssl_fd, return_value_error);
}
