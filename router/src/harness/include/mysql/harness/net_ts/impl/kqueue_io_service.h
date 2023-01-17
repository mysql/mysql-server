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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_KQUEUE_IO_SERVICE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_KQUEUE_IO_SERVICE_H_

#include "router_config.h"  // HAVE_QUEUE

#ifdef HAVE_KQUEUE
#include <array>

#include "mysql/harness/net_ts/impl/io_service_base.h"
#include "mysql/harness/net_ts/impl/kqueue.h"

namespace net {
class kqueue_io_service : public IoServiceBase {
 public:
  using native_handle_type = impl::socket::native_handle_type;

  ~kqueue_io_service() override { close(); }

  bool is_open() const noexcept {
    return epfd_ != impl::file::kInvalidHandle &&
           wakeup_fds_.first != impl::file::kInvalidHandle &&
           wakeup_fds_.second != impl::file::kInvalidHandle;
  }

  stdx::expected<void, std::error_code> open() override {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }

    auto res = impl::kqueue::create();
    if (!res) return stdx::make_unexpected(res.error());

    epfd_ = *res;

    auto pipe_res = impl::file::pipe(O_NONBLOCK);
    if (!pipe_res) return stdx::make_unexpected(pipe_res.error());

    wakeup_fds_ = *pipe_res;

    // set both ends of the pipe non-blocking as
    //
    // - read() shouldn't block is pipe is empty
    // - write() shouldn't block is pipe is full as it only matters there is
    // something in the pipe to wakeup the poll_one()
    auto non_block_wakeup_0_res =
        impl::socket::native_non_blocking(wakeup_fds_.first, true);
    if (!non_block_wakeup_0_res) return non_block_wakeup_0_res;
    auto non_block_wakeup_1_res =
        impl::socket::native_non_blocking(wakeup_fds_.second, true);
    if (!non_block_wakeup_1_res) return non_block_wakeup_1_res;

    return {};
  }

  void on_notify() {
    std::array<uint8_t, 256> buf;
    ssize_t res;
    do {
      res = ::read(wakeup_fds_.first, buf.data(), buf.size());
    } while (res != -1 || errno == EINTR);
  }

  void notify() override {
    ssize_t res;
    do {
      res = ::write(wakeup_fds_.second, ".", 1);
      // retry if interrupted
    } while ((res == -1) && (errno == EINTR));
  }

  stdx::expected<void, std::error_code> close() {
    if (wakeup_fds_.first != impl::file::kInvalidHandle) {
      impl::file::close(wakeup_fds_.first);
      wakeup_fds_.first = impl::file::kInvalidHandle;
    }

    if (wakeup_fds_.second != impl::file::kInvalidHandle) {
      impl::file::close(wakeup_fds_.second);
      wakeup_fds_.second = impl::file::kInvalidHandle;
    }

    if (epfd_ != impl::file::kInvalidHandle) {
      impl::file::close(epfd_);
      epfd_ = impl::file::kInvalidHandle;
    }

    return {};
  }

  stdx::expected<void, std::error_code> add_fd_interest(
      native_handle_type fd, impl::socket::wait_type wt) override {
    struct kevent ev;

    short filter{0};

    switch (wt) {
      case impl::socket::wait_type::wait_read:
        filter = EVFILT_READ;
        break;
      case impl::socket::wait_type::wait_write:
        filter = EVFILT_WRITE;
        break;
      default:
        std::terminate();
        break;
    }

    // edge-triggered: EV_CLEAR
    EV_SET(&ev, fd, filter, EV_ADD | EV_ONESHOT | EV_CLEAR, 0, 0, NULL);

    changelist_.push_back(ev);

    return {};
  }

  stdx::expected<void, std::error_code> queue_remove_fd_interest(
      native_handle_type fd, short filter) {
    struct kevent ev;

    EV_SET(&ev, fd, filter, EV_DELETE, 0, 0, NULL);

    changelist_.push_back(ev);

    return {};
  }

  stdx::expected<void, std::error_code> after_event_fired(
      const struct kevent & /*ev*/) {
    // as ONESHOT is used, there is no need to remove-fd-interest again
    return {};
  }

  // TODO: should be renamed to "before_close()" as it is a no-op on kqueue,
  // but a requirement on linux epoll
  stdx::expected<void, std::error_code> remove_fd(
      native_handle_type /* fd */) override {
#if 0
    struct kevent ev;

    EV_SET(&ev, fd, 0, EV_DELETE, 0, 0, NULL);

    changelist_.push_back(ev);
#endif

    return {};
  }

  /**
   * @returns a fdevent or std::error_code
   * @retval fd_event on success
   * @retval std::error_code on failure, std::error_code(success) if no events
   * where registered.
   */
  stdx::expected<fd_event, std::error_code> poll_one(
      std::chrono::milliseconds timeout) override {
    if (!is_open()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    if (fd_events_processed_ == fd_events_size_) {
      struct timespec ts, *p_ts{};

      if (timeout.count() != -1) {
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
        timeout -= secs;

        ts = {secs.count(),
              std::chrono::duration_cast<std::chrono::nanoseconds>(timeout)
                  .count()};

        p_ts = &ts;
      }

      auto res =
          impl::kqueue::kevent(epfd_, changelist_.data(), changelist_.size(),
                               fd_events_.data(), fd_events_.size(), p_ts);
      if (!res) return stdx::make_unexpected(res.error());

      changelist_.clear();

      fd_events_processed_ = 0;
      fd_events_size_ = *res;

      if (fd_events_size_ == 0) {
        return stdx::make_unexpected(make_error_code(std::errc::timed_out));
      }
    }

    const auto ev = fd_events_[fd_events_processed_++];

    // ev.flags may also set EV_EOF

    if (ev.flags & EV_ERROR) {
      if (ev.data == 0) {
        return stdx::make_unexpected(make_error_code(std::errc::interrupted));
      } else {
        // .data is errno
        //
        // if EV_RECEIPT is set, ev.data will be 0 in case of OK
        // eitherwise ...
        // - ENOENT

        return fd_event{static_cast<native_handle_type>(ev.ident), POLLERR};
      }
    }

    if (static_cast<impl::file::file_handle_type>(ev.ident) ==
        wakeup_fds_.first) {
      // wakeup fd fired
      //
      // - don't remove the interest for it
      // - report to the caller that we don't have an event yet by saying we got
      // interrupted
      on_notify();

      return stdx::make_unexpected(make_error_code(std::errc::interrupted));
    }

    after_event_fired(ev);

    short events{};
    if (ev.filter == EVFILT_READ) {
      events = POLLIN;
    } else if (ev.filter == EVFILT_WRITE) {
      events = POLLOUT;
    }

    // ev.ident is a uintptr_t ... as it supports many kinds of event-sources
    // ... but we only added a 'int' as file-handle
    return fd_event{static_cast<native_handle_type>(ev.ident), events};
  }

 private:
  std::array<struct kevent, 16> fd_events_;
  size_t fd_events_processed_{0};
  size_t fd_events_size_{0};
  int epfd_{-1};
  std::vector<struct kevent> changelist_;

  std::pair<impl::file::file_handle_type, impl::file::file_handle_type>
      wakeup_fds_{impl::file::kInvalidHandle, impl::file::kInvalidHandle};
};
}  // namespace net
#endif

#endif
