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

#ifndef SSL_INIT_CALLBACK_INCLUDED
#define SSL_INIT_CALLBACK_INCLUDED

#include <atomic>
#include <string>

#include <sql/auth/auth_common.h> /* ssl_artifacts_status */

/** The runtime value of whether admin TLS used different config or not */
extern std::atomic_bool g_admin_ssl_configured;
/**
 The configure time value of whether admin TLS used different config or not.
 The value for this is determined during system variable update.
 True means that the ADMIN channel is using its own TLS configuration.
 False means that the ADMIN channel is reusing the main channel's
 TLS configuration.
 To put this value into effect (and update @ref g_admin_ssl_configured)
 one needs to execute the "ALTER INSTANCE RELOAD TLS" SQL command.
*/
extern bool opt_admin_ssl_configured;

extern std::string mysql_main_channel;
extern std::string mysql_admin_channel;
extern bool opt_tls_certificates_enforced_validation;

/** helper class to deal with optionally empty strings */
class OptionalString {
 public:
  OptionalString() : value_(), empty_(true) {}
  OptionalString(const char *s) : value_(s ? s : ""), empty_(!s) {}
  ~OptionalString() = default;
  OptionalString(const OptionalString &) = default;

  const char *c_str() const { return empty_ ? nullptr : value_.c_str(); }
  OptionalString &assign(const char *s) {
    value_.assign(s ? s : "");
    empty_ = !s;
    return *this;
  }

 private:
  std::string value_;
  bool empty_;
};

/* Class to encasulate callbacks for init/reinit */
class Ssl_init_callback {
 public:
  virtual void read_parameters(OptionalString *ca, OptionalString *capath,
                               OptionalString *version, OptionalString *cert,
                               OptionalString *cipher,
                               OptionalString *ciphersuites,
                               OptionalString *key, OptionalString *crl,
                               OptionalString *crl_path,
                               bool *session_cache_mode,
                               long *session_cache_timeout) = 0;

  virtual bool provision_certs() = 0;

  virtual bool warn_self_signed_ca() = 0;

  virtual ~Ssl_init_callback() = default;
};

/**
  Class to encasulate callbacks for init/reinit
  for client server connection port
*/
class Ssl_init_callback_server_main final : public Ssl_init_callback {
 public:
  void read_parameters(OptionalString *ca, OptionalString *capath,
                       OptionalString *version, OptionalString *cert,
                       OptionalString *cipher, OptionalString *ciphersuites,
                       OptionalString *key, OptionalString *crl,
                       OptionalString *crl_path, bool *session_cache_mode,
                       long *session_cache_timeout) override;

  bool provision_certs() override;

  bool warn_self_signed_ca() override;

  ~Ssl_init_callback_server_main() override = default;

 private:
  ssl_artifacts_status auto_detect_ssl();
};

/**
  Class to encasulate callbacks for init/reinit
  for admin connection port
*/
class Ssl_init_callback_server_admin final : public Ssl_init_callback {
 public:
  void read_parameters(OptionalString *ca, OptionalString *capath,
                       OptionalString *version, OptionalString *cert,
                       OptionalString *cipher, OptionalString *ciphersuites,
                       OptionalString *key, OptionalString *crl,
                       OptionalString *crl_path, bool *session_cache_mode,
                       long *session_cache_timeout) override;

  bool provision_certs() override {
    /*
      No automatic provisioning. Always return
      success to fallback to system variables.
    */
    return false;
  }

  bool warn_self_signed_ca() override;

  ~Ssl_init_callback_server_admin() override = default;
};

extern Ssl_init_callback_server_main server_main_callback;
extern Ssl_init_callback_server_admin server_admin_callback;

/**
  Helper method to validate values of --tls-version and --admin-tls-version
*/
bool validate_tls_version(const char *val);

enum class TLS_version { TLSv12 = 0, TLSv13 };
/**
  Helper method to validate values of --ssl-cipher and --admin-ssl-cipher
*/
bool validate_ciphers(const char *option, const char *val, TLS_version version);
#endif  // !SSL_INIT_CALLBACK_INCLUDED
