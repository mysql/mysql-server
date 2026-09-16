/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once

#include "log0handler_interface.h"
#include "univ.i"     //to_int
#include "ut0core.h"  //ut::Location
#include "ut0log.h"   //ib::fatal
namespace ib::redo {
inline void must_succeed(Status res, const ut::Location &loc) {
  if (res != Status::SUCCESS) {
    ib::fatal(loc) << "Redo Log Handler has failed with " << to_int(res);
  }
}
inline void must_persist_all(
    const ut::Location &loc,
    Handler_interface::Durability desired_guarantee =
        Handler_interface::Durability::FULLY_PERSISTED) {
  must_succeed(handler->persist_smaller_than(
                   handler->peek_first_unassigned_lsn(), desired_guarantee),
               loc);
}
}  // namespace ib::redo

#ifdef UNIV_DEBUG
inline void DBUG_INJECT_CRASH_WITH_LOG_FLUSH(const char *name) {
  DBUG_EXECUTE_IF(name, {
    ib::redo::must_persist_all(UT_LOCATION_HERE);
    DBUG_SUICIDE();
  });
}

inline void DBUG_INJECT_CRASH_WITH_LOG_FLUSH(const char *prefix,
                                             unsigned count) {
  char buf[64];
  snprintf(buf, sizeof buf, "%s_%u", prefix, count);
  DBUG_INJECT_CRASH_WITH_LOG_FLUSH(buf);
}
#else
#define DBUG_INJECT_CRASH_WITH_LOG_FLUSH(...)
#endif
