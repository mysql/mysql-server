/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#include <memory>
#include <string>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/my_loglevel.h"
#include "mysql/strings/m_ctype.h"
#if !defined(HAVE_PSI_INTERFACE)
#include "mysql/psi/mysql_rwlock.h"
#endif
#include "mysql/service_mysql_alloc.h"
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
    "Failed to set X509 verification parameter",
    "Invalid certificates"};

const char *sslGetErrString(enum enum_ssl_init_error e) {
  assert(SSL_INITERR_NOERROR < e && e < SSL_INITERR_LASTERR);
  return ssl_error_string[e];
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

/*
  OpenSSL 1.1 supports native platform threads,
  so we don't need the following callback functions.
*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
/* OpenSSL specific */

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_key key_rwlock_openssl;

static PSI_rwlock_info openssl_rwlocks[] = {
    {&key_rwlock_openssl, "CRYPTO_dynlock_value::lock", 0, 0, nullptr}};
#endif

typedef struct CRYPTO_dynlock_value {
  mysql_rwlock_t lock;
} openssl_lock_t;

/* Array of locks used by openssl internally for thread synchronization.
   The number of locks is equal to CRYPTO_num_locks.
*/
static openssl_lock_t *openssl_stdlocks;

/*OpenSSL callback functions for multithreading. We implement all the functions
  as we are using our own locking mechanism.
*/
static void openssl_lock(int mode, openssl_lock_t *lock,
                         const char *file [[maybe_unused]],
                         int line [[maybe_unused]]) {
  int err;
  char const *what;

  switch (mode) {
    case CRYPTO_LOCK | CRYPTO_READ:
      what = "read lock";
      err = mysql_rwlock_rdlock(&lock->lock);
      break;
    case CRYPTO_LOCK | CRYPTO_WRITE:
      what = "write lock";
      err = mysql_rwlock_wrlock(&lock->lock);
      break;
    case CRYPTO_UNLOCK | CRYPTO_READ:
    case CRYPTO_UNLOCK | CRYPTO_WRITE:
      what = "unlock";
      err = mysql_rwlock_unlock(&lock->lock);
      break;
    default:
      /* Unknown locking mode. */
      DBUG_PRINT("error",
                 ("Fatal OpenSSL: %s:%d: interface problem (mode=0x%x)\n", file,
                  line, mode));

      fprintf(stderr, "Fatal: OpenSSL interface problem (mode=0x%x)", mode);
      fflush(stderr);
      my_abort();
  }
  if (err) {
    DBUG_PRINT("error", ("Fatal OpenSSL: %s:%d: can't %s OpenSSL lock\n", file,
                         line, what));

    fprintf(stderr, "Fatal: can't %s OpenSSL lock", what);
    fflush(stderr);
    my_abort();
  }
}

static void openssl_lock_function(int mode, int n,
                                  const char *file [[maybe_unused]],
                                  int line [[maybe_unused]]) {
  if (n < 0 || n > CRYPTO_num_locks()) {
    /* Lock number out of bounds. */
    DBUG_PRINT("error", ("Fatal OpenSSL: %s:%d: interface problem (n = %d)",
                         file, line, n));

    fprintf(stderr, "Fatal: OpenSSL interface problem (n = %d)", n);
    fflush(stderr);
    my_abort();
  }
  openssl_lock(mode, &openssl_stdlocks[n], file, line);
}

static openssl_lock_t *openssl_dynlock_create(const char *file [[maybe_unused]],
                                              int line [[maybe_unused]]) {
  openssl_lock_t *lock;

  DBUG_PRINT("info", ("openssl_dynlock_create: %s:%d", file, line));

  lock = (openssl_lock_t *)my_malloc(PSI_NOT_INSTRUMENTED,
                                     sizeof(openssl_lock_t), MYF(0));

#ifdef HAVE_PSI_INTERFACE
  mysql_rwlock_init(key_rwlock_openssl, &lock->lock);
#else
  mysql_rwlock_init(0, &lock->lock);
#endif
  return lock;
}

static void openssl_dynlock_destroy(openssl_lock_t *lock,
                                    const char *file [[maybe_unused]],
                                    int line [[maybe_unused]]) {
  DBUG_PRINT("info", ("openssl_dynlock_destroy: %s:%d", file, line));

  mysql_rwlock_destroy(&lock->lock);
  my_free(lock);
}

static unsigned long openssl_id_function() {
  return (unsigned long)my_thread_self();
}

// End of mutlithreading callback functions

static void init_ssl_locks() {
  int i = 0;
#ifdef HAVE_PSI_INTERFACE
  const char *category = "sql";
  int count = static_cast<int>(array_elements(openssl_rwlocks));
  mysql_rwlock_register(category, openssl_rwlocks, count);
#endif

  openssl_stdlocks = (openssl_lock_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                                      sizeof(openssl_lock_t));
  for (i = 0; i < CRYPTO_num_locks(); ++i)
#ifdef HAVE_PSI_INTERFACE
    mysql_rwlock_init(key_rwlock_openssl, &openssl_stdlocks[i].lock);
#else
    mysql_rwlock_init(0, &openssl_stdlocks[i].lock);
#endif
}

static void set_lock_callback_functions(bool init) {
  CRYPTO_set_locking_callback(init ? openssl_lock_function : nullptr);
  CRYPTO_set_id_callback(init ? openssl_id_function : nullptr);
  CRYPTO_set_dynlock_create_callback(init ? openssl_dynlock_create : nullptr);
  CRYPTO_set_dynlock_destroy_callback(init ? openssl_dynlock_destroy : nullptr);
  CRYPTO_set_dynlock_lock_callback(init ? openssl_lock : nullptr);
}

static void init_lock_callback_functions() {
  set_lock_callback_functions(true);
}

static void deinit_lock_callback_functions() {
  set_lock_callback_functions(false);
}

#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

void vio_ssl_end() {
  if (ssl_initialized) {
    fips_deinit();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_thread_state(nullptr);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    ERR_free_strings();
    EVP_cleanup();

    CRYPTO_cleanup_all_ex_data();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    deinit_lock_callback_functions();

    for (int i = 0; i < CRYPTO_num_locks(); ++i)
      mysql_rwlock_destroy(&openssl_stdlocks[i].lock);
    OPENSSL_free(openssl_stdlocks);

#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    ssl_initialized = false;
  }
}

void ssl_start() {
  if (!ssl_initialized) {
    ssl_initialized = true;

    fips_init();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    init_ssl_locks();
    init_lock_callback_functions();
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  }
}

long process_tls_version(const char *tls_version) {
  const char *separator = ",";
  char *token, *lasts = nullptr;

#ifdef HAVE_TLSv13
  const char *tls_version_name_list[] = {"TLSv1.2", "TLSv1.3"};
  const char ctx_flag_default[] = "TLSv1.2,TLSv1.3";
  const long tls_ctx_list[] = {SSL_OP_NO_TLSv1_2, SSL_OP_NO_TLSv1_3};
  long tls_ctx_flag = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 |
                      SSL_OP_NO_TLSv1_3 | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
#else
  const char *tls_version_name_list[] = {"TLSv1.2"};
  const char ctx_flag_default[] = "TLSv1.2";
  const long tls_ctx_list[] = {SSL_OP_NO_TLSv1_2};
  long tls_ctx_flag = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 |
                      SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
#endif /* HAVE_TLSv13 */
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

/************************ VioSSLFd **********************************/
static struct st_VioSSLFd *new_VioSSLFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher,
    const char *ciphersuites [[maybe_unused]], bool is_client,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags, const char *server_host [[maybe_unused]]) {
  struct st_VioSSLFd *ssl_fd;
  long ssl_ctx_options =
      SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
  std::string tls12_cipher_list, tls13_cipher_list;
  DBUG_TRACE;
  DBUG_PRINT(
      "enter",
      ("key_file: '%s'  cert_file: '%s'  ca_file: '%s'  ca_path: '%s'  "
       "cipher: '%s' crl_file: '%s' crl_path: '%s' ssl_ctx_flags: '%ld' ",
       key_file ? key_file : "NULL", cert_file ? cert_file : "NULL",
       ca_file ? ca_file : "NULL", ca_path ? ca_path : "NULL",
       cipher ? cipher : "NULL", crl_file ? crl_file : "NULL",
       crl_path ? crl_path : "NULL", ssl_ctx_flags));

  if (ssl_ctx_flags < 0) {
    *error = SSL_TLS_VERSION_INVALID;
    DBUG_PRINT("error", ("TLS version invalid : %s", sslGetErrString(*error)));
    report_errors();
    return nullptr;
  }

  ssl_ctx_options = (ssl_ctx_options | ssl_ctx_flags) &
                    (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                     SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2
#ifdef HAVE_TLSv13
                     | SSL_OP_NO_TLSv1_3
#endif /* HAVE_TLSv13 */
                     | SSL_OP_NO_TICKET);
  if (!(ssl_fd = ((struct st_VioSSLFd *)my_malloc(
            key_memory_vio_ssl_fd, sizeof(struct st_VioSSLFd), MYF(0)))))
    return nullptr;

  if (!(ssl_fd->ssl_context = SSL_CTX_new(is_client ?
#ifdef HAVE_TLSv13
                                                    TLS_client_method()
                                                    : TLS_server_method()
#else  /* HAVE_TLSv13 */
                                                    SSLv23_client_method()
                                                    : SSLv23_server_method()
#endif /* HAVE_TLSv13 */
                                              ))) {
    *error = SSL_INITERR_MEMFAIL;
    DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
    report_errors();
    my_free(ssl_fd);
    return nullptr;
  }

#ifdef HAVE_TLSv13
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
#endif /* HAVE_TLSv13 */

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
  if (set_dh(ssl_fd->ssl_context)) {
    printf("%s\n", ERR_error_string(ERR_get_error(), nullptr));
    *error = SSL_INITERR_DHFAIL;
    goto error;
  }

  /* ECDH stuff */
  if (set_ecdh(ssl_fd->ssl_context)) {
    *error = SSL_INITERR_ECDHFAIL;
    goto error;
  }

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
  /*
    OpenSSL 1.0.2 and up provides support for hostname validation.
    If server_host parameter is set it contains either IP address or
    server's hostname. Pass it to the lib to perform automatic checks.
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
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

  SSL_CTX_set_options(ssl_fd->ssl_context, ssl_ctx_options);

  DBUG_PRINT("exit", ("OK 1"));

  return ssl_fd;

error:
  DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
  report_errors();
  SSL_CTX_free(ssl_fd->ssl_context);
  my_free(ssl_fd);
  return nullptr;
}

/************************ VioSSLConnectorFd
 * **********************************/
struct st_VioSSLFd *new_VioSSLConnectorFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher, const char *ciphersuites,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags, const char *server_host) {
  struct st_VioSSLFd *ssl_fd;
  int verify = SSL_VERIFY_PEER;

  /*
    Turn off verification of servers certificate if both
    ca_file and ca_path is set to nullptr
  */
  if (ca_file == nullptr && ca_path == nullptr) verify = SSL_VERIFY_NONE;

  if (!(ssl_fd = new_VioSSLFd(key_file, cert_file, ca_file, ca_path, cipher,
                              ciphersuites, true, error, crl_file, crl_path,
                              ssl_ctx_flags, server_host))) {
    return nullptr;
  }

  /* Init the VioSSLFd as a "connector" ie. the client side */

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, nullptr);

  return ssl_fd;
}

/************************ VioSSLAcceptorFd **********************************/
struct st_VioSSLFd *new_VioSSLAcceptorFd(
    const char *key_file, const char *cert_file, const char *ca_file,
    const char *ca_path, const char *cipher, const char *ciphersuites,
    enum enum_ssl_init_error *error, const char *crl_file, const char *crl_path,
    const long ssl_ctx_flags) {
  struct st_VioSSLFd *ssl_fd;
  int verify = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  if (!(ssl_fd = new_VioSSLFd(key_file, cert_file, ca_file, ca_path, cipher,
                              ciphersuites, false, error, crl_file, crl_path,
                              ssl_ctx_flags, nullptr))) {
    return nullptr;
  }
  /* Init the the VioSSLFd as a "acceptor" ie. the server side */

  /* Set max number of cached sessions, returns the previous size */
  SSL_CTX_sess_set_cache_size(ssl_fd->ssl_context, 128);

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, nullptr);

  /*
    Set session_id - an identifier for this server session
    Use the ssl_fd pointer
   */
  SSL_CTX_set_session_id_context(ssl_fd->ssl_context,
                                 (const unsigned char *)ssl_fd, sizeof(ssl_fd));

  return ssl_fd;
}

void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd) {
  SSL_CTX_free(fd->ssl_context);
  my_free(fd);
}
