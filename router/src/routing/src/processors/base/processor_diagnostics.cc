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

#include "processors/base/processor_diagnostics.h"

#include <sstream>

#include "mysql/harness/logging/logging.h"
#include "processors/base/processor.h"

IMPORT_LOG_FUNCTIONS()

namespace routing::processor_diagnostics {

std::string processor_state(const BasicProcessor *processor) {
  const auto stage_name = processor->diagnostic_stage_name();

  if (!stage_name) return "n/a";

  return std::string(*stage_name);
}

static std::string processor_with_state(const BasicProcessor *processor) {
  if (processor == nullptr) return "<null>{state=n/a}";

  return processor->diagnostic_name() + "{state=" + processor_state(processor) +
         "}";
}

static std::string processor_stack_below_top_as_string(
    const std::vector<std::unique_ptr<BasicProcessor>> &processors) {
  if (processors.size() <= 1) return "[]";

  std::ostringstream oss;
  oss << "[";
  bool first = true;

  // Safe: std::next(rbegin) is only used when size() > 1.
  // Skip the stack top (rbegin), print the rest from nearest context down
  // as a JSON-like list.
  for (auto it = std::next(processors.rbegin()); it != processors.rend();
       ++it) {
    const auto *processor = it->get();

    if (!first) oss << ", ";
    first = false;

    oss << processor->diagnostic_name()
        << "{state=" << processor_state(processor) << "}";
  }

  oss << "]";
  return oss.str();
}

std::string processor_failed_message(
    std::error_code ec,
    const std::vector<std::unique_ptr<BasicProcessor>> &stack) {
  const auto *top_processor = stack.empty() ? nullptr : stack.back().get();
  std::ostringstream oss;

  oss << "classic::loop() processor failed: " << ec.message() << " ("
      << ec.category().name() << ":" << ec.value()
      << "), top_processor=" << processor_with_state(top_processor)
      << ", processor_stack_size=" << stack.size()
      << ", processor_stack_below_top="
      << processor_stack_below_top_as_string(stack);

  return oss.str();
}

void log_processor_failed(
    std::error_code ec,
    const std::vector<std::unique_ptr<BasicProcessor>> &stack) {
  const auto msg = processor_failed_message(ec, stack);
  log_error("%s", msg.c_str());
}

}  // namespace routing::processor_diagnostics
