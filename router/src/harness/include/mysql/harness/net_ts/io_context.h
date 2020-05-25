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

#ifndef MYSQL_HARNESS_NET_TS_IO_CONTEXT_H_
#define MYSQL_HARNESS_NET_TS_IO_CONTEXT_H_

#include <atomic>
#include <chrono>
#include <memory>  // unique_ptr
#include <mutex>

#include "my_config.h"  // HAVE_EPOLL
#include "mysql/harness/net_ts/executor.h"
#include "mysql/harness/net_ts/impl/socket_service.h"
#include "mysql/harness/net_ts/netfwd.h"

namespace net {

class io_context : public execution_context {
 public:
  class executor_type;

  using count_type = size_t;
  using native_handle_type = impl::socket::native_handle_type;

  io_context()
      : socket_service_{std::make_unique<net::impl::socket::SocketService>()} {}
  explicit io_context(
      std::unique_ptr<net::impl::socket::SocketServiceBase> &&socket_service)
      : socket_service_{std::move(socket_service)} {}
  explicit io_context(int /* concurrency_hint */) : io_context() {}
  io_context(const io_context &) = delete;
  io_context &operator=(const io_context &) = delete;

  executor_type get_executor() noexcept;

  bool stopped() const noexcept { return stopped_; }

  void stop() noexcept { stopped_ = true; }

  void restart() noexcept { stopped_ = false; }

  impl::socket::SocketServiceBase *socket_service() const {
    return socket_service_.get();
  }

 private:
  count_type do_one(std::unique_lock<std::mutex> &lk,
                    std::chrono::milliseconds timeout);

  template <typename _Clock, typename _WaitTraits>
  friend class basic_waitable_timer;

  friend class basic_socket_impl_base;

  template <class Protocol>
  friend class basic_socket_impl;

  template <class Protocol>
  friend class basic_socket;

  template <class Protocol>
  friend class basic_stream_socket;

  template <class Protocol>
  friend class basic_socket_acceptor;

  bool stopped_{false};
  std::atomic<count_type> work_count_{};

  std::unique_ptr<impl::socket::SocketServiceBase> socket_service_;
};

class io_context::executor_type {
 public:
  executor_type(const executor_type &rhs) noexcept = default;
  executor_type(executor_type &&rhs) noexcept = default;
  executor_type &operator=(const executor_type &rhs) noexcept = default;
  executor_type &operator=(executor_type &&rhs) noexcept = default;

  ~executor_type() = default;

  bool running_in_this_thread() const noexcept {
    // TODO: check if this task is running in this thread. Currently, it is
    // "yes", as we don't allow post()ing to other threads

    // track call-chain
    return true;
  }
  io_context &context() const noexcept { return *io_ctx_; }

  void on_work_started() const noexcept { ++io_ctx_->work_count_; }
  void on_work_finished() const noexcept { --io_ctx_->work_count_; }

  template <class Func, class ProtoAllocator>
  void dispatch(Func &&f, const ProtoAllocator &a) const {
    if (running_in_this_thread()) {
      // run it in this thread.
      std::decay_t<Func>(std::forward<Func>(f))();
    } else {
      // run it in a worker thread
      post(std::forward<Func>(f), a);
    }
  }

  // TODO: implement
  template <class Func, class ProtoAllocator>
  void post(Func &&f, const ProtoAllocator & /* a */) const {
    // supposed to call the function in another thread, but as we only have it
    // in one thread right now ... execute directly

    std::decay_t<Func>(std::forward<Func>(f))();
  }

  template <class Func, class ProtoAllocator>
  void defer(Func &&f, const ProtoAllocator &a) const {
    // same as post
    post(std::forward<Func>(f), a);
  }

 private:
  friend io_context;

  explicit executor_type(io_context &ctx) : io_ctx_{std::addressof(ctx)} {}

  io_context *io_ctx_{nullptr};
};

inline bool operator==(const io_context::executor_type &a,
                       const io_context::executor_type &b) noexcept {
  return std::addressof(a.context()) == std::addressof(b.context());
}
inline bool operator!=(const io_context::executor_type &a,
                       const io_context::executor_type &b) noexcept {
  return !(a == b);
}

// io_context::executor_type is an executor even though it doesn't have an
// default constructor
template <>
struct is_executor<io_context::executor_type> : std::true_type {};

inline io_context::executor_type io_context::get_executor() noexcept {
  return executor_type(*this);
}

}  // namespace net

#endif
