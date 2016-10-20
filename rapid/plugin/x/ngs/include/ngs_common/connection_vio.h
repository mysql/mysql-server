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


#ifndef _NGS_CONNECTION_VIO_H_
#define _NGS_CONNECTION_VIO_H_


#include "ngs/memory.h"
#include "ngs/thread.h"
#include "my_global.h"
#include "violite.h"

#include "ngs_common/types.h"
#include "ngs_common/options.h"
#include "ngs_common/connection_type.h"
#include "ngs_common/socket_interface.h"

#ifdef WIN32
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#endif


namespace ngs
{

class Ssl_context;

class Connection_vio
{
public:
  Connection_vio(Ssl_context &ssl_context, Vio *vio);
  virtual ~Connection_vio();

  my_socket get_socket_id();
  virtual IOptions_session_ptr options();

  ssize_t read(char *buffer, const std::size_t buffer_size);
  ssize_t write(const Const_buffer_sequence &data);
  ssize_t write(const char *buffer, const std::size_t buffer_size);

  sockaddr_storage * peer_address(std::string &address, uint16 &port);
  virtual Connection_type connection_type();

  enum Shutdown_type
  {
    Shutdown_send = SHUT_WR,
    Shutdown_recv = SHUT_RD,
    Shutdown_both = SHUT_RDWR
  };

  int shutdown(Shutdown_type how_to_shutdown);
  void close();

  /* psf-related methods */
  void mark_idle();
  void mark_active();
  void set_socket_thread_owner();

private:
  friend class Ssl_context;

  Mutex m_shutdown_mutex;
  Vio  *m_vio;
  IOptions_session_ptr m_options_session;
  Ssl_context &m_ssl_context;
};

/* A shared SSL context object.

 SSL sessions can be established for a Connection_vio object through this context.  */
class Ssl_context
{
public:
  Ssl_context();
  bool setup(const char* tls_version,
              const char* ssl_key,
              const char* ssl_ca,
              const char* ssl_capath,
              const char* ssl_cert,
              const char* ssl_cipher,
              const char* ssl_crl,
              const char* ssl_crlpath);
  ~Ssl_context();

  bool activate_tls(Connection_vio &conn, int handshake_timeout);

  IOptions_context_ptr options() { return m_options; }
  bool has_ssl() { return NULL != m_ssl_acceptor; }

private:
  st_VioSSLFd *m_ssl_acceptor;
  IOptions_context_ptr m_options;
};


typedef ngs::shared_ptr<Connection_vio> Connection_ptr;
typedef ngs::Memory_instrumented<Ssl_context>::Unique_ptr Ssl_context_unique_ptr;

} // namespace ngs


#endif // _NGS_CONNECTION_VIO_H_
