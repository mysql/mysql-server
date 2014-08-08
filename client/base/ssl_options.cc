/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
    callback(this, &Mysql_connection_options::Ssl_options::option_callback);

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  this->create_new_option((bool*)&::opt_use_ssl, "ssl",
      "If set to ON, this option enforces that SSL is established before "
      "client attempts to authenticate to the server. To disable client SSL "
      "capabilities use --ssl=OFF.")
#ifdef MYSQL_CLIENT
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(
        this, &Mysql_connection_options::Ssl_options::use_ssl_option_callback))
#endif
    ;
  this->create_new_option(&::opt_ssl_ca, "ssl-ca", "CA file in PEM format.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_capath, "ssl-capath", "CA directory.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_cert, "ssl-cert",
      "X509 cert in PEM format.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_cipher, "ssl-cipher",
      "SSL cipher to use.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_key, "ssl-key",
      "X509 key in PEM format.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_crl, "ssl-crl",
      "Certificate revocation list.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));
  this->create_new_option(&::opt_ssl_crlpath, "ssl-crlpath",
      "Certificate revocation list path.")
    ->add_callback(new Instance_callback<void, char*,
      Mysql_connection_options::Ssl_options>(callback));

#ifdef MYSQL_CLIENT
  this->create_new_option((bool*)&::opt_ssl_verify_server_cert,
    "ssl-verify-server-cert",
    "Verify server's \"Common Name\" in its cert against hostname used "
    "when connecting. This option is disabled by default.");
#endif
#endif /* HAVE_OPENSSL */
}


void Mysql_connection_options::Ssl_options::option_callback(
  char *argument __attribute__((unused)))
{
  /*
    Enable use of SSL if we are using any ssl option
    One can disable SSL later by using --skip-ssl or --ssl=0
  */
  ::opt_use_ssl= TRUE;
}


void Mysql_connection_options::Ssl_options::use_ssl_option_callback(
  char *argument __attribute__((unused)))
{
  ::opt_ssl_enforce= ::opt_use_ssl;
}


void Mysql_connection_options::Ssl_options::apply_for_connection(MYSQL* connection)
{
  SSL_SET_OPTIONS(connection);
}
