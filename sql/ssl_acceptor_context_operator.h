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

#ifndef SSL_ACCEPTOR_CONTEXT_OPERATOR
#define SSL_ACCEPTOR_CONTEXT_OPERATOR

#include <my_rcu_lock.h>                   /* MyRcuLock */
#include "sql/ssl_acceptor_context_data.h" /** Ssl_acceptor_context_data */

/* Types of supported contexts */
enum class Ssl_acceptor_context_type {
  context_server_main = 0,
  context_server_admin,
  context_last
};

class Lock_and_access_ssl_acceptor_context;
class TLS_channel;

/** TLS context access protector */
class Ssl_acceptor_context_container {
 protected:
  Ssl_acceptor_context_container(Ssl_acceptor_context_data *data);
  ~Ssl_acceptor_context_container();
  void switch_data(Ssl_acceptor_context_data *new_data);

  using Ssl_acceptor_context_data_lock = MyRcuLock<Ssl_acceptor_context_data>;

  Ssl_acceptor_context_data_lock *lock_;

  /* F.R.I.E.N.D.S. */
  friend class Lock_and_access_ssl_acceptor_context;
  friend class TLS_channel;
};

extern Ssl_acceptor_context_container *mysql_main;
extern Ssl_acceptor_context_container *mysql_admin;

/** TLS context manager */
class TLS_channel {
 public:
  /**
  Initialize the single instance of the acceptor

  @param [out] out          Object initialized by the function
  @param [in]  channel      Name of the channel
  @param [in]  use_ssl_arg  Pass false if you don't want the actual
                            SSL context created
                            (as in when SSL is initially disabled)
  @param [in]  callbacks    Handle to the initialization callback object
  @param [in]  db_init      Whether database is being initialized or not

  @returns Initialization status
    @retval true failure to init
    @retval false initialized ok
*/
  static bool singleton_init(Ssl_acceptor_context_container **out,
                             std::string channel, bool use_ssl_arg,
                             Ssl_init_callback *callbacks, bool db_init);

  /**
    De-initialize the single instance of the acceptor

    @param [in] container TLS acceptor context object
  */
  static void singleton_deinit(Ssl_acceptor_context_container *container);
  /**
    Re-initialize the single instance of the acceptor

    @param [in,out] container TLS acceptor context object
    @param [in]     channel   Name of the channel
    @param [in]     callbacks Handle to the initialization callback object
    @param [out]    error     SSL Error information
    @param [in]     force     Activate the SSL settings even if this will lead
                              to disabling SSL
  */
  static void singleton_flush(Ssl_acceptor_context_container *container,
                              std::string channel, Ssl_init_callback *callbacks,
                              enum enum_ssl_init_error *error, bool force);
};

using Ssl_acceptor_context_data_lock = MyRcuLock<Ssl_acceptor_context_data>;

/** TLS context access wrapper for ease of use */
class Lock_and_access_ssl_acceptor_context {
 public:
  Lock_and_access_ssl_acceptor_context(Ssl_acceptor_context_container *context)
      : read_lock_(context->lock_) {}
  ~Lock_and_access_ssl_acceptor_context() = default;

  /** Access protected @ref Ssl_acceptor_context_data */
  operator const Ssl_acceptor_context_data *() {
    const Ssl_acceptor_context_data *c = read_lock_;
    return c;
  }

  /**
    Access to the SSL_CTX from the protected @ref Ssl_acceptor_context_data
  */
  operator SSL_CTX *() {
    const Ssl_acceptor_context_data *c = read_lock_;
    return c->ssl_acceptor_fd_->ssl_context;
  }

  /**
    Access to the SSL from the protected @ref Ssl_acceptor_context_data
  */
  operator SSL *() {
    const Ssl_acceptor_context_data *c = read_lock_;
    return c->acceptor_;
  }

  /**
    Access to st_VioSSLFd from the protected @ref Ssl_acceptor_context_data
  */
  operator struct st_VioSSLFd *() {
    const Ssl_acceptor_context_data *c = read_lock_;
    return c->ssl_acceptor_fd_;
  }

  /**
    Fetch given property from underlying TLS context

    @param [in] property_type Property to be fetched

   @returns Value of property for given context. Empty in case of failure.
  */
  std::string show_property(Ssl_acceptor_context_property_type property_type);

  /**
    Fetch channel name

    @returns Name of underlying channel
  */
  std::string channel_name();

  /**
    TLS context validity

    @returns Validity of TLS context
      @retval true  Valid
      @retval false Invalid
  */
  bool have_ssl();

 private:
  /** Read lock over TLS context */
  Ssl_acceptor_context_data_lock::ReadLock read_lock_;
};

bool have_ssl();

#endif  // SSL_ACCEPTOR_CONTEXT_OPERATOR
