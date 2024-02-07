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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_storage_impl.h"

void Gcs_xcom_statistics_storage_impl::add_sucessful_paxos_round() {
  m_stats_manager_interface->set_count_var_value(kSucessfulProposalRounds);
}

void Gcs_xcom_statistics_storage_impl::add_empty_proposal_round() {
  m_stats_manager_interface->set_count_var_value(kEmptyProposalRounds);
}

void Gcs_xcom_statistics_storage_impl::add_bytes_sent(uint64_t bytes_sent) {
  m_stats_manager_interface->set_sum_var_value(kBytesSent, bytes_sent);
}

void Gcs_xcom_statistics_storage_impl::add_proposal_time(
    unsigned long long proposal_time) {
  m_stats_manager_interface->set_sum_timestamp_var_value(
      kCumulativeProposalTime, proposal_time);
}

void Gcs_xcom_statistics_storage_impl::add_three_phase_paxos() {
  m_stats_manager_interface->set_count_var_value(kFullProposalCount);
}

void Gcs_xcom_statistics_storage_impl::add_message() {
  m_stats_manager_interface->set_count_var_value(kMessagesSent);
}

void Gcs_xcom_statistics_storage_impl::add_bytes_received(
    uint64_t bytes_received) {
  m_stats_manager_interface->set_sum_var_value(kMessageBytesReceived,
                                               bytes_received);
}

void Gcs_xcom_statistics_storage_impl::set_last_proposal_time(
    unsigned long long proposal_time) {
  m_stats_manager_interface->set_timestamp_var_value(kLastProposalRoundTime,
                                                     proposal_time);
}
