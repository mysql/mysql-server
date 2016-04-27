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

#include "ngs_common/connection_vio.h"
#include "ngs_common/options_ssl.h"

#define LOG_DOMAIN "ngs.client"
#include "ngs/log.h"

#include "mysql/service_my_snprintf.h"

using namespace ngs;

class ngs::Connection_vio_impl
{
public:
  Connection_vio_impl(my_socket sock)
  {
    m_vio = vio_new(sock, VIO_TYPE_TCPIP, 0);
    if (!m_vio)
      throw std::bad_alloc();
  }
  ~Connection_vio_impl() { vio_delete(m_vio); }
  Vio *get_vio() { return m_vio; }
private:
  Vio *m_vio;
};

class Options_session_supports_ssl : public Options_session_default
{
public:
  Options_session_supports_ssl() {}

  bool supports_tls() { return true; }
};


Connection_vio::Connection_vio(Ssl_context &ssl_context, my_socket sock)
: m_ssl_context(ssl_context)
{
  m_impl = new ngs::Connection_vio_impl(sock);
  // enable TCP_NODELAY
  vio_fastsend(m_impl->get_vio());

  vio_keepalive(m_impl->get_vio(), TRUE);
}

Connection_vio::~Connection_vio()
{
  delete m_impl;
}

int Connection_vio::get_socket_id()
{
  return vio_fd(m_impl->get_vio());
}


ssize_t Connection_vio::write(const Const_buffer_sequence &data)
{
  ssize_t c = 0;
  for (Const_buffer_sequence::const_iterator it = data.begin(); it != data.end(); ++it)
  {
    ssize_t n = write(it->first, it->second);
    if (n <= 0)
      return n;

    c += n;
  }
  return c;
}


ssize_t Connection_vio::write(const char *buffer, const std::size_t buffer_size)
{
  ssize_t bytes_to_send = buffer_size;

  do
  {
    ssize_t result = 0;
    {
      Mutex_lock lock(m_shutdown_mutex);
      result = vio_write(m_impl->get_vio(), (const uchar*)buffer, bytes_to_send);
    }

    if (result <= 0)
      return result;

    bytes_to_send -= result;
    buffer += result;
  } while (bytes_to_send > 0);

  return buffer_size;
}


ssize_t Connection_vio::read(char *buffer, const std::size_t buffer_size)
{
  ssize_t bytes_to_send = buffer_size;
  do
  {
    const ssize_t result = vio_read(m_impl->get_vio(), (uchar*)buffer, bytes_to_send);

    if (result <= 0)
      return result;

    bytes_to_send -= result;
    buffer += result;
  }while(bytes_to_send > 0);

  return buffer_size;
}


int Connection_vio::shutdown(Shutdown_type how_to_shutdown)
{
  Mutex_lock lock(m_shutdown_mutex);
  return vio_shutdown(m_impl->get_vio());
}


void Connection_vio::close()
{
  // vio_shutdown cloeses socket, no need to reimplement close
  shutdown(Shutdown_both);
}


IOptions_session_ptr Connection_vio::options()
{
  if (!m_options_session)
  {
    if (m_ssl_context.has_ssl())
      m_options_session.reset(new Options_session_supports_ssl());
    else
      m_options_session.reset(new Options_session_default());
  }

  return m_options_session;
}


Ssl_context::Ssl_context()
: m_ssl_acceptor(NULL),
  m_options(new Options_context_default())
{
}

void Ssl_context::setup(const char* tls_version,
                        const char* ssl_key,
                        const char* ssl_ca,
                        const char* ssl_capath,
                        const char* ssl_cert,
                        const char* ssl_cipher,
                        const char* ssl_crl,
                        const char* ssl_crlpath)
{
  enum_ssl_init_error error = SSL_INITERR_NOERROR;

  long ssl_ctx_flags= process_tls_version(tls_version);

  m_ssl_acceptor = new_VioSSLAcceptorFd(ssl_key, ssl_cert,
                      ssl_ca, ssl_capath,
                      ssl_cipher,
                      &error,
                      ssl_crl, ssl_crlpath, ssl_ctx_flags);

  if (NULL == m_ssl_acceptor)
  {
    log_warning("Failed at SSL configuration: \"%s\"", sslGetErrString(error));
    return;
  }

  m_options.reset(new Options_context_ssl(m_ssl_acceptor));
}


Ssl_context::~Ssl_context()
{
  if (m_ssl_acceptor)
    free_vio_ssl_acceptor_fd(m_ssl_acceptor);
}


/** Start a TLS session in the connection.
 */
bool Ssl_context::activate_tls(Connection_vio &conn, int handshake_timeout)
{
  unsigned long error;
  if (sslaccept(m_ssl_acceptor, conn.m_impl->get_vio(), handshake_timeout, &error) != 0)
  {
    log_warning("Error during SSL handshake for client connection (%i)", (int)error);
    return false;
  }
  conn.m_options_session = IOptions_session_ptr(new Options_session_ssl(conn.m_impl->get_vio()));
  return true;
}

void Connection_vio::close_socket(my_socket fd)
{
  if (fd >= 0)
  {
#ifndef _WIN32
    ::close(fd);
#else
    ::closesocket(fd);
#endif
  }
}

void Connection_vio::get_error(int& err, std::string& strerr)
{
#ifdef _WIN32
  char *s = NULL;
  err = WSAGetLastError();
  if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL))
  {
    char text[256];
    my_snprintf(text, sizeof(text), "Error %i", err);
    strerr = text;
  }
  else
  {
    strerr = s;
    LocalFree(s);
  }
#else
  err = errno;
  strerr = strerror(err);
#endif
}


my_socket Connection_vio::accept(my_socket sock, struct sockaddr* addr, socklen_t& len, int& err, std::string& strerr)
{
  bool cont = false;
  my_socket res = 0;
  do
  {
    cont = false;
    res = ::accept(sock, addr, &len);
#ifdef _WIN32
    if (res == INVALID_SOCKET)
    {
      if (WSAGetLastError() == WSAEINTR)
        cont = true;
      else
        get_error(err, strerr);
    }
#else
    if (res < 0)
    {
      if (errno == EINTR)
        cont = true;
      else
        get_error(err, strerr);
    }
#endif
  } while (cont);

  return res;
}

my_socket Connection_vio::create_and_bind_socket(const unsigned short port)
{
  int err;
  std::string errstr;

  my_socket result = socket(AF_INET, SOCK_STREAM, 0);
  if (result == INVALID_SOCKET)
  {
    get_error(err, errstr);
    log_error("Could not create server socket: %s (%i)", errstr.c_str(), err);
    return INVALID_SOCKET;
  }

  {
    int one = 1;
    setsockopt(result, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(result, (const struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    // lets decide later if its an error or not
    get_error(err, errstr);
    log_error("Could not bind to port %i: %s (%i)", port, errstr.c_str(), err);

    ngs::Connection_vio::close_socket(result);

    return INVALID_SOCKET;
  }

  if (listen(result, 9999) < 0)
  {
    // lets decide later if its an error or not
    get_error(err, errstr);
    log_error("Listen error: %s (%i)", errstr.c_str(), err);

    ngs::Connection_vio::close_socket(result);

    return INVALID_SOCKET;
  }

  return result;
}
