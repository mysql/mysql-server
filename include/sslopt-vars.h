/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SSLOPT_VARS_INCLUDED
#define SSLOPT_VARS_INCLUDED

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
/* Always try to use SSL per default */
static my_bool opt_use_ssl   = TRUE;
/* Fall back on unencrypted connections per default */
static my_bool opt_ssl_enforce= FALSE;
static char *opt_ssl_ca      = 0;
static char *opt_ssl_capath  = 0;
static char *opt_ssl_cert    = 0;
static char *opt_ssl_cipher  = 0;
static char *opt_ssl_key     = 0;
static char *opt_ssl_crl     = 0;
static char *opt_ssl_crlpath = 0;
#ifndef MYSQL_CLIENT
#error This header is supposed to be used only in the client
#endif
#define SSL_SET_OPTIONS(mysql) \
  if (opt_use_ssl) \
  { \
    mysql_ssl_set(mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca, \
      opt_ssl_capath, opt_ssl_cipher); \
    mysql_options(mysql, MYSQL_OPT_SSL_CRL, opt_ssl_crl); \
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath); \
    mysql_options(mysql, MYSQL_OPT_SSL_ENFORCE, &opt_ssl_enforce); \
  } \
  mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, \
    (char*)&opt_ssl_verify_server_cert)

static my_bool opt_ssl_verify_server_cert= 0;
#else
#define SSL_SET_OPTIONS(mysql) do { } while(0)
#endif
#endif /* SSLOPT_VARS_INCLUDED */
