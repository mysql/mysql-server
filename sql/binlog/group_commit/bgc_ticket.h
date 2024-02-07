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

#ifndef BINLOG_BCG_TICKET_H
#define BINLOG_BCG_TICKET_H

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>

#include "sql/memory/aligned_atomic.h"

namespace binlog {

class AtomicBgcTicket;
class BgcTicketGuard;

/// @brief Represents the Binlog Group Commit Ticket - BGC Ticket.
/// @details Context: BGC ticket is granted to sessions with the same View,
/// which may be changed with the View Change Event (server connecting to /
/// disconnecting from group). When View Change Event is generated, it is
/// granted a separate ticket value (value+1). All sessions that should be
/// executed after View Change Event are assigned with a ticket value + 2.
/// Implementation: Ticket is composed of 1 synchronization bit, used for
/// critical section implementation and 63 bits that represent a ticket value.
/// This class contains operation common for BGC tickets, such as: value
/// extraction, changing synchronization bit between 0 and 1, ticket comparison,
/// formatting, incrementation, internal constants. After reachning the maximum
/// value for a ticket, ticket value is wrapped up to the first valid ticket
/// value.
/// @see Bgc_ticket_manager
class BgcTicket {
 public:
  using ValueType = std::uint64_t;

  /// @brief Default value of ticket, which means it is not being used.
  static constexpr BgcTicket::ValueType kTicketUnset = 0;

  /// @brief Constructor
  /// @param[in] ticket Pattern to copy from
  explicit BgcTicket(const ValueType &ticket = first_ticket_value);

  friend class AtomicBgcTicket;

  /// @brief "Less than" operator
  /// @param[in] lhs Left hand side BgcTicket argument (const ref)
  /// @param[in] rhs Right hand side BgcTicket argument (const ref)
  friend bool operator<(const BgcTicket &lhs, const BgcTicket &rhs);
  /// @brief "Greater than" operator
  /// @param[in] lhs Left hand side BgcTicket argument (const ref)
  /// @param[in] rhs Right hand side BgcTicket argument (const ref)
  /// @retval true lhs > rhs
  /// @retval false rhs <= lhs
  friend bool operator>(const BgcTicket &lhs, const BgcTicket &rhs);

  /// @brief Comparison operator (>=)
  /// @param[in] lhs Left hand side BgcTicket argument (const ref)
  /// @param[in] rhs Right hand side BgcTicket argument (const ref)
  /// @retval true lhs >= rhs
  /// @retval false !(rhs >= lhs)
  friend bool operator>=(const BgcTicket &lhs, const BgcTicket &rhs);

  /// @brief Comparison operator
  /// @details Comparison without (!) first bit, meaning that if some thread
  /// locked on lhs or rhs, tickets can still be equal
  /// @param[in] lhs Left hand side BgcTicket argument (const ref)
  /// @param[in] rhs Right hand side BgcTicket argument (const ref)
  /// @retval true lhs == rhs
  /// @retval false rhs != lhs
  friend bool operator==(const BgcTicket &lhs, const BgcTicket &rhs);

  /// @brief Comparison operator
  /// @details Comparison without (!) first bit
  /// @param[in] lhs Left hand side BgcTicket argument (const ref)
  /// @param[in] rhs Right hand side BgcTicket argument (const ref)
  /// @retval true lhs != rhs
  /// @retval false rhs == lhs
  friend bool operator!=(const BgcTicket &lhs, const BgcTicket &rhs);

  /// @brief Obtain ticket value (1st bit cleared out)
  /// @return Ticket value
  ValueType get_value() const;

  /// @brief Obtain ticket, may be in use or not (cannot be easily compared)
  /// @details Cannot be easily compared, use comparison operator from BgcTicket
  /// instead
  /// @returns Internal value of the ticket (value+sync bit)
  const ValueType &get() const { return m_ticket; }

  /// @brief sets synchronization bit to 1, only 1 thread can operate on this
  /// ticket
  void set_in_use();
  /// @brief sets synchronization bit to 0, other thread may "lock" this ticket
  /// for writing
  void set_used();
  /// @brief checks whether this ticket is "locked" for writing
  /// @retval true Ticket is in use
  /// @retval false Ticket is not in use
  bool is_in_use() const;

  /// @brief Returns incremented ticket, this remain unchanged
  /// @returns Incremented ticket
  BgcTicket next() const;

  /// @brief Increments this ticket value
  void set_next();

  /// @brief Checks whether the ticket value differs from kTicketUnset
  /// @retval true Ticket value is set
  /// @retval false Ticket value is unset
  bool is_set() const;

  /// @brief Stream operator impl for BgcTicket class
  /// @param[in] os Reference to stream obj
  /// @param[in] arg Constant reference to BgcTicket object
  /// @returns Reference to changed stream obj
  friend std::ostream &operator<<(std::ostream &os, const BgcTicket &arg);

 public:
  /// 64-bit mask with all 0s except for the most significant bit, to extract
  /// synchronization bit
  static constexpr ValueType set_bit = 1ULL << 63;
  /// 64-bit mask with all 0s except for the most significant bit, to clear out
  /// synchronization bit - get ticket value
  static constexpr ValueType clear_bit = set_bit - 1;
  /// Maximum allowed value for a ticket, after which will wrap around to
  /// first_ticket_value
  static constexpr ValueType max_ticket_value =
      std::numeric_limits<ValueType>::max() & clear_bit;
  /// Minimum allowed value for a ticket
  static constexpr ValueType first_ticket_value = 1;

 protected:
  ValueType m_ticket;  ///< 1 bit for synchronization, 63 bits - ticket value
};

}  // namespace binlog

#endif  // BINLOG_BCG_TICKET_H
