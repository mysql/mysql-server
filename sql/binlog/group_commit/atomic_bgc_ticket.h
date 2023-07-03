/* Copyright (c) 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINLOG_ATOMIC_BCG_TICKET_H
#define BINLOG_ATOMIC_BCG_TICKET_H

#include <functional>
#include <limits>
#include <memory>

#include "sql/binlog/group_commit/atomic_bgc_ticket_guard.h"
#include "sql/binlog/group_commit/bgc_ticket.h"
#include "sql/memory/aligned_atomic.h"

namespace binlog {

class AtomicBgcTicketGuard;
class BgcTicket;

/// @brief Implements atomic ops on BgcTicket object
/// @details Impl based on Aligned_atomic
/// @see Bgc_ticket_manager
class AtomicBgcTicket {
 public:
  virtual ~AtomicBgcTicket() = default;

  // Remove copy-move semantics
  AtomicBgcTicket(AtomicBgcTicket const &) = delete;
  AtomicBgcTicket(AtomicBgcTicket &&) = delete;
  AtomicBgcTicket &operator=(AtomicBgcTicket const &) = delete;
  AtomicBgcTicket &operator=(AtomicBgcTicket &&) = delete;

  /// @brief Copying ctor
  /// @param[in] src Pattern to copy from
  /// @details AtomicBgcTicket is created based on src BgcTicket object
  AtomicBgcTicket(const BgcTicket &src);

  /// @brief Copying ctor
  /// @param[in] src Pattern to copy from
  /// @details AtomicBgcTicket is created based on src ValueType object
  explicit AtomicBgcTicket(const BgcTicket::ValueType &src);

  friend class AtomicBgcTicketGuard;
  friend class BgcTicket;

  /// @brief Creates the "lock" that is held for the returned
  /// AtomicBgcTicketGuard object lifetime
  /// @param[in] inc_next_before_release Before "release" operation, ticket
  /// value is set to the value+1 in case inc_next_before_acquire is equal false
  /// and inc_next_before_release is equal to true
  AtomicBgcTicketGuard scoped_lock(bool inc_next_before_release = false);
  /// @brief Creates the "lock" that is held for the returned
  /// AtomicBgcTicketGuard object lifetime
  AtomicBgcTicketGuard scoped_lock(const BgcTicket &next_value);

  /// @brief Stream operator impl for AtomicBgcTicket class
  /// @param[in] os Reference to stream obj
  /// @param[in] arg Constant reference to AtomicBgcTicket object
  /// @returns Reference to changed stream obj
  friend std::ostream &operator<<(std::ostream &os, const AtomicBgcTicket &arg);

  /// @brief Ticket mutator, atomic store op
  /// @param[in] value Sets atomically m_ticket internal variable to "value"
  void store(const BgcTicket &value);

  /// @brief Ticket accessor, atomic load op
  /// @returns BGC Ticket obtained during the atomic load op
  BgcTicket load() const;

 protected:
  /// @brief Sets ticket synchronization value to "in use". Only one thread is
  /// allowed to get into the critical section that starts with the "set_in_use"
  /// op and ends with the "set_used"
  /// @param[in] inc_next_before_acquire During the "acquire" operation, ticket
  /// value is set to the value+1 in case inc_next_before_acquire is equal to
  /// true.
  /// @param[in] inc_next_before_release Before "release" operation, ticket
  /// value is set to the value+1 in case inc_next_before_acquire is equal
  /// false.
  /// @returns previous ticket value (obtained before acquire op) and next
  /// ticket value (set before release op)
  std::pair<BgcTicket, BgcTicket> set_in_use(
      bool inc_next_before_acquire = false,
      bool inc_next_before_release = false);

  /// @brief Sets ticket synchronization value to "used/free". Only one thread
  /// is allowed to get into the critical section that starts with the
  /// "set_in_use" op and ends with the "set_used"
  /// @param[in] next_value Next ticket value set during the "release" op.
  void set_used(const BgcTicket &next_value);
  memory::Aligned_atomic<BgcTicket::ValueType>
      m_ticket;  ///< internal ticket representation
};

}  // namespace binlog

#endif  // BINLOG_ATOMIC_BCG_TICKET_H
