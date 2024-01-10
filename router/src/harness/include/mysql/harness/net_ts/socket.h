/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include <bitset>
#include <stdexcept>  // length_error
#include <system_error>

#ifdef _WIN32
#include <WS2tcpip.h>  // inet_ntop
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/types.h>  // before netinet* on FreeBSD for uint8_t

#include <arpa/inet.h>    // htons
#include <netinet/in.h>   // in_addr_t
#include <netinet/ip6.h>  // in6_addr_t
#include <netinet/tcp.h>  // TCP_NODELAY
#include <sys/ioctl.h>    // FIONREAD, SIOATMARK
#include <termios.h>      // TIOCOUTQ
#ifdef __sun__
#include <sys/filio.h>   // FIONREAD
#include <sys/sockio.h>  // SIOATMARK
#endif
#endif

#include "mysql/harness/net_ts/netfwd.h"

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected.h"

namespace net {

// 18.3 [socket.err]
// implemented in impl/socket_error.h

// 18.5 [socket.opt]

namespace socket_option {

/**
 * base-class of socket options.
 *
 * can be used to implement type safe socket options.
 *
 * @see socket_option::integer
 * @see socket_option::boolean
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

// 18.4 [socket.base]

class socket_base {
 public:
  using broadcast = socket_option::boolean<SOL_SOCKET, SO_BROADCAST>;
  using debug = socket_option::boolean<SOL_SOCKET, SO_DEBUG>;
  using do_not_route = socket_option::boolean<SOL_SOCKET, SO_DONTROUTE>;
  using error =
      socket_option::integer<SOL_SOCKET, SO_ERROR>;  // not part of std
  using keep_alive = socket_option::boolean<SOL_SOCKET, SO_KEEPALIVE>;

  class linger;

  using out_of_band_inline = socket_option::boolean<SOL_SOCKET, SO_OOBINLINE>;
  using receive_buffer_size = socket_option::integer<SOL_SOCKET, SO_RCVBUF>;
  using receive_low_watermark = socket_option::integer<SOL_SOCKET, SO_RCVLOWAT>;
  using reuse_address = socket_option::boolean<SOL_SOCKET, SO_REUSEADDR>;
  using send_buffer_size = socket_option::integer<SOL_SOCKET, SO_SNDBUF>;
  using send_low_watermark = socket_option::integer<SOL_SOCKET, SO_SNDLOWAT>;

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
     * set recipient of the message.
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

/**
 * template-less base-class of basic_socket_impl.
 *
 * all of the parts of basic_socket_impl that are not dependent on Protocol
 * - native_handle
 * - non-blocking flags
 */
class basic_socket_impl_base {
 public:
  using native_handle_type = impl::socket::native_handle_type;
  using executor_type = io_context::executor_type;

  constexpr explicit basic_socket_impl_base(io_context &ctx) : io_ctx_{&ctx} {}

  basic_socket_impl_base(const basic_socket_impl_base &rhs) = delete;
  basic_socket_impl_base &operator=(const basic_socket_impl_base &rhs) = delete;

  basic_socket_impl_base(basic_socket_impl_base &&rhs) noexcept
      : native_handle_{std::exchange(rhs.native_handle_,
                                     impl::socket::kInvalidSocket)},
        non_blocking_{std::exchange(rhs.non_blocking_, 0)},
        native_non_blocking_{std::exchange(rhs.native_non_blocking_, 0)},
        io_ctx_{std::move(rhs.io_ctx_)} {}

  basic_socket_impl_base &operator=(basic_socket_impl_base &&rhs) noexcept {
    native_handle_ =
        std::exchange(rhs.native_handle_, impl::socket::kInvalidSocket);
    non_blocking_ = std::exchange(rhs.non_blocking_, 0);
    native_non_blocking_ = std::exchange(rhs.native_non_blocking_, 0);
    io_ctx_ = rhs.io_ctx_;

    return *this;
  }

  ~basic_socket_impl_base() = default;

  constexpr native_handle_type native_handle() const noexcept {
    return native_handle_;
  }

  constexpr bool is_open() const noexcept {
    return native_handle() != impl::socket::kInvalidSocket;
  }

  constexpr bool non_blocking() const { return non_blocking_; }

  stdx::expected<void, std::error_code> non_blocking(bool mode) {
    if (!is_open()) {
      return stdx::unexpected(
          std::make_error_code(std::errc::bad_file_descriptor));
    }
    non_blocking_ = mode;

    return {};
  }

  bool native_non_blocking() const {
    if (static_cast<char>(-1) != native_non_blocking_)
      return native_non_blocking_;

    auto res = io_ctx_->socket_service()->native_non_blocking(native_handle());
    if (res) native_non_blocking_ = *res;

    return native_non_blocking_;
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool mode) {
    if (!is_open()) {
      return stdx::unexpected(
          std::make_error_code(std::errc::bad_file_descriptor));
    }

    if (!mode && non_blocking()) {
      return stdx::unexpected(
          std::make_error_code(std::errc::invalid_argument));
    }

    auto res =
        io_ctx_->socket_service()->native_non_blocking(native_handle(), mode);
    if (!res) return res;

    native_non_blocking_ = mode ? 1 : 0;

    return res;
  }

  executor_type get_executor() noexcept { return io_ctx_->get_executor(); }

  stdx::expected<void, std::error_code> close() {
    if (is_open()) {
      cancel();

      auto res = io_ctx_->socket_service()->close(native_handle());

      // after close() finished, the socket's state is undefined even if it
      // failed. See "man close" on Linux.
      native_handle_ = impl::socket::kInvalidSocket;

      return res;
    }

    return {};
  }

  stdx::expected<void, std::error_code> cancel() {
    if (!is_open()) {
      return stdx::unexpected(make_error_code(std::errc::bad_file_descriptor));
    }

    io_ctx_->cancel(native_handle());
    return {};
  }

  stdx::expected<native_handle_type, std::error_code> release() {
    if (is_open()) {
      cancel();
    }
    return std::exchange(native_handle_, impl::socket::kInvalidSocket);
  }

 protected:
  native_handle_type native_handle_{impl::socket::kInvalidSocket};

  bool non_blocking_{false};
  mutable char native_non_blocking_{
#ifdef _WIN32
      // on windows we can't detect the non-blocking state.
      //
      // Let's assume it defaults to 0.  *fingerscross*
      0
#else
      // on unixes we don't know what the state of non-blocking is after
      // accept(), better check for it.
      static_cast<char>(-1)
#endif
  };

  io_context *io_ctx_;
};

// 18.6 [socket.basic]
template <typename Protocol>
class basic_socket_impl : public basic_socket_impl_base {
 public:
  using __base = basic_socket_impl_base;

  using executor_type = io_context::executor_type;
  using protocol_type = Protocol;
  using endpoint_type = typename protocol_type::endpoint;
  using error_type = std::error_code;
  using socket_type = typename protocol_type::socket;

  constexpr explicit basic_socket_impl(io_context &ctx) noexcept
      : __base{ctx} {}

  basic_socket_impl(const basic_socket_impl &) = delete;
  basic_socket_impl &operator=(const basic_socket_impl &) = delete;
  basic_socket_impl(basic_socket_impl &&rhs) = default;

  basic_socket_impl &operator=(basic_socket_impl &&rhs) noexcept {
    if (this == std::addressof(rhs)) {
      return *this;
    }

    close();
    __base::operator=(std::move(rhs));

    return *this;
  }

  ~basic_socket_impl() {
    if (is_open()) close();
  }

  stdx::expected<void, error_type> open(
      const protocol_type &protocol = protocol_type(), int flags = 0) {
    if (is_open()) {
      return stdx::unexpected(make_error_code(socket_errc::already_open));
    }
    auto res = io_ctx_->socket_service()->socket(
        protocol.family(), protocol.type() | flags, protocol.protocol());
    if (!res) return stdx::unexpected(res.error());
#ifdef SOCK_NONBLOCK
    if ((flags & SOCK_NONBLOCK) != 0) {
      native_non_blocking_ = 1;
    }
#endif

    return assign(protocol, res.value());
  }

  stdx::expected<void, error_type> assign(
      const protocol_type &protocol, const native_handle_type &native_handle) {
    if (is_open()) {
      return stdx::unexpected(make_error_code(socket_errc::already_open));
    }
    protocol_ = protocol;
    native_handle_ = native_handle;

    return {};
  }

  stdx::expected<void, error_type> bind(const endpoint_type &endpoint) {
    return io_ctx_->socket_service()->bind(
        native_handle(),
        reinterpret_cast<const struct sockaddr *>(endpoint.data()),
        endpoint.size());
  }

  stdx::expected<socket_type, error_type> accept(io_context &io_ctx,
                                                 struct sockaddr *endpoint_data,
                                                 socklen_t *endpoint_size,
                                                 int flags = 0) {
    using ret_type = stdx::expected<socket_type, error_type>;
    if (flags != 0) {
      auto res = io_ctx_->socket_service()->accept4(
          native_handle(), endpoint_data, endpoint_size, flags);
      if (res) {
        return socket_type{io_ctx, protocol_, res.value()};
      } else if (res.error() != std::errc::operation_not_supported) {
        return stdx::unexpected(res.error());
      }

      // otherwise fall through to accept without flags
    }
    auto res = io_ctx_->socket_service()->accept(native_handle(), endpoint_data,
                                                 endpoint_size);
    if (!res) return stdx::unexpected(res.error());

    return ret_type{std::in_place, io_ctx, protocol_, std::move(res.value())};
  }

  stdx::expected<socket_type, error_type> accept(io_context &io_ctx,
                                                 int flags = 0) {
    return accept(io_ctx, static_cast<struct sockaddr *>(nullptr), nullptr,
                  flags);
  }

  stdx::expected<socket_type, error_type> accept(io_context &io_ctx,
                                                 endpoint_type &endpoint,
                                                 int flags = 0) {
    socklen_t endpoint_len = endpoint.capacity();

    auto res = accept(io_ctx, static_cast<struct sockaddr *>(endpoint.data()),
                      &endpoint_len, flags);
    if (res) {
      endpoint.resize(endpoint_len);
    }
    return res;
  }

  stdx::expected<void, error_type> listen(
      int backlog = socket_base::max_listen_connections) {
    return io_ctx_->socket_service()->listen(native_handle(), backlog);
  }

  template <typename SettableSocketOption>
  stdx::expected<void, error_type> set_option(
      const SettableSocketOption &option) {
    return io_ctx_->socket_service()->setsockopt(
        native_handle(), option.level(protocol_), option.name(protocol_),
        option.data(protocol_), static_cast<socklen_t>(option.size(protocol_)));
  }

  template <typename GettableSocketOption>
  stdx::expected<void, error_type> get_option(
      GettableSocketOption &option) const {
    socklen_t option_len = option.size(protocol_);
    auto res = io_ctx_->socket_service()->getsockopt(
        native_handle(), option.level(protocol_), option.name(protocol_),
        option.data(protocol_), &option_len);

    if (!res) return res;

    option.resize(protocol_, option_len);

    return {};
  }

  stdx::expected<endpoint_type, error_type> local_endpoint() const {
    endpoint_type ep;
    size_t ep_len = ep.capacity();

    auto res = io_ctx_->socket_service()->getsockname(
        native_handle(), reinterpret_cast<struct sockaddr *>(ep.data()),
        &ep_len);
    if (!res) return stdx::unexpected(res.error());

    ep.resize(ep_len);

    return ep;
  }

  stdx::expected<endpoint_type, error_type> remote_endpoint() const {
    endpoint_type ep;
    size_t ep_len = ep.capacity();

    auto res = io_ctx_->socket_service()->getpeername(
        native_handle(), reinterpret_cast<struct sockaddr *>(ep.data()),
        &ep_len);
    if (!res) return stdx::unexpected(res.error());

    ep.resize(ep_len);

    return ep;
  }

  // NOLINTNEXTLINE(google-runtime-int)
  template <unsigned long Name, class T>
  class IoControl {
   public:
    using value_type = T;

    constexpr IoControl() : val_{} {}

    constexpr explicit IoControl(value_type v) : val_{v} {}

    // NOLINTNEXTLINE(google-runtime-int)
    constexpr unsigned long name() const { return Name; }

    constexpr void *data() { return &val_; }

    constexpr value_type value() const { return val_; }

   private:
    value_type val_;
  };

  using io_control_bytes_avail_recv = IoControl<FIONREAD, int>;
  using io_control_at_mark = IoControl<SIOCATMARK, int>;
#ifndef _WIN32
  using io_control_bytes_avail_send = IoControl<TIOCOUTQ, int>;
#else
  using io_control_socket_set_high_water_mark = IoControl<SIOCSHIWAT, int>;
  using io_control_socket_get_high_water_mark = IoControl<SIOCGHIWAT, int>;
  using io_control_socket_set_low_water_mark = IoControl<SIOCSLOWAT, int>;
  using io_control_socket_get_low_water_mark = IoControl<SIOCGLOWAT, int>;
#endif

  template <class IoControlCommand>
  stdx::expected<void, error_type> io_control(IoControlCommand &cmd) const {
    return io_ctx_->socket_service()->ioctl(native_handle(), cmd.name(),
                                            cmd.data());
  }

  stdx::expected<size_t, error_type> available() const {
    io_control_bytes_avail_recv ioc;

    auto res = io_control(ioc);
    if (!res) return stdx::unexpected(res.error());

    if (ioc.value() < 0) {
      return stdx::unexpected(make_error_code(std::errc::invalid_seek));
    }

    // value is not negative, safe to cast to unsigned
    return static_cast<size_t>(ioc.value());
  }

  stdx::expected<bool, error_type> at_mark() const {
    io_control_at_mark ioc;

    auto res = io_control(ioc);
    if (!res) return stdx::unexpected(res.error());

    return ioc.value();
  }

  stdx::expected<void, error_type> shutdown(
      socket_base::shutdown_type st) const {
    return io_ctx_->socket_service()->shutdown(native_handle(),
                                               static_cast<int>(st));
  }

  stdx::expected<void, std::error_code> wait(socket_base::wait_type wt) {
    return io_ctx_->socket_service()->wait(native_handle(), wt);
  }

 private:
  protocol_type protocol_{endpoint_type{}.protocol()};
};

template <typename Protocol>
class basic_socket : public socket_base, private basic_socket_impl<Protocol> {
  using __base = basic_socket_impl<Protocol>;

 public:
  using executor_type = io_context::executor_type;
  using protocol_type = Protocol;
  using native_handle_type = impl::socket::native_handle_type;
  using error_type = impl::socket::error_type;
  using endpoint_type = typename protocol_type::endpoint;

  executor_type get_executor() noexcept { return __base::get_executor(); }

  stdx::expected<void, error_type> assign(
      const protocol_type &protocol, const native_handle_type &native_handle) {
    return __base::assign(protocol, native_handle);
  }

  stdx::expected<void, error_type> open(
      const protocol_type &protocol = protocol_type(), int flags = 0) {
    return __base::open(protocol, flags);
  }

  stdx::expected<void, error_type> connect(const endpoint_type &endpoint) {
    if (!is_open()) {
      auto res = open(endpoint.protocol());
      if (!res) return res;
    }
    return get_executor().context().socket_service()->connect(
        native_handle(),
        reinterpret_cast<const struct sockaddr *>(endpoint.data()),
        endpoint.size());
  }

  template <class CompletionToken>
  auto async_connect(const endpoint_type &endpoint, CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code)> init{token};

    if (!is_open()) {
      auto res = open(endpoint.protocol());
      if (!res) {
        init.completion_handler(res.error());

        return;
      }
    }

    net::defer(get_executor(), [this, endpoint,
                                __compl_handler =
                                    std::move(init.completion_handler)]() {
      // remember the non-blocking flag.
      const auto old_non_blocking = native_non_blocking();
      if (old_non_blocking == false) native_non_blocking(true);

      auto res = connect(endpoint);

      // restore the non-blocking flag if needed.
      if (old_non_blocking == false) native_non_blocking(false);

      if (res) {
        __compl_handler({});
      } else {
        const auto ec = res.error();

        if ((ec !=
             make_error_condition(std::errc::operation_in_progress)) /* posix */
            && (ec != make_error_condition(
                          std::errc::operation_would_block)) /* windows */) {
          __compl_handler(ec);
        } else {
          get_executor().context().async_wait(
              native_handle(), net::socket_base::wait_write,
              [this, __compl_handler = std::move(__compl_handler)](
                  std::error_code error_code) mutable {
                if (error_code) {
                  __compl_handler(error_code);
                  return;
                }

                // finish the non-blocking connect
                net::socket_base::error so_error;

                auto result = get_option(so_error);
                if (!result) {
                  __compl_handler(result.error());
                  return;
                }

                // if so_error.value() is 0, the error_code will be 0 too
                __compl_handler(
                    impl::socket::make_error_code(so_error.value()));
              });
        }
      }
    });

    return init.result.get();
  }

  stdx::expected<void, error_type> bind(const endpoint_type &endpoint) {
    return __base::bind(endpoint);
  }

  native_handle_type native_handle() const noexcept {
    return __base::native_handle();
  }

  template <typename SettableSocketOption>
  stdx::expected<void, error_type> set_option(
      const SettableSocketOption &option) {
    return __base::set_option(option);
  }

  template <typename GettableSocketOption>
  stdx::expected<void, error_type> get_option(
      GettableSocketOption &option) const {
    return __base::get_option(option);
  }

  stdx::expected<void, error_type> close() { return __base::close(); }

  stdx::expected<void, error_type> cancel() { return __base::cancel(); }

  stdx::expected<native_handle_type, error_type> release() {
    return __base::release();
  }

  constexpr bool is_open() const { return __base::is_open(); }

  stdx::expected<endpoint_type, error_type> local_endpoint() const {
    return __base::local_endpoint();
  }

  stdx::expected<endpoint_type, error_type> remote_endpoint() const {
    return __base::remote_endpoint();
  }

  stdx::expected<size_t, error_type> available() const {
    return __base::available();
  }

  bool non_blocking() const { return __base::non_blocking(); }

  stdx::expected<void, std::error_code> non_blocking(bool mode) {
    return __base::non_blocking(mode);
  }

  bool native_non_blocking() const { return __base::native_non_blocking(); }

  stdx::expected<void, std::error_code> native_non_blocking(bool mode) {
    return __base::native_non_blocking(mode);
  }

  stdx::expected<void, std::error_code> wait(socket_base::wait_type wt) {
    return __base::wait(wt);
  }

  stdx::expected<void, error_type> shutdown(
      socket_base::shutdown_type st) const {
    return __base::shutdown(st);
  }

  template <typename CompletionToken>
  auto async_wait(wait_type w, CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code)> init{token};

    get_executor().context().async_wait(
        native_handle(), w,
        [__compl_handler = std::move(init.completion_handler)](
            std::error_code ec) mutable { __compl_handler(ec); });

    return init.result.get();
  }

 protected:
  explicit basic_socket(io_context &ctx) : __base{ctx} {}

  basic_socket(io_context &ctx, const protocol_type &proto)
      : __base{ctx, proto} {}

  basic_socket(io_context &ctx, const protocol_type &proto,
               const native_handle_type &native_handle)
      : __base{ctx} {
    assign(proto, native_handle);
  }

  // as demanded by networking-ts
  // NOLINTNEXTLINE(hicpp-use-equals-delete,modernize-use-equals-delete)
  basic_socket(const basic_socket &) = delete;
  // NOLINTNEXTLINE(hicpp-use-equals-delete,modernize-use-equals-delete)
  basic_socket &operator=(const basic_socket &) = delete;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_socket(basic_socket &&other) = default;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_socket &operator=(basic_socket &&) = default;

  ~basic_socket() = default;
};

// 18.7 [socket.dgram]
template <typename Protocol>
class basic_datagram_socket : public basic_socket<Protocol> {
  using __base = basic_socket<Protocol>;

 public:
  using protocol_type = Protocol;
  using native_handle_type = impl::socket::native_handle_type;
  using endpoint_type = typename protocol_type::endpoint;
  using error_type = std::error_code;

  explicit basic_datagram_socket(io_context &ctx) : __base(ctx) {}
  basic_datagram_socket(io_context &ctx, const protocol_type &proto)
      : __base(ctx, proto) {}

  basic_datagram_socket(const basic_datagram_socket &) = delete;
  basic_datagram_socket &operator=(const basic_datagram_socket &) = delete;

  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_datagram_socket(basic_datagram_socket &&other) = default;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_datagram_socket &operator=(basic_datagram_socket &&) = default;

  ~basic_datagram_socket() = default;

  //
  basic_datagram_socket(io_context &ctx, const protocol_type &protocol,
                        const native_handle_type &native_handle)
      : __base(ctx, protocol, native_handle) {}

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive(
      const MutableBufferSequence &buffers, socket_base::message_flags flags) {
    socket_base::msg_hdr msg(buffers);

    return this->get_executor().context().socket_service()->recvmsg(
        __base::native_handle(), msg, flags);
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive(
      const MutableBufferSequence &buffers) {
    return receive(buffers, 0);
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive_from(
      const MutableBufferSequence &buffers, endpoint_type &sender,
      socket_base::message_flags flags) {
    socket_base::msg_hdr msg(buffers);
    msg.set_sender(sender);

    auto res = this->get_executor().context().socket_service()->recvmsg(
        __base::native_handle(), msg, flags);

    if (res) {
      // resize the endpoint
      msg.resize_sender(sender);
    }

    return res;
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive_from(
      const MutableBufferSequence &buffers, endpoint_type &sender) {
    return receive_from(buffers, sender, 0);
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> read_some(
      const MutableBufferSequence &buffers) {
    return receive(buffers);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send(
      const ConstBufferSequence &buffers, socket_base::message_flags flags) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    socket_base::msg_hdr msg(buffers);

    return this->get_executor().context().socket_service()->sendmsg(
        __base::native_handle(), msg, flags);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send(
      const ConstBufferSequence &buffers) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    return send(buffers, 0);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send_to(
      const ConstBufferSequence &buffers, const endpoint_type &recipient,
      socket_base::message_flags flags) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    socket_base::msg_hdr msg(buffers);
    msg.set_recipient(recipient);

    return this->get_executor().context().socket_service()->sendmsg(
        __base::native_handle(), msg, flags);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send_to(
      const ConstBufferSequence &buffers, const endpoint_type &recipient) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    return send_to(buffers, recipient, 0);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffers) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    return send(buffers);
  }

  stdx::expected<bool, error_type> shutdown(
      socket_base::shutdown_type st) const {
    return __base::shutdown(st);
  }

  template <class MutableBufferSequence, class CompletionToken>
  auto async_receive(const MutableBufferSequence &buffers,
                     socket_base::message_flags flags,
                     CompletionToken &&token) {
    static_assert(
        net::is_mutable_buffer_sequence<MutableBufferSequence>::value);

    async_completion<CompletionToken, void(std::error_code, size_t)> init{
        token};

    if ((flags & socket_base::message_peek).any()) {
      // required by std::
      init.completion_handler(make_error_code(std::errc::invalid_argument), 0);

      return init.result.get();
    }

    this->get_executor().context().async_wait(
        this->native_handle(), socket_base::wait_read,
        [socket_service = this->get_executor().context().socket_service(),
         compl_handler = std::move(init.completion_handler), &buffers,
         native_handle = this->native_handle(),
         flags](std::error_code ec) mutable {
          if (ec) {
            compl_handler(ec, 0);
            return;
          }

          socket_base::msg_hdr msgs(buffers);

          auto res = socket_service->recvmsg(native_handle, msgs, flags);
          if (!res) {
            compl_handler(res.error(), 0);
          } else {
            compl_handler({}, res.value());
          }
        });

    return init.result.get();
  }

  template <class MutableBufferSequence, class CompletionToken>
  auto async_receive(const MutableBufferSequence &buffers,
                     CompletionToken &&token) {
    static_assert(
        net::is_mutable_buffer_sequence<MutableBufferSequence>::value);

    return async_receive(buffers, 0, std::forward<CompletionToken>(token));
  }
};

// 18.8 [socket.stream]
template <typename Protocol>
class basic_stream_socket : public basic_socket<Protocol> {
  using __base = basic_socket<Protocol>;

 public:
  using protocol_type = Protocol;
  using native_handle_type = impl::socket::native_handle_type;
  using endpoint_type = typename protocol_type::endpoint;
  using error_type = std::error_code;

  explicit basic_stream_socket(io_context &ctx) : __base(ctx) {}
  basic_stream_socket(io_context &ctx, const protocol_type &proto)
      : __base(ctx, proto) {}

  basic_stream_socket(const basic_stream_socket &) = delete;
  basic_stream_socket &operator=(const basic_stream_socket &) = delete;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_stream_socket(basic_stream_socket &&other) = default;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_stream_socket &operator=(basic_stream_socket &&) = default;

  ~basic_stream_socket() = default;
  //
  basic_stream_socket(io_context &ctx, const protocol_type &protocol,
                      const native_handle_type &native_handle)
      : __base(ctx, protocol, native_handle) {}

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive(
      const MutableBufferSequence &buffers, socket_base::message_flags flags) {
    if (buffer_size(buffers) == 0) return 0;

    socket_base::msg_hdr msg(buffers);

    auto res = this->get_executor().context().socket_service()->recvmsg(
        __base::native_handle(), msg, flags);

    if (res && res.value() == 0) {
      // remote did a orderly shutdown
      return stdx::unexpected(make_error_code(net::stream_errc::eof));
    }

    return res;
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> receive(
      const MutableBufferSequence &buffers) {
    return receive(buffers, 0);
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> read_some(
      const MutableBufferSequence &buffers) {
    return receive(buffers);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send(
      const ConstBufferSequence &buffers, socket_base::message_flags flags) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    if (buffer_size(buffers) == 0) return 0;

    socket_base::msg_hdr msg(buffers);

    auto res = this->get_executor().context().socket_service()->sendmsg(
        __base::native_handle(), msg, flags);

    if (res && res.value() == 0) {
      // remote did a orderly shutdown
      return stdx::unexpected(make_error_code(net::stream_errc::eof));
    }

    return res;
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> send(
      const ConstBufferSequence &buffers) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    return send(buffers, 0);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffers) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");
    return send(buffers);
  }

  stdx::expected<void, error_type> shutdown(
      socket_base::shutdown_type st) const {
    return __base::shutdown(st);
  }

  template <class MutableBufferSequence, class CompletionToken>
  auto async_receive(const MutableBufferSequence &buffers,
                     socket_base::message_flags flags,
                     CompletionToken &&token) {
    static_assert(
        net::is_mutable_buffer_sequence<MutableBufferSequence>::value);

    async_completion<CompletionToken, void(std::error_code, size_t)> init{
        token};
    if ((flags & socket_base::message_peek).any()) {
      // required by std::
      init.completion_handler(make_error_code(std::errc::invalid_argument), 0);

      return init.result.get();
    }

    if (buffer_size(buffers) == 0) {
      init.completion_handler({}, 0);

      return init.result.get();
    }

    this->get_executor().context().async_wait(
        this->native_handle(), socket_base::wait_read,
        [socket_service = this->get_executor().context().socket_service(),
         compl_handler = std::move(init.completion_handler), buffers,
         native_handle = this->native_handle(), flags](std::error_code ec) {
          if (ec) {
            compl_handler(ec, 0);
            return;
          }

          socket_base::msg_hdr msgs(buffers);

          const auto res = socket_service->recvmsg(native_handle, msgs, flags);
          if (!res) {
            compl_handler(res.error(), 0);
          } else if (res.value() == 0) {
            // remote did a orderly shutdown
            compl_handler(make_error_code(stream_errc::eof), 0);
          } else {
            compl_handler({}, res.value());
          }

          return;
        });

    return init.result.get();
  }

  template <class MutableBufferSequence, class CompletionToken>
  auto async_receive(const MutableBufferSequence &buffers,
                     CompletionToken &&token) {
    static_assert(
        net::is_mutable_buffer_sequence<MutableBufferSequence>::value);

    return async_receive(buffers, 0, std::forward<CompletionToken>(token));
  }

  template <class ConstBufferSequence, class CompletionToken>
  auto async_send(const ConstBufferSequence &buffers,
                  socket_base::message_flags flags, CompletionToken &&token) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value);
    async_completion<CompletionToken, void(std::error_code, size_t)> init{
        token};
    if (buffer_size(buffers) == 0) {
      init.completion_handler({}, 0);

      return init.result.get();
    }

    this->get_executor().context().async_wait(
        this->native_handle(), socket_base::wait_write,
        [socket_service = this->get_executor().context().socket_service(),
         compl_handler = std::move(init.completion_handler), buffers,
         native_handle = this->native_handle(), flags](std::error_code ec) {
          if (ec) {
            compl_handler(ec, 0);
            return;
          }

          socket_base::msg_hdr msgs(buffers);

          const auto res = socket_service->sendmsg(native_handle, msgs, flags);
          if (!res) {
            compl_handler(res.error(), 0);
          } else {
            compl_handler({}, res.value());
          }

          return;
        });

    return init.result.get();
  }

  template <class ConstBufferSequence, class CompletionToken>
  auto async_send(const ConstBufferSequence &buffers, CompletionToken &&token) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value);
    return async_send(buffers, 0, std::forward<CompletionToken>(token));
  }
};

// 18.9 [socket.acceptor]
template <typename AcceptableProtocol>
class basic_socket_acceptor : public socket_base,
                              private basic_socket_impl<AcceptableProtocol> {
  using __base = basic_socket_impl<AcceptableProtocol>;

 public:
  using executor_type = io_context::executor_type;
  using native_handle_type = impl::socket::native_handle_type;
  using protocol_type = AcceptableProtocol;
  using socket_type = typename protocol_type::socket;
  using endpoint_type = typename protocol_type::endpoint;
  using error_type = std::error_code;

  explicit basic_socket_acceptor(io_context &ctx)
      : __base(ctx), protocol_{endpoint_type().protocol()} {}

  executor_type get_executor() noexcept { return __base::get_executor(); }

  stdx::expected<void, error_type> open(
      const protocol_type &protocol = protocol_type(), int flags = 0) {
    return __base::open(protocol, flags);
  }

  stdx::expected<void, error_type> assign(
      const protocol_type &protocol,
      const native_handle_type &native_acceptor) {
    return __base::assign(protocol, native_acceptor);
  }

  stdx::expected<native_handle_type, error_type> release() {
    return __base::release();
  }

  native_handle_type native_handle() const { return __base::native_handle(); }

  constexpr bool is_open() const { return __base::is_open(); }

  stdx::expected<void, error_type> close() { return __base::close(); }

  stdx::expected<void, error_type> cancel() { return __base::cancel(); }

  template <typename SettableSocketOption>
  stdx::expected<void, error_type> set_option(
      const SettableSocketOption &option) {
    return __base::set_option(option);
  }

  template <typename GettableSocketOption>
  stdx::expected<void, error_type> get_option(
      GettableSocketOption &option) const {
    return __base::get_option(option);
  }

  bool non_blocking() const { return __base::non_blocking(); }

  stdx::expected<void, std::error_code> non_blocking(bool mode) {
    return __base::non_blocking(mode);
  }

  bool native_non_blocking() const { return __base::native_non_blocking(); }

  stdx::expected<void, std::error_code> native_non_blocking(bool mode) {
    return __base::native_non_blocking(mode);
  }

  stdx::expected<void, error_type> bind(const endpoint_type &endpoint) {
    return __base::bind(endpoint);
  }

  stdx::expected<void, error_type> listen(int backlog) {
    return __base::listen(backlog);
  }

  stdx::expected<endpoint_type, error_type> local_endpoint() const {
    return __base::local_endpoint();
  }

  // enable_connection_aborted(bool) - not implemented

  bool enable_connection_aborted() const { return enable_connection_aborted_; }

  stdx::expected<socket_type, error_type> accept(int flags = 0) {
    return accept(get_executor().context(), flags);
  }

  stdx::expected<socket_type, error_type> accept(io_context &io_ctx,
                                                 int flags = 0) {
    // in case of ECONNABORTED, retry if not explicitly enabled
    while (true) {
      auto accept_res = __base::accept(io_ctx, flags);

      if (accept_res || enable_connection_aborted() ||
          accept_res.error() !=
              make_error_condition(std::errc::connection_aborted)) {
        return accept_res;
      }
    }
  }

  stdx::expected<socket_type, error_type> accept(endpoint_type &endpoint,
                                                 int flags = 0) {
    return accept(get_executor().context(), endpoint, flags);
  }

  stdx::expected<socket_type, error_type> accept(io_context &io_ctx,
                                                 endpoint_type &endpoint,
                                                 int flags = 0) {
    // in case of ECONNABORTED, retry if not explicitly enabled
    while (true) {
      auto accept_res = __base::accept(io_ctx, endpoint, flags);

      if (accept_res || enable_connection_aborted() ||
          accept_res.error() !=
              make_error_condition(std::errc::connection_aborted)) {
        return accept_res;
      }
    }
  }

  template <class CompletionToken>
  auto async_accept(io_context &io_ctx, CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code, socket_type)> init{
        token};

    // do something
    io_ctx.get_executor().context().async_wait(
        native_handle(), socket_base::wait_read,
        [this, __compl_handler = std::move(init.completion_handler),
         __protocol = protocol_, __fd = native_handle(),
         &io_ctx](std::error_code ec) mutable {
          if (ec) {
            __compl_handler(ec, socket_type(io_ctx));
            return;
          }

          while (true) {
            auto res = this->get_executor().context().socket_service()->accept(
                __fd, nullptr, nullptr);

            if (!res && !enable_connection_aborted() &&
                res.error() ==
                    make_error_condition(std::errc::connection_aborted)) {
              continue;
            }

            if (!res) {
              __compl_handler(res.error(), socket_type(io_ctx));
            } else {
              __compl_handler({}, socket_type{io_ctx, __protocol, res.value()});
            }
            return;
          }
        });

    return init.result.get();
  }

  template <class CompletionToken>
  auto async_accept(endpoint_type &endpoint, CompletionToken &&token) {
    return async_accept(get_executor().context(), endpoint,
                        std::forward<CompletionToken>(token));
  }

  /**
   * accept a connection with endpoint async'.
   *
   * - returns immediately
   * - calls completiontoken when finished
   *
   * @param [in,out] io_ctx io-context to execute the waiting/execution in
   * @param [out] endpoint remote endpoint of the accepted connection
   * @param [in] token completion token of type 'void(std::error_code,
   * socket_type)'
   */
  template <class CompletionToken>
  auto async_accept(io_context &io_ctx, endpoint_type &endpoint,
                    CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code, socket_type)> init{
        token};

    // - wait for acceptor to become readable
    // - accept() it.
    // - call completion with socket
    io_ctx.get_executor().context().async_wait(
        native_handle(), socket_base::wait_read,
        [this, __compl_handler = std::move(init.completion_handler),
         __protocol = protocol_, __fd = native_handle(), &__ep = endpoint,
         &io_ctx](std::error_code ec) mutable {
          if (ec) {
            __compl_handler(ec, socket_type(io_ctx));
            return;
          }

          while (true) {
            socklen_t endpoint_len = __ep.capacity();

            auto res = this->get_executor().context().socket_service()->accept(
                __fd, static_cast<sockaddr *>(__ep.data()), &endpoint_len);

            if (!res && !enable_connection_aborted() &&
                res.error() ==
                    make_error_condition(std::errc::connection_aborted)) {
              continue;
            }

            if (!res) {
              __compl_handler(res.error(), socket_type(io_ctx));
            } else {
              __ep.resize(endpoint_len);

              __compl_handler({}, socket_type{io_ctx, __protocol, res.value()});
            }
            return;
          }
        });

    return init.result.get();
  }

  template <class CompletionToken>
  auto async_accept(CompletionToken &&token) {
    return async_accept(get_executor().context(),
                        std::forward<CompletionToken>(token));
  }

  stdx::expected<void, std::error_code> wait(socket_base::wait_type wt) {
    return __base::wait(wt);
  }

  template <typename CompletionToken>
  auto async_wait(wait_type w, CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code)> init{token};

    get_executor().context().async_wait(
        native_handle(), w,
        [compl_handler = std::move(init.completion_handler)](
            std::error_code ec) mutable { compl_handler(ec); });

    return init.result.get();
  }

 private:
  protocol_type protocol_;
  bool enable_connection_aborted_{false};
};

// 20.1 [socket.algo.connect]

/**
 * connect to the first endpoint that is connectable from a sequence of
 * endpoints.
 *
 * @param s socket that should be connected to an endpoint
 * @param endpoints a sequence of endpoints
 * @param c ConnectionCondition that must return true if the provided endpoint
 * should be attempted to be connected to
 *
 * @returns endpoint the connect succeeded for on success, last error-code
 * otherwise
 */
template <class Protocol, class EndpointSequence, class ConnectCondition>
stdx::expected<typename Protocol::endpoint, std::error_code> connect(
    basic_socket<Protocol> &s, const EndpointSequence &endpoints,
    ConnectCondition c) {
  return connect(s, std::begin(endpoints), std::end(endpoints), c);
}

/**
 * connect to the first endpoint that is connectable.
 *
 * @param s socket that should be connected to an endpoint
 * @param endpoints a sequence of endpoints
 *
 * @returns endpoint the connect succeeded for on success, last error-code
 * otherwise
 */
template <class Protocol, class EndpointSequence>
stdx::expected<typename Protocol::endpoint, std::error_code> connect(
    basic_socket<Protocol> &s, const EndpointSequence &endpoints) {
  return connect(s, endpoints, [](auto, auto) { return true; });
}

/**
 * connect to the first endpoint that is connectable from a range [first, last).
 *
 * @param s socket that should be connected to an endpoint
 * @param first iterator to the first endpoint
 * @param last iterator after to the last endpoint
 * @param c ConnectionCondition that must return true if the provided endpoint
 * should be attempted to be connected to
 *
 * @returns endpoint the connect succeeded for on success, last error-code
 * otherwise
 */
template <class Protocol, class InputIterator, class ConnectCondition>
stdx::expected<InputIterator, std::error_code> connect(
    basic_socket<Protocol> &s, InputIterator first, InputIterator last,
    ConnectCondition c) {
  // capture that last error-code for the connect-condition
  std::error_code ec;

  for (InputIterator cur = first; cur != last; ++cur) {
    // check if the endpoint should be connected to
    if (!c(ec, *cur)) continue;

    auto res = s.close();
    if (!res) {
      ec = res.error();
      continue;
    }

    res = s.open(typename Protocol::endpoint(*cur).protocol());
    if (!res) {
      ec = res.error();
      continue;
    }

    res = s.connect(*cur);
    if (!res) {
      ec = res.error();
    } else {
      return cur;
    }
  }

  return stdx::unexpected(make_error_code(socket_errc::not_found));
}

template <class Protocol, class InputIterator, class ConnectCondition>
stdx::expected<InputIterator, std::error_code> connect(
    basic_socket<Protocol> &s, InputIterator first, InputIterator last) {
  return connect(s, first, last, [](auto, auto) { return true; });
}

}  // namespace net

#if defined(MYSQL_HARNESS_HEADER_ONLY)
#include "mysql/harness/net_ts/socket.cpp"
#endif

#endif
