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

#ifndef SSLOPT_VARS_INCLUDED
#define SSLOPT_VARS_INCLUDED

/**
  @file include/sslopt-vars.h
*/

#include <stdio.h>

#include "m_string.h"
#include "my_inttypes.h"
#include "mysql.h"
#include "typelib.h"

#if defined(HAVE_OPENSSL)

#ifdef MYSQL_SERVER
#error This header is supposed to be used only in the client
#endif 

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql.h"
#include "typelib.h"

const char *ssl_mode_names_lib[] =
  {"DISABLED", "PREFERRED", "REQUIRED", "VERIFY_CA", "VERIFY_IDENTITY",
   NullS };
TYPELIB ssl_mode_typelib = {array_elements(ssl_mode_names_lib) - 1, "",
                            ssl_mode_names_lib, NULL};

static uint opt_ssl_mode     = SSL_MODE_PREFERRED;
static char *opt_ssl_ca      = 0;
static char *opt_ssl_capath  = 0;
static char *opt_ssl_cert    = 0;
static char *opt_ssl_cipher  = 0;
static char *opt_ssl_key     = 0;
static char *opt_ssl_crl     = 0;
static char *opt_ssl_crlpath = 0;
static char *opt_tls_version = 0;
static bool ssl_mode_set_explicitly= FALSE;

static void set_client_ssl_options(MYSQL *mysql)
{
  /*
    Print a warning if explicitly defined combination of --ssl-mode other than
    VERIFY_CA or VERIFY_IDENTITY with explicit --ssl-ca or --ssl-capath values.
  */
  if (ssl_mode_set_explicitly &&
      opt_ssl_mode < SSL_MODE_VERIFY_CA &&
      (opt_ssl_ca || opt_ssl_capath))
  {
    fprintf(stderr, "WARNING: no verification of server certificate will be done. "
                    "Use --ssl-mode=VERIFY_CA or VERIFY_IDENTITY.\n");
  }
   
  /* Set SSL parameters: key, cert, ca, capath, cipher, clr, clrpath. */
  if (opt_ssl_mode >= SSL_MODE_VERIFY_CA)
    mysql_ssl_set(mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
                  opt_ssl_capath, opt_ssl_cipher);
  else
    mysql_ssl_set(mysql, opt_ssl_key, opt_ssl_cert, NULL,
                  NULL, opt_ssl_cipher);
  mysql_options(mysql, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
  mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
  mysql_options(mysql, MYSQL_OPT_TLS_VERSION, opt_tls_version);
  mysql_options(mysql, MYSQL_OPT_SSL_MODE, &opt_ssl_mode);
}
 
#define SSL_SET_OPTIONS(mysql) set_client_ssl_options(mysql);
#else
#define SSL_SET_OPTIONS(mysql) do { } while(0)
#endif
#endif /* SSLOPT_VARS_INCLUDED */
