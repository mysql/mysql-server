/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#ifndef _NGS_CONNECTION_VIO_H_
#define _NGS_CONNECTION_VIO_H_


#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs_common/connection_type.h"
#include "plugin/x/ngs/include/ngs_common/options.h"
#include "plugin/x/ngs/include/ngs_common/socket_interface.h"
#include "plugin/x/ngs/include/ngs_common/types.h"
#include "violite.h"

#ifdef WIN32
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#endif


namespace ngs
{

class Ssl_context;
class Vio_interface;

class Connection_vio
{
public:
  Connection_vio(Ssl_context_interface &ssl_context,
      std::unique_ptr<Vio_interface> vio);

  virtual ~Connection_vio() = default;

  my_socket get_socket_id();
  virtual IOptions_session_ptr options();

  ssize_t read(char *buffer, const std::size_t buffer_size,
      const long read_timeout);
  ssize_t write(const Const_buffer_sequence &data, const long write_timeout);
  ssize_t write(const char *buffer, const std::size_t buffer_size,
      const long write_timeout);

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
  std::unique_ptr<Vio_interface> m_vio;
  IOptions_session_ptr m_options_session;
  Ssl_context_interface &m_ssl_context;
};

/* A shared SSL context object.

 SSL sessions can be established for a Connection_vio object through this context.  */
class Ssl_context : public Ssl_context_interface {
public:
  Ssl_context();
  bool setup(const char* tls_version,
              const char* ssl_key,
              const char* ssl_ca,
              const char* ssl_capath,
              const char* ssl_cert,
              const char* ssl_cipher,
              const char* ssl_crl,
              const char* ssl_crlpath) override;
  ~Ssl_context();

  bool activate_tls(Connection_vio &conn, const int handshake_timeout) override;

  IOptions_context_ptr options() override { return m_options; }
  bool has_ssl() override { return nullptr != m_ssl_acceptor; }

private:
  st_VioSSLFd *m_ssl_acceptor;
  IOptions_context_ptr m_options;
};


typedef ngs::shared_ptr<Connection_vio> Connection_ptr;
typedef ngs::Memory_instrumented<Ssl_context>::Unique_ptr Ssl_context_unique_ptr;

} // namespace ngs


#endif // _NGS_CONNECTION_VIO_H_
