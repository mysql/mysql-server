/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef BINLOG_ATOMIC_BCG_TICKET_GUARD_H
#define BINLOG_ATOMIC_BCG_TICKET_GUARD_H

#include <functional>
#include <limits>
#include <memory>

#include "sql/binlog/group_commit/atomic_bgc_ticket.h"
#include "sql/binlog/group_commit/bgc_ticket.h"
#include "sql/memory/aligned_atomic.h"

namespace binlog {

class BgcTicket;
class AtomicBgcTicket;

class AtomicBgcTicketGuard;

/// @brief RAII opaque for AtomicBgcTicket (set in use/set used synchronization
/// operations in constructor/destructor)
class AtomicBgcTicketGuard {
 public:
  virtual ~AtomicBgcTicketGuard();

  // Remove copy-move semantics
  AtomicBgcTicketGuard(AtomicBgcTicketGuard const &) = delete;
  AtomicBgcTicketGuard &operator=(AtomicBgcTicketGuard const &) = delete;

  /// @brief "Next" value accessor. Before "release" operation, ticket value is
  /// set to the returned "next" ticket.
  /// @returns BgcTicket with next ticket value
  BgcTicket get_next() const;
  /// @brief "Previous" value accessor. During the "acquire" operation, ticket
  /// value is set to the returned "previous" ticket.
  /// @returns BgcTicket with previous ticket value
  BgcTicket get_prev() const;

  /// @brief Function used to manually manipulate the "next" ticket value. The
  /// 'set_next' function will determine the value of the ticket after the
  /// "release" operation. It can be the same value, but it might be the value
  /// incremented by one, depending on the current usage.
  /// @param next Next value for the AtomicBgcTicket referenced by this instance
  /// of the AtomicBgcTicketGuard
  void set_next(const BgcTicket &next);

  friend class AtomicBgcTicket;

 protected:
  /// @brief Constructor
  /// @param[in] bgcTicket Reference to atomic ticket instance
  /// @param[in] next_value Before "release" operation, ticket value is set to
  /// the next_value.
  AtomicBgcTicketGuard(AtomicBgcTicket &bgcTicket, const BgcTicket &next_value);
  /// @brief Constructor
  /// @param[in] bgcTicket Reference to atomic ticket instance
  /// @param[in] inc_next_before_release Before "release" operation, ticket
  /// value is set to the value+1 in case inc_next_before_release is equal to
  /// true
  AtomicBgcTicketGuard(AtomicBgcTicket &bgcTicket,
                       bool inc_next_before_release = false);

 protected:
  std::reference_wrapper<AtomicBgcTicket>
      m_ref;  ///< For threads to synchronize properly, AtomicBgcTicketGuard
              ///< instances must operate on the same instance of
              ///< AtomicBgcTicket. This is a reference to atomic Bgc ticket
              ///< instance, used by different instances of the BgcTicketGuard
              ///< class (e.g. front/back ticket in BgcTicketManager class)
  BgcTicket m_next_value;  ///< value to be set during the "release" op
  BgcTicket m_prev_value;  ///< previous ticket value, obtained during the
                           ///< "acquire" op
};

}  // namespace binlog

#endif  // BINLOG_ATOMIC_BCG_TICKET_GUARD_H
