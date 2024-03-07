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

#ifndef MYSQL_HARNESS_NET_TS_IO_CONTEXT_H_
#define MYSQL_HARNESS_NET_TS_IO_CONTEXT_H_

#include <atomic>
#include <chrono>
#include <iterator>
#include <limits>  // numeric_limits
#include <list>
#include <map>
#include <memory>  // unique_ptr
#include <mutex>
#include <system_error>  // error_code
#include <unordered_map>
#include <utility>
#include <vector>

#include "my_config.h"  // HAVE_EPOLL
#include "mysql/harness/net_ts/executor.h"
#include "mysql/harness/net_ts/impl/callstack.h"
#include "mysql/harness/net_ts/impl/kqueue_io_service.h"
#include "mysql/harness/net_ts/impl/linux_epoll_io_service.h"
#include "mysql/harness/net_ts/impl/poll_io_service.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_service.h"
#include "mysql/harness/net_ts/netfwd.h"
#include "mysql/harness/stdx/expected.h"

namespace net {

#if defined(HAVE_EPOLL)
using io_service_impl_default = linux_epoll_io_service;
#else
using io_service_impl_default = poll_io_service;
#endif

class io_context : public execution_context {
 public:
  class executor_type;

  using count_type = size_t;
  using native_handle_type = impl::socket::native_handle_type;

  io_context()
      : io_context{std::make_unique<net::impl::socket::SocketService>(),
                   std::make_unique<io_service_impl_default>()} {}

  io_context(
      std::unique_ptr<net::impl::socket::SocketServiceBase> &&socket_service,
      std::unique_ptr<IoServiceBase> &&io_service)
      : socket_service_{std::move(socket_service)},
        io_service_{std::move(io_service)},
        io_service_open_res_{io_service_->open()} {}

  explicit io_context(int /* concurrency_hint */) : io_context() {}

  ~io_context() {
    active_ops_.release_all();
    cancelled_ops_.clear();
    // Make sure the services are destroyed before our internal fields. The
    // services own the timers that can indirectly call our methods when
    // destructed. See UT NetTS_io_context.pending_timer_on_destroy for an
    // example.
    destroy();
  }

  io_context(const io_context &) = delete;
  io_context &operator=(const io_context &) = delete;

  executor_type get_executor() noexcept;

  count_type run();

  template <class Rep, class Period>
  count_type run_for(const std::chrono::duration<Rep, Period> &rel_time);

  template <class Clock, class Duration>
  count_type run_until(
      const std::chrono::time_point<Clock, Duration> &abs_time);

  count_type run_one();

  template <class Rep, class Period>
  count_type run_one_for(const std::chrono::duration<Rep, Period> &rel_time);

  template <class Clock, class Duration>
  count_type run_one_until(
      const std::chrono::time_point<Clock, Duration> &abs_time);

  count_type poll();
  count_type poll_one();
  void stop() {
    {
      std::lock_guard<std::mutex> lk(mtx_);
      stopped_ = true;
    }

    notify_io_service_if_not_running_in_this_thread();
  }

  bool stopped() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return stopped_;
  }

  void restart() {
    std::lock_guard<std::mutex> lk(mtx_);
    stopped_ = false;
  }

  impl::socket::SocketServiceBase *socket_service() const {
    return socket_service_.get();
  }

  IoServiceBase *io_service() const { return io_service_.get(); }

  /**
   * get the status of the implicit open() call of the io-service.
   *
   * the io_service_.open() may fail due to out-of-file-descriptors.
   *
   * run() will fail silently if the io-service failed to open.
   *
   * @returns std::error_code on error
   */
  stdx::expected<void, std::error_code> open_res() const noexcept {
    return io_service_open_res_;
  }

 private:
  /**
   * queued work from io_context::executor_type::dispatch()/post()/defer().
   */
  class DeferredWork {
   public:
    // simple, generic storage of callable.
    //
    // std::function<void()> is similar, but doesn't work for move-only
    // callables like lambda's that capture a move-only type
    class BasicCallable {
     public:
      virtual ~BasicCallable() = default;

      virtual void invoke() = 0;
    };

    template <class Func>
    class Callable : public BasicCallable {
     public:
      Callable(Func &&f) : f_{std::forward<Func>(f)} {}

      void invoke() override { f_(); }

     private:
      Func f_;
    };

    using op_type = std::unique_ptr<BasicCallable>;

    /**
     * run a deferred work item.
     *
     * @returns number work items run.
     * @retval 0 work queue was empty, nothing was run.
     */
    size_t run_one() {
      // tmp list to hold the current operation to run.
      //
      // makes it simple and fast to move the head element and shorten the time
      // the lock is held.
      decltype(work_) tmp;

      // lock is only needed as long as we modify the work-queue.
      {
        std::lock_guard<std::mutex> lk(work_mtx_);

        if (work_.empty()) return 0;

        // move the head of the work queue out and release the lock.
        //
        // note: std::list.splice() moves pointers.
        tmp.splice(tmp.begin(), work_, work_.begin());
      }

      // run the deferred work.
      tmp.front()->invoke();

      // and destruct the list at the end.

      return 1;
    }

    /**
     * queue work for later execution.
     */
    template <class Func, class ProtoAllocator>
    void post(Func &&f, const ProtoAllocator & /* a */) {
      std::lock_guard<std::mutex> lk(work_mtx_);

      work_.emplace_back(
          std::make_unique<Callable<Func>>(std::forward<Func>(f)));
    }

    /**
     * check if work is queued for later execution.
     *
     * @retval true if some work is queued.
     */
    bool has_outstanding_work() const {
      std::lock_guard<std::mutex> lk(work_mtx_);
      return !work_.empty();
    }

   private:
    mutable std::mutex work_mtx_;
    std::list<op_type> work_;
  };

  DeferredWork deferred_work_;

  /**
   * defer work for later execution.
   */
  template <class Func, class ProtoAllocator>
  void defer_work(Func &&f, const ProtoAllocator &a) {
    deferred_work_.post(std::forward<Func>(f), a);

    // wakeup the possibly blocked io-thread.
    notify_io_service_if_not_running_in_this_thread();
  }

  template <class Clock, class Duration>
  count_type do_one_until(
      std::unique_lock<std::mutex> &lk,
      const std::chrono::time_point<Clock, Duration> &abs_time);

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

  // must be first member-var to ensure it is destroyed last
  std::unique_ptr<impl::socket::SocketServiceBase> socket_service_;
  std::unique_ptr<IoServiceBase> io_service_;
  stdx::expected<void, std::error_code> io_service_open_res_;

  // has outstanding work
  //
  // work is outstanding when
  //
  // - work-count from on_work_started()/on_work_finished() is more than 0 and
  // - any active or cancelled operations are still ongoing
  bool has_outstanding_work() const {
    if (!cancelled_ops_.empty()) return true;
    if (active_ops_.has_outstanding_work()) return true;
    if (deferred_work_.has_outstanding_work()) return true;

    if (work_count_ > 0) return true;

    return false;
  }

  // monitor all .run()s in the io-context needs to be stopped.
  class monitor {
   public:
    monitor(io_context &ctx) : ctx_{ctx} {}

    monitor(const monitor &) = delete;
    monitor(monitor &&) = delete;

    ~monitor() {
      std::lock_guard<std::mutex> lk(ctx_.mtx_);

      // ctx_.call_stack_.pop_back();

      if (!ctx_.has_outstanding_work()) {
        // like stop(), just that we already have the mutex
        ctx_.stopped_ = true;
        ctx_.io_service_->notify();
      }
    }

   private:
    io_context &ctx_;
  };

  stdx::expected<void, std::error_code> cancel(native_handle_type fd);

  /**
   * base class of async operation.
   *
   * - file-descriptor
   * - wait-event
   */
  class async_op {
   public:
    using wait_type = impl::socket::wait_type;

    async_op(native_handle_type fd, wait_type ev) : fd_{fd}, event_{ev} {}

    virtual ~async_op() = default;

    virtual void run(io_context &) = 0;

    void cancel() { fd_ = impl::socket::kInvalidSocket; }
    bool is_cancelled() const { return fd_ == impl::socket::kInvalidSocket; }

    native_handle_type native_handle() const { return fd_; }
    wait_type event() const { return event_; }

   private:
    native_handle_type fd_;
    wait_type event_;
  };

  /**
   * async operation with callback.
   */
  template <class Op>
  class async_op_impl : public async_op {
   public:
    async_op_impl(Op &&op, native_handle_type fd, impl::socket::wait_type wt)
        : async_op{fd, wt}, op_{std::forward<Op>(op)} {}

    void run(io_context & /* io_ctx */) override {
      if (is_cancelled()) {
        op_(make_error_code(std::errc::operation_canceled));
      } else {
        op_(std::error_code{});
      }
    }

   private:
    Op op_;
  };

  class AsyncOps {
   public:
    using element_type = std::unique_ptr<async_op>;

    bool has_outstanding_work() const {
      std::lock_guard<std::mutex> lk(mtx_);

      return !ops_.empty();
    }

    void push_back(element_type &&t) {
      const auto handle = t->native_handle();

      std::lock_guard<std::mutex> lk(mtx_);

      auto it = ops_.find(handle);
      if (it != ops_.end()) {
        it->second.push_back(std::move(t));
      } else {
        std::vector<element_type> v;
        v.push_back(std::move(t));
        ops_.emplace(handle, std::move(v));
      }
    }

    element_type extract_first(native_handle_type fd, short events) {
      return extract_first(fd, [events](auto const &el) {
        return static_cast<short>(el->event()) & events;
      });
    }

    element_type extract_first(native_handle_type fd) {
      return extract_first(fd, [](auto const &) { return true; });
    }

    void release_all() {
      // We expect that this method is called before AsyncOps destructor, to
      // make sure that ops_ map is empty when the destructor executes. If the
      // ops_ is not empty when destructed, the destructor of its element can
      // trigger a method that will try to access that map (that is destructed).
      // Example: we have an AsyncOp that captures some Socket object with a
      // smart pointer. When the destructor of this AsyncOp is called, it can
      // also call the destructor of that Socket, which in turn will call
      // socket.close(), causing the Socket to unregister its operations in
      // its respective io_context object which is us (calls extract_first()).
      std::list<element_type> ops_to_delete;
      {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto &fd_ops : ops_) {
          for (auto &fd_op : fd_ops.second) {
            ops_to_delete.push_back(std::move(fd_op));
          }
        }
        ops_.clear();
        // It is important that we release the mtx_ here before the
        // ops_to_delete go out of scope and are deleted. AsyncOp destructor can
        // indirectly call extract_first which would lead to a deadlock.
      }
    }

   private:
    template <class Pred>
    element_type extract_first(native_handle_type fd, Pred &&pred) {
      std::lock_guard<std::mutex> lk(mtx_);

      const auto it = ops_.find(fd);
      if (it != ops_.end()) {
        auto &async_ops = it->second;

        const auto end = async_ops.end();
        for (auto cur = async_ops.begin(); cur != end; ++cur) {
          auto &el = *cur;

          if (el->native_handle() == fd && pred(el)) {
            auto op = std::move(el);

            if (async_ops.size() == 1) {
              // remove the current container and with it its only element
              ops_.erase(it);
            } else {
              // remove the current entry
              async_ops.erase(cur);
            }

            return op;
          }
        }
      }

      return {};
    }

    std::unordered_map<native_handle_type, std::vector<element_type>> ops_{
        16 * 1024};

    mutable std::mutex mtx_;
  };

  AsyncOps active_ops_;

  // cancelled async operators.
  std::list<std::unique_ptr<async_op>> cancelled_ops_;

  template <class Op>
  void async_wait(native_handle_type fd, impl::socket::wait_type wt, Op &&op) {
    // add the socket-wait op to the queue
    active_ops_.push_back(
        std::make_unique<async_op_impl<Op>>(std::forward<Op>(op), fd, wt));

    {
      auto res = io_service_->add_fd_interest(fd, wt);
      if (!res) {
#if 0
        // fd may be -1 or so
        std::cerr << "!! add_fd_interest(" << fd << ", ..."
                  << ") " << res.error() << " " << res.error().message()
                  << std::endl;
#endif
        // adding failed. Cancel it again.
        //
        // code should be similar to ::cancel(fd)
        std::lock_guard<std::mutex> lk(mtx_);

        if (auto async_op =
                active_ops_.extract_first(fd, static_cast<short>(wt))) {
          async_op->cancel();
          cancelled_ops_.push_back(std::move(async_op));
        }
      }
    }

    notify_io_service_if_not_running_in_this_thread();
  }

  class timer_queue_base : public execution_context::service {
   protected:
    explicit timer_queue_base(execution_context &ctx) : service{ctx} {}

    mutable std::mutex queue_mtx_;

   public:
    virtual bool run_one() = 0;
    virtual std::chrono::milliseconds next() const = 0;
  };

  template <class Timer>
  class timer_queue : public timer_queue_base {
   public:
    using key_type = timer_queue;

    explicit timer_queue(execution_context &ctx) : timer_queue_base{ctx} {
      // add timer_queue to io_context

      auto &io_ctx = static_cast<io_context &>(ctx);

      // @note: don't move this lock+push into the timer_queue_base constructor
      //
      // @see
      // https://github.com/google/sanitizers/wiki/ThreadSanitizerPopularDataRaces#data-race-on-vptr-during-construction
      std::lock_guard<std::mutex> lk(io_ctx.mtx_);
      io_ctx.timer_queues_.push_back(this);
    }

    void shutdown() noexcept override {}

    io_context &context() noexcept {
      return static_cast<io_context &>(service::context());
    }

    template <class Op>
    void push(const Timer &timer, Op &&op) {
      context().get_executor().on_work_started();

      std::lock_guard<std::mutex> lk(queue_mtx_);

#if 0
      pending_timers_.insert(
          std::upper_bound(
              pending_timers_.begin(), pending_timers_.end(), timer.expiry(),
              [](const auto &a, const auto &b) { return a < b->expiry(); }),
          std::make_unique<pending_timer_op<Op>>(timer, std::forward<Op>(op)));
#else
      if (timer.id() == nullptr) abort();

      // add timer
      pending_timers_.emplace(std::make_pair(
          timer.id(),
          std::make_unique<pending_timer_op<Op>>(timer, std::forward<Op>(op))));

      if (timer.id() == nullptr) abort();
      if (timer.expiry() == Timer::time_point::min()) abort();

      // sorted timer ids by expiry
      pending_timer_expiries_.emplace(
          std::make_pair(timer.expiry(), timer.id()));
#endif
    }

    std::chrono::milliseconds next() const override {
      typename Timer::time_point expiry;
      {
        std::lock_guard<std::mutex> lk(queue_mtx_);

        // no pending timers, return the max-timeout
        if (cancelled_timers_.empty()) {
          if (pending_timer_expiries_.empty())
            return std::chrono::milliseconds::max();

#if 0
          expiry = pending_timers_.front()->expiry();
#else
          expiry = pending_timer_expiries_.begin()->first;
#endif
        } else {
          // cancelled timers should be executed directly
          return std::chrono::milliseconds::min();
        }

        // the lock isn't needed anymore.
      }

      auto duration = Timer::traits_type::to_wait_duration(expiry);
      if (duration < duration.zero()) {
        duration = duration.zero();
      }

      // round up the next wait-duration to wait /at least/ the expected time.
      //
      // In case the expiry is 990us, wait 1ms
      // If it is 0ns, leave it at 0ms;

      auto duration_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(duration);

      using namespace std::chrono_literals;

      // round up to the next millisecond.
      if ((duration - duration_ms).count() != 0) {
        duration_ms += 1ms;
      }

      return duration_ms;
    }

    bool run_one() override {
      std::unique_ptr<pending_timer> pt;

      {
        std::lock_guard<std::mutex> lk(queue_mtx_);

        // if the pending-timers queue is empty, leave
        // if the top is cancelled or expired, run it
        if (cancelled_timers_.empty()) {
          if (pending_timers_.empty()) return false;

#if 0
          // list
          if (pending_timers_.front()->expiry() > Timer::clock_type::now()) {
            return false;
          }
          pt = std::move(pending_timers_.front());
          pending_timers_.pop_front();
#else
          if (pending_timers_.size() != pending_timer_expiries_.size()) abort();

          auto min = Timer::time_point::min();
          for (const auto &cur : pending_timer_expiries_) {
            if (cur.first < min) abort();

            min = cur.first;
          }

          const auto now = Timer::clock_type::now();

          // multimap
          auto pending_expiry_it = pending_timer_expiries_.begin();
          auto timepoint = pending_expiry_it->first;

          if (timepoint > now) {
            // not expired yet. leave
            return false;
          }
          typename Timer::Id *timer_id = pending_expiry_it->second;

          auto pending_it = pending_timers_.find(timer_id);
          if (pending_it == pending_timers_.end()) {
            abort();
          }
          if (pending_it->second->id() != timer_id) {
            abort();
          }
          if (pending_it->second->expiry() != pending_expiry_it->first) {
            abort();
          }

          pt = std::move(pending_it->second);
          pending_timer_expiries_.erase(pending_expiry_it);
          pending_timers_.erase(pending_it);
#endif
        } else {
          pt = std::move(cancelled_timers_.front());
          cancelled_timers_.pop_front();
        }
      }

      pt->run();

      context().get_executor().on_work_finished();

      return true;
    }

    size_t cancel(const Timer &t) {
      size_t count{};

      {
        std::lock_guard<std::mutex> lk(queue_mtx_);

#if 0
        const auto end = pending_timers_.end();

        // the same timer may be pushed multiple times to the queue
        // therefore, check all entries
        for (auto cur = pending_timers_.begin(); cur != end;) {
          auto &cur_timer = cur->second;
          if (cur_timer->id() == t.id()) {
            cur_timer->cancel();
            ++count;

            auto nxt = std::next(cur);
            // move the timer over to the cancelled timers
            cancelled_timers_.splice(cancelled_timers_.end(), pending_timers_,
                                     cur);
            cur = nxt;
          } else {
            ++cur;
          }
        }
#else
        auto eq_range = pending_timers_.equal_range(t.id());

        for (auto cur = eq_range.first; cur != eq_range.second;) {
          auto expiry_eq_range =
              pending_timer_expiries_.equal_range(cur->second->expiry());

          size_t erase_count{};

          for (auto expiry_cur = expiry_eq_range.first;
               expiry_cur != expiry_eq_range.second;) {
            if (expiry_cur->first == cur->second->expiry() &&
                expiry_cur->second == cur->second->id() && erase_count == 0) {
              expiry_cur = pending_timer_expiries_.erase(expiry_cur);
              ++erase_count;
            } else {
              ++expiry_cur;
            }
          }

          // nothing found ... boom
          if (erase_count == 0) abort();

          cur->second->cancel();

          // move timer to cancelled timers
          cancelled_timers_.emplace_back(std::move(cur->second));

          ++count;

          cur = pending_timers_.erase(cur);
        }
#endif
      }

      return count;
    }

    class pending_timer {
     public:
      using time_point = typename Timer::time_point;
      using timer_id = typename Timer::Id *;

      pending_timer(const Timer &timer)
          : expiry_{timer.expiry()}, id_{timer.id()} {}

      virtual ~pending_timer() = default;

      bool is_cancelled() const { return id_ == nullptr; }
      void cancel() {
        id_ = nullptr;

        // ensure that it bubbles up to the top
        expiry_ = expiry_.min();
      }

      time_point expiry() const noexcept { return expiry_; }
      timer_id id() const { return id_; }

      virtual void run() = 0;

     private:
      time_point expiry_;
      timer_id id_;
    };

    template <class Op>
    class pending_timer_op : public pending_timer {
     public:
      pending_timer_op(const Timer &timer, Op &&op)
          : pending_timer(timer), op_{std::move(op)} {}

      void run() override {
        if (this->is_cancelled()) {
          op_(make_error_code(std::errc::operation_canceled));
        } else {
          op_(std::error_code{});
        }
      }

     private:
      Op op_;
    };

    // cancelled timers, earliest cancelled timer first
    std::list<std::unique_ptr<pending_timer>> cancelled_timers_;

    // active timers, smallest time-point first
    std::multimap<typename Timer::time_point, typename Timer::Id *>
        pending_timer_expiries_;
    std::multimap<typename Timer::Id *, std::unique_ptr<pending_timer>>
        pending_timers_;
  };

  /**
   * async wait for a timer expire.
   *
   * adds the op and timer to the timer_queue
   *
   * @param timer  timer
   * @param op     completion handler to call when timer is triggered
   */
  template <class Timer, class Op>
  void async_wait(const Timer &timer, Op &&op) {
    auto &queue = use_service<timer_queue<Timer>>(*this);

    queue.push(timer, std::forward<Op>(op));

    // wakeup the blocked poll_one() to handle possible timer events.
    notify_io_service_if_not_running_in_this_thread();
  }

  /**
   * cancel all async-ops of a timer.
   */
  template <class Timer>
  size_t cancel(const Timer &timer) {
    if (!has_service<timer_queue<Timer>>(*this)) {
      return 0;
    }

    const auto count = use_service<timer_queue<Timer>>(*this).cancel(timer);
    if (count) {
      // if a timer was canceled, interrupt the io-service
      notify_io_service_if_not_running_in_this_thread();
    }
    return count;
  }

  // cancel oldest
  template <class Timer>
  size_t cancel_one(const Timer & /* timer */) {
    // TODO: implement if async_wait is implemented
    return 0;
  }

  /** pointers to the timer-queues of this io-contexts.
   *
   * timer-queues are one per timer-type (std::chrono::steady_clock,
   * std::chrono::system_clock, ...)
   *
   * timer_queue_base is the base class of the timer-queues
   *
   * the timer-queue's themselves are ownered by the io_context's executor via
   * execution_context::add_service()
   *
   * protected via 'mtx_'
   */
  std::vector<timer_queue_base *> timer_queues_;

  /**
   * mutex that protects the core parts of the io-context.
   *
   * - timer_queues_
   */
  mutable std::mutex mtx_{};

  mutable std::mutex do_one_mtx_{};
  mutable std::condition_variable do_one_cond_{};
  bool is_running_{false};

  void wait_no_runner_(std::unique_lock<std::mutex> &lk) {
    lk.lock();
    wait_no_runner_unlocked_(lk);
  }

  void wait_no_runner_unlocked_(std::unique_lock<std::mutex> &lk) {
    do_one_cond_.wait(lk, [this]() { return is_running_ == false; });

    is_running(true);
  }

  void wake_one_runner_(std::unique_lock<std::mutex> &lk) {
    is_running(false);
    lk.unlock();
    do_one_cond_.notify_one();
  }

  void is_running(bool v) { is_running_ = v; }
  bool is_running() const { return is_running_; }

  void notify_io_service_if_not_running_in_this_thread();
};
}  // namespace net

namespace net {
inline io_context::count_type io_context::run() {
  count_type n = 0;

  std::unique_lock<std::mutex> lk(do_one_mtx_);

  using namespace std::chrono_literals;

  // in the first round, we already have the lock, the all other rounds we
  // need to take the lock first
  for (wait_no_runner_unlocked_(lk); do_one(lk, -1ms) != 0;
       wait_no_runner_(lk)) {
    if (n != std::numeric_limits<count_type>::max()) ++n;
  }
  return n;
}

inline io_context::count_type io_context::run_one() {
  using namespace std::chrono_literals;

  std::unique_lock<std::mutex> lk(do_one_mtx_);

  wait_no_runner_unlocked_(lk);

  return do_one(lk, -1ms);
}

template <class Rep, class Period>
io_context::count_type io_context::run_for(
    const std::chrono::duration<Rep, Period> &rel_time) {
  return run_until(std::chrono::steady_clock::now() + rel_time);
}

template <class Clock, class Duration>
io_context::count_type io_context::run_until(
    const std::chrono::time_point<Clock, Duration> &abs_time) {
  count_type n = 0;

  std::unique_lock<std::mutex> lk(do_one_mtx_);

  using namespace std::chrono_literals;

  // in the first round, we already have the lock, the all other rounds we
  // need to take the lock first
  for (wait_no_runner_unlocked_(lk); do_one_until(lk, abs_time) != 0;
       wait_no_runner_(lk)) {
    if (n != std::numeric_limits<count_type>::max()) ++n;
  }
  return n;
}

template <class Rep, class Period>
inline io_context::count_type io_context::run_one_for(
    const std::chrono::duration<Rep, Period> &rel_time) {
  return run_one_until(std::chrono::steady_clock::now() + rel_time);
}

template <class Clock, class Duration>
inline io_context::count_type io_context::run_one_until(
    const std::chrono::time_point<Clock, Duration> &abs_time) {
  std::unique_lock<std::mutex> lk(do_one_mtx_);

  wait_no_runner_unlocked_(lk);

  return do_one_until(lk, abs_time);
}

inline io_context::count_type io_context::poll() {
  count_type n = 0;
  std::unique_lock<std::mutex> lk(do_one_mtx_);

  using namespace std::chrono_literals;

  for (wait_no_runner_unlocked_(lk); do_one(lk, 0ms) != 0;
       wait_no_runner_(lk)) {
    if (n != std::numeric_limits<count_type>::max()) ++n;
  }
  return n;
}

inline io_context::count_type io_context::poll_one() {
  std::unique_lock<std::mutex> lk(do_one_mtx_);

  using namespace std::chrono_literals;

  wait_no_runner_unlocked_(lk);
  return do_one(lk, 0ms);
}

class io_context::executor_type {
 public:
  executor_type(const executor_type &rhs) noexcept = default;
  executor_type(executor_type &&rhs) noexcept = default;
  executor_type &operator=(const executor_type &rhs) noexcept = default;
  executor_type &operator=(executor_type &&rhs) noexcept = default;

  ~executor_type() = default;

  bool running_in_this_thread() const noexcept {
    return impl::Callstack<io_context>::contains(io_ctx_) != nullptr;
  }
  io_context &context() const noexcept { return *io_ctx_; }

  void on_work_started() const noexcept { ++io_ctx_->work_count_; }
  void on_work_finished() const noexcept { --io_ctx_->work_count_; }

  /**
   * execute function.
   *
   * Effect:
   *
   * The executor
   *
   * - MAY block forward progress of the caller until f() finishes.
   */
  template <class Func, class ProtoAllocator>
  void dispatch(Func &&f, const ProtoAllocator &a) const {
    if (running_in_this_thread()) {
      // run it in this thread.
      std::decay_t<Func>(std::forward<Func>(f))();
    } else {
      // queue function call for later execution.
      post(std::forward<Func>(f), a);
    }
  }

  /**
   * queue function for execution.
   *
   * Effects:
   *
   * The executor
   *
   * - SHALL NOT block forward progress of the caller pending completion of f().
   * - MAY begin f() progress before the call to post completes.
   */
  template <class Func, class ProtoAllocator>
  void post(Func &&f, const ProtoAllocator &a) const {
    io_ctx_->defer_work(std::forward<Func>(f), a);
  }

  /**
   * defer function call for later execution.
   *
   * Effect:
   *
   * The executor:
   *
   * - SHALL NOT block forward progress of the caller pending completion of f().
   * - SHOULD NOT begin f()'s progress before the call to defer()
   *   completes.
   */
  template <class Func, class ProtoAllocator>
  void defer(Func &&f, const ProtoAllocator &a) const {
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

/**
 * cancel all async-ops of a file-descriptor.
 */
inline stdx::expected<void, std::error_code> io_context::cancel(
    native_handle_type fd) {
  bool need_notify{false};
  {
    // check all async-ops
    std::lock_guard<std::mutex> lk(mtx_);

    while (auto op = active_ops_.extract_first(fd)) {
      op->cancel();

      cancelled_ops_.push_back(std::move(op));

      need_notify = true;
    }
  }

  // wakeup the loop to deliver the cancelled fds
  if (true || need_notify) {
    io_service_->remove_fd(fd);

    notify_io_service_if_not_running_in_this_thread();
  }

  return {};
}

template <class Clock, class Duration>
inline io_context::count_type io_context::do_one_until(
    std::unique_lock<std::mutex> &lk,
    const std::chrono::time_point<Clock, Duration> &abs_time) {
  using namespace std::chrono_literals;

  const auto rel_time = abs_time - std::chrono::steady_clock::now();
  auto rel_time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(rel_time);

  if (rel_time_ms < 0ms) {
    // expired already.
    rel_time_ms = 0ms;
  } else if (rel_time_ms < rel_time) {
    // std::chrono::ceil()
    rel_time_ms += 1ms;
  }

  return do_one(lk, rel_time_ms);
}

inline void io_context::notify_io_service_if_not_running_in_this_thread() {
  if (impl::Callstack<io_context>::contains(this) == nullptr) {
    io_service_->notify();
  }
}

// precond: lk MUST be locked
inline io_context::count_type io_context::do_one(
    std::unique_lock<std::mutex> &lk, std::chrono::milliseconds timeout) {
  impl::Callstack<io_context>::Context ctx(this);

  timer_queue_base *timer_q{nullptr};

  monitor mon(*this);

  if (!has_outstanding_work()) {
    wake_one_runner_(lk);
    return 0;
  }

  while (true) {
    // 1. deferred work.
    // 2. timer
    // 3. triggered events.

    // timer (2nd round)
    if (timer_q) {
      if (timer_q->run_one()) {
        wake_one_runner_(lk);
        return 1;
      } else {
        timer_q = nullptr;
      }
    }

    // deferred work
    if (deferred_work_.run_one()) {
      wake_one_runner_(lk);
      return 1;
    }

    // timer
    std::chrono::milliseconds min_duration{0};
    {
      std::lock_guard<std::mutex> lock(mtx_);
      // check the smallest timestamp of all timer-queues
      for (auto q : timer_queues_) {
        const auto duration = q->next();

        if (duration == duration.zero()) {
          timer_q = q;
          min_duration = duration;
          break;
        } else if ((duration != duration.max()) &&
                   (timeout != timeout.zero()) &&
                   (duration < min_duration || timer_q == nullptr)) {
          timer_q = q;
          min_duration = duration;
        }
      }
    }

    // if we have a timer that has fired or was cancelled, run it right away
    if (timer_q && min_duration <= min_duration.zero()) continue;

    if (auto op = [this]() -> std::unique_ptr<async_op> {
          // handle all the cancelled ops without polling first
          std::lock_guard<std::mutex> lock(mtx_);

          // ops have all cancelled operators at the front
          if (!cancelled_ops_.empty() &&
              cancelled_ops_.front()->is_cancelled()) {
            auto cancelled_op = std::move(cancelled_ops_.front());

            cancelled_ops_.pop_front();

            return cancelled_op;
          }

          return {};
        }()) {
      // before we unlock the concurrent io-context-thread-lock increment the
      // work-count to ensure the next waiting thread exiting in case:
      //
      // - no io-events registered
      // - no timers registered
      get_executor().on_work_started();
      wake_one_runner_(lk);
      op->run(*this);
      get_executor().on_work_finished();

      return 1;
    }

    if (stopped() || !io_service_open_res_) {
      break;
    }

    // adjust min-duration according to caller's timeout
    //
    // - if there is no timer queued, use the caller's timeout
    // - if there is a timer queued, reduce min_duration to callers timeout if
    //   it is lower and non-negative.
    //
    // note: negative timeout == infinite.
    if (timer_q == nullptr ||
        (timeout > timeout.zero() && timeout < min_duration)) {
      min_duration = timeout;
    }

    auto res = io_service_->poll_one(min_duration);
    if (!res) {
      if (res.error() == std::errc::interrupted) {
        // poll again as it got interrupted
        continue;
      }
      if (res.error() == std::errc::timed_out && min_duration != timeout &&
          timer_q != nullptr) {
        // poll_one() timed out, we have a timer-queue and the timer's expiry is
        // less than the global timeout or there is no timeout.
        continue;
      }

      wake_one_runner_(lk);

#if 0
      // if the poll returns another error, it is a ok that we don't further
      // check it as we exit cleanly. Still it would be nice to be aware of it
      // in debug builds.
      assert(res.error() == io_service_errc::no_fds ||
             res.error() == std::errc::timed_out);
#endif

      // either poll() timed out or there where no file-descriptors that fired
      return 0;
    }

    //    std::cerr << __LINE__ << ": " << res.value().fd << " - "
    //              << res.value().event << std::endl;

    if (auto op = [this](native_handle_type fd,
                         short events) -> std::unique_ptr<async_op> {
          std::lock_guard<std::mutex> lock(mtx_);

          return active_ops_.extract_first(fd, events);
        }(res->fd, res->event)) {
      get_executor().on_work_started();
      wake_one_runner_(lk);
      op->run(*this);
      get_executor().on_work_finished();
      return 1;
    }
    // we may not find an async-op for this event if it already has been
    // cancelled. Loop around let the "is-cancelled" check handle it.
  }

  wake_one_runner_(lk);
  return 0;
}

}  // namespace net

#endif
