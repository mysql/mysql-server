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

#ifndef MYSQL_HARNESS_TLS_RELOAD_COMPONENT_INCLUDED
#define MYSQL_HARNESS_TLS_RELOAD_COMPONENT_INCLUDED

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "harness_export.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

/** Coordinates asynchronous reloads of TLS material owned by Router plugins. */
class HARNESS_EXPORT TlsReloadComponent {
 public:
  using Result = stdx::expected<void, std::string>;

  using Callback = std::function<Result()>;
  using RegistrationId = std::size_t;
  static constexpr RegistrationId kInvalidRegistrationId{0};

  static TlsReloadComponent &get_instance();

  TlsReloadComponent(const TlsReloadComponent &) = delete;
  TlsReloadComponent &operator=(const TlsReloadComponent &) = delete;

  /** Starts the worker. */
  void init();
  /** Stops and joins the worker before plugin TLS state is destroyed. */
  void stop();

  RegistrationId register_callback(std::string owner, Callback callback);
  void unregister_callback(RegistrationId id);

  /** Queues a reload without performing TLS I/O in the caller's context. */
  void request_reload();

 private:
  TlsReloadComponent();
  ~TlsReloadComponent();

  class Impl;
  std::mutex impl_mtx_;
  std::shared_ptr<Impl> impl_;
  RegistrationId next_id_{kInvalidRegistrationId + 1};
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_TLS_RELOAD_COMPONENT_INCLUDED
