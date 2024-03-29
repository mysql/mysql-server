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
#ifndef MYSQL_HARNESS_NET_TS_WIN32_NAMED_PIPE_H_
#define MYSQL_HARNESS_NET_TS_WIN32_NAMED_PIPE_H_

#include "mysql/harness/stdx/expected.h"
#if defined(_WIN32)
#include <system_error>

#include <Windows.h>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/io_context.h"

/**
 * @file
 *
 * Named Pipe.
 *
 * windows only.
 *
 * @note
 * Experimental. Just barely enough that it mimiks the synchronous interface
 * for net::basis_socket and allows to:
 *
 * - for the server-side: open(), bind(), accept()
 * - for the client-side: open(), connect(), read(), write()
 *
 * and manage the lifetime of the managed HANDLE and automatically close() a
 * pipe on destruct.
 *
 * It doesn't support non-blocking IO, nor overlapped IO.
 */

namespace win32 {
inline std::error_code last_error_code() {
  return {static_cast<int>(GetLastError()), std::system_category()};
}
}  // namespace win32

namespace local {
namespace impl {
namespace named_pipe {
using native_handle_type = HANDLE;
static const native_handle_type kInvalidHandle{INVALID_HANDLE_VALUE};
}  // namespace named_pipe
}  // namespace impl

/**
 * implementation base for basic_named_pipe.
 *
 * all methods that are independent of the template parameters like Protocol.
 */
class basic_named_pipe_impl_base {
 public:
  using executor_type = net::io_context::executor_type;
  using native_handle_type = impl::named_pipe::native_handle_type;

  basic_named_pipe_impl_base(net::io_context &io_ctx) : io_ctx_{&io_ctx} {}

  basic_named_pipe_impl_base(const basic_named_pipe_impl_base &) = delete;
  basic_named_pipe_impl_base &operator=(const basic_named_pipe_impl_base &) =
      delete;
  basic_named_pipe_impl_base(basic_named_pipe_impl_base &&rhs) noexcept
      : native_handle_{std::exchange(rhs.native_handle_,
                                     impl::named_pipe::kInvalidHandle)},
        native_non_blocking_{std::exchange(rhs.native_non_blocking_, 0)},
        io_ctx_{std::move(rhs.io_ctx_)} {}

  basic_named_pipe_impl_base &operator=(
      basic_named_pipe_impl_base &&rhs) noexcept {
    native_handle_ =
        std::exchange(rhs.native_handle_, impl::named_pipe::kInvalidHandle);
    native_non_blocking_ = std::exchange(rhs.native_non_blocking_, 0);
    io_ctx_ = rhs.io_ctx_;

    return *this;
  }

  ~basic_named_pipe_impl_base() = default;

  constexpr native_handle_type native_handle() const noexcept {
    return native_handle_;
  }

  stdx::expected<void, std::error_code> open() { return {}; }

  bool is_open() const noexcept {
    return native_handle_ != impl::named_pipe::kInvalidHandle;
  }

  executor_type get_executor() noexcept { return io_ctx_->get_executor(); }

  stdx::expected<size_t, std::error_code> cancel() {
    // TODO: once the io-context has support for OVERLAPPED IO on windows
    // a full implemenantation can be done.

    return stdx::make_unexpected(
        make_error_code(std::errc::function_not_supported));
  }

  stdx::expected<native_handle_type, std::error_code> release() {
    if (is_open()) {
      cancel();
    }
    return std::exchange(native_handle_, impl::named_pipe::kInvalidHandle);
  }

 protected:
  native_handle_type native_handle_{impl::named_pipe::kInvalidHandle};

  int native_non_blocking_{0};

  net::io_context *io_ctx_;

  void native_handle(native_handle_type handle) { native_handle_ = handle; }
};

/**
 * implementation base class of basic_named_pipe.
 *
 * generic functionality that involves the Protocol template parameter like
 *
 * - open(), close(), assign(), release()
 */
template <class Protocol>
class basic_named_pipe_impl : public basic_named_pipe_impl_base {
 private:
  using __base = basic_named_pipe_impl_base;

 public:
  using protocol_type = Protocol;
  using endpoint_type = typename protocol_type::endpoint;

  constexpr explicit basic_named_pipe_impl(net::io_context &ctx) noexcept
      : __base{ctx} {}

  basic_named_pipe_impl(const basic_named_pipe_impl &) = delete;
  basic_named_pipe_impl &operator=(const basic_named_pipe_impl &) = delete;
  basic_named_pipe_impl(basic_named_pipe_impl &&rhs) = default;

  basic_named_pipe_impl &operator=(basic_named_pipe_impl &&rhs) noexcept {
    if (this == std::addressof(rhs)) {
      return *this;
    }
    __base::operator=(std::move(rhs));

    return *this;
  }

  ~basic_named_pipe_impl() {}

  executor_type get_executor() noexcept { return __base::get_executor(); }

  stdx::expected<void, std::error_code> open() { return __base::open(); }

  constexpr bool is_open() const noexcept { return __base::is_open(); }

  stdx::expected<void, std::error_code> assign(
      const protocol_type &protocol, const native_handle_type &native_handle) {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }
    protocol_ = protocol;
    native_handle_ = native_handle;

    return {};
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool v) {
    DWORD wait_mode = v ? PIPE_NOWAIT : PIPE_WAIT;

    bool success =
        SetNamedPipeHandleState(native_handle_, &wait_mode, nullptr, nullptr);

    if (!success) {
      return stdx::make_unexpected(win32::last_error_code());
    }

    return {};
  }

  stdx::expected<void, std::error_code> connect(const endpoint_type &ep) {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }

    using clock_type = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    const auto retry_step = 10ms;
    const auto end_time = clock_type::now() + 1s;
    do {
      native_handle_ =
          CreateFile(TEXT(ep.path().c_str()), GENERIC_READ | GENERIC_WRITE, 0,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

      if (native_handle_ == impl::named_pipe::kInvalidHandle) {
        auto const error_code = win32::last_error_code();
        // if the pipe is not ready, try waiting max 1 second for it to become
        // available
        const std::error_code ec_pipe_busy{ERROR_PIPE_BUSY,
                                           std::system_category()};

        if ((error_code == ec_pipe_busy) && clock_type::now() < end_time) {
          std::this_thread::sleep_for(retry_step);
          continue;
        }

        return stdx::make_unexpected(error_code);
      } else
        break;
    } while (true);

    return {};
  }

 private:
  protocol_type protocol_{endpoint_type{}.protocol()};
};

/**
 * base-class of basic_named_pipe_socket.
 *
 * provides functionality that's common between socket and acceptor like:
 *
 * - read_some
 * - write_some
 */
template <class Protocol>
class basic_named_pipe : private basic_named_pipe_impl<Protocol> {
 private:
  using __base = basic_named_pipe_impl<Protocol>;

 public:
  using protocol_type = Protocol;
  using executor_type = typename __base::executor_type;
  using native_handle_type = typename __base::native_handle_type;
  using endpoint_type = typename protocol_type::endpoint;

  // constructors are protected, below.

  executor_type get_executor() noexcept { return __base::get_executor(); }

  stdx::expected<void, std::error_code> assign(
      const protocol_type &protocol, const native_handle_type &native_handle) {
    return __base::assign(protocol, native_handle);
  }

  stdx::expected<void, std::error_code> open() { return __base::open(); }

  constexpr bool is_open() const noexcept { return __base::is_open(); }

  native_handle_type native_handle() const { return __base::native_handle(); }
  void native_handle(native_handle_type handle) {
    __base::native_handle(handle);
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool v) {
    return __base::native_non_blocking(v);
  }

  stdx::expected<void, std::error_code> connect(const endpoint_type &ep) {
    return __base::connect(ep);
  }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> read_some(
      const MutableBufferSequence &buffers) {
    size_t transferred{};
    const auto bufend = buffer_sequence_end(buffers);
    for (auto bufcur = buffer_sequence_begin(buffers); bufcur != bufend;
         ++bufcur) {
      DWORD numRead{0};
      if (!ReadFile(native_handle(), bufcur->data(), bufcur->size(), &numRead,
                    NULL)) {
        return stdx::make_unexpected(win32::last_error_code());
      }

      transferred += numRead;

      // for now, only process the first buffer as it simplifies the
      // error-handling after ReadFile().
      break;
    }

    return transferred;
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffers) {
    size_t transferred{};

    const auto bufend = buffer_sequence_end(buffers);
    for (auto bufcur = buffer_sequence_begin(buffers); bufcur != bufend;
         ++bufcur) {
      DWORD written{0};

      if (!WriteFile(native_handle(), bufcur->data(), bufcur->size(), &written,
                     NULL)) {
        return stdx::make_unexpected(win32::last_error_code());
      }

      transferred += written;

      // for now, only process the first buffer.
      break;
    }

    return transferred;
  }

 protected:
  basic_named_pipe(net::io_context &ctx) : __base{ctx} {}

  basic_named_pipe(net::io_context &ctx, const protocol_type &proto)
      : __base{ctx, proto} {}

  basic_named_pipe(net::io_context &ctx, const protocol_type &proto,
                   native_handle_type native_handle)
      : __base{ctx} {
    assign(proto, native_handle);
  }

  basic_named_pipe(const basic_named_pipe &) = delete;
  basic_named_pipe &operator=(const basic_named_pipe &) = delete;
  basic_named_pipe(basic_named_pipe &&) = default;
  basic_named_pipe &operator=(basic_named_pipe &&) = default;

  ~basic_named_pipe() = default;
};

/**
 * client side of a basic_named_pipe.
 *
 * @tparam Protocol a protocol class that provides .protocol() and .type()
 */
template <class Protocol>
class basic_named_pipe_socket : public basic_named_pipe<Protocol> {
 private:
  using __base = basic_named_pipe<Protocol>;

 public:
  using protocol_type = Protocol;
  using native_handle_type = typename __base::native_handle_type;
  using endpoint_type = typename protocol_type::endpoint;

  explicit basic_named_pipe_socket(net::io_context &ctx) : __base(ctx) {}
  basic_named_pipe_socket(net::io_context &ctx, const protocol_type &proto)
      : __base(ctx, proto) {}

  basic_named_pipe_socket(net::io_context &ctx, const protocol_type &proto,
                          native_handle_type native_handle)
      : __base(ctx, proto, native_handle) {}

  basic_named_pipe_socket(const basic_named_pipe_socket &) = delete;
  basic_named_pipe_socket &operator=(const basic_named_pipe_socket &) = delete;

  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_named_pipe_socket(basic_named_pipe_socket &&other) = default;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_named_pipe_socket &operator=(basic_named_pipe_socket &&) = default;

  ~basic_named_pipe_socket() {
    if (is_open()) close();
  }

  constexpr bool is_open() const noexcept { return __base::is_open(); }

  native_handle_type native_handle() const { return __base::native_handle(); }

  stdx::expected<void, std::error_code> open() {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }

    return {};
  }

  stdx::expected<void, std::error_code> close() {
    if (is_open()) {
      DisconnectNamedPipe(native_handle());
      __base::native_handle(impl::named_pipe::kInvalidHandle);
    }

    return {};
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffers) {
    return __base::write_some(buffers);
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool v) {
    if (is_open()) {
      return __base::native_non_blocking(v);
    } else {
      native_non_blocking_ = static_cast<int>(v);

      return {};
    }
  }

  stdx::expected<void, std::error_code> connect(const endpoint_type &ep) {
    auto connect_res = __base::connect(ep);
    if (!connect_res) return connect_res;

    if (native_non_blocking_ == 1) {
      native_non_blocking(true);
    }

    return connect_res;
  }

 private:
  int native_non_blocking_{-1};
};

/**
 * server side of a named pipe.
 *
 * can accept a connection from an named pipe path.
 */
template <class Protocol>
class basic_named_pipe_acceptor : public basic_named_pipe_impl<Protocol> {
 private:
  using __base = basic_named_pipe_impl<Protocol>;

 public:
  using protocol_type = Protocol;
  using socket_type = basic_named_pipe_socket<protocol_type>;
  using endpoint_type = typename protocol_type::endpoint;
  using executor_type = typename __base::executor_type;
  using native_handle_type = typename __base::native_handle_type;

  explicit basic_named_pipe_acceptor(net::io_context &ctx) : __base(ctx) {}
  basic_named_pipe_acceptor(net::io_context &ctx, const protocol_type &proto)
      : __base(ctx, proto) {}

  basic_named_pipe_acceptor(const basic_named_pipe_acceptor &) = delete;
  basic_named_pipe_acceptor &operator=(const basic_named_pipe_acceptor &) =
      delete;

  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_named_pipe_acceptor(basic_named_pipe_acceptor &&other) = default;
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  basic_named_pipe_acceptor &operator=(basic_named_pipe_acceptor &&) = default;

  ~basic_named_pipe_acceptor() {
    if (is_open()) close();
  }

  executor_type get_executor() noexcept { return __base::get_executor(); }

  bool is_open() const { return __base::is_open(); }

  native_handle_type native_handle() const { return __base::native_handle(); }

  void native_handle(native_handle_type handle) {
    __base::native_handle(handle);
  }

  stdx::expected<void, std::error_code> open() { return __base::open(); }

  stdx::expected<void, std::error_code> close() {
    if (is_open()) {
      CloseHandle(native_handle());
      native_handle(impl::named_pipe::kInvalidHandle);
    }

    return {};
  }

  stdx::expected<native_handle_type, std::error_code> release() {
    return __base::release();
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool v) {
    if (is_open()) {
      return __base::native_non_blocking(v);
    } else {
      native_non_blocking_ = static_cast<int>(v);

      return {};
    }
  }

  /**
   * bind endpoint to socket.
   *
   * in accordance with net-ts' acceptors' bind().
   *
   * @param ep    an endpoint this method binds to a socket
   * @param flags flags passed to CreateNamedPipe() as is. Use for PIPE_NOWAIT.
   *
   * @retval std::errc::invalid_argument if ep.path() is empty
   * @retval std::errc::invalid_argument if already bound.
   */
  stdx::expected<void, std::error_code> bind(const endpoint_type &ep,
                                             int flags = 0) {
    if (ep.path().empty()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    // already bound.
    if (!ep_.path().empty()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    ep_ = ep;

    const auto protocol = protocol_type();

    if (native_non_blocking_ == 1) flags |= PIPE_NOWAIT;

    if (!is_open()) {
      auto handle = CreateNamedPipe(
          TEXT(ep_.path().c_str()), PIPE_ACCESS_DUPLEX,
          protocol.type() | protocol.protocol() | PIPE_REJECT_REMOTE_CLIENTS |
              flags,
          back_log_, 1024 * 16, 1024 * 16, NMPWAIT_USE_DEFAULT_WAIT, NULL);

      if (handle == impl::named_pipe::kInvalidHandle) {
        auto ec = win32::last_error_code();
        return stdx::make_unexpected(ec);
      }

      native_handle(handle);
    }

    return {};
  }

  /**
   * max waiting pending connections.
   */
  stdx::expected<void, std::error_code> listen(
      int back_log = PIPE_UNLIMITED_INSTANCES) {
    if (back_log <= 0) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }
    if (back_log > PIPE_UNLIMITED_INSTANCES) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    back_log_ = back_log;

    return {};
  }

  /**
   * accept a client connection.
   *
   * - CreateNamedPipe() on the endpoint assigned by bind()
   * - ConnectNamedPipe() to accept the client connection.
   *
   * @return a connected named pipe.
   * @retval std::errc::invalid_argument if no endpoint bound.
   */
  stdx::expected<socket_type, std::error_code> accept() {
    // not bound.
    if (ep_.path().empty()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    const auto protocol = protocol_type();

    const bool connected = ConnectNamedPipe(native_handle(), NULL);
    if (!connected) {
      auto last_ec = win32::last_error_code();

      const std::error_code ec_pipe_connected{ERROR_PIPE_CONNECTED,  // 535
                                              std::system_category()};
      const std::error_code ec_no_data{ERROR_NO_DATA,  // 232
                                       std::system_category()};

      // ERROR_PIPE_CONNECTED is also a success
      // ERROR_NO_DATA too, it just means the pipe is already closed, but quite
      // likely readable.
      if (last_ec == ec_pipe_connected || last_ec == ec_no_data) {
        return {std::in_place, get_executor().context(), protocol,
                native_handle()};
      }

      return stdx::make_unexpected(last_ec);
    }

    return {std::in_place, get_executor().context(), protocol, native_handle()};
  }

  stdx::expected<endpoint_type, std::error_code> local_endpoint() const {
    return ep_;
  }

 private:
  endpoint_type ep_;
  int back_log_{PIPE_UNLIMITED_INSTANCES};
  int native_non_blocking_{-1};  // unset.
};

/**
 * endpoint of a named pipe.
 */
template <typename Protocol>
class basic_named_pipe_endpoint {
 public:
  using protocol_type = Protocol;

  basic_named_pipe_endpoint() = default;
  basic_named_pipe_endpoint(std::string path)
      : path_{path.substr(0, std::min(max_path_len(), path.size()))} {}

  std::string path() const { return path_; }

  constexpr protocol_type protocol() const noexcept { return protocol_type(); }

  size_t size() const { return path().size(); }

  size_t capacity() const { return max_path_len(); }

  void resize(size_t size) { path_.resize(size); }

 private:
  constexpr size_t max_path_len() const { return 256; }

  std::string path_;
};

/**
 * protocol class for message oriented named pipes.
 */
class message_protocol {
 public:
  using endpoint = basic_named_pipe_endpoint<message_protocol>;
  using socket = basic_named_pipe_socket<message_protocol>;
  using acceptor = basic_named_pipe_acceptor<message_protocol>;

  int type() const { return PIPE_TYPE_MESSAGE; }
  int protocol() const { return PIPE_READMODE_MESSAGE; }
};

/**
 * protocol class for byte oriented named pipes.
 */
class byte_protocol {
 public:
  using endpoint = basic_named_pipe_endpoint<byte_protocol>;
  using socket = basic_named_pipe_socket<byte_protocol>;
  using acceptor = basic_named_pipe_acceptor<byte_protocol>;

  int type() const { return PIPE_TYPE_BYTE; }
  int protocol() const { return PIPE_READMODE_BYTE; }
};

}  // namespace local

#endif

#endif
