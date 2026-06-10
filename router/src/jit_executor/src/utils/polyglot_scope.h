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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_SCOPE_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_SCOPE_H_

#include "utils/polyglot_api_clean.h"

namespace shcore {
namespace polyglot {

/**
 * Utility class to create a polyglot handle scope which is wiped out as soon as
 * the instance goes out of scope in C++, this is used to avoid polluting the
 * main scope with references preventing garbage collection from cleaning no
 * longer needed objects.
 */
class Polyglot_scope final {
 public:
  Polyglot_scope() = delete;
  explicit Polyglot_scope(poly_thread thread);

  Polyglot_scope(const Polyglot_scope &) = delete;
  Polyglot_scope &operator=(const Polyglot_scope &) = delete;

  Polyglot_scope(Polyglot_scope &&) = delete;
  Polyglot_scope &operator=(Polyglot_scope &&) = delete;

  void close();

  ~Polyglot_scope() { close(); }

 private:
  poly_thread m_thread;
  bool m_open = false;
};
}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_SCOPE_H_