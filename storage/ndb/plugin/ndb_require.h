/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA*/

#ifndef NDB_REQUIRE_H
#define NDB_REQUIRE_H

#include <cstdlib>
#include "include/my_compiler.h"

namespace ndbcluster {

/**
 * @brief ndbrequire() aborts the process, when the condition passed evaluates
 * to false. Unlike assert(), it fails in both debug and release libraries.
 * "ndbrequire" is contained inside the "ndbcluster" namespace and is supposed
 * to be used only within ndbcluster code.
 *
 * @parma expr the condition to be evaluated.
 */
static inline void ndbrequire(bool expr) {
  if (unlikely(!expr)) {
    std::abort();
  }
}
}  // namespace ndbcluster

#endif
