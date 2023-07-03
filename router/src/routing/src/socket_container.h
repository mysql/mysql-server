/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SOCKETCONTAINER_INCLUDED
#define ROUTING_SOCKETCONTAINER_INCLUDED

#include <mutex>
#include <vector>

/**
 * container of sockets.
 *
 * allows to disconnect all of them.
 *
 * thread-safe.
 */
template <class Protocol>
class SocketContainer {
 public:
  using protocol_type = Protocol;
  using socket_type = typename protocol_type::socket;

  // as a ref will get returned, the socket_type' object needs a stable address.
  // - std::list<socket_type> provide that.
  // - std::vector<std::unique_ptr<socket_type>> should work too.
  using container_type = std::list<socket_type>;

  /**
   * move ownership of socket_type to the container.
   *
   * @return a ref to the stored socket.
   */
  socket_type &push_back(socket_type &&sock) {
    std::lock_guard<std::mutex> lk(mtx_);

    sockets_.push_back(std::move(sock));

    return sockets_.back();
  }

  /**
   * move ownership of socket_type to the container.
   *
   * @return a ref to the stored socket.
   */
  template <class... Args>
  socket_type &emplace_back(Args &&... args) {
    std::lock_guard<std::mutex> lk(mtx_);

    sockets_.emplace_back(std::forward<Args>(args)...);

    return sockets_.back();
  }

  /**
   * release socket from container.
   *
   * moves ownership of the socket to the caller.
   *
   * @return socket
   */
  socket_type release(socket_type &client_sock) {
    std::lock_guard<std::mutex> lk(mtx_);

    return release_unlocked(client_sock);
  }

  socket_type release_unlocked(socket_type &client_sock) {
    for (auto cur = sockets_.begin(); cur != sockets_.end(); ++cur) {
      if (cur->native_handle() == client_sock.native_handle()) {
        auto sock = std::move(*cur);
        sockets_.erase(cur);
        return sock;
      }
    }

    // not found.
    return socket_type{client_sock.get_executor().context()};
  }

  template <class F>
  auto run(F &&f) {
    std::lock_guard<std::mutex> lk(mtx_);

    return f();
  }

  /**
   * disconnect all sockets.
   */

  void disconnect_all() {
    std::lock_guard<std::mutex> lk(mtx_);

    for (auto &sock : sockets_) {
      sock.cancel();
    }
  }

  /**
   * check if the container is empty.
   */
  bool empty() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return sockets_.empty();
  }

  /**
   * get size of container.
   */
  size_t size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return sockets_.size();
  }

 private:
  container_type sockets_;

  mutable std::mutex mtx_;
};

#endif
