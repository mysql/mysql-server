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

#ifndef STATISTICS_STORAGE_INTERFACE_DEFAULT_IMPL_H
#define STATISTICS_STORAGE_INTERFACE_DEFAULT_IMPL_H

#include "xcom/statistics/include/statistics_storage_interface.h"

// Default void implementation for XCom_Statistics_Storage_Interface
class Xcom_statistics_storage_interface_default_impl
    : public Xcom_statistics_storage_interface {
 public:
  /* purecov: begin inspected */
  explicit Xcom_statistics_storage_interface_default_impl() = default;
  ~Xcom_statistics_storage_interface_default_impl() override = default;

  void add_sucessful_paxos_round() override {}
  void add_empty_proposal_round() override {}
  void add_bytes_sent([[maybe_unused]] uint64_t bytes_sent) override {}
  void add_proposal_time([
      [maybe_unused]] unsigned long long proposal_time) override {}
  void add_three_phase_paxos() override {}
  void add_message() override {}
  void add_bytes_received([[maybe_unused]] uint64_t bytes_received) override {}
  void set_last_proposal_time([
      [maybe_unused]] unsigned long long proposal_time) override {}
  /* purecov: end */
};

#endif  // STATISTICS_STORAGE_INTERFACE_DEFAULT_IMPL_H