/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CALLBACKS_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CALLBACKS_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "mysqlrouter/jit_executor_db_interface.h"

namespace jit_executor {

struct GlobalCallbacks {
  std::function<std::shared_ptr<db::ISession>(bool read_only)> get_session;
  std::function<std::optional<std::string>()> get_current_mrs_user_id;
  std::function<std::string(const std::string &)> get_content_set_path;
  std::function<void()> interrupt;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CALLBACKS_H_