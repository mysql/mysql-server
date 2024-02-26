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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_LINUX_EPOLL_IO_SERVICE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_LINUX_EPOLL_IO_SERVICE_H_

#include "my_config.h"  // HAVE_EPOLL

#define USE_EVENTFD

#ifdef HAVE_EPOLL
#include <chrono>
#include <mutex>
#include <optional>
#include <system_error>
#include <unordered_map>

#if defined(USE_EVENTFD)
#include <sys/eventfd.h>
#endif

#include <iostream>
#include <sstream>

#include "mysql/harness/net_ts/impl/io_service_base.h"
#include "mysql/harness/net_ts/impl/linux_epoll.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
// See
//
// - https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/
// - https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/
class linux_epoll_io_service : public IoServiceBase {
 public:
  using native_handle_type = impl::socket::native_handle_type;

  static constexpr const int kSettableEvents = EPOLLIN | EPOLLOUT;
  static constexpr const int kAlwaysEnabledEvents = EPOLLHUP | EPOLLERR;
  static constexpr const int kAllEvents =
      kSettableEvents | kAlwaysEnabledEvents;

  ~linux_epoll_io_service() override { close(); }

  bool is_open() const noexcept {
    return epfd_ != impl::file::kInvalidHandle &&
           (notify_fd_ != impl::file::kInvalidHandle ||
            (wakeup_fds_.first != impl::file::kInvalidHandle &&
             wakeup_fds_.second != impl::file::kInvalidHandle));
  }

  stdx::expected<void, std::error_code> open() noexcept override {
    if (is_open()) {
      return stdx::make_unexpected(
          make_error_code(net::socket_errc::already_open));
    }

    auto res = impl::epoll::create();
    if (!res) return stdx::make_unexpected(res.error());

    epfd_ = *res;
#if defined(USE_EVENTFD)
    notify_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (notify_fd_ != impl::file::kInvalidHandle) {
      add_fd_interest_permanent(notify_fd_, impl::socket::wait_type::wait_read);

      return {};
    }
#endif
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

    add_fd_interest_permanent(wakeup_fds_.first,
                              impl::socket::wait_type::wait_read);

    return {};
  }

  void on_notify() {
    if (notify_fd_ != impl::file::kInvalidHandle) {
      uint64_t some{1};
      ssize_t res;
      do {
        res = ::read(notify_fd_, &some, sizeof(some));
        // in case of EINTR, loop again
        // otherwise exit
        //
        // no need to loop again on success, as the read() will reset the
        // counter to 0 anyway.
      } while (res == -1 && errno == EINTR);
    } else {
      std::array<uint8_t, 256> buf;
      ssize_t res;
      do {
        res = ::read(wakeup_fds_.first, buf.data(), buf.size());
        // in case of EINTR, loop again
        // in case of read > 0, loop again
        // otherwise exist
      } while (res > 0 || (res == -1 && errno == EINTR));
    }
  }

  /**
   * notify the poll_one() that something may have changed.
   *
   * can be called from another thread then poll_one().
   */
  void notify() override {
    if (!is_open()) return;

    // optimization idea:
    //
    // if notify() runs in the same thread as poll_one() runs in,
    // then there is no need to interrupt the poll_one() as it couldn't be
    // running
    //
    // it would save the poll_one(), read(), write() call.

    if (notify_fd_ != impl::file::kInvalidHandle) {
      ssize_t res;
      do {
        uint64_t one{1};
        res = ::write(notify_fd_, &one, sizeof(one));
        // retry if interrupted
      } while ((res == -1) && (errno == EINTR));
    } else {
      ssize_t res;
      do {
        res = ::write(wakeup_fds_.second, ".", 1);
        // retry if interrupted
      } while ((res == -1) && (errno == EINTR));
    }
  }

  stdx::expected<void, std::error_code> close() {
    if (wakeup_fds_.first != impl::file::kInvalidHandle) {
      remove_fd(wakeup_fds_.first);

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

    if (notify_fd_ != impl::file::kInvalidHandle) {
      impl::file::close(notify_fd_);
      notify_fd_ = impl::file::kInvalidHandle;
    }

    return {};
  }

  class FdInterest {
   public:
    stdx::expected<void, std::error_code> merge(int epfd, native_handle_type fd,
                                                impl::socket::wait_type wt,
                                                bool oneshot) {
      uint32_t new_events{};
      switch (wt) {
        case impl::socket::wait_type::wait_read:
          new_events = EPOLLIN;
          break;
        case impl::socket::wait_type::wait_write:
          new_events = EPOLLOUT;
          break;
        case impl::socket::wait_type::wait_error:
          new_events = EPOLLERR | EPOLLHUP;
          break;
      }

      epoll_event ev{};
      ev.data.fd = fd;
      new_events |= EPOLLET;

      if (oneshot) {
        new_events |= EPOLLONESHOT;
      }

      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(b.mtx_);
      const auto it = b.interest_.find(fd);

      auto old_events = (it == b.interest_.end()) ? 0 : it->second;
      auto merged_events = new_events | old_events;

      // the events passed to epoll should only contain IN|OUT
      ev.events = merged_events & ~kAlwaysEnabledEvents;

      if ((old_events & kAllEvents) == 0) {
        // no events where registered before, add.
        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::add, fd, &ev);
        if (!ctl_res) return ctl_res;
      } else {
        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::mod, fd, &ev);
        if (!ctl_res) return ctl_res;
      }

      // the tracked events should contain IN|OUT|ERR|HUP
      if (it != b.interest_.end()) {
        it->second = merged_events;
      } else {
        b.interest_.emplace(fd, merged_events);
      }

      return {};
    }

    stdx::expected<void, std::error_code> erase(int epfd,
                                                native_handle_type fd) {
      auto &b = bucket(fd);
      // std::cerr << __LINE__ << ": del: " << fd << std::endl;

      std::lock_guard<std::mutex> lk(b.mtx_);

      // may be called from another thread through ->cancel()
      const auto it = b.interest_.find(fd);
      if (it != b.interest_.end()) {
        if ((it->second & kAllEvents) != 0) {
          auto epoll_ctl_res =
              impl::epoll::ctl(epfd, impl::epoll::Cmd::del, fd, nullptr);
          if (!epoll_ctl_res) return epoll_ctl_res;
        }

        b.interest_.erase(it);
      } else {
        // return ENOENT as epoll_ctl() would do
        return stdx::make_unexpected(
            make_error_code(std::errc::no_such_file_or_directory));
      }

      return {};
    }

    // remove interest for revent from file-descriptor.
    stdx::expected<void, std::error_code> remove_fd_interest(
        int epfd, native_handle_type fd, uint32_t revent) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(b.mtx_);

      const auto it = b.interest_.find(fd);
      if (it == b.interest_.end()) {
        // return ENOENT as epoll_ctl() would do
        return stdx::make_unexpected(
            make_error_code(std::errc::no_such_file_or_directory));
      }

      // fd is found
      auto &interest = *it;

      // one-shot-events which fired
      const auto fd_events = revent & kAllEvents;
      const auto updated_fd_events = interest.second & ~fd_events;

      if ((updated_fd_events & kSettableEvents) != 0) {
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = updated_fd_events & ~kAlwaysEnabledEvents;

        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::mod, fd, &ev);
        if (!ctl_res) return stdx::make_unexpected(ctl_res.error());
      } else if ((updated_fd_events & kAllEvents) == 0) {
        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::del, fd, nullptr);
        if (!ctl_res) return stdx::make_unexpected(ctl_res.error());
      }

      interest.second = updated_fd_events;

      return {};
    }

    /**
     * update registered fd-interest after a oneshot event fired.
     */
    stdx::expected<void, std::error_code> after_event_fired(
        int epfd, native_handle_type fd, uint32_t revent) {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(b.mtx_);

      const auto it = b.interest_.find(fd);
      if (it == b.interest_.end()) {
        // return ENOENT as epoll_ctl() would do
        return stdx::make_unexpected(
            make_error_code(std::errc::no_such_file_or_directory));
      }

      auto &interest = *it;

      if (!(interest.second & EPOLLONESHOT)) {
        // not a oneshot event. The interest hasn't changed.
        return {};
      }

      // check that the one-shot-events IN and OUT are expected and tracked.
      //
      // interest   | revent   | result
      // -----------+----------+-------
      // {}         | {IN}     | Fail
      // {}         | {OUT}    | Fail
      // {}         | {IN,OUT} | Fail
      // {}         | {ERR}    | Ok({})
      // {}         | {IN,ERR} | Fail
      // {IN}       | {IN}     | Ok({})
      // {IN}       | {OUT}    | Fail
      // {IN}       | {IN,OUT} | Fail
      // {IN}       | {ERR}    | Ok({IN})
      // {IN}       | {IN,ERR} | Ok({})
      // {IN,OUT}   | {IN}     | Ok({OUT})
      // {IN,OUT}   | {OUT}    | Ok({IN})
      // {IN,OUT}   | {IN,OUT} | Ok({})
      // {IN,OUT}   | {ERR}    | Ok({IN,OUT})
      // {IN,OUT}   | {IN,ERR} | Ok({OUT})

      // events which fired
      const auto fd_events = revent & kAllEvents;

      // events that we are interested in.
      const auto fd_interest = interest.second & kAllEvents;

      if (fd_events != 0 &&  //
          (fd_events & fd_interest) == 0) {
        std::cerr << "after_event_fired(" << fd << ", "
                  << std::bitset<32>(fd_events) << ") not in "
                  << std::bitset<32>(fd_interest) << std::endl;
        return stdx::make_unexpected(
            make_error_code(std::errc::argument_out_of_domain));
      }

      // update the fd-interest
      const auto updated_fd_events = interest.second & ~fd_events;

      if ((updated_fd_events & kSettableEvents) != 0) {
        // if a one shot event with multiple waiting events fired for one of the
        // events, it removes all interests for the fd.
        //
        // waiting for:      IN|OUT
        // fires:            IN
        // epoll.interesting:0
        // not fired:        OUT
        //
        // add back the events that have not fired yet.
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = updated_fd_events & ~kAlwaysEnabledEvents;

        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::mod, fd, &ev);
        if (!ctl_res) return stdx::make_unexpected(ctl_res.error());
      } else if ((updated_fd_events & kAllEvents) == 0) {
        // no interest anymore.
        const auto ctl_res =
            impl::epoll::ctl(epfd, impl::epoll::Cmd::del, fd, nullptr);
        if (!ctl_res) return stdx::make_unexpected(ctl_res.error());
      }

      interest.second = updated_fd_events;

      return {};
    }

    std::optional<int32_t> interest(native_handle_type fd) const {
      auto &b = bucket(fd);

      std::lock_guard<std::mutex> lk(b.mtx_);

      const auto it = b.interest_.find(fd);
      if (it != b.interest_.end()) {
        return it->second;
      } else {
        return std::nullopt;
      }
    }

   private:
    // segmented map of fd-to-interest
    //
    // allows to split the map and the mutex
    struct locked_bucket {
      mutable std::mutex mtx_;
      std::unordered_map<impl::socket::native_handle_type, uint32_t> interest_;
    };

    // get locked bucket by file-descriptor.
    locked_bucket &bucket(native_handle_type fd) {
      const size_t ndx = fd % buckets_.size();

      return buckets_[ndx];
    }

    const locked_bucket &bucket(native_handle_type fd) const {
      const size_t ndx = fd % buckets_.size();

      return buckets_[ndx];
    }

    // segment the fd-to-interest map into N buckets
    std::array<locked_bucket, 101> buckets_;
  };

  stdx::expected<void, std::error_code> add_fd_interest(
      native_handle_type fd, impl::socket::wait_type wt) override {
    return registered_events_.merge(epfd_, fd, wt, true);
  }

  stdx::expected<void, std::error_code> add_fd_interest_permanent(
      native_handle_type fd, impl::socket::wait_type wt) {
    return registered_events_.merge(epfd_, fd, wt, false);
  }

  stdx::expected<void, std::error_code> remove_fd(
      native_handle_type fd) override {
    std::lock_guard lk(fd_events_mtx_);
    auto res = registered_events_.erase(epfd_, fd);
    if (res) {
      // remove all events which are already fetched by poll_one()
      for (size_t ndx = fd_events_processed_; ndx < fd_events_size_;) {
        auto ev = fd_events_[ndx];

        if (ev.data.fd == fd) {
          // found one, move it to the end and throw away this one.
          if (ndx != fd_events_size_ - 1) {
            std::swap(fd_events_[ndx], fd_events_[fd_events_size_ - 1]);
          }

          --fd_events_size_;
        } else {
          ++ndx;
        }
      }
    }

    return res;
  }

  stdx::expected<void, std::error_code> remove_fd_interest(
      native_handle_type fd, uint32_t revents) {
    return registered_events_.remove_fd_interest(epfd_, fd, revents);
  }

  /**
   * get current fd-interest.
   *
   * @returns fd-interest as bitmask of raw EPOLL* flags
   */
  std::optional<int32_t> interest(native_handle_type fd) const {
    return registered_events_.interest(fd);
  }

  stdx::expected<fd_event, std::error_code> pop_event() {
    size_t ndx = fd_events_processed_;

    auto ev = fd_events_[ndx];

    // if there are multiple events:
    // - OUT before IN.
    // - IN before ERR|HUP.
    // - ERR before HUP.
    short revent{};
    if (ev.events & EPOLLOUT) {
      fd_events_[ndx].events &= ~EPOLLOUT;
      revent = EPOLLOUT;
    } else if (ev.events & EPOLLIN) {
      fd_events_[ndx].events &= ~EPOLLIN;
      revent = EPOLLIN;
    } else if (ev.events & EPOLLERR) {
      fd_events_[ndx].events &= ~EPOLLERR;
      revent = EPOLLERR;
    } else if (ev.events & EPOLLHUP) {
      fd_events_[ndx].events &= ~EPOLLHUP;
      revent = EPOLLHUP;
    }

    // all interesting events processed, go the next one.
    if ((fd_events_[ndx].events & (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP)) ==
        0) {
      fd_events_processed_++;
    }

    return fd_event{ev.data.fd, revent};
  }

  stdx::expected<fd_event, std::error_code> update_fd_events(
      std::chrono::milliseconds timeout) {
    decltype(fd_events_) evs{};

    auto res = impl::epoll::wait(epfd_, evs.data(), evs.size(), timeout);

    if (!res) return stdx::make_unexpected(res.error());

    std::lock_guard lk(fd_events_mtx_);
    fd_events_ = evs;

    fd_events_processed_ = 0;
    fd_events_size_ = *res;

    if (fd_events_size_ == 0) {
      return stdx::make_unexpected(make_error_code(std::errc::timed_out));
    }

    for (size_t ndx{}; ndx < fd_events_size_; ++ndx) {
      const ::epoll_event ev = fd_events_[ndx];

      auto after_res = after_event_fired(epfd_, ev.data.fd, ev.events);
      if (!after_res) {
        std::ostringstream oss;
        oss << "after_event_fired(" << ev.data.fd << ", "
            << std::bitset<32>(ev.events) << ") " << after_res.error() << " "
            << after_res.error().message() << std::endl;
        std::cerr << oss.str();
      }
    }

    return pop_event();
  }

  /**
   * poll one event from the registered fd-interest.
   *
   * removes the interest of the event that fired
   *
   * @param timeout wait at most timeout milliseconds
   *
   * @returns fd_event which fired
   * @retval std::errc::timed_out in case of timeout
   */
  stdx::expected<fd_event, std::error_code> poll_one(
      std::chrono::milliseconds timeout) override {
    if (!is_open()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    auto ev_res = [this]() -> stdx::expected<fd_event, std::error_code> {
      std::lock_guard lk(fd_events_mtx_);

      if (fd_events_processed_ == fd_events_size_) {
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

    if ((notify_fd_ != impl::file::kInvalidHandle)
            ? (ev.fd == notify_fd_)
            : (ev.fd == wakeup_fds_.first)) {
      // wakeup fd fired
      //
      // - don't remove the interest for it
      // - report to the caller that we don't have an event yet by saying we got
      // interrupted
      on_notify();

      return stdx::make_unexpected(make_error_code(std::errc::interrupted));
    }

    return ev;
  }

 private:
  FdInterest registered_events_;

  // the event-set should be large enough to get a full picture as we otherwise
  // might starve connections because we fetch a hot set of fds instead of the
  // full set
  //
  // ready-set = [ 1 2 3 4 5 6 ]
  //
  // epoll_wait(.., 4, ...) = [ 1 2 3 4 ]
  // epoll_ctl(MOD, POLLIN, 1)
  // epoll_ctl(MOD, POLLIN, 2)
  // epoll_ctl(MOD, POLLIN, 3)
  // epoll_ctl(MOD, POLLIN, 4)
  //
  // ... 1, 2, 3, 4 may become ready in the meantime
  //
  // epoll_wait(.., 4, ...) = [ 1 2 3 4 ]
  //
  // ... and 5, 6 never get processed.
  std::mutex fd_events_mtx_;
  std::array<epoll_event, 8192> fd_events_{};
  size_t fd_events_processed_{0};
  size_t fd_events_size_{0};
  impl::file::file_handle_type epfd_{impl::file::kInvalidHandle};

  std::pair<impl::file::file_handle_type, impl::file::file_handle_type>
      wakeup_fds_{impl::file::kInvalidHandle, impl::file::kInvalidHandle};

  impl::file::file_handle_type notify_fd_{impl::file::kInvalidHandle};

  stdx::expected<void, std::error_code> after_event_fired(int epfd,
                                                          native_handle_type fd,
                                                          uint32_t revents) {
    return registered_events_.after_event_fired(epfd, fd, revents);
  }
};
}  // namespace net

#endif
#endif
