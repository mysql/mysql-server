/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_interface.h"

Gcs_xcom_statistics::Gcs_xcom_statistics(
    Gcs_xcom_statistics_manager_interface *stats_mgr)
    : m_stats_mgr(stats_mgr) {}

Gcs_xcom_statistics::~Gcs_xcom_statistics() = default;

uint64_t Gcs_xcom_statistics::get_all_sucessful_proposal_rounds() const {
  return m_stats_mgr->get_count_var_value(kSucessfulProposalRounds);
}

uint64_t Gcs_xcom_statistics::get_all_empty_proposal_rounds() const {
  return m_stats_mgr->get_count_var_value(kEmptyProposalRounds);
}

uint64_t Gcs_xcom_statistics::get_all_bytes_sent() const {
  return m_stats_mgr->get_sum_var_value(kBytesSent);
}

void Gcs_xcom_statistics::get_suspicious_count(
    std::list<Gcs_node_suspicious> &suspicious_out) const {
  auto suspicious = m_stats_mgr->get_all_suspicious();

  suspicious_out.insert(suspicious_out.begin(),
                        std::make_move_iterator(suspicious.begin()),
                        std::make_move_iterator(suspicious.end()));
}

uint64_t Gcs_xcom_statistics::get_all_full_proposal_count() const {
  return m_stats_mgr->get_count_var_value(kFullProposalCount);
}

uint64_t Gcs_xcom_statistics::get_all_messages_sent() const {
  return m_stats_mgr->get_count_var_value(kMessagesSent);
}

uint64_t Gcs_xcom_statistics::get_all_message_bytes_received() const {
  return m_stats_mgr->get_sum_var_value(kMessageBytesReceived);
}

unsigned long long Gcs_xcom_statistics::get_cumulative_proposal_time() const {
  return m_stats_mgr->get_timestamp_var_value(kCumulativeProposalTime);
}

unsigned long long Gcs_xcom_statistics::get_last_proposal_round_time() const {
  return m_stats_mgr->get_timestamp_var_value(kLastProposalRoundTime);
}
