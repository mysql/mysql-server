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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
   */

#include <ctime>

#include "gcs_base_test.h"

#include "gcs_xcom_statistics_manager.h"

namespace gcs_xcom_statistics_unittest {
class XcomStatisticsManagerTest : public GcsBaseTest {
 protected:
  XcomStatisticsManagerTest() = default;

  void SetUp() override {
    xcom_stats_manager_if = new Gcs_xcom_statistics_manager_interface_impl();
  }

  void TearDown() override { delete xcom_stats_manager_if; }

  Gcs_xcom_statistics_manager_interface *xcom_stats_manager_if;
};

TEST_F(XcomStatisticsManagerTest, SumVarValuesSetAndGetTest) {
  xcom_stats_manager_if->set_sum_var_value(kBytesSent, 365);
  xcom_stats_manager_if->set_sum_var_value(kMessageBytesReceived, 32);
  xcom_stats_manager_if->set_sum_var_value(kMessageBytesReceived, 32);
  xcom_stats_manager_if->set_sum_var_value(kBytesSent, 28);

  ASSERT_EQ(xcom_stats_manager_if->get_sum_var_value(kBytesSent), (365 + 28));
  ASSERT_EQ(xcom_stats_manager_if->get_sum_var_value(kMessageBytesReceived),
            (32 + 32));
}

TEST_F(XcomStatisticsManagerTest, CountVarValuesSetAndGetTest) {
  uint cyclekSucessfulProposalRounds = 4;
  uint cyclekEmptyProposalRounds = 10;
  uint cyclekFullProposalCount = 17;
  uint cyclekMessagesSent = 1;

  for (uint i = 0; i < cyclekSucessfulProposalRounds; i++)
    xcom_stats_manager_if->set_count_var_value(kSucessfulProposalRounds);

  for (uint i = 0; i < cyclekEmptyProposalRounds; i++)
    xcom_stats_manager_if->set_count_var_value(kEmptyProposalRounds);

  for (uint i = 0; i < cyclekFullProposalCount; i++)
    xcom_stats_manager_if->set_count_var_value(kFullProposalCount);

  for (uint i = 0; i < cyclekMessagesSent; i++)
    xcom_stats_manager_if->set_count_var_value(kMessagesSent);

  ASSERT_EQ(
      xcom_stats_manager_if->get_count_var_value(kSucessfulProposalRounds),
      cyclekSucessfulProposalRounds);
  ASSERT_EQ(xcom_stats_manager_if->get_count_var_value(kEmptyProposalRounds),
            cyclekEmptyProposalRounds);
  ASSERT_EQ(xcom_stats_manager_if->get_count_var_value(kFullProposalCount),
            cyclekFullProposalCount);
  ASSERT_EQ(xcom_stats_manager_if->get_count_var_value(kMessagesSent),
            cyclekMessagesSent);
}

TEST_F(XcomStatisticsManagerTest, TimestampVarValuesSetAndGetNotSumTest) {
  xcom_stats_manager_if->set_timestamp_var_value(kLastProposalRoundTime, 365);
  xcom_stats_manager_if->set_timestamp_var_value(kLastProposalRoundTime, 32);

  ASSERT_EQ(
      xcom_stats_manager_if->get_timestamp_var_value(kLastProposalRoundTime),
      32);
}

TEST_F(XcomStatisticsManagerTest, TimestampVarValuesSetAndGetSumTest) {
  xcom_stats_manager_if->set_sum_timestamp_var_value(kCumulativeProposalTime,
                                                     365);
  xcom_stats_manager_if->set_sum_timestamp_var_value(kCumulativeProposalTime,
                                                     32);

  ASSERT_EQ(
      xcom_stats_manager_if->get_timestamp_var_value(kCumulativeProposalTime),
      (365 + 32));
}

TEST_F(XcomStatisticsManagerTest, AddAndGetOneSuspiciousTest) {
  xcom_stats_manager_if->add_suspicious_for_a_node("node1");

  auto all_suspicious = xcom_stats_manager_if->get_all_suspicious();

  ASSERT_EQ(all_suspicious.size(), 1);
  ASSERT_EQ(all_suspicious.at(0).m_node_address, "node1");
  ASSERT_EQ(all_suspicious.at(0).m_node_suspicious_count, 1);
}

TEST_F(XcomStatisticsManagerTest, AddAndGetMultipleSuspiciousTest) {
  xcom_stats_manager_if->add_suspicious_for_a_node("node1");
  xcom_stats_manager_if->add_suspicious_for_a_node("node1");
  xcom_stats_manager_if->add_suspicious_for_a_node("node1");
  xcom_stats_manager_if->add_suspicious_for_a_node("node2");
  xcom_stats_manager_if->add_suspicious_for_a_node("node3");
  xcom_stats_manager_if->add_suspicious_for_a_node("node3");

  auto all_suspicious = xcom_stats_manager_if->get_all_suspicious();

  ASSERT_EQ(all_suspicious.size(), 3);

  std::string node_to_find{"node1"};

  auto find_node = [&](Gcs_node_suspicious &elem) {
    return elem.m_node_address.compare(node_to_find) == 0;
  };

  auto result1 =
      std::find_if(begin(all_suspicious), end(all_suspicious), find_node);
  ASSERT_TRUE(result1 != all_suspicious.end());
  ASSERT_EQ((*result1).m_node_suspicious_count, 3);

  node_to_find.assign("node2");
  auto result2 =
      std::find_if(begin(all_suspicious), end(all_suspicious), find_node);
  ASSERT_TRUE(result2 != all_suspicious.end());
  ASSERT_EQ((*result2).m_node_suspicious_count, 1);

  node_to_find.assign("node3");
  auto result3 =
      std::find_if(begin(all_suspicious), end(all_suspicious), find_node);
  ASSERT_TRUE(result3 != all_suspicious.end());
  ASSERT_EQ((*result3).m_node_suspicious_count, 2);
}

}  // namespace gcs_xcom_statistics_unittest