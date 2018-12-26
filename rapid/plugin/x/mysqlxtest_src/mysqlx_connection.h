/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db


#ifndef _MYSQLX_CONNECTION_H_
#define _MYSQLX_CONNECTION_H_

#include "mysqlx_error.h"

#include "violite.h"

#ifdef WIN32
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#endif

#define CR_UNKNOWN_ERROR        2000
#define CR_SOCKET_CREATE_ERROR  2001
#define CR_CONNECTION_ERROR     2002
#define CR_UNKNOWN_HOST         2005
#define CR_SERVER_GONE_ERROR    2006
#define CR_BROKEN_PIPE          2007
#define CR_WRONG_HOST_INFO      2009
#define CR_COMMANDS_OUT_OF_SYNC 2014
#define CR_NAMEDPIPE_CONNECTION 2015
#define CR_NAMEDPIPEWAIT_ERROR  2016
#define CR_NAMEDPIPEOPEN_ERROR  2017
#define CR_NAMEDPIPESETSTATE_ERROR 2018
#define CR_SSL_CONNECTION_ERROR 2026
#define CR_MALFORMED_PACKET     2027
#define CR_INVALID_AUTH_METHOD  2028

struct sockaddr_un;

namespace mysqlx
{

enum Shutdown_type
{
  Shutdown_send = SHUT_WR,
  Shutdown_recv = SHUT_RD,
  Shutdown_both = SHUT_RDWR
};

class Connection
{
public:
  Connection(const char *ssl_key = NULL,
                         const char *ssl_ca = NULL, const char *ssl_ca_path = NULL,
                         const char *ssl_cert = NULL, const char *ssl_cipher = NULL,
                         const char *tls_version = NULL, const std::size_t timeout = 0l);

  ~Connection();

  Error connect_to_localhost(const std::string &named_pipe_or_unix_socket);
  Error connect(sockaddr *sockaddr, const std::size_t addr_size);
  Error connect(my_socket s, sockaddr *sockaddr, const std::size_t addr_size);

  Error activate_tls();
  Error shutdown(Shutdown_type how_to_shutdown);

  Error write(const void *data, const std::size_t data_length);
  Error read(void *data, const std::size_t data_length);
  Error read_with_timeout(void *data, std::size_t &data_length, const int deadline_milliseconds);

  void close();

  bool supports_ssl();

private:

  Error get_ssl_init_error(const int init_error_id);
  Error get_ssl_error(const int error_id);
  Error get_socket_error(const int error_id);
  std::string get_socket_error_description(const int error_id);

  const std::size_t   m_timeout;
  st_VioSSLFd        *m_vioSslFd;
  Vio                *m_vio;
  bool                m_ssl;
  bool                m_ssl_active;
  enum_ssl_init_error m_ssl_init_error;

};


} // namespace mysqlx


#endif // _MYSQLX_CONNECTION_H_
