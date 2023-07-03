/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_EXECUTOR_H_
#define MYSQL_HARNESS_NET_TS_EXECUTOR_H_

#include <algorithm>  // for_each
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>  // allocator
#include <mutex>
#include <queue>
#include <stdexcept>  // logic_error
#include <thread>
#include <type_traits>  // decay_t, enable_if
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "my_compiler.h"
#include "mysql/harness/net_ts/impl/callstack.h"
#include "mysql/harness/net_ts/netfwd.h"

namespace net {
enum class fork_event { prepare, parent, child };

// 13.3 [async.async.result]
template <class CompletionToken, class Signature>
class async_result;

template <class CompletionToken, class Signature>
class async_result {
 public:
  using completion_handler_type = CompletionToken;
  using return_type = void;

  explicit async_result(completion_handler_type &) {}
  async_result(const async_result &) = delete;
  async_result &operator=(const async_result &) = delete;

  return_type get() {}
};

// 13.4 [async.async.completion]
template <class CompletionToken, class Signature>
class async_completion;

template <class CompletionToken, class Signature>
class async_completion {
  using result_type = async_result<std::decay_t<CompletionToken>, Signature>;

 public:
  using completion_handler_type = typename result_type::completion_handler_type;

 private:
  using handler_type = std::conditional_t<
      std::is_same<CompletionToken, completion_handler_type>::value,
      completion_handler_type &, completion_handler_type>;

 public:
  explicit async_completion(CompletionToken &t)
      : completion_handler{std::forward<handler_type>(t)},
        result{completion_handler} {}
  async_completion(const async_completion &) = delete;
  async_completion &operator=(const async_completion &) = delete;

  handler_type completion_handler;
  result_type result;
};

// 13.5 [async.assoc.alloc]

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")

template <class T, class ProtoAllocator = std::allocator<void>>
struct associated_allocator;

template <class T, class ProtoAllocator = std::allocator<void>>
using associated_allocator_t =
    typename associated_allocator<T, ProtoAllocator>::type;

MY_COMPILER_DIAGNOSTIC_POP()

template <class T, class ProtoAllocator, typename = std::void_t<>>
struct associated_allocator_impl {
  using type = ProtoAllocator;

  static type __get(const T & /* t */, const ProtoAllocator &a) noexcept {
    return a;
  }
};

template <class T, class ProtoAllocator>
struct associated_allocator_impl<T, ProtoAllocator,
                                 std::void_t<typename T::allocator_type>> {
  using type = typename T::allocator_type;

  static type __get(const T &t, const ProtoAllocator & /* a */) noexcept {
    return t.get_allocator();
  }
};

template <class T, class ProtoAllocator>
struct associated_allocator : associated_allocator_impl<T, ProtoAllocator> {
  static auto get(const T &t,
                  const ProtoAllocator &a = ProtoAllocator()) noexcept {
    using Impl = associated_allocator<T, ProtoAllocator>;
    return Impl::__get(t, a);
  }
};

// 13.6 [async.assoc.alloc.get]
template <class T>
associated_allocator_t<T> get_associated_allocator(const T &t) noexcept {
  return associated_allocator<T>::get(t);
}

template <class T, class ProtoAllocator>
associated_allocator_t<T> get_associated_allocator(
    const T &t, const ProtoAllocator &a) noexcept {
  return associated_allocator<T>::get(t, a);
}

// 13.7 [async.exec.ctx]
class service_already_exists : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

class execution_context {
 public:
  class service;

  // 13.7.1 [async.exec.ctx.cons]
  execution_context() = default;
  execution_context(const execution_context &) = delete;
  execution_context &operator=(const execution_context &) = delete;

  // 13.7.2 [async.exec.ctx.dtor]
  virtual ~execution_context() {
    shutdown();
    destroy();
  }

  // 13.7.3 [async.exec.ctx.ops]
  void notify_fork(fork_event e) {
    // prepare is in reverse
    if (e == fork_event::prepare) {
      std::for_each(services_.rbegin(), services_.rend(),
                    [e](auto &svc) { svc.ptr_->notify_fork(e); });
    } else {
      std::for_each(services_.begin(), services_.end(),
                    [e](auto &svc) { svc.ptr_->notify_fork(e); });
    }
  }

 protected:
  // 13.7.4 [async.exec.ctx.protected]
  void shutdown() noexcept {
    // shutdown in reverse insert-order
    std::for_each(services_.rbegin(), services_.rend(), [](auto &svc) {
      if (svc.active_) {
        svc.ptr_->shutdown();
        svc.active_ = false;
      }
    });
  }

  void destroy() noexcept {
    // destroy in reverse insert-order
    while (!services_.empty()) services_.pop_back();

    keys_.clear();
  }

  // as service has a protected destructor unique_ptr can't call it itself and
  // we much provide our own deleter: service_deleter
  template <class Service>
  static void service_deleter(service *svc) {
    delete static_cast<Service *>(svc);
  }

  struct ServicePtr {
    template <class Service>
    ServicePtr(Service *svc) : ptr_(svc, &service_deleter<Service>) {}

    // each service is only shutdown once.
    bool active_{true};

    std::unique_ptr<service, void (*)(service *)> ptr_;
  };

  using service_key_type = std::type_index;

  /**
   * maps selected type to unique identifier.
   */
  template <class Key>
  static service_key_type service_key() {
    return std::type_index(typeid(Key));
  }

  // mutex for services_, keys_
  mutable std::mutex services_mtx_;

  // services in insertion-order
  std::list<ServicePtr> services_;
  std::unordered_map<service_key_type, service *> keys_;

  template <typename Service, class... Args>
  service *add_service(Args &&... args) {
    services_.push_back(
        ServicePtr{new Service{*this, std::forward<Args>(args)...}});

    return services_.back().ptr_.get();
  }

  template <class Service>
  friend typename Service::key_type &use_service(execution_context &ctx);

  template <class Service>
  friend bool has_service(const execution_context &ctx) noexcept;

  template <class Service, class... Args>
  friend Service &make_service(execution_context &ctx, Args &&... args);
};

// 13.7.5 [async.exec.ctx.globals]
template <class Service>
typename Service::key_type &use_service(execution_context &ctx) {
  using Key = typename Service::key_type;

  static_assert(std::is_base_of<execution_context::service, Key>::value,
                "Key must derive from execution_context::service");
  static_assert(std::is_base_of<Key, Service>::value,
                "Service must derive from Key");

  auto key = execution_context::service_key<Key>();

  std::lock_guard<std::mutex> lk(ctx.services_mtx_);

  auto &svc = ctx.keys_[key];
  if (svc == nullptr) {
    // if no service registered, add one
    svc = ctx.add_service<Service>();
  }

  return static_cast<Key &>(*svc);
}

template <class Service, class... Args>
Service &make_service(execution_context &ctx, Args &&... args) {
  using Key = typename Service::key_type;

  static_assert(std::is_base_of<execution_context::service, Key>::value,
                "Key must derive from execution_context::service");
  static_assert(std::is_base_of<Key, Service>::value,
                "Service must derive from Key");

  auto key = execution_context::service_key<Key>();

  std::lock_guard<std::mutex> lk(ctx.services_mtx_);
  auto &svc = ctx.keys_[key];
  if (svc == nullptr) {
    // if no service registered, add one
    svc = ctx.add_service<Service>(std::forward(args)...);
  } else {
    throw service_already_exists(
        "can't make_service(), Service already exists");
  }

  return static_cast<Service &>(*svc);
}

template <class Service>
bool has_service(const execution_context &ctx) noexcept {
  using Key = typename Service::key_type;

  std::lock_guard<std::mutex> lk(ctx.services_mtx_);
  return ctx.keys_.count(execution_context::service_key<Key>()) > 0;
}

// 13.8 [async.exec.ctx.svc]
class execution_context::service {
 protected:
  explicit service(execution_context &owner) : context_{owner} {}
  service(const service &) = delete;
  service &operator=(const service &) = delete;
  virtual ~service() = default;
  execution_context &context() noexcept { return context_; }

 private:
  virtual void shutdown() noexcept = 0;
  virtual void notify_fork(fork_event) noexcept {}

  friend class execution_context;
  execution_context &context_;
};

// 13.9 [async.is.exec]
//
namespace impl {

template <class T, class = std::void_t<>>
struct is_executor : std::false_type {};

// checker for the requirements of a executor
//
// see 13.2.2 [async.reqmts.executor]
//
// - copy-constructible
// - destructible
// - ... no exceptions
// - operator==
// - operator!=
// - .context()
// - .on_work_started()
// - .on_work_finished()
// - .dispatch(void (*)(), allocator)
// - .post(void (*)(), allocator)
// - .defer(void (*)(), allocator)
template <class T, typename U = std::remove_const_t<T>>
auto executor_requirements(U *__x = nullptr, const U *__const_x = nullptr,
                           void (*f)() = nullptr,
                           const std::allocator<int> &a = {})
    -> std::enable_if_t<
        std::conjunction<
            std::is_copy_constructible<T>,
            // methods/operators must exist
            std::is_same<decltype(*__const_x == *__const_x), bool>,
            std::is_same<decltype(*__const_x != *__const_x), bool>,
            std::is_void<decltype(__x->on_work_started())>,
            std::is_void<decltype(__x->on_work_finished())>,
            std::is_void<decltype(__x->dispatch(std::move(f), a))>,
            std::is_void<decltype(__x->post(std::move(f), a))>,
            std::is_void<decltype(__x->defer(std::move(f), a))>>::value,

        // context() may either return execution_context & or E&
        std::void_t<decltype(__x->context()), void()>>;

template <class T>
struct is_executor<T, decltype(executor_requirements<T>())> : std::true_type {};
}  // namespace impl

template <class T>
struct is_executor : impl::is_executor<T> {};

template <class T>
constexpr bool is_executor_v = is_executor<T>::value;

// 13.10 [async.executor.arg]
struct executor_arg_t {};

constexpr executor_arg_t executor_arg = executor_arg_t();

// 13.11 [async.uses.executor]

namespace impl {

template <class T, class Executor, typename = std::void_t<>>
struct uses_executor : std::false_type {};

template <class T, class Executor>
struct uses_executor<T, Executor, std::void_t<typename T::executor_type>>
    : std::is_convertible<Executor, typename T::executor_type> {};

}  // namespace impl

template <class T, class Executor>
struct uses_executor : impl::uses_executor<T, Executor>::type {};

template <class T, class Executor>
constexpr bool uses_executor_v = uses_executor<T, Executor>::value;

// 13.12 [async.assoc.exec]
template <class T, class Executor, typename = std::void_t<>>
struct associated_executor_impl {
  using type = Executor;

  static type __get(const T & /* t */, const Executor &ex) noexcept {
    return ex;
  }
};

template <class T, class Executor>
struct associated_executor_impl<T, Executor,
                                std::void_t<typename T::executor_type>> {
  using type = typename T::executor_type;

  static type __get(const T &t, const Executor & /* a */) noexcept {
    return t.get_executor();
  }
};

template <class T, class Executor = system_executor>
struct associated_executor;

template <class T, class Executor = system_executor>
using associated_executor_t = typename associated_executor<T, Executor>::type;

template <class T, class Executor>
struct associated_executor : associated_executor_impl<T, Executor> {
  static auto get(const T &t, const Executor &ex = Executor()) noexcept {
    using Impl = associated_executor<T, Executor>;
    return Impl::__get(t, ex);
  }
};

// 13.13 [async.assoc.exec.get]
template <class T>
associated_executor_t<T> get_associated_executor(const T &t) noexcept;

template <class T, class Executor>
associated_executor_t<T, Executor> get_associated_executor(
    const T &t, const Executor &ex) noexcept;

template <class T, class ExecutorContext>
associated_executor_t<T, typename ExecutorContext::executor_type>
get_associated_executor(const T &t, const ExecutorContext &ctx) noexcept;

template <class T>
associated_executor_t<T> get_associated_executor(const T &t) noexcept {
  return associated_executor<T>::get(t);
}

template <class T, class Executor>
associated_executor_t<T, Executor> get_associated_executor(
    const T &t, const Executor &ex) noexcept {
  return associated_executor<T, Executor>::get(t, ex);
}

template <class T, class ExecutorContext>
associated_executor_t<T, typename ExecutorContext::executor_type>
get_associated_executor(const T &t, const ExecutorContext &ctx) noexcept {
  return get_associated_executor(t, ctx.get_executor());
}

// 13.14 [async.exec.binder] - not implemented

// 13.15 [async.bind.executor] - not implemented

// 13.16 [async.exec.work.guard]

template <class Executor>
class executor_work_guard {
 public:
  using executor_type = Executor;

  explicit executor_work_guard(const executor_type &ex) noexcept
      : ex_{ex}, owns_{true} {
    ex_.on_work_started();
  }
  executor_work_guard(const executor_work_guard &other) noexcept
      : ex_{other.ex_}, owns_{other.owns_} {
    if (owns_) {
      ex_.on_work_started();
    }
  }
  executor_work_guard(executor_work_guard &&other) noexcept
      : ex_{std::move(other.ex_)}, owns_{std::exchange(other.owns_, false)} {}

  executor_work_guard &operator=(const executor_work_guard &other) = delete;

  ~executor_work_guard() {
    if (owns_) {
      ex_.on_work_finished();
    }
  }

  executor_type get_executor() const noexcept { return ex_; }

  bool owns_work() const noexcept { return owns_; }

  void reset() noexcept {
    if (owns_) {
      ex_.on_work_finished();
    }
    owns_ = false;
  }

 private:
  Executor ex_;
  bool owns_;
};

// 13.17 [async.make.work.guard]

// NOTE: 'is_executor_v' should be used here, but sun-cc fails with
//
// Error: Could not find a match for
//   net::make_work_guard<Executor>(net::io_context::executor_type)
// needed in
//   net::executor_work_guard<net::io_context::executor_type>
//   net::make_work_guard<net::io_context>(net::io_context&).
//
// Using the `is_executor<...>::value` makes it work correctly with all
// compilers
template <class Executor>
std::enable_if_t<is_executor<Executor>::value, executor_work_guard<Executor>>
make_work_guard(const Executor &ex) {
  return executor_work_guard<Executor>(ex);
}

template <class ExecutionContext>
std::enable_if_t<
    std::is_convertible<ExecutionContext &, execution_context &>::value,
    executor_work_guard<typename ExecutionContext::executor_type>>
make_work_guard(ExecutionContext &ctx) {
  return make_work_guard(ctx.get_executor());
}

template <class T>
std::enable_if_t<!is_executor<T>::value &&
                     !std::is_convertible<T &, execution_context &>::value,
                 executor_work_guard<associated_executor_t<T>>>
make_work_guard(const T &t) {
  return make_work_guard(get_associated_executor(t));
}

template <class T, class U>
auto make_work_guard(const T &t, U &&u)
    -> decltype(make_work_guard(get_associated_executor(t,
                                                        std::forward<U>(u)))) {
  return make_work_guard(get_associated_executor(t, std::forward<U>(u)));
}

class system_context;

// 13.18 [async.system.exec]
class system_executor {
 public:
  system_executor() = default;

  system_context &context() const noexcept;

  void on_work_started() const noexcept {}
  void on_work_finished() const noexcept {}

  template <class Func, class ProtoAllocator>
  void dispatch(Func &&f, const ProtoAllocator &a) const;
  template <class Func, class ProtoAllocator>
  void post(Func &&f, const ProtoAllocator &a) const;
  template <class Func, class ProtoAllocator>
  void defer(Func &&f, const ProtoAllocator &a) const;
};

// 13.18.2 [async.system.exec.comparisons]
inline bool operator==(const system_executor &, const system_executor &) {
  return true;
}

inline bool operator!=(const system_executor &, const system_executor &) {
  return false;
}

// 13.19 [async.system.context]
//
// just barely enough of a system_context to run everything in the
// main-thread
class system_context : public execution_context {
 public:
  using executor_type = system_executor;

  system_context() = delete;
  system_context(const system_context &) = delete;
  system_context &operator=(const system_context &) = delete;
  ~system_context() override {
    stop();
    join();
  }

  executor_type get_executor() noexcept { return {}; }

  void stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    stopped_ = true;

    cv_.notify_all();
  }
  bool stopped() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return stopped_;
  }
  void join() {
    if (thread_.joinable()) thread_.join();
  }

 private:
  struct __tag {};
  system_context(__tag) {}

  friend class system_executor;

  void run_() {
    for (;;) {
      std::function<void()> f;
      {
        std::unique_lock<std::mutex> lk(mtx_);

        cv_.wait(lk, [this] { return stopped_ || !tasks_.empty(); });

        if (stopped_) return;

        f = std::move(tasks_.front());
        tasks_.pop();
      }
      f();
    }
  }

  void post_(std::function<void()> f) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (stopped_) return;

    if (!thread_.joinable()) {
      thread_ = std::thread(&system_context::run_, this);
    }

    tasks_.push(std::move(f));

    cv_.notify_one();
  }

  static system_context &get_() noexcept {
    static system_context sc(__tag{});
    return sc;
  }

  std::thread thread_;
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  bool stopped_{false};
};

// 13.18.1 [async.system.exec.ops]

inline system_context &system_executor::context() const noexcept {
  return system_context::get_();
}

template <class Func, class ProtoAllocator>
void system_executor::post(Func &&f, const ProtoAllocator &) const {
  system_context::get_().post_(std::forward<Func>(f));
}

template <class Func, class ProtoAllocator>
void system_executor::dispatch(Func &&f, const ProtoAllocator &) const {
  std::decay_t<Func>{std::forward<Func>(f)}();
}
template <class Func, class ProtoAllocator>
void system_executor::defer(Func &&f, const ProtoAllocator &a) const {
  post(std::forward<Func>(f), a);
}

// 13.20 [async.bad.exec] - not implemented

// 13.21 [async.executor] - not implemented
//
//
namespace impl {
/**
 * function object for net::dispatch(), net::post(), net::defer().
 */
template <class CompletionHandler>
class Dispatcher {
 public:
  explicit Dispatcher(CompletionHandler &handler)
      : handler_{std::move(handler)},
        work_guard_{net::make_work_guard(handler_)} {}

  void operator()() {
    auto alloc = get_associated_allocator(handler_);

    work_guard_.get_executor().dispatch(std::move(handler_), alloc);

    work_guard_.reset();
  }

 private:
  CompletionHandler handler_;
  decltype(net::make_work_guard(handler_)) work_guard_;
};

template <class CompletionHandler>
Dispatcher<CompletionHandler> make_dispatcher(CompletionHandler &handler) {
  return Dispatcher<CompletionHandler>{handler};
}
}  // namespace impl

// 13.22 [async.dispatch]

template <class CompletionToken>
auto dispatch(CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto ex = get_associated_executor(completion.completion_handler);
  auto alloc = get_associated_allocator(completion.completion_handler);
  ex.dispatch(std::move(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class Executor, class CompletionToken>
std::enable_if_t<
    is_executor<Executor>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
dispatch(const Executor &ex, CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto alloc = get_associated_allocator(completion.completion_handler);

  ex.dispatch(impl::make_dispatcher(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class ExecutionContext, class CompletionToken>
std::enable_if_t<
    std::is_convertible<ExecutionContext &, execution_context &>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
dispatch(ExecutionContext &ctx, CompletionToken &&token) {
  return net::dispatch(ctx.get_executor(),
                       std::forward<CompletionToken>(token));
}

// 13.23 [async.post]

/**
 * queue a function call for later execution.
 */
template <class CompletionToken>
auto post(CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto ex = get_associated_executor(completion.completion_handler);
  auto alloc = get_associated_allocator(completion.completion_handler);
  ex.post(std::move(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class Executor, class CompletionToken>
std::enable_if_t<
    is_executor<Executor>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
post(const Executor &ex, CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto alloc = get_associated_allocator(completion.completion_handler);

  ex.post(impl::make_dispatcher(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class ExecutionContext, class CompletionToken>
std::enable_if_t<
    std::is_convertible<ExecutionContext &, execution_context &>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
post(ExecutionContext &ctx, CompletionToken &&token) {
  return net::post(ctx.get_executor(), std::forward<CompletionToken>(token));
}

// 13.24 [async.defer]

template <class CompletionToken>
auto defer(CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto ex = get_associated_executor(completion.completion_handler);
  auto alloc = get_associated_allocator(completion.completion_handler);
  ex.defer(std::move(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class Executor, class CompletionToken>
std::enable_if_t<
    is_executor<Executor>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
defer(const Executor &ex, CompletionToken &&token) {
  async_completion<CompletionToken, void()> completion(token);

  auto alloc = get_associated_allocator(completion.completion_handler);

  ex.defer(impl::make_dispatcher(completion.completion_handler), alloc);

  return completion.result.get();
}

/**
 * queue a function call for later execution.
 */
template <class ExecutionContext, class CompletionToken>
std::enable_if_t<
    std::is_convertible<ExecutionContext &, execution_context &>::value,
    typename async_result<std::decay_t<CompletionToken>, void()>::return_type>
defer(ExecutionContext &ctx, CompletionToken &&token) {
  return net::defer(ctx.get_executor(), std::forward<CompletionToken>(token));
}

// 13.25 [async.strand] - partially implemented

template <class Executor>
class strand {
 public:
  using inner_executor_type = Executor;

  strand() = default;

  explicit strand(Executor ex) : inner_ex_{ex} {}

  template <class ProtoAllocator>
  strand(std::allocator_arg_t, const ProtoAllocator & /* alloc */, Executor ex)
      : inner_ex_{ex} {}

  strand(const strand &other) noexcept : inner_ex_{other.inner_ex_} {}
  strand(strand &&other) noexcept : inner_ex_{std::move(other.inner_ex_)} {}

  template <class OtherExecutor>
  strand(const strand<OtherExecutor> &other) noexcept
      : inner_ex_{other.inner_ex_} {}
  template <class OtherExecutor>
  strand(strand<OtherExecutor> &&other) noexcept
      : inner_ex_{std::move(other.inner_ex_)} {}

  strand operator=(const strand &other) noexcept {
    inner_ex_ = other.inner_ex_;

    return *this;
  }

  strand operator=(strand &&other) noexcept {
    inner_ex_ = std::move(other.inner_ex_);

    return *this;
  }

  template <class OtherExecutor>
  strand operator=(const strand<OtherExecutor> &other) noexcept {
    inner_ex_ = other.inner_ex_;

    return *this;
  }

  template <class OtherExecutor>
  strand operator=(strand<OtherExecutor> &&other) noexcept {
    inner_ex_ = std::move(other.inner_ex_);

    return *this;
  }

  ~strand();

  // strand ops

  inner_executor_type get_inner_executor() const noexcept { return inner_ex_; }

  bool running_in_this_thread() const noexcept {
    return impl::Callstack<strand>::contains(this) != nullptr;
  }

  execution_context &context() const noexcept { return inner_ex_.context(); }

  void on_work_started() const noexcept { inner_ex_.on_work_started(); }
  void on_work_finished() const noexcept { inner_ex_.on_work_finished(); }

  template <class Func, class ProtoAllocator>
  void dispatch(Func &&f, const ProtoAllocator & /* a */) const {
    if (running_in_this_thread()) {
      std::forward<Func>(f)();
    }
  }
  template <class Func, class ProtoAllocator>
  void post(Func &&f, const ProtoAllocator &a) const;
  template <class Func, class ProtoAllocator>
  void defer(Func &&f, const ProtoAllocator &a) const;

 private:
  Executor inner_ex_;

  bool running_{false};
  std::queue<std::function<void()>> jobs_;
};

template <class Executor>
bool operator==(const strand<Executor> &a, const strand<Executor> &b);

template <class Executor>
bool operator!=(const strand<Executor> &a, const strand<Executor> &b) {
  return !(a == b);
}

// 13.26 [async.use.future] - not implemented

// 13.27 [async.packaged.task.spec] - not implemented

}  // namespace net

#endif
