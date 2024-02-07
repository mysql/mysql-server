/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef STATISTICS_STORAGE_INTERFACE_H
#define STATISTICS_STORAGE_INTERFACE_H

#include <stdint.h>

/**
 * @brief Interface class for all statistics that XCom will provide.
 *
 */
class Xcom_statistics_storage_interface {
 public:
  virtual ~Xcom_statistics_storage_interface() = default;

  /**
   * @brief Adds one successful PAXOS round
   *
   */
  virtual void add_sucessful_paxos_round() = 0;

  /**
   * @brief Adds one Noop proposal round
   *
   */
  virtual void add_empty_proposal_round() = 0;

  /**
   * @brief Adds to bytes sent to all members
   *
   * @param bytes_sent the amount of bytes sent to the network.
   */
  virtual void add_bytes_sent(uint64_t bytes_sent) = 0;

  /**
   * @brief Adds to the cumulative proposal time
   *
   * @param proposal_time Proposal time to add
   */
  virtual void add_proposal_time(unsigned long long proposal_time) = 0;

  /**
   * @brief Adds one 3-Phase PAXOS round
   *
   */
  virtual void add_three_phase_paxos() = 0;

  /**
   * @brief Adds one message sent.
   *
   */
  virtual void add_message() = 0;

  /**
   * @brief Adds to bytes received in this member
   */
  virtual void add_bytes_received(uint64_t bytes_received) = 0;

  /**
   * @brief Sets the last proposal time
   */
  virtual void set_last_proposal_time(unsigned long long proposal_time) = 0;
};

#endif  // STATISTICS_STORAGE_INTERFACE_H
