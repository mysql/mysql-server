/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
#include <optional>
#include <system_error>
#include <vector>

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
  ~poll_io_service() override { close(); }

  static constexpr const short kSettableEvents = POLLIN | POLLOUT;
  static constexpr const short kAlwaysEnabledEvents = POLLHUP | POLLERR;
  static constexpr const short kAllEvents =
      kSettableEvents | kAlwaysEnabledEvents;

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

  stdx::expected<void, std::error_code> close() {
    if (wakeup_fds_.first != impl::socket::kInvalidSocket) {
      impl::socket::close(wakeup_fds_.first);
      wakeup_fds_.first = impl::socket::kInvalidSocket;
    }
    if (wakeup_fds_.second != impl::socket::kInvalidSocket) {
      impl::socket::close(wakeup_fds_.second);

      wakeup_fds_.second = impl::socket::kInvalidSocket;
    }

    return {};
  }

  static stdx::expected<short, std::error_code> poll_event_from_wait_type(
      impl::socket::wait_type event) {
    switch (event) {
      case impl::socket::wait_type::wait_read:
        return POLLIN;
      case impl::socket::wait_type::wait_write:
        return POLLOUT;
      case impl::socket::wait_type::wait_error:
        return POLLERR | POLLHUP;
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

      auto it = std::find_if(b.begin(), b.end(), [fd = t.fd](auto fd_ev) {
        return fd_ev.fd == fd;
      });
      if (it == b.end()) {
        b.push_back(std::move(t));
      } else {
        it->event |= t.event;
      }
    }

    stdx::expected<void, std::error_code> erase_all(native_handle_type fd) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(mtx_);
      const auto end = b.end();

      for (auto cur = b.begin(); cur != end;) {
        if (cur->fd == fd) {
          cur = b.erase(cur);

          return {};
        } else {
          ++cur;
        }
      }

      // not found
      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }

    std::vector<pollfd> poll_fds() const {
      std::vector<pollfd> fds;
      {
        std::lock_guard<std::mutex> lk(mtx_);
        size_t count{};

        for (const auto &b : buckets_) {
          count += b.size();
        }

        // reserve a few more than needed.
        fds.reserve(count);

        for (const auto &b : buckets_) {
          for (auto const &fd_int : b) {
            if (fd_int.event != 0) {
              fds.push_back(
                  {fd_int.fd,
                   static_cast<short>(fd_int.event & ~kAlwaysEnabledEvents),
                   0});
            }
          }
        }
      }

      return fds;
    }

    stdx::expected<void, std::error_code> erase_fd_event(native_handle_type fd,
                                                         short event) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(mtx_);
      auto it = std::find_if(b.begin(), b.end(),
                             [fd](auto fd_ev) { return fd_ev.fd == fd; });
      if (it == b.end()) {
        return stdx::make_unexpected(
            make_error_code(std::errc::no_such_file_or_directory));
      }

      it->event &= ~event;

      return {};
    }

    std::optional<int32_t> interest(native_handle_type fd) const {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(mtx_);

      for (auto const &fd_ev : b) {
        if (fd_ev.fd == fd) return fd_ev.event;
      }

      return std::nullopt;
    }

   private:
    container_type &bucket(native_handle_type fd) {
      size_t ndx = fd % buckets_.size();

      return buckets_[ndx];
    }

    const container_type &bucket(native_handle_type fd) const {
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

    std::lock_guard<std::mutex> lk(mtx_);

    auto res = fd_interests_.erase_all(fd);
    if (res) {
      // remove all events which are already fetched by poll_one()

      auto end = triggered_events_.end();
      for (auto cur = triggered_events_.begin(); cur != end;) {
        if (cur->fd == fd) {
          cur = triggered_events_.erase(cur);
        } else {
          ++cur;
        }
      }
    }

    return res;
  }

  /**
   * get current fd-interest.
   *
   * @returns fd-interest as bitmask of raw POLL* flags
   */
  std::optional<int32_t> interest(native_handle_type fd) const {
    return fd_interests_.interest(fd);
  }

  stdx::expected<fd_event, std::error_code> pop_event() {
    fd_event ev;

    auto &head = triggered_events_.front();

    ev.fd = head.fd;

    // if there are multiple events: get OUT before IN.
    if (head.event & POLLOUT) {
      head.event &= ~POLLOUT;
      ev.event = POLLOUT;
    } else if (head.event & POLLIN) {
      // disable HUP if it is sent together with IN
      if (head.event & POLLHUP) head.event &= ~POLLHUP;

      head.event &= ~POLLIN;
      ev.event = POLLIN;
    } else if (head.event & POLLERR) {
      head.event &= ~POLLERR;
      ev.event = POLLERR;
    } else if (head.event & POLLHUP) {
      head.event &= ~POLLHUP;
      ev.event = POLLHUP;
    }

    if ((head.event & (POLLIN | POLLOUT | POLLERR | POLLHUP)) == 0) {
      triggered_events_.pop_front();
    }

    return ev;
  }

  stdx::expected<fd_event, std::error_code> update_fd_events(
      std::chrono::milliseconds timeout) {
    // build fds for poll() from fd-interest

    auto poll_fds = fd_interests_.poll_fds();
    auto res = impl::poll::poll(poll_fds.data(), poll_fds.size(), timeout);
    if (!res) return stdx::make_unexpected(res.error());

    size_t num_revents = res.value();  // number of pollfds with revents

    // translate poll()'s revents into triggered events.
    std::lock_guard lk(mtx_);

    for (auto ev : poll_fds) {
      if (ev.revents != 0) {
        --num_revents;

        // If the caller wants (ev.events) only:
        //
        // - POLLIN|POLLOUT
        //
        // but poll() returns:
        //
        // - POLLHUP
        //
        // then return POLLIN|POLLOUT.
        //
        // This handles the connection close cases which is signaled as:
        //
        // - POLLIN|POLLHUP on the Unixes
        // - POLLHUP on Windows.
        //
        // and the connect() failure case:
        //
        // - POLLHUP on FreeBSD/MacOSX
        // - POLLOUT on Linux
        //
        // As the caller is only interested in POLLIN|POLLOUT, the POLLHUP would
        // be unhandled and be reported on the next call of poll() again.
        const auto revents =
            ((ev.events & (POLLIN | POLLOUT)) &&  //
             ((ev.revents & (POLLIN | POLLOUT | POLLHUP)) == POLLHUP))
                ? ev.revents | (ev.events & (POLLIN | POLLOUT))
                : ev.revents;

        triggered_events_.emplace_back(ev.fd, revents);
        if (ev.fd != wakeup_fds_.first) {
          // mimik one-shot events.
          //
          // but don't remove interest in the wakeup file-descriptors
          remove_fd_interest(ev.fd, revents);
        }
      }

      if (0 == num_revents) break;
    }

    return pop_event();
  }

  stdx::expected<fd_event, std::error_code> poll_one(
      std::chrono::milliseconds timeout) override {
    if (!is_open()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    auto ev_res = [this]() -> stdx::expected<fd_event, std::error_code> {
      std::lock_guard<std::mutex> lk(mtx_);

      if (triggered_events_.empty()) {
        // no event.
        return stdx::make_unexpected(
            make_error_code(std::errc::no_such_file_or_directory));
      }

      return pop_event();
    }();

    if (!ev_res) {
      if (ev_res.error() == std::errc::no_such_file_or_directory) {
        ev_res = update_fd_events(timeout);
      }

      if (!ev_res) return stdx::make_unexpected(ev_res.error());
    }

    auto ev = *ev_res;

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
    // don't notify if there is no one listening
    if (!is_open()) return;

    stdx::expected<size_t, std::error_code> res;
    do {
      std::array<uint8_t, 1> buf = {{'.'}};
      res = impl::socket::send(wakeup_fds_.second, buf.data(), buf.size(), 0);
      // retry if interrupted
    } while (res ==
             stdx::make_unexpected(make_error_code(std::errc::interrupted)));
  }

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

    return fd_interests_.erase_fd_event(fd, event);
  }

 private:
  std::pair<impl::socket::native_handle_type, impl::socket::native_handle_type>
      wakeup_fds_{impl::socket::kInvalidSocket, impl::socket::kInvalidSocket};

  FdInterests fd_interests_;

  std::list<fd_event> triggered_events_;

  // mutex for triggered_events
  std::mutex mtx_;
};
}  // namespace net

#endif
