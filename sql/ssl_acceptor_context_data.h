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

#ifndef SSL_ACCEPTOR_CONTEXT_DATA_INCLUDED
#define SSL_ACCEPTOR_CONTEXT_DATA_INCLUDED

#include <string>

#include "my_rcu_lock.h"           /* MyRcuLock */
#include "openssl/ossl_typ.h"      /* SSL */
#include "sql/ssl_init_callback.h" /* Ssl_init_callback */
#include "violite.h"               /* st_VioSSLFd, enum_ssl_init_error */

class Ssl_acceptor_context_container;
class TLS_channel;
class Lock_and_access_ssl_acceptor_context;

/**
  Properties exposed by Ssl Acceptor context

  Note: Add new value before "last" and update
        Ssl_acceptor_context_propert_type_names.
*/
enum class Ssl_acceptor_context_property_type {
  accept_renegotiates = 0,
  accepts,
  callback_cache_hits,
  client_connects,
  connect_renegotiates,
  ctx_verify_depth,
  ctx_verify_mode,
  current_tls_ca,
  current_tls_capath,
  current_tls_cert,
  current_tls_cipher,
  current_tls_ciphersuites,
  current_tls_crl,
  current_tls_crlpath,
  current_tls_key,
  current_tls_version,
  finished_accepts,
  finished_connects,
  server_not_after,
  server_not_before,
  session_cache_hits,
  session_cache_misses,
  session_cache_mode,
  session_cache_overflows,
  session_cache_size,
  session_cache_timeouts,
  used_session_cache_entries,
  session_cache_timeout,
  last
};
/**
  Note: Add new value before "last" and update
        Ssl_acceptor_context_propert_type_names.
*/

/**
  Fetch a string representation of SSL acceptor context property

  @param [in] property_type Property type

  @returns name of the property
*/
std::string Ssl_ctx_property_name(
    Ssl_acceptor_context_property_type property_type);

/**
  Increment operator for Ssl_acceptor_context_type
  Used by iterator

  @param [in,out] property_type Current position in Ssl_acceptor_context_type

  @returns incremented value for property_type
*/
Ssl_acceptor_context_property_type &operator++(
    Ssl_acceptor_context_property_type &property_type);

/**
  Container of SSL Acceptor context data
*/
class Ssl_acceptor_context_data final {
 public:
  /**
   Ctor

   @param [in]  channel          Name of the channel
   @param [in]  use_ssl_arg      Don't bother at all to try and construct
                                 an SSL_CTX and just make an empty
                                 SslAcceptorContext. Used to pass the
                                 --ssl/--admin-ssl options at startup.
   @param [in]  callbacks        TLS context initialization callbacks
                                 to get values of various options and
                                 perform validation
   @param [in]  report_ssl_error Report any SSL errors resulting from trying
                                 to initialize the SSL_CTX to error log
   @param [out] out_error        An optional slot to return SSL_CTX
                                 initialization error information
  */
  Ssl_acceptor_context_data(std::string channel, bool use_ssl_arg,
                            Ssl_init_callback *callbacks,
                            bool report_ssl_error = true,
                            enum enum_ssl_init_error *out_error = nullptr);

  /** Destructor */
  ~Ssl_acceptor_context_data();

 protected:
  /* Disable copy/assignment */
  Ssl_acceptor_context_data(const Ssl_acceptor_context_data &) = delete;
  Ssl_acceptor_context_data operator=(const Ssl_acceptor_context_data &) =
      delete;

  /* Disable move constructs */
  Ssl_acceptor_context_data(Ssl_acceptor_context_data &&) = delete;
  Ssl_acceptor_context_data operator=(Ssl_acceptor_context_data &&) = delete;

  /**
    Fetch given property from underlying TLS context

    @param [in] property_type Property to be fetched

    @returns Value of property for given context. Empty in case of failure.
  */
  std::string show_property(
      Ssl_acceptor_context_property_type property_type) const;

  /** TLS context validity */
  bool have_ssl() const { return ssl_acceptor_fd_ != nullptr; }

  /** Get channel name */
  const char *channel_name() const { return channel_.c_str(); }

  /** Get Acceptor context */
  operator struct st_VioSSLFd *() { return ssl_acceptor_fd_; }

  /** Get SSL handle */
  operator SSL *() { return acceptor_; }

  /** Get current CA */
  const char *current_ca() const { return current_ca_.c_str(); }

  /** Get current CA Path */
  const char *current_capath() const { return current_capath_.c_str(); }

  /** Get current Certificate */
  const char *current_cert() const { return current_cert_.c_str(); }

  /** Get current Key */
  const char *current_key() const { return current_key_.c_str(); }

  /** Get current CRL certificate */
  const char *current_crl() const { return current_crl_.c_str(); }

  /** Get current CRL Path */
  const char *current_crlpath() const { return current_crlpath_.c_str(); }

  /** Get current TLS version */
  const char *current_version() const { return current_version_.c_str(); }

  /** Get current TLSv1.2 ciphers */
  const char *current_cipher() const { return current_cipher_.c_str(); }

  /** Get current TLSv1.3 ciphers */
  const char *current_ciphersuites() const {
    return current_ciphersuites_.c_str();
  }

 private:
  /** Channel name */
  std::string channel_;

  /** SSL_CTX barerer */
  struct st_VioSSLFd *ssl_acceptor_fd_;

  /**
    An SSL for @ref ssl_acceptor_fd_ to allow access to parameters not in
    SSL_CTX to be available even if the current connection is not
    encrypted.
  */
  SSL *acceptor_;

  /**
    Copies of the current effective values for quick return via the
    status vars
  */
  OptionalString current_ca_, current_capath_, current_version_, current_cert_,
      current_cipher_, current_ciphersuites_, current_key_, current_crl_,
      current_crlpath_;
  long current_tls_session_cache_timeout_;
  bool current_tls_session_cache_mode_;

  /* F.R.I.E.N.D.S. */
  friend class Ssl_acceptor_context_container;
  friend class TLS_channel;
  friend class Lock_and_access_ssl_acceptor_context;
};

#endif  // SSL_ACCEPTOR_CONTEXT_DATA_INCLUDED
