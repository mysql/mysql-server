/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#ifndef SSLOPT_VARS_INCLUDED
#define SSLOPT_VARS_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <functional>

#ifdef MYSQL_SERVER
#error This header is supposed to be used only in the client
#endif

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql.h"
#include "nulls.h"
#include "template_utils.h"
#include "typelib.h"

const char *ssl_mode_names_lib[] = {"DISABLED",  "PREFERRED",       "REQUIRED",
                                    "VERIFY_CA", "VERIFY_IDENTITY", NullS};
TYPELIB ssl_mode_typelib = {array_elements(ssl_mode_names_lib) - 1, "",
                            ssl_mode_names_lib, nullptr};

const char *ssl_fips_mode_names_lib[] = {"OFF", "ON", "STRICT", NullS};
TYPELIB ssl_fips_mode_typelib = {array_elements(ssl_fips_mode_names_lib) - 1,
                                 "", ssl_fips_mode_names_lib, nullptr};

static uint opt_ssl_mode = SSL_MODE_PREFERRED;
static char *opt_ssl_ca = nullptr;
static char *opt_ssl_capath = nullptr;
static char *opt_ssl_cert = nullptr;
static char *opt_ssl_cipher = nullptr;
static char *opt_tls_ciphersuites = nullptr;
static char *opt_ssl_key = nullptr;
static char *opt_ssl_crl = nullptr;
static char *opt_ssl_crlpath = nullptr;
static char *opt_tls_version = nullptr;
static ulong opt_ssl_fips_mode = SSL_FIPS_MODE_OFF;
static bool ssl_mode_set_explicitly = false;
static char *opt_ssl_session_data = nullptr;
static bool opt_ssl_session_data_continue_on_failed_reuse = false;
static char *opt_tls_sni_servername = nullptr;

static inline int set_client_ssl_options(MYSQL *mysql) {
  /*
    Print a warning if explicitly defined combination of --ssl-mode other than
    VERIFY_CA or VERIFY_IDENTITY with explicit --ssl-ca or --ssl-capath values.
  */
  if (ssl_mode_set_explicitly && opt_ssl_mode < SSL_MODE_VERIFY_CA &&
      (opt_ssl_ca || opt_ssl_capath)) {
    fprintf(stderr,
            "WARNING: no verification of server certificate will be done. "
            "Use --ssl-mode=VERIFY_CA or VERIFY_IDENTITY.\n");
  }

  /* Set SSL parameters: key, cert, ca, capath, cipher, clr, clrpath. */
  mysql_options(mysql, MYSQL_OPT_SSL_KEY, opt_ssl_key);
  mysql_options(mysql, MYSQL_OPT_SSL_CERT, opt_ssl_cert);
  mysql_options(mysql, MYSQL_OPT_SSL_CIPHER, opt_ssl_cipher);
  if (opt_ssl_mode >= SSL_MODE_VERIFY_CA) {
    mysql_options(mysql, MYSQL_OPT_SSL_CA, opt_ssl_ca);
    mysql_options(mysql, MYSQL_OPT_SSL_CAPATH, opt_ssl_capath);
  } else {
    mysql_options(mysql, MYSQL_OPT_SSL_CA, nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CAPATH, nullptr);
  }
  mysql_options(mysql, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
  mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
  mysql_options(mysql, MYSQL_OPT_TLS_VERSION, opt_tls_version);
  mysql_options(mysql, MYSQL_OPT_SSL_MODE, &opt_ssl_mode);
  if (opt_ssl_fips_mode > 0) {
    mysql_options(mysql, MYSQL_OPT_SSL_FIPS_MODE, &opt_ssl_fips_mode);
    if (mysql_errno(mysql) == CR_SSL_FIPS_MODE_ERR) return 1;
  }
  mysql_options(mysql, MYSQL_OPT_TLS_CIPHERSUITES, opt_tls_ciphersuites);
  mysql_options(mysql, MYSQL_OPT_TLS_SNI_SERVERNAME, opt_tls_sni_servername);
  if (opt_ssl_session_data) {
    FILE *fi = fopen(opt_ssl_session_data, "rb");
    char buff[4096], *bufptr = &buff[0];
    size_t read = 0;

    if (!fi) {
      fprintf(stderr, "Error: Can't open the ssl session data file.\n");
      return 1;
    }
    long file_length = sizeof(buff) - 1;
    if (0 == fseek(fi, 0, SEEK_END)) {
      file_length = ftell(fi);
      if (file_length > 0)
        file_length = std::min(file_length, 65536L);
      else
        file_length = sizeof(buff) - 1;
      fseek(fi, 0, SEEK_SET);
    }
    if (file_length > (long)(sizeof(buff) - 1)) {
      bufptr = (char *)malloc(file_length + 1);
      if (bufptr)
        bufptr[file_length] = 0;
      else {
        bufptr = &buff[0];
        file_length = sizeof(buff) - 1;
      }
    }
    read = fread(bufptr, 1, file_length, fi);
    if (!read) {
      fprintf(stderr, "Error: Can't read the ssl session data file.\n");
      fclose(fi);
      if (bufptr != &buff[0]) free(bufptr);
      return 1;
    }
    assert(read <= (size_t)file_length);
    bufptr[read] = 0;
    fclose(fi);

    int ret = 0;
    if (read) ret = mysql_options(mysql, MYSQL_OPT_SSL_SESSION_DATA, buff);
    if (bufptr != &buff[0]) free(bufptr);
    return ret;
  }
  return 0;
}

inline static bool ssl_client_check_post_connect_ssl_setup(
    MYSQL *mysql, std::function<void(const char *)> report_error) {
  if (opt_ssl_session_data && !opt_ssl_session_data_continue_on_failed_reuse &&
      !mysql_get_ssl_session_reused(mysql)) {
    report_error(
        "--ssl-session-data specified but the session was not reused.");
    return true;
  }
  return false;
}

#define SSL_SET_OPTIONS(mysql) set_client_ssl_options(mysql)

const char *SSL_SET_OPTIONS_ERROR = "Failed to set ssl related options.\n";

#endif /* SSLOPT_VARS_INCLUDED */
