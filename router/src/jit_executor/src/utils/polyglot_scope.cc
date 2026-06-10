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

#include "utils/polyglot_scope.h"

#include "mysql/harness/logging/logging.h"
#include "utils/polyglot_api_clean.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {

IMPORT_LOG_FUNCTIONS()

Polyglot_scope::Polyglot_scope(poly_thread thread) : m_thread{thread} {
  throw_if_error(poly_open_handle_scope, m_thread);

  m_open = true;
}

void Polyglot_scope::close() {
  if (m_open) {
    if (const auto rc = poly_close_handle_scope(m_thread); rc != poly_ok) {
      log_error("%s", Polyglot_error(m_thread, rc).format().c_str());
    } else {
      m_open = false;
    }
  }
}

}  // namespace polyglot
}  // namespace shcore
