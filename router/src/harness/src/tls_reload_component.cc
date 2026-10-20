/*
Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_reload_component.h"

#include <algorithm>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/wait_variable.h"

IMPORT_LOG_FUNCTIONS()

namespace mysql_harness {

class TlsReloadComponent::Impl {
 public:
  struct Registration {
    Registration(RegistrationId id, std::string owner, Callback callback)
        : id{id}, owner{std::move(owner)}, callback{std::move(callback)} {}

    RegistrationId id;
    std::string owner;
    Callback callback;
    bool registered{true};
    utility::WaitableVariable<std::size_t> active_calls{0};
  };

  enum class State { kIdle, kRequested, kActive, kShutdown };

  bool is_shutdown() { return state.is(State::kShutdown); }

  void worker_main() {
    while (true) {
      if (state.wait({State::kRequested, State::kShutdown}) ==
          State::kShutdown) {
        return;
      }
      if (!state.exchange(/* expected_state */ State::kRequested,
                          /* next_state */ State::kActive)) {
        continue;
      }

      std::vector<std::shared_ptr<Registration>> callbacks;
      {
        std::lock_guard<std::mutex> lock{registrations_mtx};
        callbacks = registrations;
      }

      for (const auto &registration : callbacks) {
        {
          std::lock_guard<std::mutex> lock{registrations_mtx};
          if (!registration->registered) continue;
          registration->active_calls.change(
              [](auto &active_calls) { ++active_calls; });
        }

        try {
          const auto result = registration->callback();
          if (result) {
            log_info("TLS reload succeeded for '%s'",
                     registration->owner.c_str());
          } else {
            log_error("TLS reload failed for '%s': %s",
                      registration->owner.c_str(), result.error().c_str());
          }
        } catch (const std::exception &e) {
          log_error("TLS reload failed for '%s': %s",
                    registration->owner.c_str(), e.what());
        } catch (...) {
          log_error("TLS reload failed for '%s' with an unknown error",
                    registration->owner.c_str());
        }

        registration->active_calls.change(
            [](auto &active_calls) { --active_calls; });
      }

      // A request received while callbacks ran changed ACTIVE to REQUESTED.
      // Preserve it so a completed file replacement cannot be missed.
      state.exchange(/* expected_state */ State::kActive,
                     /* next_state */ State::kIdle);
    }
  }

  std::mutex registrations_mtx;
  std::thread worker;
  std::vector<std::shared_ptr<Registration>> registrations;
  utility::WaitableVariable<State> state{State::kIdle};
};

TlsReloadComponent::TlsReloadComponent() : impl_{std::make_shared<Impl>()} {}
TlsReloadComponent::~TlsReloadComponent() { stop(); }

TlsReloadComponent &TlsReloadComponent::get_instance() {
  static TlsReloadComponent instance;
  return instance;
}

void TlsReloadComponent::init() {
  std::shared_ptr<Impl> impl;
  {
    std::lock_guard<std::mutex> lock{impl_mtx_};
    if (impl_->is_shutdown()) impl_ = std::make_shared<Impl>();
    impl = impl_;
  }

  std::lock_guard<std::mutex> lock{impl->registrations_mtx};
  if (impl->worker.joinable()) return;
  impl->worker = std::thread{&Impl::worker_main, impl.get()};
}

void TlsReloadComponent::stop() {
  std::shared_ptr<Impl> impl;
  {
    std::lock_guard<std::mutex> lock{impl_mtx_};
    impl = impl_;
  }

  impl->state.set(Impl::State::kShutdown);
  if (impl->worker.joinable()) impl->worker.join();
  std::lock_guard<std::mutex> lock{impl->registrations_mtx};
  impl->registrations.clear();
}

TlsReloadComponent::RegistrationId TlsReloadComponent::register_callback(
    std::string owner, Callback callback) {
  std::shared_ptr<Impl> impl;
  RegistrationId id;
  {
    std::lock_guard<std::mutex> lock{impl_mtx_};
    // A new registration after shutdown starts a new component lifecycle.
    // It does not revive the shutdown Impl; it creates a fresh idle one.
    if (impl_->is_shutdown()) impl_ = std::make_shared<Impl>();
    impl = impl_;
    id = next_id_++;
  }

  std::lock_guard<std::mutex> lock{impl->registrations_mtx};
  impl->registrations.emplace_back(std::make_shared<Impl::Registration>(
      id, std::move(owner), std::move(callback)));
  return id;
}

void TlsReloadComponent::unregister_callback(RegistrationId id) {
  if (id == kInvalidRegistrationId) return;
  std::shared_ptr<Impl> impl;
  {
    std::lock_guard<std::mutex> lifecycle_lock{impl_mtx_};
    impl = impl_;
  }
  std::unique_lock<std::mutex> lock{impl->registrations_mtx};
  auto &registrations = impl->registrations;
  const auto it = std::find_if(
      registrations.begin(), registrations.end(),
      [id](const auto &registration) { return registration->id == id; });
  if (it == registrations.end()) return;

  auto registration = *it;
  registration->registered = false;
  registrations.erase(it);

  // A callback cannot wait for itself to return. It has already started, and
  // marking the registration inactive still prevents every later invocation.
  if (std::this_thread::get_id() == impl->worker.get_id()) return;
  lock.unlock();
  registration->active_calls.wait(0);
}

void TlsReloadComponent::request_reload() {
  std::shared_ptr<Impl> impl;
  {
    std::lock_guard<std::mutex> lock{impl_mtx_};
    impl = impl_;
  }
  // REQUESTED is intentionally retained when an active pass is in progress.
  impl->state.exchange(
      /* expected_states */ {Impl::State::kIdle, Impl::State::kActive},
      /* next_state */ Impl::State::kRequested);
}

}  // namespace mysql_harness
