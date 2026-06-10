/*
 * Copyright (c) 2014, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_SCOPED_CALLBACK_H_
#define ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_SCOPED_CALLBACK_H_

#include <functional>
#include <utility>

#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT ScopedCallback {
 public:
  explicit ScopedCallback(std::function<void()> c) noexcept
      : callback_{std::move(c)} {}

  ScopedCallback() = default;
  ScopedCallback(const ScopedCallback &) = delete;
  ScopedCallback &operator=(const ScopedCallback &) = delete;

  ScopedCallback(ScopedCallback &&o) noexcept { *this = std::move(o); }
  ScopedCallback &operator=(ScopedCallback &&o) noexcept {
    if (this != &o) std::swap(callback_, o.callback_);
    return *this;
  }

  ~ScopedCallback() noexcept;

  void call() {
    if (!callback_) return;
    std::exchange(callback_, nullptr)();
  }

  void cancel() { callback_ = nullptr; }

 private:
  std::function<void()> callback_;
};

}  // namespace mysql_harness

#endif  // ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_SCOPED_CALLBACK_H_
