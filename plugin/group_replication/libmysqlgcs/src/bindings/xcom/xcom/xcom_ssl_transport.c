/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef XCOM_HAVE_OPENSSL
#include <assert.h>
#include <stdlib.h>
#include <wolfssl_fix_namespace_pollution_pre.h>

#include <openssl/dh.h>
#include <openssl/opensslv.h>

#include <wolfssl_fix_namespace_pollution.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include <wolfssl_fix_namespace_pollution_pre.h>

#include "openssl/engine.h"

#include <wolfssl_fix_namespace_pollution.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.h"

static const char *ssl_mode_options[] = {
    "DISABLED", "PREFERRED", "REQUIRED", "VERIFY_CA", "VERIFY_IDENTITY",
};

static const char *ssl_fips_mode_options[] = {"OFF", "ON", "STRICT"};

#define SSL_MODE_OPTIONS_COUNT \
  (sizeof(ssl_mode_options) / sizeof(*ssl_mode_options))

#define SSL_MODE_FIPS_OPTIONS_COUNT \
  (sizeof(ssl_fips_mode_options) / sizeof(*ssl_fips_mode_options))

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
    0x00, 0x05, 0x85, 0xB7,
};

static unsigned char dh2048_g[] = {
    0x05,
};

static DH *get_dh2048(void) {
  DH *dh;
  if ((dh = DH_new())) {
    BIGNUM *p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
    BIGNUM *g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
    if (!p || !g
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        || !DH_set0_pqg(dh, p, NULL, g)
#endif /* OPENSSL_VERSION_NUMBER >= 0x10100000L */
    ) {
      /* DH_free() will free 'p' and 'g' at once. */
      DH_free(dh);
      return NULL;
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    dh->p = p;
    dh->g = g;
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  }
  return (dh);
}

static char *ssl_pw = NULL;
static int ssl_mode = SSL_DISABLED;
static int ssl_fips_mode = SSL_FIPS_MODE_OFF;
static int ssl_init_done = 0;

SSL_CTX *server_ctx = NULL;
SSL_CTX *client_ctx = NULL;

/*
  Note that the functions, i.e. strtok and strcasecmp, are not
  considering multiple-byte characters as the original server
  code does.
*/
static long process_tls_version(const char *tls_version) {
  const char *separator = ", ";
  char *token = NULL;
  const char *tls_version_name_list[] = {"TLSv1", "TLSv1.1", "TLSv1.2"};
#define TLS_VERSIONS_COUNTS \
  (sizeof(tls_version_name_list) / sizeof(*tls_version_name_list))
  unsigned int tls_versions_count = TLS_VERSIONS_COUNTS;
  const long tls_ctx_list[TLS_VERSIONS_COUNTS] = {
      SSL_OP_NO_TLSv1, SSL_OP_NO_TLSv1_1, SSL_OP_NO_TLSv1_2};
  const char *ctx_flag_default = "TLSv1,TLSv1.1,TLSv1.2";
  long tls_ctx_flag = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
  unsigned int index = 0;
  char tls_version_option[TLS_VERSION_OPTION_SIZE] = "";
  int tls_found = 0;
  char *saved_ctx = NULL;

  if (!tls_version || !xcom_strcasecmp(tls_version, ctx_flag_default)) return 0;

  if (strlen(tls_version) - 1 > sizeof(tls_version_option)) return -1;

  strncpy(tls_version_option, tls_version, sizeof(tls_version_option));
  token = xcom_strtok(tls_version_option, separator, &saved_ctx);
  while (token) {
    for (index = 0; index < tls_versions_count; index++) {
      if (!xcom_strcasecmp(tls_version_name_list[index], token)) {
        tls_found = 1;
        tls_ctx_flag = tls_ctx_flag & (~tls_ctx_list[index]);
        break;
      }
    }
    token = xcom_strtok(NULL, separator, &saved_ctx);
  }

  if (!tls_found)
    return -1;
  else
    return tls_ctx_flag;
}

/* purecov: begin deadcode */
static int PasswordCallBack(char *passwd, int sz, int rw MY_ATTRIBUTE((unused)),
                            void *userdata MY_ATTRIBUTE((unused))) {
  const char *pw = ssl_pw ? ssl_pw : "yassl123";
  strncpy(passwd, pw, (size_t)sz);
  return (int)strlen(pw);
}
/* purecov: end */

static int configure_ssl_algorithms(SSL_CTX *ssl_ctx, const char *cipher,
                                    const char *tls_version) {
  DH *dh = NULL;
  long ssl_ctx_options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
  char cipher_list[SSL_CIPHER_LIST_SIZE] = {0};
  long ssl_ctx_flags = -1;

  SSL_CTX_set_default_passwd_cb(ssl_ctx, PasswordCallBack);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

  ssl_ctx_flags = process_tls_version(tls_version);
  if (ssl_ctx_flags < 0) {
    G_ERROR("TLS version is invalid: %s", tls_version);
    goto error;
  }

  ssl_ctx_options = (ssl_ctx_options | ssl_ctx_flags) &
                    (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                     SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2);

  SSL_CTX_set_options(ssl_ctx, ssl_ctx_options);

  /*
    Set the ciphers that can be used. Note, howerver, that the
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
    goto error;
  }

  dh = get_dh2048();
  if (SSL_CTX_set_tmp_dh(ssl_ctx, dh) == 0) {
    G_ERROR("Error setting up Diffie-Hellman key exchange");
    goto error;
  }
  DH_free(dh);

  return 0;

error:
  if (dh) DH_free(dh);
  return 1;
}

#ifndef HAVE_WOLFSSL
#define OPENSSL_ERROR_LENGTH 512
static int configure_ssl_fips_mode(const uint fips_mode) {
  int rc = -1;
  unsigned int fips_mode_old = -1;
  char err_string[OPENSSL_ERROR_LENGTH] = {'\0'};
  unsigned long err_library = 0;
  if (fips_mode > 2) {
    goto EXIT;
  }
  fips_mode_old = FIPS_mode();
  if (fips_mode_old == fips_mode) {
    rc = 1;
    goto EXIT;
  }
  if (!(rc = FIPS_mode_set(fips_mode))) {
    err_library = ERR_get_error();
    ERR_error_string_n(err_library, err_string, sizeof(err_string) - 1);
    err_string[sizeof(err_string) - 1] = '\0';
    G_ERROR("openssl fips mode set failed: %s", err_string);
  }
EXIT:
  return rc;
}
#endif

static int configure_ssl_ca(SSL_CTX *ssl_ctx, const char *ca_file,
                            const char *ca_path) {
  /* Load certs from the trusted ca. */
  if (SSL_CTX_load_verify_locations(ssl_ctx, ca_file, ca_path) == 0) {
    G_WARNING("Failed to locate and verify ca_file: %s, ca_path: %s", ca_file,
              ca_path);
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

static int configure_ssl_revocation(SSL_CTX *ssl_ctx MY_ATTRIBUTE((unused)),
                                    const char *crl_file,
                                    const char *crl_path) {
  int retval = 0;
  if (crl_file || crl_path) {
#ifndef HAVE_WOLFSSL
    X509_STORE *store = SSL_CTX_get_cert_store(ssl_ctx);
    /* Load crls from the trusted ca */
    if (X509_STORE_load_locations(store, crl_file, crl_path) == 0 ||
        X509_STORE_set_flags(
            store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) == 0) {
      G_ERROR("X509_STORE_load_locations for CRL error");
      retval = 1;
    }
#endif
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
    G_WARNING("Using the key file also as a certification file: %s.", key_file);
    cert_file = key_file;
  }

  if (!key_file && cert_file) {
    G_WARNING("Using the certification file also as a key file: %s.",
              cert_file);
    key_file = cert_file;
  }

  if (cert_file &&
      SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
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
                    SSL_CTX *ssl_ctx) {
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

  if (configure_ssl_algorithms(ssl_ctx, cipher, tls_version)) goto error;

  if (configure_ssl_ca(ssl_ctx, ca_file, ca_path)) goto error;

  if (configure_ssl_revocation(ssl_ctx, crl_file, crl_path)) goto error;

  if (configure_ssl_keys(ssl_ctx, key_file, cert_file)) goto error;

  G_DEBUG("Success initializing SSL");

  return 0;

error:
  G_MESSAGE("Error initializing SSL");
  return 1;
}

int xcom_use_ssl() {
  assert(ssl_mode >= SSL_DISABLED && ssl_mode < LAST_SSL_MODE);
  return ssl_mode != SSL_DISABLED;
}

/* purecov: begin deadcode */
void xcom_set_default_passwd(char *pw) {
  if (ssl_pw) free(ssl_pw);
  ssl_pw = strdup(pw);
}
/* purecov: end */

int xcom_get_ssl_mode(const char *mode) {
  int retval = INVALID_SSL_MODE;
  int idx = 0;

  for (; idx < (int)SSL_MODE_OPTIONS_COUNT; ++idx) {
    if (strcmp(mode, ssl_mode_options[idx]) == 0) {
      retval = idx + 1; /* The enumeration is shifted. */
      break;
    }
  }
  assert(retval >= INVALID_SSL_MODE && retval <= LAST_SSL_MODE);

  return retval;
}

int xcom_set_ssl_mode(int mode) {
  int retval = INVALID_SSL_MODE;

  mode = (mode == SSL_PREFERRED ? SSL_DISABLED : mode);
  if (mode >= SSL_DISABLED && mode < LAST_SSL_MODE) retval = ssl_mode = mode;

  assert(retval >= INVALID_SSL_MODE && retval < LAST_SSL_MODE);
  assert(ssl_mode >= SSL_DISABLED && ssl_mode < LAST_SSL_MODE);

  return retval;
}

int xcom_get_ssl_fips_mode(const char *mode) {
  int retval = INVALID_SSL_FIPS_MODE;
  int idx = 0;

  for (; idx < (int)SSL_MODE_FIPS_OPTIONS_COUNT; ++idx) {
    if (strcmp(mode, ssl_fips_mode_options[idx]) == 0) {
      retval = idx;
      break;
    }
  }
  assert(retval > INVALID_SSL_FIPS_MODE && retval < LAST_SSL_FIPS_MODE);

  return retval;
}

int xcom_set_ssl_fips_mode(int mode) {
  int retval = INVALID_SSL_FIPS_MODE;

  if (mode >= SSL_FIPS_MODE_OFF && mode < LAST_SSL_FIPS_MODE) {
    retval = ssl_fips_mode = mode;
  }

  assert(retval > INVALID_SSL_FIPS_MODE && retval < LAST_SSL_FIPS_MODE);
  return retval;
}

int xcom_init_ssl(const char *server_key_file, const char *server_cert_file,
                  const char *client_key_file, const char *client_cert_file,
                  const char *ca_file, const char *ca_path,
                  const char *crl_file, const char *crl_path,
                  const char *cipher, const char *tls_version) {
  int verify_server = SSL_VERIFY_NONE;
  int verify_client = SSL_VERIFY_NONE;

#ifndef HAVE_WOLFSSL
  if (configure_ssl_fips_mode(ssl_fips_mode) != 1) {
    G_ERROR("Error setting the ssl fips mode");
    goto error;
  }
#endif

  SSL_library_init();
  SSL_load_error_strings();

  if (ssl_mode == SSL_DISABLED) {
    G_WARNING("SSL is not enabled");
    return ssl_init_done;
  }

  if (ssl_init_done) {
    G_WARNING("SSL already initialized");
    return ssl_init_done;
  }

  G_DEBUG("Configuring SSL for the server")
  server_ctx = SSL_CTX_new(SSLv23_server_method());
  if (!server_ctx) {
    G_ERROR("Error allocating SSL Context object for the server");
    goto error;
  }
  if (init_ssl(server_key_file, server_cert_file, ca_file, ca_path, crl_file,
               crl_path, cipher, tls_version, server_ctx))
    goto error;

  if (ssl_mode != SSL_REQUIRED)
    verify_server = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  SSL_CTX_set_verify(server_ctx, verify_server, NULL);

  G_DEBUG("Configuring SSL for the client")
  client_ctx = SSL_CTX_new(SSLv23_client_method());
  if (!client_ctx) {
    G_ERROR("Error allocating SSL Context object for the client");
    goto error;
  }
  if (init_ssl(client_key_file, client_cert_file, ca_file, ca_path, crl_file,
               crl_path, cipher, tls_version, client_ctx))
    goto error;

  if (ssl_mode != SSL_REQUIRED) verify_client = SSL_VERIFY_PEER;
  SSL_CTX_set_verify(client_ctx, verify_client, NULL);

  ssl_init_done = 1;

  return ssl_init_done;

error:
  xcom_destroy_ssl();

  return ssl_init_done;
}

void xcom_cleanup_ssl() {
  if (!xcom_use_ssl()) return;

#ifndef HAVE_WOLFSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
#endif
}

void xcom_destroy_ssl() {
  if (!xcom_use_ssl()) return;

  G_DEBUG("Destroying SSL");

  ssl_init_done = 0;

  if (server_ctx) {
    SSL_CTX_free(server_ctx);
    server_ctx = NULL;
  }

  if (client_ctx) {
    SSL_CTX_free(client_ctx);
    client_ctx = NULL;
  }

#if defined(HAVE_WOLFSSL) && defined(WITH_SSL_STANDALONE)
  yaSSL_CleanUp();
#elif defined(WITH_SSL_STANDALONE)
  ENGINE_cleanup();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
#endif

  xcom_cleanup_ssl();

  G_DEBUG("Success destroying SSL");
}

int ssl_verify_server_cert(SSL *ssl, const char *server_hostname) {
  X509 *server_cert = NULL;
  char *cn = NULL;
  int cn_loc = -1;
  ASN1_STRING *cn_asn1 = NULL;
  X509_NAME_ENTRY *cn_entry = NULL;
  X509_NAME *subject = NULL;
  int ret_validation = 1;

  G_DEBUG("Verifying server certificate and expected host name: %s",
          server_hostname);

  if (ssl_mode != SSL_VERIFY_IDENTITY) return 0;

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

  /*
   Some notes for future development
   We should check host name in alternative name first and then if needed check
   in common name.
   Currently yssl doesn't support alternative name.
   openssl 1.0.2 support X509_check_host method for host name validation, we may
   need to start using
   X509_check_host in the future.
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

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  cn = (char *)ASN1_STRING_data(cn_asn1);
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  cn = (char *)ASN1_STRING_get0_data(cn_asn1);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

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

error:
  if (server_cert) X509_free(server_cert);

  return ret_validation;
}
#else
int avoid_compile_warning = 1;
#endif
