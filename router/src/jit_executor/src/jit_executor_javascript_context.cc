/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "jit_executor_javascript_context.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "jit_executor_common_context.h"
#include "jit_executor_javascript.h"
#include "mysqlrouter/jit_executor_common.h"
#include "mysqlrouter/polyglot_file_system.h"
#include "utils/polyglot_utils.h"

namespace jit_executor {

JavaScriptContext::JavaScriptContext(size_t id, CommonContext *common_context,
                                     const std::string &debug_port) {
  m_language = std::make_shared<JavaScript>(common_context, debug_port);
  m_language_started = m_language->start(id, common_context->file_system(),
                                         common_context->globals());
}

std::string JavaScriptContext::execute(
    const std::string &module, const std::string &object,
    const std::string &function, const std::vector<Value> &parameters,
    int timeout, ResultType result_type,
    const GlobalCallbacks &global_callbacks) {
  std::string code = "import('" + module + "').then((m) => m." + object + "." +
                     function + "(" +
                     m_language->get_parameter_string(parameters) +
                     ")).catch(error=>synch_error(error))";

  return m_language->execute(code, timeout, result_type, global_callbacks);
}

bool JavaScriptContext::wait_for_idle() { return m_language->wait_for_idle(); }
bool JavaScriptContext::is_idle() { return m_language->is_idle(); }

size_t JavaScriptContext::id() { return m_language->id(); }

bool JavaScriptContext::started() const { return m_language_started; }

}  // namespace jit_executor