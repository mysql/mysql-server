/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client_priv.h"
#include <vector>
#include "mysql_connection_options.h"
#include "sslopt-vars.h"
#include "instance_callback.h"

using namespace Mysql::Tools::Base::Options;

void Mysql_connection_options::Ssl_options::create_options()
{
  Instance_callback<void, char*, Mysql_connection_options::Ssl_options>
    callback(this, &Mysql_connection_options::Ssl_options::mode_option_callback);

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  this->create_new_option(&this->m_ssl_mode_string, "ssl-mode",
      "SSL connection mode.")
#ifdef MYSQL_CLIENT
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(
        this, &Mysql_connection_options::Ssl_options::mode_option_callback))
#endif
    ;
  this->create_new_option(&::opt_ssl_ca, "ssl-ca", "CA file in PEM format.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(
        this, &Mysql_connection_options::Ssl_options::ca_option_callback));
  this->create_new_option(&::opt_ssl_capath, "ssl-capath", "CA directory.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(
        this, &Mysql_connection_options::Ssl_options::ca_option_callback));
  this->create_new_option(&::opt_ssl_cert, "ssl-cert",
      "X509 cert in PEM format.");
  this->create_new_option(&::opt_ssl_cipher, "ssl-cipher",
      "SSL cipher to use.");
  this->create_new_option(&::opt_ssl_key, "ssl-key",
      "X509 key in PEM format.");
  this->create_new_option(&::opt_ssl_crl, "ssl-crl",
      "Certificate revocation list.");
  this->create_new_option(&::opt_ssl_crlpath, "ssl-crlpath",
      "Certificate revocation list path.");
  this->create_new_option(&::opt_tls_version, "tls-version",
      "TLS version to use.");
#endif /* HAVE_OPENSSL */
}


void Mysql_connection_options::Ssl_options::ca_option_callback(
  char *argument MY_ATTRIBUTE((unused)))
{
  if (!ssl_mode_set_explicitly)
    ::opt_ssl_mode= SSL_MODE_VERIFY_CA;
}


void Mysql_connection_options::Ssl_options::mode_option_callback(
  char *argument)
{
  ::opt_ssl_mode= find_type_or_exit(argument, &ssl_mode_typelib, "ssl-mode");
  ssl_mode_set_explicitly= true;
}


void Mysql_connection_options::Ssl_options::apply_for_connection(
  MYSQL* connection)
{
  SSL_SET_OPTIONS(connection);
}
