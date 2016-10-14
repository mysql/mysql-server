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

#include "mysqlx_connection.h"
#include "my_global.h"
#include <sstream>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif // HAVE_SYS_UN_H

#ifdef WIN32
#  define snprintf _snprintf
#endif // WIN32

namespace mysqlx
{

Connection::Connection(const char *ssl_key,
                       const char *ssl_ca, const char *ssl_ca_path,
                       const char *ssl_cert, const char *ssl_cipher,
                       const char *tls_version, const std::size_t timeout)
: m_timeout(timeout),
  m_vio(NULL),
  m_ssl_active(false),
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

Connection::~Connection()
{
  close();

  if (NULL != m_vioSslFd)
  {
    free_vio_ssl_acceptor_fd(m_vioSslFd);
    m_vioSslFd = NULL;
  }
}

Error Connection::connect_to_localhost(const std::string &named_pipe_or_unix_socket)
{
#if defined(HAVE_SYS_UN_H)
  sockaddr_un addr;

  if (named_pipe_or_unix_socket.empty())
    throw Error(CR_UNKNOWN_HOST, "UNIX Socket file was not specified");

  if (named_pipe_or_unix_socket.length() > (sizeof(addr.sun_path) - 1))
  {
    std::stringstream stream;

    stream << "UNIX Socket file name too long, size should be less or equal " << sizeof(addr.sun_path) - 1;
    throw Error(CR_UNKNOWN_HOST, stream.str());
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family= AF_UNIX;
  strncpy(addr.sun_path, named_pipe_or_unix_socket.c_str(), sizeof(addr.sun_path)-1);
  addr.sun_path[sizeof(addr.sun_path)-1] = 0;

  return connect((sockaddr*)&addr, sizeof(addr));
#else
  return Error(CR_SOCKET_CREATE_ERROR, "Named pipes aren't supported on current OS");
#endif // defined(HAVE_SYS_UN_H)
}

Error Connection::connect(sockaddr *addr, const std::size_t addr_size)
{
  my_socket s = ::socket(addr->sa_family,
                         SOCK_STREAM,
                         addr->sa_family == AF_UNIX ? 0: IPPROTO_TCP);

  return connect(s, (sockaddr*)addr, addr_size);
}

Error Connection::connect(my_socket s, sockaddr *addr, const std::size_t addr_size)
{
  int err = 0;
  enum_vio_type type = VIO_TYPE_TCPIP;

  if (s == INVALID_SOCKET)
    return Error(CR_SOCKET_CREATE_ERROR, "Invalid socket");

  int res = ::connect(s, (const sockaddr*)addr, (socklen_t)addr_size);
  if (0 != res)
  {
    err = socket_errno;
    ::closesocket(s);

    return get_socket_error(err);
  }

  if (AF_UNIX == addr->sa_family)
    type = VIO_TYPE_SOCKET;

  m_vio = vio_new(s, type, 0);
  return Error();
}

std::string Connection::get_socket_error_description(const int error_id)
{
  std::string strerr;
#ifdef _WIN32
  char *s = NULL;
  if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL))
  {
    char text[256];
    snprintf(text, sizeof(text), "Error %i", error_id);
    strerr = text;
  }
  else
  {
    strerr = s;
    LocalFree(s);

    while ( strerr.size() &&
            (strerr[strerr.size()-1] == '\n' || strerr[strerr.size()-1] == '\r'))
    {
      strerr.erase( strerr.size()-1 );
    }

    if ( strerr.size() && strerr[strerr.size()-1] == '.' )
    {
      strerr.erase( strerr.size()-1 );
    }
  }
#else
  strerr = strerror(error_id);
#endif

  return strerr;
}

Error Connection::get_ssl_init_error(const int init_error_id)
{
  return Error(CR_SSL_CONNECTION_ERROR, sslGetErrString((enum_ssl_init_error)init_error_id));
}


#ifdef _WIN32
#define SOCKET_ERROR_WIN_OR_POSIX(W,P) W
#else
#define SOCKET_ERROR_WIN_OR_POSIX(W,P) P
#endif // _WIN32

#define SOCKET_EPIPE        SOCKET_ERROR_WIN_OR_POSIX(ERROR_BROKEN_PIPE, EPIPE)
#define SOCKET_ECONNABORTED SOCKET_ERROR_WIN_OR_POSIX(WSAECONNABORTED, ECONNABORTED)

Error Connection::get_socket_error(const int error_id)
{
  switch (error_id)
  {
#if defined(__APPLE__)
    // OSX return this undocumented error in case of kernel race-conndition
    // lets ignore it and next call to any io function should return correct
    // error
    case EPROTOTYPE:
       return Error();
#endif // defined(__APPLE__)

    case SOCKET_ECONNABORTED:
    case SOCKET_ECONNRESET:
      return Error(CR_SERVER_GONE_ERROR, "MySQL server has gone away");

    case SOCKET_EPIPE:
      return Error(CR_BROKEN_PIPE, "MySQL server has gone away");

    default:
      return Error(CR_UNKNOWN_ERROR, get_socket_error_description(error_id));
  }
}

Error Connection::get_ssl_error(const int error_id)
{
  const unsigned int buffer_size = 1024;
  std::string r;

  r.resize(buffer_size);

  char *buffer = &r[0];

  ERR_error_string_n(error_id, buffer, buffer_size);
  return Error(CR_SSL_CONNECTION_ERROR, buffer);
}

Error Connection::activate_tls()
{
  if (NULL == m_vioSslFd)
      return get_ssl_init_error(m_ssl_init_error);

  unsigned long error;
  if (0 != sslconnect(m_vioSslFd, m_vio, 60, &error))
  {
    return get_ssl_error(error);
  }

  m_ssl_active = true;

  return Error();
}

Error Connection::shutdown(Shutdown_type how_to_shutdown)
{
  if ( 0 != ::shutdown(vio_fd(m_vio), (int)how_to_shutdown) )
    return get_socket_error(socket_errno);

  return Error();
}

Error Connection::write(const void *data, const std::size_t data_length)
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

      return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET);
    }
    else if (0 == result)
      return get_socket_error(SOCKET_ECONNRESET);

    left_data_to_write -= result;
    data_to_send += result;
  }while(left_data_to_write > 0);

  return Error();
}

Error Connection::read(void *data_head, const std::size_t data_length)
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

      vio_error = vio_error == 0 ? SOCKET_ECONNRESET : vio_error;
      return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET);
    }
    else if (0 == result)
      return get_socket_error(SOCKET_ECONNRESET);

    data_to_send -= result;
    data += result;
  }while(data_to_send != 0);

  return Error();
}

Error Connection::read_with_timeout(void *data, std::size_t &data_length, const int deadline_milliseconds)
{
  int result = vio_io_wait(m_vio, VIO_IO_EVENT_READ, deadline_milliseconds);

  if (-1 == result)
  {
    int vio_error = vio_errno(m_vio);
    vio_error = vio_error == 0 ? SOCKET_ECONNRESET : vio_error;

    return get_socket_error(0 != vio_error ? vio_error : SOCKET_ECONNRESET );
  }
  else if (0 == result)
  {
    data_length = 0;
  }
  else
  {
    return read(data, data_length);
  }

  return Error();
}

void Connection::close()
{
  if (m_vio)
  {
    ::closesocket(vio_fd(m_vio));
    vio_delete(m_vio);
    m_vio = NULL; // memory leak
  }
}

bool Connection::supports_ssl()
{
  return m_ssl;
}

} // namespace mysqlx
