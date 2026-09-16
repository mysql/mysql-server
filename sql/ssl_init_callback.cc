/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/services/log_builtins.h>
#include <mysql/service_mysql_alloc.h>
#include <mysqld_error.h>
#include <sql/auth/auth_common.h>
#include <sql/mysqld.h>
#include <sql/options_mysqld.h>
#include <sql/sql_initialize.h>
#include <sql/ssl_acceptor_context_operator.h>
#include <sql/ssl_init_callback.h>
#include <sql/sys_vars.h>
#include <sql/sys_vars_shared.h> /* AutoRLock , PolyLock_mutex */
#include <tls_ciphers.h>
#include <violite.h>
#include "include/dh_ecdh_config.h"

/* Internal flag */
std::atomic_bool g_admin_ssl_configured(false);

std::string mysql_main_channel("mysql_main");
std::string mysql_admin_channel("mysql_admin");

/** SSL context options */

bool opt_tls_certificates_enforced_validation{false};

/* Related to client server connection port */
static const char *opt_ssl_ca = nullptr;
static const char *opt_ssl_key = nullptr;
static const char *opt_ssl_cert = nullptr;
static char *opt_ssl_capath = nullptr;
static char *opt_ssl_cipher = nullptr;
static char *opt_tls_ciphersuites = nullptr;
static char *opt_tls_kex = nullptr;
static char *opt_ssl_crl = nullptr;
static char *opt_ssl_crlpath = nullptr;
static char *opt_tls_version = nullptr;
static bool opt_force_pqc = false;
static bool opt_use_pqc_sign = false;
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
static char *opt_admin_tls_kex = nullptr;
static const char *opt_admin_ssl_crl = nullptr;
static const char *opt_admin_ssl_crlpath = nullptr;
static const char *opt_admin_tls_version = nullptr;
static bool opt_admin_force_pqc = false;
static bool opt_admin_use_pqc_sign = false;
bool opt_admin_ssl_configured = false;

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

static bool tls_force_pqc_supported_for_version(const char *tls_version
                                                [[maybe_unused]]) {
#if OPENSSL_VERSION_NUMBER < 0x30500000L
  return false;
#else
  return !(process_tls_version(tls_version) & SSL_OP_NO_TLSv1_3);
#endif
}

static void adjust_startup_force_pqc_for_tls_version(
    const char *force_pqc_name [[maybe_unused]],
    const char *tls_version_name [[maybe_unused]],
    const char *tls_version [[maybe_unused]],
    bool *force_pqc [[maybe_unused]]) {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  if (!*force_pqc || tls_force_pqc_supported_for_version(tls_version)) return;

  const std::string error =
      std::string(force_pqc_name) +
      "=ON requires TLSv1.3 with OpenSSL "
      "3.5.0 or newer, but " +
      tls_version_name + "=" + (tls_version ? tls_version : "") +
      " disables TLSv1.3; setting " + force_pqc_name + "=OFF";
  LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, error.c_str());
  *force_pqc = false;
#endif
}

static bool validate_force_pqc_tls_version(sys_var *self,
                                           const char *tls_version) {
  if (tls_force_pqc_supported_for_version(tls_version)) return false;

  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str, "ON");
  return true;
}

static const char *effective_admin_tls_version() {
  return opt_admin_ssl_configured ? opt_admin_tls_version : opt_tls_version;
}

static bool validate_force_pqc_tls_kex(sys_var *self, const char *tls_kex) {
  std::string filtered_tls_kex;
  if (!sanitize_tls_kex_list(tls_kex, true, &filtered_tls_kex)) return false;

  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str, "ON");
  return true;
}

static bool validate_use_pqc_sign(sys_var *self, bool use_pqc_sign) {
  if (!use_pqc_sign || tls_pqc_sign_supported()) return false;

  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str, "ON");
  return true;
}

static bool check_force_pqc(sys_var *self, THD *, set_var *var) {
  if (!var->save_result.ulonglong_value) return false;

  const AutoRLock lock(&lock_ssl_ctx);
  return validate_force_pqc_tls_version(self, opt_tls_version) ||
         validate_force_pqc_tls_kex(self, opt_tls_kex);
}

static bool check_admin_force_pqc(sys_var *self, THD *, set_var *var) {
  if (!var->save_result.ulonglong_value) return false;

  const AutoRLock main_lock(&lock_ssl_ctx);
  const AutoRLock admin_lock(&lock_admin_ssl_ctx);
  return validate_force_pqc_tls_version(self, effective_admin_tls_version()) ||
         validate_force_pqc_tls_kex(self, opt_admin_tls_kex);
}

static bool check_use_pqc_sign(sys_var *self, THD *, set_var *var) {
  return validate_use_pqc_sign(self, var->save_result.ulonglong_value);
}

static bool tls_force_pqc_enabled_in_context(
    Ssl_acceptor_context_container *container, bool *enabled) {
  if (container == nullptr) return false;

  Lock_and_access_ssl_acceptor_context context(container);
  if (!context.have_ssl()) return false;

  const auto *ssl_acceptor = static_cast<struct st_VioSSLFd *>(context);
  if (ssl_acceptor == nullptr) return false;

  *enabled = ssl_acceptor->tls_force_pqc;
  return true;
}

static bool tls_use_pqc_sign_enabled_in_context(
    Ssl_acceptor_context_container *container, bool *enabled) {
  if (container == nullptr) return false;

  Lock_and_access_ssl_acceptor_context context(container);
  if (!context.have_ssl()) return false;

  const auto *ssl_acceptor = static_cast<struct st_VioSSLFd *>(context);
  if (ssl_acceptor == nullptr) return false;

  *enabled = ssl_acceptor->tls_use_pqc_sign;
  return true;
}

static bool tls_kex_in_context(Ssl_acceptor_context_container *container,
                               std::string *tls_kex) {
  if (container == nullptr) return false;

  Lock_and_access_ssl_acceptor_context context(container);
  if (!context.have_ssl()) return false;

  auto *ssl_acceptor = static_cast<struct st_VioSSLFd *>(context);
  if (ssl_acceptor == nullptr) return false;

  tls_kex->assign(ssl_acceptor->tls_kex ? ssl_acceptor->tls_kex : "");
  return true;
}

static void set_tls_option_string(char **option_value,
                                  const std::string &value) {
  my_free(*option_value);
  *option_value = my_strdup(PSI_NOT_INSTRUMENTED, value.c_str(), MYF(MY_WME));
}
static void log_tls_channel_restore_error(const std::string &channel,
                                          enum enum_ssl_init_error error) {
  if (error == SSL_INITERR_NOERROR) return;

  LogErr(WARNING_LEVEL, ER_WARN_TLS_CHANNEL_INITIALIZATION_ERROR,
         channel.c_str());
  LogErr(WARNING_LEVEL, ER_SSL_LIBRARY_ERROR, sslGetErrString(error));
}

static bool update_tls_acceptor_boolean_option(
    bool (*enabled_in_context)(Ssl_acceptor_context_container *, bool *),
    bool *option_value, Ssl_acceptor_context_container *container,
    const std::string &channel_name, Ssl_init_callback *callback) {
  const bool new_value = *option_value;
  bool old_value = !new_value;
  const bool have_old_value = enabled_in_context(container, &old_value);

  if (have_old_value && old_value == new_value) return false;
  if (container == nullptr) return false;

  enum enum_ssl_init_error error = SSL_INITERR_NOERROR;
  TLS_channel::singleton_flush(container, channel_name, callback, &error, false,
                               false);
  if (error == SSL_INITERR_NOERROR) return false;

  *option_value = old_value;

  enum enum_ssl_init_error restore_error = SSL_INITERR_NOERROR;
  TLS_channel::singleton_flush(container, channel_name, callback,
                               &restore_error, false, false);
  log_tls_channel_restore_error(channel_name, restore_error);

  my_error(ER_DA_SSL_LIBRARY_ERROR, MYF(0), sslGetErrString(error));
  return true;
}

static bool update_tls_acceptor_string_option(
    bool (*value_in_context)(Ssl_acceptor_context_container *, std::string *),
    char **option_value, Ssl_acceptor_context_container *container,
    const std::string &channel_name, Ssl_init_callback *callback) {
  const std::string new_value = *option_value ? *option_value : "";
  std::string old_value;
  const bool have_old_value = value_in_context(container, &old_value);

  if (have_old_value && old_value == new_value) return false;
  if (container == nullptr) return false;

  enum enum_ssl_init_error error = SSL_INITERR_NOERROR;
  TLS_channel::singleton_flush(container, channel_name, callback, &error, false,
                               false);
  if (error == SSL_INITERR_NOERROR) return false;

  set_tls_option_string(option_value, old_value);

  enum enum_ssl_init_error restore_error = SSL_INITERR_NOERROR;
  TLS_channel::singleton_flush(container, channel_name, callback,
                               &restore_error, false, false);
  log_tls_channel_restore_error(channel_name, restore_error);

  my_error(ER_DA_SSL_LIBRARY_ERROR, MYF(0), sslGetErrString(error));
  return true;
}

static bool update_force_pqc(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_boolean_option(
      tls_force_pqc_enabled_in_context, &opt_force_pqc, mysql_main,
      mysql_main_channel, &server_main_callback);
}

static bool update_admin_force_pqc(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_boolean_option(
      tls_force_pqc_enabled_in_context, &opt_admin_force_pqc, mysql_admin,
      mysql_admin_channel, &server_admin_callback);
}

static bool update_use_pqc_sign(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_boolean_option(
      tls_use_pqc_sign_enabled_in_context, &opt_use_pqc_sign, mysql_main,
      mysql_main_channel, &server_main_callback);
}

static bool update_admin_use_pqc_sign(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_boolean_option(
      tls_use_pqc_sign_enabled_in_context, &opt_admin_use_pqc_sign, mysql_admin,
      mysql_admin_channel, &server_admin_callback);
}

static bool update_tls_kex(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_string_option(tls_kex_in_context, &opt_tls_kex,
                                           mysql_main, mysql_main_channel,
                                           &server_main_callback);
}

static bool update_admin_tls_kex(sys_var *, THD *, enum_var_type) {
  return update_tls_acceptor_string_option(
      tls_kex_in_context, &opt_admin_tls_kex, mysql_admin, mysql_admin_channel,
      &server_admin_callback);
}

bool admin_tls_configured(sys_var *, THD *, enum_var_type) {
  opt_admin_ssl_configured = true;
  return false;
}

bool validate_ciphers(const char *option, const char *val,
                      TLS_version version) {
  bool retval = false;
  /* If nothing is specified viosslfactories.cc will use default ciphers */
  if (!val || !*val) return retval;
  std::string ciphers{val};
  std::string haystack{};
  switch (version) {
    case TLS_version::TLSv12:
      haystack.assign(default_tls12_ciphers);
      break;
    case TLS_version::TLSv13:
      haystack.assign(default_tls13_ciphers);
      break;
    default:
      break;
  };
  auto index = ciphers.find(':');

  while (index != std::string::npos) {
    auto needle = ciphers.substr(0, index);
    needle.erase(std::remove(needle.begin(), needle.end(), ' '), needle.end());
    if ((needle[0] != '!') && (haystack.find(needle) == std::string::npos)) {
      LogErr(ERROR_LEVEL, ER_BLOCKED_CIPHER, option, needle.c_str());
      retval = true;
    }
    ciphers.erase(0, index + 1);
    index = ciphers.find(':');
  }
  ciphers.erase(std::remove(ciphers.begin(), ciphers.end(), ' '),
                ciphers.end());
  if ((ciphers[0] != '!') && (haystack.find(ciphers) == std::string::npos)) {
    LogErr(ERROR_LEVEL, ER_BLOCKED_CIPHER, option, ciphers.c_str());
    retval = true;
  }
  return retval;
}

static bool check_tls12_ciphers(sys_var *var, THD *, set_var *value) {
  return validate_ciphers(var->name.str, value->save_result.string_value.str,
                          TLS_version::TLSv12);
}

static bool check_tls13_ciphers(sys_var *var, THD *, set_var *value) {
  return validate_ciphers(var->name.str, value->save_result.string_value.str,
                          TLS_version::TLSv13);
}

static bool validate_tls_kex_setting(sys_var *self, const char *tls_kex,
                                     bool force_pqc) {
  std::string filtered_tls_kex;
  if (!sanitize_tls_kex_list(tls_kex, force_pqc, &filtered_tls_kex))
    return false;

  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), self->name.str,
           tls_kex ? tls_kex : "");
  return true;
}

static bool check_tls_kex(sys_var *var, THD *, set_var *value) {
  const AutoRLock lock(&lock_ssl_ctx);
  return validate_tls_kex_setting(var, value->save_result.string_value.str,
                                  opt_force_pqc);
}

static bool check_admin_tls_kex(sys_var *var, THD *, set_var *value) {
  const AutoRLock lock(&lock_admin_ssl_ctx);
  return validate_tls_kex_setting(var, value->save_result.string_value.str,
                                  opt_admin_force_pqc);
}

/*
  If you are adding new system variable for SSL communication, please take a
  look at do_auto_cert_generation() function in sql_authentication.cc and
  add new system variable in checks if required.
*/

/* Related to client server connection port */
static Sys_var_charptr Sys_ssl_ca("ssl_ca",
                                  "CA file in PEM format (check OpenSSL docs)",
                                  PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_ca),
                                  CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
                                  DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_capath(
    "ssl_capath", "CA directory (check OpenSSL docs)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_capath), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_tls_version(
    "tls_version", "TLS version, permitted values are TLSv1.2, TLSv1.3",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_tls_version),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_VERSION), IN_FS_CHARSET, "TLSv1.2,TLSv1.3",
    &lock_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(check_tls_version));

static Sys_var_charptr Sys_ssl_cert(
    "ssl_cert", "X509 cert in PEM format",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_cert), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_cipher(
    "ssl_cipher", "SSL cipher to use",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_cipher),
    CMD_LINE(REQUIRED_ARG, OPT_SSL_CIPHER), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(check_tls12_ciphers));

static Sys_var_charptr Sys_tls_ciphersuites(
    "tls_ciphersuites", "TLS v1.3 ciphersuite to use",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_tls_ciphersuites),
    CMD_LINE(REQUIRED_ARG, OPT_TLS_CIPHERSUITES), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_ssl_ctx, NOT_IN_BINLOG,
    ON_CHECK(check_tls13_ciphers));

static Sys_var_charptr Sys_ssl_key("ssl_key", "X509 key in PEM format",
                                   PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_key),
                                   CMD_LINE(REQUIRED_ARG), IN_FS_CHARSET,
                                   DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_crl(
    "ssl_crl", "CRL file in PEM format (check OpenSSL docs)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_crl), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_ssl_crlpath(
    "ssl_crlpath", "CRL directory (check OpenSSL docs)",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_crlpath), CMD_LINE(REQUIRED_ARG),
    IN_FS_CHARSET, DEFAULT(nullptr), &lock_ssl_ctx);

static Sys_var_charptr Sys_tls_kex("tls_kex", "TLS key exchange groups to use",
                                   GLOBAL_VAR(opt_tls_kex),
                                   CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
                                   DEFAULT(""), NO_MUTEX_GUARD, NOT_IN_BINLOG,
                                   ON_CHECK(check_tls_kex),
                                   ON_UPDATE(update_tls_kex));

static Sys_var_bool Sys_force_pqc(
    "force_pqc",
    "If set to TRUE, only accept TLS connections negotiated with a PQC "
    "compatible key exchange group.",
    GLOBAL_VAR(opt_force_pqc), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_force_pqc),
    ON_UPDATE(update_force_pqc));

static Sys_var_bool Sys_use_pqc_sign(
    "use_pqc_sign",
    "If set to FALSE, advertise only classical TLS handshake signature "
    "algorithms on the main connection channel.",
    GLOBAL_VAR(opt_use_pqc_sign), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_use_pqc_sign),
    ON_UPDATE(update_use_pqc_sign));

static Sys_var_charptr Sys_admin_tls_kex(
    "admin_tls_kex", "TLS key exchange groups to use for --admin-port",
    GLOBAL_VAR(opt_admin_tls_kex), CMD_LINE(REQUIRED_ARG), IN_SYSTEM_CHARSET,
    DEFAULT(""), NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_admin_tls_kex),
    ON_UPDATE(update_admin_tls_kex));

static Sys_var_bool Sys_admin_force_pqc(
    "admin_force_pqc",
    "If set to TRUE, only accept admin TLS connections negotiated with a "
    "PQC compatible key exchange group.",
    GLOBAL_VAR(opt_admin_force_pqc), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_admin_force_pqc),
    ON_UPDATE(update_admin_force_pqc));

static Sys_var_bool Sys_admin_use_pqc_sign(
    "admin_use_pqc_sign",
    "If set to FALSE, advertise only classical TLS handshake signature "
    "algorithms on the admin connection channel.",
    GLOBAL_VAR(opt_admin_use_pqc_sign), CMD_LINE(OPT_ARG), DEFAULT(false),
    NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(check_use_pqc_sign),
    ON_UPDATE(update_admin_use_pqc_sign));

#define PFS_TRAILING_PROPERTIES                                         \
  NO_MUTEX_GUARD, NOT_IN_BINLOG, ON_CHECK(nullptr), ON_UPDATE(nullptr), \
      nullptr, sys_var::PARSE_EARLY

static Sys_var_bool Sys_var_opt_ssl_session_cache_mode(
    "ssl_session_cache_mode", "Is TLS session cache enabled or not",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_session_cache_mode),
    CMD_LINE(OPT_ARG), DEFAULT(true), PFS_TRAILING_PROPERTIES);

/* 86400 is 1 day in seconds */
static Sys_var_long Sys_var_opt_ssl_session_cache_timeout(
    "ssl_session_cache_timeout",
    "The timeout to expire sessions in the TLS session cache",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_ssl_session_cache_timeout),
    CMD_LINE(REQUIRED_ARG), VALID_RANGE(0, 86400), DEFAULT(300), BLOCK_SIZE(1),
    PFS_TRAILING_PROPERTIES);

/* Related to admin connection port */
static Sys_var_charptr Sys_admin_ssl_ca(
    "admin_ssl_ca",
    "CA file in PEM format (check OpenSSL docs) for "
    "--admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_ca),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CA), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_capath(
    "admin_ssl_capath", "CA directory (check OpenSSL docs) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_capath),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CAPATH), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_tls_version(
    "admin_tls_version",
    "TLS version for --admin-port, permitted values are TLSv1.2, TLSv1.3",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_tls_version),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_TLS_VERSION), IN_FS_CHARSET,
    "TLSv1.2,TLSv1.3", &lock_admin_ssl_ctx, NOT_IN_BINLOG,
    ON_CHECK(check_admin_tls_version), ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_cert(
    "admin_ssl_cert", "X509 cert in PEM format for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_cert),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CERT), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_cipher(
    "admin_ssl_cipher", "SSL cipher to use for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_cipher),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CIPHER), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_admin_ssl_ctx, NOT_IN_BINLOG,
    ON_CHECK(check_tls12_ciphers), ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_tls_ciphersuites(
    "admin_tls_ciphersuites", "TLS v1.3 ciphersuite to use for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_tls_ciphersuites),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_TLS_CIPHERSUITES), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_admin_ssl_ctx, NOT_IN_BINLOG,
    ON_CHECK(check_tls13_ciphers), ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_key(
    "admin_ssl_key", "X509 key in PEM format for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_key),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_KEY), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_crl(
    "admin_ssl_crl",
    "CRL file in PEM format (check OpenSSL docs) for "
    "--admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_crl),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CRL), IN_FS_CHARSET, DEFAULT(nullptr),
    &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

static Sys_var_charptr Sys_admin_ssl_crlpath(
    "admin_ssl_crlpath", "CRL directory (check OpenSSL docs) for --admin-port",
    PERSIST_AS_READONLY GLOBAL_VAR(opt_admin_ssl_crlpath),
    CMD_LINE(REQUIRED_ARG, OPT_ADMIN_SSL_CRLPATH), IN_FS_CHARSET,
    DEFAULT(nullptr), &lock_admin_ssl_ctx, NOT_IN_BINLOG, ON_CHECK(nullptr),
    ON_UPDATE(admin_tls_configured));

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

    issuer = X509_NAME_oneline(
        const_cast<X509_NAME *>(X509_get_issuer_name(ca_cert)), nullptr, 0);
    subject = X509_NAME_oneline(
        const_cast<X509_NAME *>(X509_get_subject_name(ca_cert)), nullptr, 0);

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
    OptionalString *tls_kex, bool *force_pqc, bool *use_pqc_sign,
    bool *session_cache_mode, long *session_cache_timeout) {
  const AutoRLock lock(&lock_ssl_ctx);
  if (ca) ca->assign(opt_ssl_ca);
  if (capath) capath->assign(opt_ssl_capath);
  if (version) version->assign(opt_tls_version);
  if (cert) cert->assign(opt_ssl_cert);
  if (cipher) cipher->assign(opt_ssl_cipher);
  if (ciphersuites) ciphersuites->assign(opt_tls_ciphersuites);
  if (key) key->assign(opt_ssl_key);
  if (crl) crl->assign(opt_ssl_crl);
  if (crl_path) crl_path->assign(opt_ssl_crlpath);
  if (tls_kex) tls_kex->assign(opt_tls_kex);
  if (force_pqc) *force_pqc = opt_force_pqc;
  if (use_pqc_sign) *use_pqc_sign = opt_use_pqc_sign;
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
  const AutoRLock lock(&lock_ssl_ctx);
  auto_detection_status = auto_detect_ssl();
  if (auto_detection_status == SSL_ARTIFACTS_AUTO_DETECTED)
    LogErr(INFORMATION_LEVEL, ER_SSL_TRYING_DATADIR_DEFAULTS,
           DEFAULT_SSL_CA_CERT, DEFAULT_SSL_SERVER_CERT,
           DEFAULT_SSL_SERVER_KEY);
  return !do_auto_cert_generation(auto_detection_status, &opt_ssl_ca,
                                  &opt_ssl_key, &opt_ssl_cert);
}

void Ssl_init_callback_server_main::adjust_startup_options() {
  const AutoRLock lock(&lock_ssl_ctx);
  adjust_startup_force_pqc_for_tls_version("force_pqc", "tls_version",
                                           opt_tls_version, &opt_force_pqc);
}

bool Ssl_init_callback_server_main::warn_self_signed_ca() {
  const AutoRLock lock(&lock_ssl_ctx);
  return warn_self_signed_ca_certs(opt_ssl_ca, opt_ssl_capath);
}

/* Admin connection port callbacks */

void Ssl_init_callback_server_admin::read_parameters(
    OptionalString *ca, OptionalString *capath, OptionalString *version,
    OptionalString *cert, OptionalString *cipher, OptionalString *ciphersuites,
    OptionalString *key, OptionalString *crl, OptionalString *crl_path,
    OptionalString *tls_kex, bool *force_pqc, bool *use_pqc_sign,
    bool *session_cache_mode, long *session_cache_timeout) {
  const AutoRLock main_lock(&lock_ssl_ctx);
  const AutoRLock admin_lock(&lock_admin_ssl_ctx);
  if (ca) ca->assign(opt_admin_ssl_ca);
  if (capath) capath->assign(opt_admin_ssl_capath);
  if (version) version->assign(effective_admin_tls_version());
  if (cert) cert->assign(opt_admin_ssl_cert);
  if (cipher) cipher->assign(opt_admin_ssl_cipher);
  if (ciphersuites) ciphersuites->assign(opt_admin_tls_ciphersuites);
  if (key) key->assign(opt_admin_ssl_key);
  if (crl) crl->assign(opt_admin_ssl_crl);
  if (crl_path) crl_path->assign(opt_admin_ssl_crlpath);
  if (tls_kex) tls_kex->assign(opt_admin_tls_kex);
  if (force_pqc) *force_pqc = opt_admin_force_pqc;
  if (use_pqc_sign) *use_pqc_sign = opt_admin_use_pqc_sign;
  if (session_cache_mode) *session_cache_mode = opt_ssl_session_cache_mode;
  if (session_cache_timeout)
    *session_cache_timeout = opt_ssl_session_cache_timeout;

  g_admin_ssl_configured = opt_admin_ssl_configured;
}

void Ssl_init_callback_server_admin::adjust_startup_options() {
  const AutoRLock main_lock(&lock_ssl_ctx);
  const AutoRLock admin_lock(&lock_admin_ssl_ctx);
  adjust_startup_force_pqc_for_tls_version(
      "admin_force_pqc",
      opt_admin_ssl_configured ? "admin_tls_version" : "tls_version",
      effective_admin_tls_version(), &opt_admin_force_pqc);
}

bool Ssl_init_callback_server_admin::warn_self_signed_ca() {
  const AutoRLock lock(&lock_ssl_ctx);
  return warn_self_signed_ca_certs(opt_admin_ssl_ca, opt_admin_ssl_capath);
}

Ssl_init_callback_server_main server_main_callback;
Ssl_init_callback_server_admin server_admin_callback;
