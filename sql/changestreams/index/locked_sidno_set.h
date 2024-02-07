// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef CHANGESTREAMS_INDEX_LOCKED_SIDNO_SET_H_INCLUDED
#define CHANGESTREAMS_INDEX_LOCKED_SIDNO_SET_H_INCLUDED

#include <functional>
#include <set>
#include "sql/changestreams/index/sidno.h"
#include "sql/rpl_gtid.h"

class Gtid_state;

namespace cs::index {

/// @brief Set that keeps track of TSID locks taken in the current scope.
/// Provides dead-lock free mechanism for added sidnos, as sidnos are ordered
/// in the set insuring a deterministic lock order
/// Locks are added to the set by execution of the add_lock_for_sidno.
/// Later, the user may call lock(), which will lock all recorded sidno locks.
/// Locks will be released in destructor.
/// @details Used to optimize TSID locking/unlocking scheme (e.g. in
/// assign_automatic_gtids_to_flush_group) to avoid constant locking/unlocking
/// of TSID locks during the commit flush stage
class Locked_sidno_set {
 public:
  Locked_sidno_set(Gtid_state &gtid_state);
  Locked_sidno_set(Locked_sidno_set &src) = delete;

  /// @brief Adds a given sidno to the internal set, does not acquire ownership
  /// @param sidno TSID being considered
  /// @details may be called several times for one sidno
  void add_lock_for_sidno(rpl_sidno sidno);

  /// @brief Locks recorded sidno locks, in order. In case set is already
  /// locked, function does not take any action
  void lock();

  /// @brief Destructor, releases all locks kept in the set
  ~Locked_sidno_set();

 protected:
  using Set_type = std::set<rpl_sidno>;  ///< Type of the set used
  Set_type m_sidno_set;  ///< SIDs for which lock will be acquired
  std::reference_wrapper<Gtid_state>
      m_gtid_state;  ///< Reference to the current Gtid_state object (avoids
                     ///< using the global gtid_state variable in the class)
  bool m_locked = false;  ///< Is set currently locked?
};

}  // namespace cs::index

#endif  // CHANGESTREAMS_INDEX_LOCKED_SIDNO_SET_H_INCLUDED
