/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>
#include <memory>
#include <unordered_map>
#include <utility>

#include "map_helpers.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"  // IWYU pragma: keep
#include "prealloced_array.h"
#include "sql/rpl_gtid.h"
#include "sql/thr_malloc.h"

#ifndef MYSQL_SERVER
#include "client/mysqlbinlog.h"  // IWYU pragma: keep
#endif

PSI_memory_key key_memory_tsid_map_Node;

Tsid_map::Tsid_map(Checkable_rwlock *_tsid_lock) : tsid_lock(_tsid_lock) {
  DBUG_TRACE;
  _sidno_to_tsid.reserve(8);
}

Tsid_map::~Tsid_map() { DBUG_TRACE; }

enum_return_status Tsid_map::clear() {
  DBUG_TRACE;
  _tsid_to_sidno.clear();
  _sidno_to_tsid.clear();
  _sorted.clear();
  RETURN_OK;
}

rpl_sidno Tsid_map::add_tsid(const Tsid &tsid) {
  DBUG_TRACE;
#ifndef NDEBUG
  std::string tsid_str = tsid.to_string();
  DBUG_PRINT("info", ("TSID=%s", tsid_str.c_str()));
#endif
  if (tsid_lock) tsid_lock->assert_some_lock();
  auto it = _tsid_to_sidno.find(tsid);
  if (it != _tsid_to_sidno.end()) {
    DBUG_PRINT("info", ("existed as sidno=%d", it->second));
    return it->second;
  }

  bool is_wrlock = false;
  if (tsid_lock) {
    is_wrlock = tsid_lock->is_wrlock();
    if (!is_wrlock) {
      tsid_lock->unlock();
      tsid_lock->wrlock();
    }
  }
  DBUG_PRINT("info", ("is_wrlock=%d tsid_lock=%p", is_wrlock, tsid_lock));
  rpl_sidno sidno;
  it = _tsid_to_sidno.find(tsid);
  if (it != _tsid_to_sidno.end())
    sidno = it->second;
  else {
    sidno = get_max_sidno() + 1;
    if (add_node(sidno, tsid) != RETURN_STATUS_OK) sidno = -1;
  }

  if (tsid_lock) {
    if (!is_wrlock) {
      tsid_lock->unlock();
      tsid_lock->rdlock();
    }
  }
  return sidno;
}

enum_return_status Tsid_map::add_node(rpl_sidno sidno, const Tsid &tsid) {
  DBUG_TRACE;
  if (tsid_lock) tsid_lock->assert_some_wrlock();
  bool tsid_to_sidno_inserted = false;
  bool sidno_to_tsid_inserted = false;
  [[maybe_unused]] bool sorted_inserted = false;
  try {
    auto insert_result = _tsid_to_sidno.insert(std::make_pair(tsid, sidno));
    if (insert_result.second) {
      auto tsid_ref = std::cref(insert_result.first->first);
      tsid_to_sidno_inserted = true;
      _sidno_to_tsid.push_back(tsid_ref);
      sidno_to_tsid_inserted = true;
      if (_sorted.insert(std::make_pair(tsid, sidno)).second) {
        sorted_inserted = true;
#ifdef MYSQL_SERVER
        // If this is the global_tsid_map, we take the opportunity to
        // resize all arrays in gtid_state while holding the wrlock.
        if (this != global_tsid_map ||
            gtid_state->ensure_sidno() == RETURN_STATUS_OK)
#endif
        {
          RETURN_OK;
        }
      }
    }
  } catch (std::bad_alloc &) {
    // error is reported below
  }

  if (tsid_to_sidno_inserted) {
    _tsid_to_sidno.erase(tsid);
  }
  if (sidno_to_tsid_inserted) {
    _sidno_to_tsid.pop_back();
  }
  if (sorted_inserted) {
    _sorted.erase(tsid);
  }

  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}

enum_return_status Tsid_map::copy(Tsid_map *dest) {
  DBUG_TRACE;
  enum_return_status return_status = RETURN_STATUS_OK;

  rpl_sidno max_sidno = get_max_sidno();
  for (rpl_sidno sidno = 1;
       sidno <= max_sidno && return_status == RETURN_STATUS_OK; sidno++) {
    Tsid tsid(sidno_to_tsid(sidno));
    return_status = dest->add_node(sidno, tsid);
  }

  return return_status;
}
