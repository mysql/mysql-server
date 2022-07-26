/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_POLL_IO_SERVICE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_POLL_IO_SERVICE_H_

#include <array>
#include <list>
#include <mutex>
#include <system_error>
#include <vector>

#include <iostream>

#include "mysql/harness/net_ts/impl/io_service_base.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

namespace net {

/**
 * io_service based on the poll() system-call.
 *
 *
 */

// https://daniel.haxx.se/blog/2016/10/11/poll-on-mac-10-12-is-broken/
// https://daniel.haxx.se/blog/2012/10/10/wsapoll-is-broken/
// http://www.greenend.org.uk/rjk/tech/poll.html
class poll_io_service : public IoServiceBase {
 public:
  ~poll_io_service() override {
    if (wakeup_fds_.first != impl::socket::kInvalidSocket)
      impl::socket::close(wakeup_fds_.first);
    if (wakeup_fds_.second != impl::socket::kInvalidSocket)
      impl::socket::close(wakeup_fds_.second);
  }

  bool is_open() const noexcept {
    return wakeup_fds_.first != impl::socket::kInvalidSocket &&
           wakeup_fds_.second != impl::socket::kInvalidSocket;
  }

  stdx::expected<void, std::error_code> open() noexcept override {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }

#if defined(_WIN32)
    // on windows we build a AF_INET socketpair()
    auto pipe_res = impl::socket::socketpair(AF_INET, SOCK_STREAM, 0);
#else
    auto pipe_res = impl::socket::socketpair(AF_UNIX, SOCK_STREAM, 0);
#endif
    if (!pipe_res) return stdx::make_unexpected(pipe_res.error());

    wakeup_fds_ = *pipe_res;

    // set both ends of the pipe non-blocking as
    //
    // - read() shouldn't block if pipe is empty
    // - write() shouldn't block if pipe is full as it only matters there is
    // something in the pipe to wakeup the poll_one()
    auto non_block_wakeup_0_res =
        impl::socket::native_non_blocking(wakeup_fds_.first, true);
    if (!non_block_wakeup_0_res) return non_block_wakeup_0_res;
    auto non_block_wakeup_1_res =
        impl::socket::native_non_blocking(wakeup_fds_.second, true);
    if (!non_block_wakeup_1_res) return non_block_wakeup_1_res;

    add_fd_interest(wakeup_fds_.first, impl::socket::wait_type::wait_read);

    return {};
  }

  stdx::expected<short, std::error_code> poll_event_from_wait_type(
      impl::socket::wait_type event) {
    switch (event) {
      case impl::socket::wait_type::wait_read:
        return POLLIN;
      case impl::socket::wait_type::wait_write:
        return POLLOUT;
      case impl::socket::wait_type::wait_error:
        return POLLERR;
      default:
        return stdx::make_unexpected(
            make_error_code(std::errc::invalid_argument));
    }
  }

  // split fd-interest
  //
  // internally splits the fds into multiple buckets to reduce
  // the search-space and lead to lower resize cost
  class FdInterests {
   public:
    using element_type = fd_event;

    // we could use list, deque, vector, ... here
    //
    // container_type | concurrency | mem-usage | tps
    // ---------------+-------------+-----------+------
    // list:          | 8000        |      137M | 56000
    // vector         | 8000        |      145M | 58000
    using container_type = std::vector<element_type>;

    void push_back(element_type &&t) {
      auto &b = bucket(t.fd);

      std::lock_guard<std::mutex> lk(mtx_);
      b.push_back(std::move(t));
    }

    element_type erase_all(native_handle_type fd) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(mtx_);
      const auto end = b.end();

      for (auto cur = b.begin(); cur != end;) {
        if (cur->fd == fd) {
          auto op = std::move(*cur);
          cur = b.erase(cur);

          return op;
        } else {
          ++cur;
        }
      }

      // not found
      return {};
    }

    std::vector<pollfd> poll_fds() const {
      std::vector<pollfd> fds;
      {
        std::lock_guard<std::mutex> lk(mtx_);
        size_t count{};

        for (const auto &b : buckets_) {
          count += b.size();
        }

        fds.reserve(count);

        for (const auto &b : buckets_) {
          for (auto const &fd_int : b) {
            fds.push_back({fd_int.fd, fd_int.event, 0});
          }
        }
      }

      return fds;
    }

    void erase_fd_event(native_handle_type fd, short event) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(mtx_);
      b.erase(std::remove_if(
                  b.begin(), b.end(),
                  [=](fd_event v) { return v.fd == fd && (v.event & event); }),
              b.end());
    }

   private:
    container_type &bucket(native_handle_type fd) {
      size_t ndx = fd % buckets_.size();

      return buckets_[ndx];
    }

    // tps @8000 client connections
    //
    // cnt : tps
    // ----:------
    //    1: 32000
    //    3: 45000
    //    7: 54000
    //   13: 56000
    //   23: 57000
    //   47: 58000
    //  101: 58000
    // 1009: 57000
    static constexpr const size_t bucket_count_{101};

    mutable std::mutex mtx_;
    std::array<container_type, bucket_count_> buckets_;
  };

  stdx::expected<void, std::error_code> add_fd_interest(
      native_handle_type fd, impl::socket::wait_type event) override {
    if (fd == impl::socket::kInvalidSocket) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    auto event_res = poll_event_from_wait_type(event);
    if (!event_res) return stdx::make_unexpected(event_res.error());

    fd_interests_.push_back({fd, event_res.value()});

    return {};
  }

  /**
   * remove fd from interest set.
   */
  stdx::expected<void, std::error_code> remove_fd(
      native_handle_type fd) override {
    if (fd == impl::socket::kInvalidSocket) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    fd_interests_.erase_all(fd);

    return {};
  }

  stdx::expected<fd_event, std::error_code> poll_one(
      std::chrono::milliseconds timeout) override {
    if (!is_open()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    fd_event ev;
    {
      std::lock_guard<std::mutex> lk(mtx_);

      if (triggered_events_.empty()) {
        // build fds for poll() from fd-interest

        auto poll_fds = fd_interests_.poll_fds();
        auto res = impl::poll::poll(poll_fds.data(), poll_fds.size(), timeout);
        if (!res) return stdx::make_unexpected(res.error());

        size_t num_revents = res.value();

        // translate poll()'s revents into triggered events.
        for (auto ev : poll_fds) {
          if (ev.revents != 0) {
            --num_revents;
            // in case of connection close at "wait_read" without any data
            //
            // windows: POLLERR|POLLHUP
            // linux:   POLLIN|POLLHUP
            //
            // as the uppoer layers expect that the waited-for event appears
            // in the output, we hve to merge it in.
            if (ev.events & POLLIN) {
              ev.revents |= POLLIN;
            }
            if (ev.events & POLLOUT) {
              ev.revents |= POLLOUT;
            }
            triggered_events_.push_back({ev.fd, ev.revents});
            if (ev.fd != wakeup_fds_.first) {
              // don't remove interest in the wakeup file-descriptors
              remove_fd_interest(ev.fd, ev.revents);
            }
          }

          if (0 == num_revents) break;
        }
      }

      if (triggered_events_.empty()) {
        return stdx::make_unexpected(std::error_code{});
      }

      ev = triggered_events_.front();
      triggered_events_.pop_front();
    }

    // we could drop mutex here, right?

    if (ev.fd == wakeup_fds_.first) {
      on_notify();
      return stdx::make_unexpected(make_error_code(std::errc::interrupted));
    }

    return ev;
  }

  void on_notify() {
    // 256 seems to be a nice sweetspot between "not run too many rounds" and
    // "copy user space to kernel space"
    std::array<uint8_t, 256> buf;
    stdx::expected<size_t, std::error_code> res;
    do {
      res = impl::socket::recv(wakeup_fds_.first, buf.data(), buf.size(), 0);
    } while (res ||
             res.error() == make_error_condition(std::errc::interrupted));
  }

  void notify() override {
    // don't notify if there is noone listening
    if (!is_open()) return;

    stdx::expected<size_t, std::error_code> res;
    do {
      std::array<uint8_t, 1> buf = {{'.'}};
      res = impl::socket::send(wakeup_fds_.second, buf.data(), buf.size(), 0);
      // retry if interrupted
    } while (res ==
             stdx::make_unexpected(make_error_code(std::errc::interrupted)));
  }

 private:
  /**
   * remove interest of event from fd.
   *
   * mtx_ must be held, when called.
   */
  stdx::expected<void, std::error_code> remove_fd_interest(
      native_handle_type fd, short event) {
    if (fd == impl::socket::kInvalidSocket) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    fd_interests_.erase_fd_event(fd, event);

    return {};
  }

  std::pair<impl::socket::native_handle_type, impl::socket::native_handle_type>
      wakeup_fds_{impl::socket::kInvalidSocket, impl::socket::kInvalidSocket};

  FdInterests fd_interests_;

  std::list<fd_event> triggered_events_;

  // mutex for triggered_events
  std::mutex mtx_;
};
}  // namespace net

#endif
