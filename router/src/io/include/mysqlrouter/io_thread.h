/*
  Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_IO_THREAD_INCLUDED
#define ROUTER_IO_THREAD_INCLUDED

#include <cstddef>  // size_t
#include <functional>
#include <future>
#include <memory>  // make_unique
#include <string>
#include <thread>
#include <utility>

#include "mysql/harness/net_ts/impl/socket_service.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysqlrouter/io_backend.h"
#include "mysqlrouter/thread_affinity.h"

struct IoThreadLifecycle {
  std::function<void(size_t)> on_start;
  std::function<void(size_t)> on_stop;
};

class IoThread {
 public:
  IoThread(size_t ndx, std::bitset<ThreadAffinity::max_cpus> cpu_affinity,
           const std::string &backend_name = "poll",
           IoThreadLifecycle lifecycle = {}, std::string name = {})
      : ndx_{ndx},
        name_{name.empty() ? "io-" + std::to_string(ndx) : std::move(name)},
        cpu_affinity_{cpu_affinity},
        io_ctx_{std::make_unique<net::impl::socket::SocketService>(),
                IoBackend::backend(backend_name)},
        lifecycle_{std::move(lifecycle)},
        startup_future_{startup_promise_.get_future()},
        thr_{&IoThread::operator(), this} {}

  ~IoThread() {
    stop();
    join();
  }

  net::io_context &context() noexcept { return io_ctx_; }

  void operator()();

  void wait_until_started() { startup_future_.get(); }

  void stop() { io_ctx_.stop(); }

  void join() {
    if (thr_.joinable()) thr_.join();
  }

  std::bitset<ThreadAffinity::max_cpus> cpu_affinity() const {
    return cpu_affinity_;
  }

 private:
  size_t ndx_;
  std::string name_;
  std::bitset<ThreadAffinity::max_cpus> cpu_affinity_;

  net::io_context io_ctx_;

  // Keep the callbacks alive before the worker starts and until it exits.
  IoThreadLifecycle lifecycle_;
  std::promise<void> startup_promise_;
  std::future<void> startup_future_;
  std::thread thr_;
};

#endif
