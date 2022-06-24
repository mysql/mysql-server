/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>
#include <sql/auth/auth_common.h>
#include <sql/mysqld.h>
#include <sql/options_mysqld.h>
#include <sql/sql_initialize.h>
#include <sql/ssl_init_callback.h>
#include <sql/sys_vars.h>
#include <sql/sys_vars_shared.h> /* AutoRLock , PolyLock_mutex */

/* Internal flag */
std::atomic_bool g_admin_ssl_configured(false);

std::string mysql_main_channel("mysql_main");
std::string mysql_admin_channel("mysql_admin");

/** SSL context options */

/* Related to client server connection port */
static const char *opt_ssl_ca = nullptr;
static const char *opt_ssl_key = nullptr;
static const char *opt_ssl_cert = nullptr;
static char *opt_ssl_capath = nullptr;
static char *opt_ssl_cipher = nullptr;
static char *opt_tls_ciphersuites = nullptr;
static char *opt_ssl_crl = nullptr;
static char *opt_ssl_crlpath = nullptr;
static char *opt_tls_version = nullptr;
static bool opt_ssl_session_cache_mode = true;
static long opt_ssl_session_cache_timeout = 300;

static PolyLock_mutex lock_ssl_ctx(&LOCK_tls_ctx_options);

/* Related to admin connection port */
static const char *opt_admin_ssl_ca = nullptr;
static const char *opt_admin_ssl_key = nullptr;
static const char *opt_admin_ssl_cert = nullptr;
static const char *opt_admin_ssl_capath = nullptr;
static const char *opt_admin_ssl_cipher = nullptr;
static const char *opt_admin_tls_ciphersuites = nullptr;
static const char *opt_admin_ssl_crl = nullptr;
static const char *opt_admin_ssl_crlpath = nullptr;
static const char *opt_admin_tls_version = nullptr;

static PolyLock_mutex lock_admin_ssl_ctx(&LOCK_admin_tls_ctx_options);

bool validate_tls_version(const char *val) {
  if (val && val[0] == 0) return false;
  std::string token;
  std::stringstream str(val);
  while (getline(str, token, ',')) {
    if (my_strcasecmp(system_charset_info, token.c_str(), "TLSv1.2") &&
        my_strcasecmp(system_charset_info, token.c_str(), "TLSv1.3"))
      return true;
  }
  return false;
}

static bool check_tls_version(sys_var *, THD *, set_var *var) {
  if (!(var->save_result.string_value.str)) return true;
  return validate_tls_version(var->save_result.string_value.str);
}

static bool check_admin_tls_version(sys_var *, THD *, set_var *var) {
  return check_tls_version(nullptr, nullptr, var);
}

/*
  If you are adding new system variable for SSL communication, please take a
  look at do_auto_cert_generation() function in sql_authentication.cc and
  add new system variable in checks if required.
*/

/* Related to client server connection port */
static Sys_var_charptr Sys_ssl_ca(
    "ssl_ca", "CA file in PEM format (check OpenSSL docs, implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_ca),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CA), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_capath(
    "ssl_capath", "CA directory (check OpenSSL docs, implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_capath),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CAPATH), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

static Sys_var_charptr Sys_tls_version(
    "tls_version",
#ifdef HAVE_TLSv13
    "TLS version, permitted values are TLSv1.2, TLSv1.3",
#else
    "TLS version, permitted values are TLSv1.2",
#endif
    PERSIST_AS_READONLY GLOBAL_VAR(opt_tls_version),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_VERSION), IN_FS_CHARSET,
#ifdef HAVE_TLSv13
    "TLSv1.2,TLSv1.3",
#else
    "TLSv1.2",
#endif /* HAVE_TLSv13 */
    &lock_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(check_tls_version));

static Sys_var_charptr Sys_ssl_cert(
    "ssl_cert", "X509 cert in PEM format (implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_cert),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CERT), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_cipher(
    "ssl_cipher", "SSL cipher to use (implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_cipher),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CIPHER), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

static Sys_var_charptr Sys_tls_ciphersuites(
    "tls_ciphersuites", "TLS v1.3 ciphersuite to use (implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_tls_ciphersuites),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_CIPHERSUITES), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_key("ssl_key",
                                   "X509 key in PEM format (implies --ssl)",
                                   PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_key),
                                   CMD_LINE(REQUIRED_ARG, OPT_SSL_KEY),
                                   IN_FS_CHARSET, DEFAULT(nullptr),
                                   &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_crl(
    "ssl_crl", "CRL file in PEM format (check OpenSSL docs, implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_crl),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CRL), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_crlpath(
    "ssl_crlpath", "CRL directory (check OpenSSL docs, implies --ssl)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_crlpath),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CRLPATH), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx);

#define PFS_TRAILING_PROPERTIES                                         \
  NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(NULL), ON_UPDATE(NULL), NULL, \
      sys_var::PARSE_EARLY

static Sys_var_bool Sys_var_opt_ssl_session_cache_mode(
    "ssl_session_cache_mode", "Is TLS session cache enabled or not",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_session_cache_mode),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

/* 84600 is 1 day in seconds */
static Sys_var_long Sys_var_opt_ssl_session_cache_timeout(
    "ssl_session_cache_timeout",
    "The timeout to expire sessions in the TLS session cache",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_session_cache_timeout),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_SESSION_CACHE_TIMEOUT),
    VALID_RANGE(0, 84600), DEFAULT(300), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

/* Related to admin connection port */
static Sys_var_charptr Sys_admin_ssl_ca(
    "admin_ssl_ca",
    "CA file in PEM format (check OpenSSL docs, implies --ssl) for "
    "--admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_ca),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CA), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_ssl_capath(
    "admin_ssl_capath",
    "CA directory (check OpenSSL docs, implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_capath),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CAPATH), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_tls_version(
    "admin_tls_version",
#ifdef HAVE_TLSv13
    "TLS version for --admin-port, permitted values are TLSv1.2, TLSv1.3",
#else
    "TLS version for --admin-port, permitted values are TLSv1.2",
#endif
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_tls_version),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_VERSION), IN_FS_CHARSET,
#ifdef HAVE_TLSv13
    "TLSv1.2,TLSv1.3",
#else
    "TLSv1.2",
#endif /* HAVE_TLSv13 */
    &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(check_admin_tls_version));

static Sys_var_charptr Sys_admin_ssl_cert(
    "admin_ssl_cert",
    "X509 cert in PEM format (implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_cert),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CERT), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_ssl_cipher(
    "admin_ssl_cipher", "SSL cipher to use (implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_cipher),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CIPHER), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_tls_ciphersuites(
    "admin_tls_ciphersuites",
    "TLS v1.3 ciphersuite to use (implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_tls_ciphersuites),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_CIPHERSUITES), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_ssl_key(
    "admin_ssl_key", "X509 key in PEM format (implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_key),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_KEY), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_ssl_crl(
    "admin_ssl_crl",
    "CRL file in PEM format (check OpenSSL docs, implies --ssl) for "
    "--admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_crl),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CRL), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

static Sys_var_charptr Sys_admin_ssl_crlpath(
    "admin_ssl_crlpath",
    "CRL directory (check OpenSSL docs, implies --ssl) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_crlpath),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CRLPATH), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx);

/* Helper functions */
static bool warn_self_signed_ca_certs(const char *ssl_ca,
                                      const char *ssl_capath) {
  bool ret_val = false;

  /* Lamda to check self sign status of one certificate */
  auto warn_one = [](const char *ca) -> bool {
    char *issuer = nullptr;
    char *subject = nullptr;
    X509 *ca_cert;
    BIO *bio;
    FILE *fp;

    if (!(fp = my_fopen(ca, O_RDONLY | MY_FOPEN_BINARY, MYF(MY_WME)))) {
      LogErr(ERROR_LEVEL, ER_CANT_OPEN_CA);
      return true;
    }

    bio = BIO_new(BIO_s_file());
    if (!bio) {
      LogErr(ERROR_LEVEL, ER_FAILED_TO_ALLOCATE_SSL_BIO);
      my_fclose(fp, MYF(0));
      return true;
    }
    BIO_set_fp(bio, fp, BIO_NOCLOSE);
    ca_cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!ca_cert) {
      /* We are not interested in anything other than X509 certificates */
      my_fclose(fp, MYF(MY_WME));
      return false;
    }

    issuer = X509_NAME_oneline(X509_get_issuer_name(ca_cert), nullptr, 0);
    subject = X509_NAME_oneline(X509_get_subject_name(ca_cert), nullptr, 0);

    /* Suppressing warning which is not relevant during initialization */
    if (!strcmp(issuer, subject) &&
        !(opt_initialize || opt_initialize_insecure)) {
      LogErr(WARNING_LEVEL, ER_CA_SELF_SIGNED, ca);
    }

    OPENSSL_free(issuer);
    OPENSSL_free(subject);
    X509_free(ca_cert);
    my_fclose(fp, MYF(MY_WME));
    return false;
  };

  if (ssl_ca && ssl_ca[0]) {
    if (warn_one(ssl_ca)) return true;
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
        if ((ret_val = warn_one(file_path.str))) break;
      }
    }
    my_dirend(ca_dir);
    dynstr_free(&file_path);

    ca_dir = nullptr;
    memset(&file_path, 0, sizeof(file_path));
  }
  return ret_val;
}

/* Client server connection port callbacks */

void Ssl_init_callback_server_main::read_parameters(
    OptionalString *ca, OptionalString *capath, OptionalString *version,
    OptionalString *cert, OptionalString *cipher, OptionalString *ciphersuites,
    OptionalString *key, OptionalString *crl, OptionalString *crl_path,
    bool *session_cache_mode, long *session_cache_timeout) {
  AutoRLock lock(&lock_ssl_ctx);
  if (ca) ca->assign(opt_ssl_ca);
  if (capath) capath->assign(opt_ssl_capath);
  if (version) version->assign(opt_tls_version);
  if (cert) cert->assign(opt_ssl_cert);
  if (cipher) cipher->assign(opt_ssl_cipher);
  if (ciphersuites) ciphersuites->assign(opt_tls_ciphersuites);
  if (key) key->assign(opt_ssl_key);
  if (crl) crl->assign(opt_ssl_crl);
  if (crl_path) crl_path->assign(opt_ssl_crlpath);
  if (session_cache_mode) *session_cache_mode = opt_ssl_session_cache_mode;
  if (session_cache_timeout)
    *session_cache_timeout = opt_ssl_session_cache_timeout;
}

ssl_artifacts_status Ssl_init_callback_server_main::auto_detect_ssl() {
  MY_STAT cert_stat, cert_key, ca_stat;
  uint result = 1;
  ssl_artifacts_status ret_status = SSL_ARTIFACTS_VIA_OPTIONS;

  /*
    No need to take the ssl_ctx_lock lock here since it's being called
    from singleton_init().
  */
  if ((!opt_ssl_cert || !opt_ssl_cert[0]) &&
      (!opt_ssl_key || !opt_ssl_key[0]) && (!opt_ssl_ca || !opt_ssl_ca[0]) &&
      (!opt_ssl_capath || !opt_ssl_capath[0]) &&
      (!opt_ssl_crl || !opt_ssl_crl[0]) &&
      (!opt_ssl_crlpath || !opt_ssl_crlpath[0])) {
    result =
        result << (my_stat(DEFAULT_SSL_SERVER_CERT, &cert_stat, MYF(0)) ? 1 : 0)
               << (my_stat(DEFAULT_SSL_SERVER_KEY, &cert_key, MYF(0)) ? 1 : 0)
               << (my_stat(DEFAULT_SSL_CA_CERT, &ca_stat, MYF(0)) ? 1 : 0);

    switch (result) {
      case 8:
        opt_ssl_ca = DEFAULT_SSL_CA_CERT;
        opt_ssl_cert = DEFAULT_SSL_SERVER_CERT;
        opt_ssl_key = DEFAULT_SSL_SERVER_KEY;
        ret_status = SSL_ARTIFACTS_AUTO_DETECTED;
        break;
      case 4:
      case 2:
        ret_status = SSL_ARTIFACT_TRACES_FOUND;
        break;
      default:
        ret_status = SSL_ARTIFACTS_NOT_FOUND;
        break;
    };
  }

  return ret_status;
}

bool Ssl_init_callback_server_main::provision_certs() {
  ssl_artifacts_status auto_detection_status;
  AutoRLock lock(&lock_ssl_ctx);
  auto_detection_status = auto_detect_ssl();
  if (auto_detection_status == SSL_ARTIFACTS_AUTO_DETECTED)
    LogErr(INFORMATION_LEVEL, ER_SSL_TRYING_DATADIR_DEFAULTS,
           DEFAULT_SSL_CA_CERT, DEFAULT_SSL_SERVER_CERT,
           DEFAULT_SSL_SERVER_KEY);
  return !do_auto_cert_generation(auto_detection_status, &opt_ssl_ca,
                                  &opt_ssl_key, &opt_ssl_cert);
}

bool Ssl_init_callback_server_main::warn_self_signed_ca() {
  AutoRLock lock(&lock_ssl_ctx);
  return warn_self_signed_ca_certs(opt_ssl_ca, opt_ssl_capath);
}

/* Admin connection port callbacks */

void Ssl_init_callback_server_admin::read_parameters(
    OptionalString *ca, OptionalString *capath, OptionalString *version,
    OptionalString *cert, OptionalString *cipher, OptionalString *ciphersuites,
    OptionalString *key, OptionalString *crl, OptionalString *crl_path,
    bool *session_cache_mode, long *session_cache_timeout) {
  AutoRLock lock(&lock_admin_ssl_ctx);
  if (ca) ca->assign(opt_admin_ssl_ca);
  if (capath) capath->assign(opt_admin_ssl_capath);
  if (version) version->assign(opt_admin_tls_version);
  if (cert) cert->assign(opt_admin_ssl_cert);
  if (cipher) cipher->assign(opt_admin_ssl_cipher);
  if (ciphersuites) ciphersuites->assign(opt_admin_tls_ciphersuites);
  if (key) key->assign(opt_admin_ssl_key);
  if (crl) crl->assign(opt_admin_ssl_crl);
  if (crl_path) crl_path->assign(opt_admin_ssl_crlpath);
  if (session_cache_mode) *session_cache_mode = opt_ssl_session_cache_mode;
  if (session_cache_timeout)
    *session_cache_timeout = opt_ssl_session_cache_timeout;

  if (opt_admin_ssl_ca || opt_admin_ssl_capath || opt_admin_ssl_cert ||
      opt_admin_ssl_cipher || opt_admin_tls_ciphersuites || opt_admin_ssl_key ||
      opt_admin_ssl_crl || opt_admin_ssl_crlpath)
    g_admin_ssl_configured = true;
}

bool Ssl_init_callback_server_admin::warn_self_signed_ca() {
  AutoRLock lock(&lock_ssl_ctx);
  return warn_self_signed_ca_certs(opt_admin_ssl_ca, opt_admin_ssl_capath);
}

Ssl_init_callback_server_main server_main_callback;
Ssl_init_callback_server_admin server_admin_callback;
