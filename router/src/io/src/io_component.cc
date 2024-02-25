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

#include "mysqlrouter/io_component.h"

#include <cstddef>  // size_t
#include <stdexcept>
#include <string>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/thread_affinity.h"

const std::error_category &io_component_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "io_component"; }
    std::string message(int ev) const override {
      switch (static_cast<IoComponentErrc>(ev)) {
        case IoComponentErrc::already_initialized:
          return "already initialized";
        case IoComponentErrc::unknown_backend:
          return "unknown backend";
      }
      return "unknown error";
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_error_code(IoComponentErrc e) {
  return {static_cast<int>(e), io_component_category()};
}

net::io_context &IoComponent::io_context() { return *io_ctx_; }

stdx::expected<void, std::error_code> IoComponent::init(
    size_t num_worker_threads, const std::string &backend_name) {
  if (io_ctx_)
    return stdx::make_unexpected(
        make_error_code(IoComponentErrc::already_initialized));
  const auto &supported_backends = IoBackend::supported();
  if (supported_backends.find(backend_name) == supported_backends.end()) {
    return stdx::make_unexpected(
        make_error_code(IoComponentErrc::unknown_backend));
  }

  ThreadAffinity main_thread{ThreadAffinity::current_thread_handle()};

  std::bitset<ThreadAffinity::max_cpus> main_cpu_affinity;
  const auto affinity_res = main_thread.affinity();
  if (affinity_res) {
    main_cpu_affinity = affinity_res.value();
  }

  size_t cur_cpu_ndx{0};

  backend_name_ = backend_name;

  io_ctx_ = std::make_unique<net::io_context>(
      std::make_unique<net::impl::socket::SocketService>(),
      IoBackend::backend(backend_name));
  for (size_t ndx{}; ndx < num_worker_threads; ++ndx) {
    std::bitset<ThreadAffinity::max_cpus> cpu_affinity;
    // assign threads in to a CPU thread
    if (affinity_res) {
      // if we could get the CPU affinity-set, try to assign each IO thread to
      // one CPU
      for (; cur_cpu_ndx < main_cpu_affinity.size(); ++cur_cpu_ndx) {
        if (main_cpu_affinity.test(cur_cpu_ndx)) {
          cpu_affinity.set(cur_cpu_ndx);

          ++cur_cpu_ndx;
          break;
        }
      }
    }
    try {
      io_threads_.emplace_back(ndx, cpu_affinity, backend_name);

      // check if the io-thread is actually started.
      auto open_res = io_threads_.back().context().open_res();
      if (!open_res) {
        throw std::system_error(open_res.error());
      }
    } catch (const std::system_error &e) {
      // creating the thread may throw with system_error in case of out of
      // resources

      // reset the component
      reset();
      return stdx::make_unexpected(e.code());
    }
  }

  return {};
}

void IoComponent::run() {
  // in case init() wasn't called yet, there is nothing to run
  if (io_ctx_) io_ctx_->run();

  // shutting down
  //
  // - signal all io-threads to stop running
  // - join io-threads
  for (auto &io_thread : io_threads_) {
    io_thread.stop();
    io_thread.join();
  }
}

void IoComponent::stop() {
  if (io_ctx_) io_ctx_->stop();
}

void IoComponent::reset() {
  io_threads_.clear();
  io_ctx_.reset();

  backend_name_.resize(0);
}

IoComponent &IoComponent::get_instance() {
  static IoComponent instance;

  return instance;
}
