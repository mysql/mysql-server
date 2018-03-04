/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_SSL_TRANSPORT_H
#define XCOM_SSL_TRANSPORT_H

#ifdef XCOM_HAVE_OPENSSL
#ifdef WIN32
// In OpenSSL before 1.1.0, we need this first.
#include <winsock2.h>
#endif  // WIN32
#include <openssl/err.h>
#include <openssl/ssl.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef SSL_SUCCESS
#define SSL_SUCCESS 1
#define SSL_ERROR 0
#endif

/*
  Possible operation modes as explained further down. If you
  want to add a new mode, do it before the LAST_SSL_MODE.
*/
enum ssl_enum_mode_options {
  INVALID_SSL_MODE = -1,
  SSL_DISABLED = 1,
  SSL_PREFERRED,
  SSL_REQUIRED,
  SSL_VERIFY_CA,
  SSL_VERIFY_IDENTITY,
  LAST_SSL_MODE
};

/*
  Return the operation mode as an integer from an operation mode provided
  as a string. Note that the string must be provided in upper case letters
  and the possible values are: "DISABLED", "PREFERRED", "REQUIRED",
  "VERIFY_CA" or "VERIFY_IDENTITY".

  If a different value is provide, INVALID_SSL_MODE (-1) is returned.
*/
int xcom_get_ssl_mode(const char *mode);

/*
  Set the operation mode which might be the following:

  . SSL_DISABLED (1): The SSL mode will be disabled and this is the default
    value.

  . SSL_PREFERRED (2): The SSL mode will be always disabled if this value is
    provided and is only allowed to keep the solution compatibility with
    MySQL server.

  . SSL_REQUIRED (4): The SSL mode will be enabled but the verifications
    described in the next modes are not performed.

  . SSL_VERIFY_CA (4) - Verify the server TLS certificate against the configured
    Certificate Authority (CA) certificates. The connection attempt fails if no
    valid matching CA certificates are found.

  . SSL_VERIFY_IDENTITY (5): Like VERIFY_CA, but additionally verify that the
    server certificate matches the host to which the connection is attempted.

  If a different value is provide, INVALID_SSL_MODE (-1) is returned.
*/
int xcom_set_ssl_mode(int mode);

/*
  Set the password used by SSL to store keys. If nothing is set, "yassl123"
  is used by default. The password provided as parameter is copied so the
  value can be discarded by the caller after the call.
*/
void xcom_set_default_passwd(char *pw);

/*
  Initialize the SSL.

  server_key_file  - Path of file that contains the server's X509 key in PEM
                     format.
  server_cert_file - Path of file that contains the server's X509 certificate in
                     PEM format.
  client_key_file  - Path of file that contains the client's X509 key in PEM
                     format.
  client_cert_file - Path of file that contains the client's X509 certificate in
                     PEM format.
  ca_file          - Path of file that contains list of trusted SSL CAs.
  ca_path          - Path of directory that contains trusted SSL CA certificates
                     in PEM format.
  crl_file         - Path of file that contains certificate revocation lists.
  crl_path         - Path of directory that contains certificate revocation list
                     files.
  cipher           - List of permitted ciphers to use for connection encryption.
  tls_version      - Protocols permitted for secure connections.

  Note that only the server_key_file/server_cert_file and the client_key_file/
  client_cert_file are required and the rest of the pointers can be NULL.
  If the key is provided along with the certificate, either the key file or
  the other can be ommited.

  The caller can free the parameters after the call if this is necessary.

  Return 0 if success 1 otherwise.
*/
int xcom_init_ssl(const char *server_key_file, const char *server_cert_file,
                  const char *client_key_file, const char *client_cert_file,
                  const char *ca_file, const char *ca_path,
                  const char *crl_file, const char *crl_path,
                  const char *cipher, const char *tls_version);

/*
  Destroy the SSL Configuration freeing allocated memory.
*/
void xcom_cleanup_ssl();
void xcom_destroy_ssl();

/*
  Return whether the SSL will be used to encrypt data or not.

  Return 1 if it is enabled 0 otherwise.
*/
int xcom_use_ssl();

/*
  Verify whether the server certificate matches the host to which
  the connection is attempted.
*/
int ssl_verify_server_cert(SSL *ssl, const char *server_hostname);

/*
  Pointers to the SSL Context for the server and client
  contexts respectively.
*/
extern SSL_CTX *server_ctx;
extern SSL_CTX *client_ctx;

#ifdef __cplusplus
}
#endif
#endif /* XCOM_HAVE_OPENSSL */
#endif /* XCOM_SSL_TRANSPORT_H */
