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

#ifndef ROUTING_SRC_PROCESSORS_BASE_PROCESSOR_DIAGNOSTICS_H_
#define ROUTING_SRC_PROCESSORS_BASE_PROCESSOR_DIAGNOSTICS_H_

#include <memory>
#include <string>
#include <system_error>
#include <vector>

class BasicProcessor;

namespace routing::processor_diagnostics {

std::string processor_state(const BasicProcessor *processor);

std::string processor_failed_message(
    std::error_code ec,
    const std::vector<std::unique_ptr<BasicProcessor>> &stack);

void log_processor_failed(
    std::error_code ec,
    const std::vector<std::unique_ptr<BasicProcessor>> &stack);

}  // namespace routing::processor_diagnostics

#endif  // ROUTING_SRC_PROCESSORS_BASE_PROCESSOR_DIAGNOSTICS_H_
