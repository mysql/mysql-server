/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include <algorithm>
#include <cstdint>
#include <mutex>  // NOLINT(build/c++11)
#include <new>

#include "my_io.h"  // NOLINT(build/include_subdir)
#include "mysql/psi/mysql_socket.h"
#include "violite.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/interface/connection_acceptor.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/ngs/memory.h"
#include "plugin/x/src/ngs/socket_events.h"
#include "plugin/x/src/operations_factory.h"

namespace ngs {

using TcpAcceptor = net::ip::tcp::acceptor;
using Socket = TcpAcceptor::socket_type;
using Endpoint_type = TcpAcceptor::endpoint_type;

class Connection_acceptor_socket : public xpl::iface::Connection_acceptor {
 public:
  using Socket_ptr = std::shared_ptr<xpl::iface::Socket>;

  Connection_acceptor_socket(Socket_ptr listener,
                             xpl::iface::System &system_interface)
      : m_socket_listener(listener), m_system_interface(system_interface) {}

  Vio *accept() override {
    Vio *vio;
    sockaddr_storage accept_address;
    MYSQL_SOCKET sock = MYSQL_INVALID_SOCKET;

    for (int i = 0; i < MAX_ACCEPT_REATTEMPT; ++i) {
      socklen_t accept_len = sizeof(accept_address);

      sock = m_socket_listener->accept(KEY_socket_x_client_connection,
                                       (struct sockaddr *)&accept_address,
                                       &accept_len);

      if (INVALID_SOCKET != mysql_socket_getfd(sock)) break;

      const int error_code = m_system_interface.get_socket_errno();
      if (error_code != SOCKET_EINTR && error_code != SOCKET_EAGAIN)
        return nullptr;
    }

    const bool is_tcpip = (accept_address.ss_family == AF_INET ||
                           accept_address.ss_family == AF_INET6);
    vio = mysql_socket_vio_new(sock,
                               is_tcpip ? VIO_TYPE_TCPIP : VIO_TYPE_SOCKET, 0);
    if (!vio) throw std::bad_alloc();

#ifdef USE_PPOLL_IN_VIO
    vio->signal_mask = mysqld::get_mysqld_signal_mask();
#endif

    // enable TCP_NODELAY
    vio_fastsend(vio);
    vio_keepalive(vio, true);

    return vio;
  }

 private:
  Socket_ptr m_socket_listener;
  xpl::iface::System &m_system_interface;
  static const int MAX_ACCEPT_REATTEMPT = 10;
};

class Socket_events::EntryTimer {
 public:
  EntryTimer(net::io_context &io) : timer{io} {}

 public:
  std::function<bool()> callback;
  std::chrono::milliseconds duration;
  net::steady_timer timer;
};

class Socket_events::EntryAcceptingSocket {
 public:
  EntryAcceptingSocket(net::io_context &io) : acceptor{io} {}
  ~EntryAcceptingSocket() { acceptor.release(); }

 public:
  std::function<void(xpl::iface::Connection_acceptor &)> callback;
  std::shared_ptr<xpl::iface::Socket> socket;
  TcpAcceptor acceptor;
};

Socket_events::Socket_events() {}

Socket_events::~Socket_events() {
  std::for_each(m_timer_events.begin(), m_timer_events.end(),
                &free_object<EntryTimer>);

  std::for_each(m_socket_events.begin(), m_socket_events.end(),
                &free_object<EntryAcceptingSocket>);
}

bool Socket_events::listen(
    std::shared_ptr<xpl::iface::Socket> sock,
    std::function<void(xpl::iface::Connection_acceptor &)> callback) {
  m_socket_events.push_back(
      allocate_object<EntryAcceptingSocket>(m_io_context));
  EntryAcceptingSocket *socket_event = m_socket_events.back();

  socket_event->callback = callback;
  socket_event->socket = sock;
  socket_event->acceptor.assign(Endpoint_type().protocol(),
                                sock->get_socket_fd());

  socket_event->acceptor.async_wait(Socket::wait_read,
                                    [this, socket_event](std::error_code ec) {
                                      callback_accept_socket(socket_event, ec);
                                    });

  return true;
}

/** Register a callback to be executed in a fixed time interval.

The callback is called from the server's event loop thread, until either
the server is stopped or the callback returns false.

NOTE: This method may only be called from the same thread as the event loop.
*/
void Socket_events::add_timer(const std::size_t delay_ms,
                              std::function<bool()> callback) {
  EntryTimer *timer_entry = allocate_object<EntryTimer>(m_io_context);
  timer_entry->duration = std::chrono::milliseconds{delay_ms};
  timer_entry->callback = callback;
  {
    MUTEX_LOCK(lock, m_timers_mutex);
    m_timer_events.push_back(timer_entry);
  }

  timer_entry->timer.expires_after(timer_entry->duration);
  timer_entry->timer.async_wait([this, timer_entry](std::error_code ec) {
    callback_timeout(timer_entry, ec);
  });
}

void Socket_events::loop() { m_io_context.run(); }

void Socket_events::break_loop() { m_io_context.stop(); }

void Socket_events::callback_timeout(EntryTimer *timer_entry,
                                     std::error_code ec) {
  if (ec || !timer_entry->callback()) {
    {
      MUTEX_LOCK(timer_lock, m_timers_mutex);

      m_timer_events.erase(std::remove(m_timer_events.begin(),
                                       m_timer_events.end(), timer_entry),
                           m_timer_events.end());
    }

    free_object(timer_entry);
  } else {
    // schedule for another round
    timer_entry->timer.expires_after(timer_entry->duration);
    timer_entry->timer.async_wait([this, timer_entry](std::error_code ec) {
      callback_timeout(timer_entry, ec);
    });
  }
}

void Socket_events::callback_accept_socket(
    EntryAcceptingSocket *acceptors_entry, std::error_code ec) {
  if (!ec) {
    xpl::Operations_factory operations_factory;
    std::shared_ptr<xpl::iface::System> system_interface(
        operations_factory.create_system_interface());
    Connection_acceptor_socket vio_socket_forge(acceptors_entry->socket,
                                                *system_interface);

    acceptors_entry->callback(vio_socket_forge);

    acceptors_entry->acceptor.async_wait(
        Socket::wait_read, [this, acceptors_entry](std::error_code ec) {
          callback_accept_socket(acceptors_entry, ec);
        });
  }
}

}  // namespace ngs
