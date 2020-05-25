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

#ifndef MYSQL_HARNESS_NET_TS_EXECUTOR_H_
#define MYSQL_HARNESS_NET_TS_EXECUTOR_H_

#include <algorithm>  // for_each
#include <functional>
#include <list>
#include <memory>  // allocator
#include <mutex>
#include <stdexcept>    // logic_error
#include <type_traits>  // decay_t, enable_if
#include <unordered_map>

#include "mysql/harness/net_ts/netfwd.h"
#include "mysql/harness/stdx/type_traits.h"  // conjunction, void_t

namespace net {
enum class fork_event { prepare, parent, child };

// 13.3 [async.async.result]

// 13.4 [async.async.completion]

// 13.5 [async.assoc.alloc]

template <class T, class ProtoAllocator = std::allocator<void>>
struct associated_allocator;

template <class T, class ProtoAllocator = std::allocator<void>>
using associated_allocator_t =
    typename associated_allocator<T, ProtoAllocator>::type;

template <class T, class ProtoAllocator, typename = stdx::void_t<>>
struct associated_allocator_impl {
  using type = ProtoAllocator;

  static type __get(const T & /* t */, const ProtoAllocator &a) noexcept {
    return a;
  }
};

template <class T, class ProtoAllocator>
struct associated_allocator_impl<T, ProtoAllocator,
                                 stdx::void_t<typename T::allocator_type>> {
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

  using service_key_type = void (*)();

  /**
   * create one function per Key and return its address.
   *
   * As it is static, the address is constant and can be used as key.
   */
  template <class Key>
  static service_key_type service_key() {
    return reinterpret_cast<service_key_type>(&service_key<Key>);
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

template <class T, class = stdx::void_t<>>
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
        stdx::conjunction<
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
        stdx::void_t<decltype(__x->context()), void()>>;

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

// 13.11 [async.uses.executor] - not implemented

// 13.13 [async.assoc.exec.get] - not implemented

// 13.14 [async.exec.binder] - not implemented

// 13.15 [async.bind.executor] - not implemented

// 13.16 [async.exec.work.guard] - not implemented

// 13.17 [async.make.work.guard] - not implemented

// 13.18 [async.system.exec]

// 13.19 [async.system.context]

// 13.18.1 [async.system.exec.ops]

// 13.20 [async.bad.exec] - not implemented

// 13.21 [async.executor] - not implemented

// 13.22 [async.dispatch] - not implemented

// 13.25 [async.strand] - not implemented

// 13.26 [async.use.future] - not implemented

// 13.27 [async.packaged.task.spec] - not implemented

}  // namespace net

#endif
