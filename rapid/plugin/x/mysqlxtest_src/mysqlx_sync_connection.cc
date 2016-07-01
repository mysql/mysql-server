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

#include <cassert>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#ifdef WIN32
#pragma warning(push, 0)
#endif
#include <boost/asio/error.hpp>
#ifdef WIN32
#pragma warning(pop)
#endif
#include "mysqlx_sync_connection.h"

namespace mysqlx
{

using namespace boost::system;


const char* ssl_error::name() const BOOST_NOEXCEPT
{
  return "SSL";
}

std::string ssl_error::message(int value) const
{
  std::string r;

  r.resize(1024);
  return ERR_error_string(value, &r[0]);
}

const char* ssl_init_error::name() const BOOST_NOEXCEPT
{
  return "SSL INIT";
}

std::string ssl_init_error::message(int value) const
{
  return sslGetErrString((enum_ssl_init_error)value);
}

Mysqlx_sync_connection::Mysqlx_sync_connection(const char *ssl_key,
                                               const char *ssl_ca, const char *ssl_ca_path,
                                               const char *ssl_cert, const char *ssl_cipher,
                                               const char *tls_version, const std::size_t timeout)
: m_timeout(timeout),
  m_vio(NULL),
  m_ssl_acvtive(false),
  m_ssl_init_error(SSL_INITERR_NOERROR)
{
  long ssl_ctx_flags = process_tls_version(tls_version);

  m_vioSslFd = new_VioSSLConnectorFd(ssl_key, ssl_cert, ssl_ca, ssl_ca_path, ssl_cipher, &m_ssl_init_error, NULL, NULL, ssl_ctx_flags);

  m_ssl = NULL != ssl_key ||
      NULL != ssl_cert ||
      NULL != ssl_ca ||
      NULL != ssl_ca_path ||
      NULL != ssl_cipher;
}

Mysqlx_sync_connection::~Mysqlx_sync_connection()
{
  close();

  if (NULL != m_vioSslFd)
  {
    free_vio_ssl_acceptor_fd(m_vioSslFd);
    m_vioSslFd = NULL;
  }
}

error_code Mysqlx_sync_connection::connect(sockaddr_in *addr, const std::size_t addr_size)
{
  int err = 0;
  while(true)
  {
    my_socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef WIN32
    if (s == INVALID_SOCKET)
      break;
#else
    if (s < 0)
      break;
#endif

    int res = ::connect(s, (const sockaddr*)addr, (socklen_t)addr_size);
    if (0 != res)
    {
#ifdef WIN32
      err = WSAGetLastError();
      ::closesocket(s);
#else
      err = errno;
      ::close(s);
#endif
      break;
    }

    m_vio = vio_new(s, VIO_TYPE_TCPIP, 0);
    return error_code();
  }

  return error_code(err, boost::asio::error::get_system_category());
}

error_code Mysqlx_sync_connection::get_ssl_error(int error_id)
{
  static ssl_error error_category;

  return error_code(error_id, error_category);
}

error_code Mysqlx_sync_connection::get_ssl_init_error(const int init_error_id)
{
  static ssl_init_error error_category;

  return error_code(init_error_id, error_category);
}

error_code Mysqlx_sync_connection::activate_tls()
{
  if (NULL == m_vioSslFd)
      return get_ssl_init_error(m_ssl_init_error);

  unsigned long error;
  if (0 != sslconnect(m_vioSslFd, m_vio, 60, &error))
  {
    return get_ssl_error(error);
  }

  m_ssl_acvtive = true;

  return error_code();
}

error_code Mysqlx_sync_connection::shutdown(Shutdown_type how_to_shutdown)
{
  if ( 0 != ::shutdown(vio_fd(m_vio), (int)how_to_shutdown) )
    return error_code( errno, posix_category);

  return error_code();
}

error_code Mysqlx_sync_connection::write(const void *data, const std::size_t data_length)
{
  std::size_t left_data_to_write = data_length;
  const unsigned char* data_to_send = (const unsigned char*)data;

  do
  {
    const int result = (int)vio_write(m_vio, data_to_send, left_data_to_write);
    //

    if (-1 == result)
    {
      const int vio_error = vio_errno(m_vio);

      return error_code(vio_error, boost::asio::error::get_system_category());
    }
    else if (0 == result)
      return error_code(boost::asio::error::connection_reset, boost::asio::error::get_system_category());

    left_data_to_write -= result;
    data_to_send += result;
  }while(left_data_to_write > 0);

  return error_code();
}

error_code Mysqlx_sync_connection::read(void *data_head, const std::size_t data_length)
{
  int result = 0;
  std::size_t data_to_send = data_length;
  char *data = (char*)data_head;

  do
  {
    result = (int)vio_read(m_vio, (unsigned char*)data, data_to_send);

    if (-1 == result)
    {
      int vio_error = vio_errno(m_vio);

      vio_error = vio_error == 0 ? boost::asio::error::connection_reset : vio_error;
      return error_code(vio_error, boost::asio::error::get_system_category());
    }
    else if (0 == result)
      return error_code(boost::asio::error::connection_reset, boost::asio::error::get_system_category());

    data_to_send -= result;
    data += result;
  }while(data_to_send != 0);

  return error_code();
}

error_code Mysqlx_sync_connection::read_with_timeout(void *data, std::size_t &data_length, const int deadline_milliseconds)
{
  int result = vio_io_wait(m_vio, VIO_IO_EVENT_READ, deadline_milliseconds);

  if (-1 == result)
  {
    int vio_error = vio_errno(m_vio);
    vio_error = vio_error == 0 ? boost::asio::error::connection_reset : vio_error;

    return error_code(vio_error, boost::asio::error::get_system_category());
  }
  else if (0 == result)
  {
    data_length = 0;
  }
  else
  {
    return read(data, data_length);
  }

  return error_code();
}

void Mysqlx_sync_connection::close()
{
  if (m_vio)
  {
#ifdef WIN32
    ::closesocket(vio_fd(m_vio));
#else
    ::close(vio_fd(m_vio));
#endif // WIN32
    vio_delete(m_vio);
    m_vio = NULL; // memory leak
  }
}

bool Mysqlx_sync_connection::supports_ssl()
{
  return m_ssl;
}

bool Mysqlx_sync_connection::is_set(const char *string)
{
  return false;
}

} // namespace mysqlx
