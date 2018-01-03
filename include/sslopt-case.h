/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file include/sslopt-case.h
*/

#if defined(HAVE_OPENSSL)

#ifdef MYSQL_SERVER
#error This header is supposed to be used only in the client
#endif

    case OPT_SSL_MODE:
      opt_ssl_mode= find_type_or_exit(argument, &ssl_mode_typelib,
                                      opt->name);
      ssl_mode_set_explicitly= TRUE;
      break;
    case OPT_SSL_CA:
    case OPT_SSL_CAPATH:
      /* Don't change ssl-mode if set explicitly. */ 
      if (!ssl_mode_set_explicitly)
        opt_ssl_mode= SSL_MODE_VERIFY_CA;
      break;
    case OPT_SSL_KEY:
    case OPT_SSL_CERT:
    case OPT_SSL_CIPHER:
    case OPT_SSL_CRL:
    case OPT_SSL_CRLPATH:
    case OPT_TLS_VERSION:
#ifdef HAVE_YASSL
      /* crl has no effect in yaSSL. */
      opt_ssl_crl= NULL;
      opt_ssl_crlpath= NULL;
#endif /* HAVE_YASSL */
      break;
#endif /* HAVE_OPENSSL */
