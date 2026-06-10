/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_H_

#include <functional>
#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_callbacks.h"
#include "mysqlrouter/jit_executor_common.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"

namespace jit_executor {

class IContext {
 public:
  virtual ~IContext() = default;
  virtual std::string execute(const std::string &module,
                              const std::string &object,
                              const std::string &function,
                              const std::vector<shcore::Value> &parameters,
                              int timeout, ResultType result_type,
                              const GlobalCallbacks &global_callbacks) = 0;

  virtual bool wait_for_idle() = 0;
  virtual bool is_idle() = 0;
  virtual size_t id() = 0;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_H_
