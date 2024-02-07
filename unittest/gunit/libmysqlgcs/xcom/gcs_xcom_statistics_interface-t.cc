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

#include "gcs_xcom_statistics_interface.h"

namespace gcs_xcom_statistics_unittest {

class Mock_gcs_xcom_statistics_manager
    : public Gcs_xcom_statistics_manager_interface {
 public:
  MOCK_METHOD(uint64_t, get_sum_var_value,
              (Gcs_cumulative_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_sum_var_value,
              (Gcs_cumulative_statistics_enum to_set, uint64_t to_add),
              (override));

  // COUNT VARS
  MOCK_METHOD(uint64_t, get_count_var_value,
              (Gcs_counter_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_count_var_value, (Gcs_counter_statistics_enum to_set),
              (override));

  // TIMESTAMP VALUES
  MOCK_METHOD(unsigned long long, get_timestamp_var_value,
              (Gcs_time_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_timestamp_var_value,
              (Gcs_time_statistics_enum to_set, unsigned long long new_value),
              (override));
  MOCK_METHOD(void, set_sum_timestamp_var_value,
              (Gcs_time_statistics_enum to_set, unsigned long long to_add),
              (override));

  // ALL OTHER VARS
  MOCK_METHOD(std::vector<Gcs_node_suspicious>, get_all_suspicious, (),
              (const, override));
  MOCK_METHOD(void, add_suspicious_for_a_node, (std::string node_id),
              (override));
};

class XcomStatisticsTest : public GcsBaseTest {
 protected:
  XcomStatisticsTest() = default;

  void SetUp() override {
    xcom_stats_if = new Gcs_xcom_statistics(&stats_mgr_mock);
  }

  void TearDown() override { delete xcom_stats_if; }

  Gcs_xcom_statistics *xcom_stats_if;

  Mock_gcs_xcom_statistics_manager stats_mgr_mock;
};

TEST_F(XcomStatisticsTest, SucessfulProposalRoundsTest) {
  constexpr auto proposal_rounds = 234;

  EXPECT_CALL(stats_mgr_mock, get_count_var_value(kSucessfulProposalRounds))
      .Times(1)
      .WillOnce(Return(proposal_rounds));

  auto proposal_rounds_ret = xcom_stats_if->get_all_sucessful_proposal_rounds();

  ASSERT_EQ(proposal_rounds_ret, proposal_rounds);
}

TEST_F(XcomStatisticsTest, EmptyProposalRoundsTest) {
  constexpr auto empty_proposal_rounds = 546;

  EXPECT_CALL(stats_mgr_mock, get_count_var_value(kEmptyProposalRounds))
      .Times(1)
      .WillOnce(Return(empty_proposal_rounds));

  auto empty_proposal_rounds_ret =
      xcom_stats_if->get_all_empty_proposal_rounds();

  ASSERT_EQ(empty_proposal_rounds_ret, empty_proposal_rounds);
}

TEST_F(XcomStatisticsTest, AllBytesSentTest) {
  constexpr uint64_t sent_bytes = 23456;

  EXPECT_CALL(stats_mgr_mock, get_sum_var_value(kBytesSent))
      .Times(1)
      .WillOnce(Return(sent_bytes));

  auto sent_bytes_ret = xcom_stats_if->get_all_bytes_sent();

  ASSERT_EQ(sent_bytes_ret, sent_bytes);
}

TEST_F(XcomStatisticsTest, SuspiciousCountTest) {
  std::vector<Gcs_node_suspicious> suspicious_list = {
      {"node1", 27}, {"node2", 3}, {"node4", 0}};

  EXPECT_CALL(stats_mgr_mock, get_all_suspicious())
      .Times(1)
      .WillOnce(Return(suspicious_list));

  std::list<Gcs_node_suspicious> suspicious_list_ret;
  xcom_stats_if->get_suspicious_count(suspicious_list_ret);

  ASSERT_EQ(suspicious_list_ret.size(), 3);

  uint32_t node_to_find = 0;
  auto find_node_lambda = [&](Gcs_node_suspicious elem) {
    return elem.m_node_address.compare(
               suspicious_list.at(node_to_find).m_node_address) == 0;
  };

  for (; node_to_find < suspicious_list_ret.size(); node_to_find++) {
    auto result_from_find = std::find_if(
        begin(suspicious_list_ret), end(suspicious_list_ret), find_node_lambda);
    ASSERT_TRUE(result_from_find != suspicious_list_ret.end());
    ASSERT_EQ((*result_from_find).m_node_suspicious_count,
              suspicious_list.at(node_to_find).m_node_suspicious_count);
  }

  auto do_not_find_node_lambda = [](Gcs_node_suspicious elem) {
    return elem.m_node_address.compare("node6") == 0;
  };

  auto result_from_find =
      std::find_if(begin(suspicious_list_ret), end(suspicious_list_ret),
                   do_not_find_node_lambda);
  ASSERT_FALSE(result_from_find != suspicious_list_ret.end());
}

TEST_F(XcomStatisticsTest, AllFullProposalCountTest) {
  constexpr auto full_proposal_count = 339988;

  EXPECT_CALL(stats_mgr_mock, get_count_var_value(kFullProposalCount))
      .Times(1)
      .WillOnce(Return(full_proposal_count));

  auto full_proposal_count_ret = xcom_stats_if->get_all_full_proposal_count();

  ASSERT_EQ(full_proposal_count, full_proposal_count_ret);
}

TEST_F(XcomStatisticsTest, AllMessagesSentTest) {
  constexpr auto message_count = 321456;

  EXPECT_CALL(stats_mgr_mock, get_count_var_value(kMessagesSent))
      .Times(1)
      .WillOnce(Return(message_count));

  auto message_count_ret = xcom_stats_if->get_all_messages_sent();

  ASSERT_EQ(message_count, message_count_ret);
}

TEST_F(XcomStatisticsTest, AllMessageBytesReceivedTest) {
  constexpr uint64_t received_bytes = 23456;

  EXPECT_CALL(stats_mgr_mock, get_sum_var_value(kMessageBytesReceived))
      .Times(1)
      .WillOnce(Return(received_bytes));

  auto received_bytes_ret = xcom_stats_if->get_all_message_bytes_received();

  ASSERT_EQ(received_bytes, received_bytes_ret);
}

TEST_F(XcomStatisticsTest, CumulativeProposalTimeTest) {
  constexpr long long cumulative_time = 22334455;

  EXPECT_CALL(stats_mgr_mock, get_timestamp_var_value(kCumulativeProposalTime))
      .Times(1)
      .WillOnce(Return(cumulative_time));

  auto cumulative_time_ret = xcom_stats_if->get_cumulative_proposal_time();

  ASSERT_EQ(cumulative_time_ret, cumulative_time);
}

TEST_F(XcomStatisticsTest, LastProposalRoundTimeTest) {
  constexpr long long last_proposal_time = 12345566;

  EXPECT_CALL(stats_mgr_mock, get_timestamp_var_value(kLastProposalRoundTime))
      .Times(1)
      .WillOnce(Return(last_proposal_time));

  auto last_proposal_time_ret = xcom_stats_if->get_last_proposal_round_time();

  ASSERT_EQ(last_proposal_time_ret, last_proposal_time);
}

}  // namespace gcs_xcom_statistics_unittest
