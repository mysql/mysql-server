/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_STMT_CLASSIFIER_INCLUDED
#define ROUTING_STMT_CLASSIFIER_INCLUDED

#include "mysql/harness/stdx/flags.h"

enum class StmtClassifier {
  StateChangeOnSuccess = 1 << 0,              // even if tracker doesn't say so.
  StateChangeOnError = 1 << 1,                // on error
  StateChangeOnTracker = 1 << 2,              // trust the tracker.
  NoStateChangeIgnoreTracker = 1 << 3,        // tracker is wrong.
  ForbiddenFunctionWithConnSharing = 1 << 4,  // forbidden function
  ForbiddenSetWithConnSharing = 1 << 5,       // forbidden set-tracker
  ReadOnly = 1 << 6,                          // statement is read-only
};

namespace stdx {
template <>
struct is_flags<StmtClassifier> : std::true_type {};
}  // namespace stdx

#endif
