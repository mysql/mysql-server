/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_LOCAL_H_
#define MYSQL_HARNESS_NET_TS_LOCAL_H_

#include <cstring>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <sys/un.h>
#define NET_TS_HAS_UNIX_SOCKET
#else
#include <WinSock2.h>
#include <Windows.h>

#ifdef AF_UNIX
#include <afunix.h>
#define NET_TS_HAS_UNIX_SOCKET
#endif
#endif

#ifdef NET_TS_HAS_UNIX_SOCKET

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/ucred.h>
#endif

#if defined(__linux__) || defined(__OpenBSD__)
#include <sys/socket.h>  // struct ucred, struct sockpeercred
#endif

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"

namespace local {

/**
 * endpoint of Unix domain sockets (AF_UNIX).
 *
 * path may be:
 *
 * - empty (unnamed sockets, aka 'autobind' on Linux)
 * - start with NUL-byte (abstract sockets, no file, used for socketpair())
 * - a path
 */
template <typename Protocol>
class basic_endpoint {
 public:
  using protocol_type = Protocol;

  constexpr basic_endpoint() noexcept : data_{} {
    data_.sun_family = protocol_type().family();
    // unix-socket in unnamed namespace
  }

  // create a unix domain socket in the 'pathname namespace' or 'abstract
  // namespace' (starting with \0)
  //
  // note: can be 'constexpr' with C++2a's http://wg21.link/P0202 applied
  basic_endpoint(std::string_view path) : data_{} {
    data_.sun_family = protocol_type().family();

    const auto truncated_path =
        path.substr(0, std::min(max_path_len(), path.size()));

    // use std::copy() instead of std::memcpy() as it will be constexpr in C++20
    std::copy(truncated_path.begin(), truncated_path.end(), data_.sun_path);

    path_len_ = truncated_path.size();
  }

  constexpr protocol_type protocol() const noexcept { return protocol_type(); }

  /**
   * get path.
   */
  std::string path() const {
    if (path_len_ > 0) {
      return {data_.sun_path, path_len_};
    } else {
      return {};
    }
  }

  const void *data() const noexcept { return &data_; }

  void *data() noexcept { return &data_; }

  /**
   * bytes used data().
   */
  constexpr size_t size() const noexcept {
    return offsetof(sockaddr_un, sun_path) + path_len_;
  }

  constexpr size_t capacity() const noexcept { return sizeof(data_); }

  /**
   * resize data().
   *
   * @param n size to resize data() too
   *
   * if n == 0, path() is considered empty,
   * if n < offsetof(sockaddr_un, sun_path), behaviour is undefined,
   * otherwise path().size() is the rest of the data()
   */
  void resize(size_t n) noexcept {
    if (n >= offsetof(sockaddr_un, sun_path)) {
      // path length: the rest of the message
      path_len_ = std::min(capacity(), n) - offsetof(sockaddr_un, sun_path);

      // ... but it may be null-terminated as long as the NUL-byte isn't at the
      // first position
      if (path_len_ > 0 && data_.sun_path[0] != '\0') {
        path_len_ = strnlen(data_.sun_path, path_len_);
      }
    } else {
      // socketpair's recvmsg sets the msg_namelen to 0 which implies
      //
      // - no path
      // - family is the same as our socket's
      path_len_ = 0;
    }
  }

 private:
  constexpr size_t max_path_len() const {
    return capacity() - offsetof(sockaddr_un, sun_path);
  }

  sockaddr_un data_;
  size_t path_len_{0};
};  // namespace local

template <typename Protocol>
::std::ostream &operator<<(::std::ostream &os,
                           const basic_endpoint<Protocol> &ep) {
  std::string path = ep.path();

  // if first char is a '\0' it is abstract socket-path as used by socketpair()
  // on Linux
  //
  // replace the '\0' by '@' to make it printable
  if (path.size() > 0 && path[0] == '\0') {
    path[0] = '@';
  }
  os << path;

  return os;
}

template <class Protocol>
stdx::expected<void, std::error_code> connect_pair(
    net::io_context *io_ctx, net::basic_socket<Protocol> &sock1,
    net::basic_socket<Protocol> &sock2) {
  Protocol proto;
  const auto res = io_ctx->socket_service()->socketpair(
      proto.family(), proto.type(), proto.protocol());
  if (!res) return stdx::make_unexpected(res.error());

  const auto fds = *res;

  const auto assign1_res = sock1.assign(proto, fds.first);
  if (!assign1_res) {
    io_ctx->socket_service()->close(fds.first);
    io_ctx->socket_service()->close(fds.second);

    return assign1_res;
  }

  const auto assign2_res = sock2.assign(proto, fds.second);
  if (!assign2_res) {
    sock1.close();
    io_ctx->socket_service()->close(fds.second);

    return assign2_res;
  }

  return {};
}

template <class Protocol>
bool operator==(const basic_endpoint<Protocol> &a,
                const basic_endpoint<Protocol> &b) {
  return a.path() == b.path();
}

template <class Protocol>
bool operator!=(const basic_endpoint<Protocol> &a,
                const basic_endpoint<Protocol> &b) {
  return !(a == b);
}

namespace socket_option {
#if defined(__linux__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__APPLE__)
template <int Level, int Name>
class cred {
 public:
#if defined(__linux__)
  using value_type = struct ucred;
#elif defined(__OpenBSD__)
  using value_type = struct sockpeercred;
#elif defined(__FreeBSD__) || defined(__APPLE__)
  using value_type = struct xucred;
#elif defined(__NetBSD__)
  using value_type = struct sockpeercred;
#else
#error "unsupported OS"
#endif

  constexpr cred() : value_{} {}

  constexpr explicit cred(value_type v) : value_{v} {}

  value_type value() const { return value_; }

  template <typename Protocol>
  constexpr int level(const Protocol & /* unused */) const noexcept {
    return Level;
  }

  template <typename Protocol>
  constexpr int name(const Protocol & /* unused */) const noexcept {
    return Name;
  }

  template <typename Protocol>
  value_type *data(const Protocol & /* unused */) {
    return &value_;
  }

  template <typename Protocol>
  const value_type *data(const Protocol & /* unused */) const {
    return &value_;
  }

  /**
   * size of data().
   *
   * may be smaller than sizeof(value_type) after resize() was called.
   */
  template <typename Protocol>
  constexpr size_t size(const Protocol & /* unused */) const {
    return size_;
  }

  /**
   * resize data().
   *
   * called by basic_socket::get_option()
   *
   * @throws std::length_error if resize() would grow past sizeof(value_type)
   */
  template <typename Protocol>
  void resize(const Protocol &p, size_t new_size) {
    // freebsd/apple new_size == 4, with sizeof(xucred) == 76
    // after socketpair(), SOCK_STREAM, LOCAL_PEERCRED
    if (new_size > size(p)) {
      throw std::length_error(
          "overrun in socket_option::cred::resize(): current_size=" +
          std::to_string(size(p)) + ", new_size=" + std::to_string(new_size));
    }

    size_ = new_size;
  }

 private:
  value_type value_;

  size_t size_{sizeof(value_)};
};
#endif
}  // namespace socket_option

class stream_protocol {
 public:
  using endpoint = local::basic_endpoint<stream_protocol>;
  using socket = net::basic_stream_socket<stream_protocol>;
  using acceptor = net::basic_socket_acceptor<stream_protocol>;

  // note: to add: socket-control-message to pass credentials
  //
  // FreeBSD, NetBSD: LOCAL_CREDS + SCM_CREDS
  // Linux: SO_PASSCRED + SCM_CREDENTIALS

  // note: to add: socket-control-message for pass fd
  //
  // FreeBSD, Linux: SCM_RIGHTS

  // note: to add: socket-opt LOCAL_CONNWAIT (FreeBSD)

  // peer's credentials at connect()-time/listen()-time
#if defined(__linux__) || defined(__OpenBSD__)
  using peer_creds = socket_option::cred<SOL_SOCKET, SO_PEERCRED>;
#elif defined(__FreeBSD__) || defined(__APPLE__)
  using peer_creds = socket_option::cred<SOL_SOCKET, LOCAL_PEERCRED>;
#elif defined(__NetBSD__)
  using peer_creds = socket_option::cred<SOL_SOCKET, LOCAL_PEEREID>;
#endif
  // solaris uses getpeerucred(), ucred_geteuid(), ucred_getegid()
  //
  // windows: SIO_AF_UNIX_GETPEERPID via WSAIoctl()

  constexpr int family() const noexcept { return AF_UNIX; }
  constexpr int type() const noexcept { return SOCK_STREAM; }
  constexpr int protocol() const noexcept { return 0; }
};

// datagram.
//
// SOCK_DGRAM for AF_UNIX.
//
// messages may be in any order when received.
class datagram_protocol {
 public:
  using endpoint = local::basic_endpoint<datagram_protocol>;
  using socket = net::basic_datagram_socket<datagram_protocol>;

  // no peer_creds on datagram_protocol as it doesn't call "connect()" nor
  // "listen()". It needs SCM_CREDS instead

  constexpr int family() const noexcept { return AF_UNIX; }
  constexpr int type() const noexcept { return SOCK_DGRAM; }
  constexpr int protocol() const noexcept { return 0; }
};

// seqpacket over AF_UNIX.
//
// seqpacket is mix between stream_protocol and datagram_protocol:
//
// - connection-oriented (accept(), ...)
// - reliable (sequence order)
//
// like SOCK_STREAM
//
// - message boundaries are visible via MSG_EOR/MSG_TRUNC in recvmsg()
//
// like SOCK_DGRAM
//
// @see recvmsg()
//
class seqpacket_protocol {
 public:
  using endpoint = local::basic_endpoint<seqpacket_protocol>;
  using socket = net::basic_datagram_socket<seqpacket_protocol>;
  using acceptor = net::basic_socket_acceptor<seqpacket_protocol>;

#if defined(__linux__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
    defined(__APPLE__) || defined(__NetBSD__)
  using peer_creds = stream_protocol::peer_creds;
#endif

  constexpr int family() const noexcept { return AF_UNIX; }
  constexpr int type() const noexcept { return SOCK_SEQPACKET; }
  constexpr int protocol() const noexcept { return 0; }
};
}  // namespace local
#endif

#endif
