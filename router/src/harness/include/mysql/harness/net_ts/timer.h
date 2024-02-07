/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_TIMER_H_
#define MYSQL_HARNESS_NET_TS_TIMER_H_

#include <chrono>
#include <thread>  // this_thread

#include "my_compiler.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/netfwd.h"
#include "mysql/harness/stdx/expected.h"

namespace net {

template <class Clock>
struct wait_traits {
  static typename Clock::duration to_wait_duration(
      const typename Clock::duration &d) {
    return d;
  }
  static typename Clock::duration to_wait_duration(
      const typename Clock::time_point &tp) {
    auto diff = tp - Clock::now();

    if (diff > Clock::duration::max()) return Clock::duration::max();
    if (diff < Clock::duration::min()) return Clock::duration::min();

    return diff;
  }
};

template <class Clock, class WaitTraits>
class basic_waitable_timer {
 public:
  using executor_type = io_context::executor_type;
  using clock_type = Clock;
  using duration = typename clock_type::duration;
  using time_point = typename clock_type::time_point;
  using traits_type = WaitTraits;

  // 15.4.1 construct/copy/destroy
  explicit basic_waitable_timer(io_context &io_ctx)
      : basic_waitable_timer(io_ctx, time_point()) {}
  basic_waitable_timer(io_context &io_ctx, const time_point &tp)
      : executor_{io_ctx.get_executor()}, expiry_{tp} {}
  basic_waitable_timer(io_context &io_ctx, const duration &d)
      : basic_waitable_timer(io_ctx, Clock::now() + d) {}

  basic_waitable_timer(const basic_waitable_timer &) = delete;
  basic_waitable_timer(basic_waitable_timer &&rhs)
      : executor_{std::move(rhs.executor_)}, expiry_{std::move(rhs.expiry_)} {
    id_.swap(rhs.id_);

    rhs.expiry_ = time_point();
  }

  ~basic_waitable_timer() { cancel(); }

  basic_waitable_timer &operator=(const basic_waitable_timer &) = delete;
  basic_waitable_timer &operator=(basic_waitable_timer &&rhs) {
    if (this == std::addressof(rhs)) {
      return *this;
    }
    cancel();

    executor_ = std::move(rhs.executor_);
    expiry_ = std::move(rhs.expiry_);
    id_.swap(rhs.id_);

    rhs.expiry_ = time_point();

    return *this;
  }

  // 15.4.4 ops
  executor_type get_executor() noexcept { return executor_; }
  size_t cancel() { return executor_.context().cancel(*this); }
  size_t cancel_one() { return executor_.context().cancel_one(*this); }

  time_point expiry() const { return expiry_; }

  size_t expires_at(const time_point &t) {
    size_t cancelled = cancel();

    expiry_ = t;

    return cancelled;
  }

  size_t expires_after(const duration &d) {
    return expires_at(clock_type::now() + d);
  }

  stdx::expected<void, std::error_code> wait() {
    while (clock_type::now() < expiry_) {
      std::this_thread::sleep_for(traits_type::to_wait_duration(expiry_));
    }

    return {};
  }

  template <class CompletionToken>
  auto async_wait(CompletionToken &&token) {
    async_completion<CompletionToken, void(std::error_code)> init{token};

    get_executor().context().async_wait(*this,
                                        std::move(init.completion_handler));

    return init.result.get();
  }

 private:
  executor_type executor_;
  time_point expiry_;

  // every timer needs a unique-id to be able to identify it (e.g. to cancel it)
  //
  // Note: empty classes like "Id" have a sizeof() > 0. It would perhaps make
  // sense to use it for something useful.
  struct Id {};

  Id *id() const { return id_.get(); }

  std::unique_ptr<Id> id_{new Id};

  friend class io_context;
};

using system_timer = basic_waitable_timer<std::chrono::system_clock>;
using steady_timer = basic_waitable_timer<std::chrono::steady_clock>;
using high_resolution_timer =
    basic_waitable_timer<std::chrono::high_resolution_clock>;

}  // namespace net

#endif
