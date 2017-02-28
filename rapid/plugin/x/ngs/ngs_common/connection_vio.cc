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
#include "ngs_common/connection_type.h"
#include "ngs_common/options_ssl.h"
#include "ngs_common/string_formatter.h"
#include "ngs/log.h"

#include <sstream>


using namespace ngs;


class Options_session_supports_ssl : public Options_session_default
{
public:
  Options_session_supports_ssl() {}

  bool supports_tls() { return true; }
};


Connection_vio::Connection_vio(Ssl_context &ssl_context, Vio *vio)
: m_vio(vio), m_ssl_context(ssl_context)
{
}


Connection_vio::~Connection_vio()
{
  if (NULL != m_vio)
    vio_delete(m_vio);
}

my_socket Connection_vio::get_socket_id()
{
  return vio_fd(m_vio);
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

  // vio_shutdown sets the vio->fd to INVALID_SOCKET thus it is not
  // possible to use following assert without major changes in vio
  // DBUG_ASSERT(INVALID_SOCKET != vio_fd(m_vio));

  do
  {
    ssize_t result = 0;
    {
      Mutex_lock lock(m_shutdown_mutex);
      result = vio_write(m_vio, (const uchar*)buffer, bytes_to_send);
    }

    if (result <= 0)
      return result;

    bytes_to_send -= result;
    buffer += result;
  } while (bytes_to_send > 0);

  return buffer_size;
}


sockaddr_storage *Connection_vio::peer_address(std::string &address, uint16 &port)
{
  address.resize(256);
  char *buffer = &address[0];

  buffer[0] = 0;

  if (vio_peer_addr(m_vio, buffer, &port, address.capacity()))
    return NULL;

  address.resize(strlen(buffer));

  return &m_vio->remote;
}


Connection_type Connection_vio::connection_type()
{
  if (options()->active_tls())
    return Connection_tls;

  const enum_vio_type type = vio_type(m_vio);

  return Connection_type_helper::convert_type(type);
}


ssize_t Connection_vio::read(char *buffer, const std::size_t buffer_size)
{
  ssize_t bytes_to_send = buffer_size;

  // vio_shutdown sets the vio->fd to INVALID_SOCKET thus it is not
  // possible to use following assert without major changes in vio
  // DBUG_ASSERT(INVALID_SOCKET != vio_fd(m_vio));

  do
  {
    const ssize_t result = vio_read(m_vio, (uchar*)buffer, bytes_to_send);

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
  return vio_shutdown(m_vio);
}


void Connection_vio::close()
{
  // vio_shutdown cloeses socket, no need to reimplement close
  shutdown(Shutdown_both);
}


void ngs::Connection_vio::mark_idle()
{
  MYSQL_SOCKET_SET_STATE(m_vio->mysql_socket, PSI_SOCKET_STATE_IDLE);
}

void ngs::Connection_vio::mark_active()
{
  MYSQL_SOCKET_SET_STATE(m_vio->mysql_socket, PSI_SOCKET_STATE_ACTIVE);
}

void ngs::Connection_vio::set_socket_thread_owner()
{
  mysql_socket_set_thread_owner(m_vio->mysql_socket);
}


IOptions_session_ptr Connection_vio::options()
{
  if (!m_options_session)
  {
    if (m_ssl_context.has_ssl())
      m_options_session = ngs::allocate_shared<Options_session_supports_ssl>();
    else
      m_options_session = ngs::allocate_shared<Options_session_default>();
  }

  return m_options_session;
}


Ssl_context::Ssl_context()
: m_ssl_acceptor(NULL),
  m_options(ngs::allocate_shared<Options_context_default>())
{
}

bool Ssl_context::setup(const char *tls_version,
                        const char *ssl_key,
                        const char *ssl_ca,
                        const char *ssl_capath,
                        const char *ssl_cert,
                        const char *ssl_cipher,
                        const char *ssl_crl,
                        const char *ssl_crlpath)
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
    return false;
  }

  m_options = ngs::allocate_shared<Options_context_ssl>(m_ssl_acceptor);

  return true;
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
  if (sslaccept(m_ssl_acceptor, conn.m_vio, handshake_timeout, &error) != 0)
  {
    log_warning("Error during SSL handshake for client connection (%i)", (int)error);
    return false;
  }
  conn.m_options_session = IOptions_session_ptr(ngs::allocate_shared<Options_session_ssl>(conn.m_vio));
  return true;
}
