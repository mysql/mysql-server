/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef XCOM_NETWORK_PROVIDER_NATIVE_LIB_H
#define XCOM_NETWORK_PROVIDER_NATIVE_LIB_H

#include "xcom/result.h"
#include "xcom/site_def.h"

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <Ws2tcpip.h>
#include <winsock2.h>
#endif /* _WIN32 */

#include <openssl/err.h>
#include <openssl/ssl.h>
#endif /*! XCOM_WITHOUT_OPENSSL*/

#define SYS_STRERROR_SIZE 512

class Xcom_network_provider_library {
 public:
  static result checked_create_socket(int domain, int type, int protocol);
  static struct addrinfo *does_node_have_v4_address(struct addrinfo *retrieved);
  static int timed_connect(int fd, struct sockaddr *sock_addr,
                           socklen_t sock_size);
  static int timed_connect_sec(int fd, struct sockaddr *sock_addr,
                               socklen_t sock_size, int timeout);
  static int timed_connect_msec(int fd, struct sockaddr *sock_addr,
                                socklen_t sock_size, int timeout);
  static int allowlist_socket_accept(int fd, site_def const *xcom_config);
  static result gcs_shut_close_socket(int *sock);
  static result announce_tcp(xcom_port port);

 private:
  static void init_server_addr(struct sockaddr **sock_addr, socklen_t *sock_len,
                               xcom_port port, int family);
  static result xcom_checked_socket(int domain, int type, int protocol);
  static result create_server_socket();
  static result create_server_socket_v4();
  static void gcs_shutdown_socket(int *sock);
  static result gcs_close_socket(int *sock);
};

#ifndef XCOM_WITHOUT_OPENSSL

#ifndef SSL_SUCCESS
#define SSL_SUCCESS 1
#define SSL_ERROR 0
#endif

class Xcom_network_provider_ssl_library {
 public:
  /*
    Initialize the SSL.

    server_key_file  - Path of file that contains the server's X509 key in PEM
                       format.
    server_cert_file - Path of file that contains the server's X509 certificate
    in PEM format. client_key_file  - Path of file that contains the client's
    X509 key in PEM format. client_cert_file - Path of file that contains the
    client's X509 certificate in PEM format. ca_file          - Path of file
    that contains list of trusted SSL CAs. ca_path          - Path of directory
    that contains trusted SSL CA certificates in PEM format. crl_file         -
    Path of file that contains certificate revocation lists. crl_path         -
    Path of directory that contains certificate revocation list files. cipher -
    List of permitted ciphers to use for connection encryption. tls_version -
    Protocols permitted for secure connections.

    Note that only the server_key_file/server_cert_file and the client_key_file/
    client_cert_file are required and the rest of the pointers can be NULL.
    If the key is provided along with the certificate, either the key file or
    the other can be omitted.

    The caller can free the parameters after the call if this is necessary.

    Return 0 if success 1 otherwise.
  */
  static int xcom_init_ssl(const char *server_key_file,
                           const char *server_cert_file,
                           const char *client_key_file,
                           const char *client_cert_file, const char *ca_file,
                           const char *ca_path, const char *crl_file,
                           const char *crl_path, const char *cipher,
                           const char *tls_version,
                           const char *tls_ciphersuites);

  /*
   Cleans Up the SSL Configuration freeing allocated memory.
   */
  static void xcom_cleanup_ssl();

  /*
    Destroy the SSL Configuration freeing allocated memory.
  */
  static void xcom_destroy_ssl();

  /*
    Verify whether the server certificate matches the host to which
    the connection is attempted.
  */
  static int ssl_verify_server_cert(SSL *ssl, const char *server_hostname);
};

/*
  Pointers to the SSL Context for the server and client
  contexts respectively.
*/
extern SSL_CTX *server_ctx;
extern SSL_CTX *client_ctx;

#endif  /* !XCOM_WITHOUT_OPENSSL */
#endif  // XCOM_NETWORK_PROVIDER_H
