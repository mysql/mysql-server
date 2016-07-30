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


#ifndef _MYSQLX_SYNC_CONNECTION_H_
#define _MYSQLX_SYNC_CONNECTION_H_


#include <boost/system/error_code.hpp>
#include "violite.h"

#ifdef WIN32
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#endif

namespace mysqlx
{

enum Shutdown_type
{
  Shutdown_send = SHUT_WR,
  Shutdown_recv = SHUT_RD,
  Shutdown_both = SHUT_RDWR
};

class ssl_error : public boost::system::error_category
{
public:
  const char* name() const BOOST_NOEXCEPT;
  std::string message(int value) const;
};

class ssl_init_error : public boost::system::error_category
{
public:
  const char* name() const BOOST_NOEXCEPT;
  std::string message(int value) const;
};

class Mysqlx_sync_connection
{
public:
  Mysqlx_sync_connection(const char *ssl_key = NULL,
                         const char *ssl_ca = NULL, const char *ssl_ca_path = NULL,
                         const char *ssl_cert = NULL, const char *ssl_cipher = NULL,
                         const char *tls_version = NULL, const std::size_t timeout = 0l);

  ~Mysqlx_sync_connection();

  boost::system::error_code connect(sockaddr_in *sockaddr, const std::size_t addr_size);
  boost::system::error_code activate_tls();
  boost::system::error_code shutdown(Shutdown_type how_to_shutdown);

  boost::system::error_code write(const void *data, const std::size_t data_length);
  boost::system::error_code read(void *data, const std::size_t data_length);
  boost::system::error_code read_with_timeout(void *data, std::size_t &data_length, const int deadline_milliseconds);

  void close();

  bool supports_ssl();

private:

  static boost::system::error_code get_ssl_error(int error_id);
  static boost::system::error_code get_ssl_init_error(const int init_error_id);
  static bool is_set(const char *string);

  const std::size_t   m_timeout;
  st_VioSSLFd        *m_vioSslFd;
  Vio                *m_vio;
  bool                m_ssl;
  bool                m_ssl_acvtive;
  enum_ssl_init_error m_ssl_init_error;

};


} // namespace mysqlx


#endif // _MYSQLX_SYNC_CONNECTION_H_
