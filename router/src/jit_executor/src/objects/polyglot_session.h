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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_OBJECTS_POLYGLOT_SESSION_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_OBJECTS_POLYGLOT_SESSION_H_

#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_object_bridge.h"
#include "objects/polyglot_result.h"

namespace shcore {
namespace polyglot {

class Session : public Object_bridge {
 public:
  explicit Session(const std::shared_ptr<jit_executor::db::ISession> &session);
  ~Session() override = default;

  std::string class_name() const override { return "Session"; }

  //! Calls the named method with the given args
  Value call(const std::string &name, const Argument_list &args) override;

  std::shared_ptr<PolyResult> run_sql(const Argument_list &args);

  void reset();

 private:
  static std::vector<std::string> m_methods;
  std::shared_ptr<jit_executor::db::ISession> m_session;

  std::vector<std::string> *methods() const override { return &m_methods; }
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_OBJECTS_POLYGLOT_SESSION_H_
