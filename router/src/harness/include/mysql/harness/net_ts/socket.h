/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_SOCKET_H_
#define MYSQL_HARNESS_NET_TS_SOCKET_H_

#include <stdexcept>  // length_error
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket.h"

namespace net {

// 18.3 [socket.err]
// implemented in impl/socket_error.h

// 18.4 [socket.base]

class socket_base {
 public:
  class broadcast;
  class debug;
  class do_not_route;
  class error;  // not part of std
  class keep_alive;
  class linger;
  class out_of_band_inline;
  class receive_buffer_size;
  class receive_low_watermark;
  class reuse_address;
  class send_buffer_size;
  class send_low_watermark;

  using message_flags = impl::socket::message_flags;

  static constexpr message_flags message_peek = impl::socket::message_peek;
  static constexpr message_flags message_out_of_band =
      impl::socket::message_out_of_band;
  static constexpr message_flags message_do_not_route =
      impl::socket::message_do_not_route;

  using wait_type = impl::socket::wait_type;
  static constexpr wait_type wait_read = wait_type::wait_read;
  static constexpr wait_type wait_write = wait_type::wait_write;
  static constexpr wait_type wait_error = wait_type::wait_error;

  enum class shutdown_type {
#ifdef _WIN32
    shutdown_receive = SD_RECEIVE,
    shutdown_send = SD_SEND,
    shutdown_both = SD_BOTH,
#else
    shutdown_receive = SHUT_RD,
    shutdown_send = SHUT_WR,
    shutdown_both = SHUT_RDWR,
#endif
  };
  static constexpr shutdown_type shutdown_receive =
      shutdown_type::shutdown_receive;
  static constexpr shutdown_type shutdown_send = shutdown_type::shutdown_send;
  static constexpr shutdown_type shutdown_both = shutdown_type::shutdown_both;

  static const int max_listen_connections{SOMAXCONN};

 protected:
  // msg_hdr used by writev(), sendv(), sendmsg(), WSASend()
  //
  // windows and posix have slight different msghdr struct layouts.
  class msg_hdr : public impl::socket::msghdr_base {
    std::array<impl::socket::iovec_base, 16> iov_{};

   public:
    template <class BufferSequence>
    explicit msg_hdr(const BufferSequence &buffers)
        : impl::socket::msghdr_base{} {
      const size_t iovs_capacity = iov_.size();
      const auto bufend = buffer_sequence_end(buffers);
      size_t i = 0;

      for (auto bufcur = buffer_sequence_begin(buffers);
           (i < iovs_capacity) && (bufcur != bufend); ++i, ++bufcur) {
#ifdef _WIN32
        iov_[i].buf =
            const_cast<CHAR *>(reinterpret_cast<const CHAR *>(bufcur->data()));
        iov_[i].len = static_cast<DWORD>(bufcur->size());
#else
        iov_[i] = {const_cast<void *>(bufcur->data()), bufcur->size()};
#endif
      }

#ifdef _WIN32
      this->lpBuffers = iov_.data();
      this->dwBufferCount = static_cast<DWORD>(i);
#else
      this->msg_iov = iov_.data();
      this->msg_iovlen = i;
#endif
    }

    /**
     * set sender of the message.
     *
     * used by the UDP and Linux TCP Fast Open.
     */
    template <class endpoint_type>
    void set_sender(endpoint_type &ep) {
#ifdef _WIN32
      this->name = static_cast<SOCKADDR *>(ep.data());
      this->namelen = ep.capacity();
#else
      this->msg_name = ep.data();
      this->msg_namelen = ep.capacity();
#endif
    }

    /**
     * set the size of the sender after data was received.
     */
    template <class endpoint_type>
    void resize_sender(endpoint_type &ep) {
#ifdef _WIN32
      ep.resize(this->namelen);
#else
      ep.resize(this->msg_namelen);
#endif
    }

    /**
     * set receipient of the message.
     */
    template <class endpoint_type>
    void set_recipient(const endpoint_type &ep) {
#ifdef _WIN32
      this->name =
          const_cast<SOCKADDR *>(static_cast<const SOCKADDR *>(ep.data()));
      this->namelen = ep.size();
#else
      this->msg_name = const_cast<void *>(ep.data());
      this->msg_namelen = ep.size();
#endif
    }
  };
};

// 18.5 [socket.opt]

namespace socket_option {

/**
 * base-class of socket options.
 *
 * can be used to implement type safe socket options.
 *
 * @see socket_option::integer
 * @see socket_option::boolen
 */
template <int Level, int Name, class T, class V = T>
class option_base {
 public:
  using value_type = T;
  using storage_type = V;

  constexpr option_base() : value_{} {}

  constexpr explicit option_base(value_type v)
      : value_{static_cast<storage_type>(v)} {}

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
  const storage_type *data(const Protocol & /* unused */) const {
    return std::addressof(value_);
  }

  template <typename Protocol>
  storage_type *data(const Protocol & /* unused */) {
    return std::addressof(value_);
  }

  template <typename Protocol>
  constexpr size_t size(const Protocol & /* unused */) const {
    return sizeof(value_);
  }

  template <class Protocol>
  void resize(const Protocol & /* p */, size_t s) {
    if (s != sizeof(value_)) {
      throw std::length_error("size != sizeof(value_)");
    }
  }

 private:
  storage_type value_;
};

/**
 * socket option that uses bool as value_type.
 */
template <int Level, int Name>
using boolean = option_base<Level, Name, bool, int>;

/**
 * socket option that uses int as value_type.
 */
template <int Level, int Name>
using integer = option_base<Level, Name, int, int>;

}  // namespace socket_option

class socket_base::broadcast
    : public socket_option::boolean<SOL_SOCKET, SO_BROADCAST> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_BROADCAST>;
  using __base::__base;
};

class socket_base::debug : public socket_option::boolean<SOL_SOCKET, SO_DEBUG> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_DEBUG>;
  using __base::__base;
};

class socket_base::do_not_route
    : public socket_option::boolean<SOL_SOCKET, SO_DONTROUTE> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_DONTROUTE>;
  using __base::__base;
};

class socket_base::error : public socket_option::integer<SOL_SOCKET, SO_ERROR> {
  using __base = socket_option::integer<SOL_SOCKET, SO_ERROR>;
  using __base::__base;
};

class socket_base::keep_alive
    : public socket_option::boolean<SOL_SOCKET, SO_KEEPALIVE> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_KEEPALIVE>;
  using __base::__base;
};

// 18.5.1

/**
 * socket option for SO_LINGER
 */
class socket_base::linger {
 public:
  linger() noexcept : value_{0, 0} {}

  linger(bool e, std::chrono::seconds t) noexcept
      : value_{e, static_cast<decltype(value_.l_linger)>(t.count())} {}

  bool enabled() const noexcept { return value_.l_onoff != 0; }
  void enabled(bool e) noexcept { value_.l_onoff = e; }

  std::chrono::seconds timeout() const noexcept {
    return std::chrono::seconds(value_.l_linger);
  }
  void timeout(std::chrono::seconds t) noexcept {
    value_.l_linger = static_cast<int>(t.count());
  }

  template <class Protocol>
  int level(const Protocol & /* p */) const noexcept {
    return SOL_SOCKET;
  }

  template <class Protocol>
  int name(const Protocol & /* p */) const noexcept {
    return SO_LINGER;
  }

  template <class Protocol>
  void *data(const Protocol & /* p */) noexcept {
    return std::addressof(value_);
  }

  template <class Protocol>
  const void *data(const Protocol & /* p */) const noexcept {
    return std::addressof(value_);
  }

  template <class Protocol>
  size_t size(const Protocol & /* p */) const noexcept {
    return sizeof(value_);
  }

  template <class Protocol>
  void resize(const Protocol & /* p */, size_t s) {
    if (s != sizeof(value_)) {
      throw std::length_error("size != sizeof(::linger)");
    }
  }

 private:
  ::linger value_;
};

class socket_base::out_of_band_inline
    : public socket_option::boolean<SOL_SOCKET, SO_OOBINLINE> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_OOBINLINE>;
  using __base::__base;
};

class socket_base::receive_buffer_size
    : public socket_option::integer<SOL_SOCKET, SO_RCVBUF> {
  using __base = socket_option::integer<SOL_SOCKET, SO_RCVBUF>;
  using __base::__base;
};

class socket_base::receive_low_watermark
    : public socket_option::integer<SOL_SOCKET, SO_RCVLOWAT> {
  using __base = socket_option::integer<SOL_SOCKET, SO_RCVLOWAT>;
  using __base::__base;
};

class socket_base::reuse_address
    : public socket_option::boolean<SOL_SOCKET, SO_REUSEADDR> {
  using __base = socket_option::boolean<SOL_SOCKET, SO_REUSEADDR>;
  using __base::__base;
};

class socket_base::send_buffer_size
    : public socket_option::integer<SOL_SOCKET, SO_SNDBUF> {
  using __base = socket_option::integer<SOL_SOCKET, SO_SNDBUF>;
  using __base::__base;
};

class socket_base::send_low_watermark
    : public socket_option::integer<SOL_SOCKET, SO_SNDLOWAT> {
  using __base = socket_option::integer<SOL_SOCKET, SO_SNDLOWAT>;
  using __base::__base;
};

}  // namespace net

#endif
