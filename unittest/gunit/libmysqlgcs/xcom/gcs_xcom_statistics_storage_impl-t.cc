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

#include "gcs_xcom_statistics_storage_impl.h"
#include "xcom/statistics/include/statistics_storage_interface.h"

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

namespace gcs_xcom_statistics_storage_impl_unittest {
class XcomStatisticsStorageImplTest : public GcsBaseTest {
 protected:
  XcomStatisticsStorageImplTest() = default;

  void SetUp() override {
    xcom_stats_storage_if =
        new Gcs_xcom_statistics_storage_impl(&stats_mgr_mock);
  }

  void TearDown() override { delete xcom_stats_storage_if; }

  Xcom_statistics_storage_interface *xcom_stats_storage_if;

  Mock_gcs_xcom_statistics_manager stats_mgr_mock;
};

TEST_F(XcomStatisticsStorageImplTest, AddSucessfulPaxosRoundTest) {
  EXPECT_CALL(stats_mgr_mock, set_count_var_value(kSucessfulProposalRounds))
      .Times(1);

  xcom_stats_storage_if->add_sucessful_paxos_round();
}

TEST_F(XcomStatisticsStorageImplTest, AddEmptyProposalRoundTest) {
  EXPECT_CALL(stats_mgr_mock, set_count_var_value(kEmptyProposalRounds))
      .Times(1);

  xcom_stats_storage_if->add_empty_proposal_round();
}

TEST_F(XcomStatisticsStorageImplTest, AddBytesSentTest) {
  constexpr uint64_t sent_bytes = 23456;

  EXPECT_CALL(stats_mgr_mock, set_sum_var_value(kBytesSent, sent_bytes))
      .Times(1);

  xcom_stats_storage_if->add_bytes_sent(sent_bytes);
}

TEST_F(XcomStatisticsStorageImplTest, AddProposalTimeTest) {
  constexpr long long set_time = 22334455;

  EXPECT_CALL(stats_mgr_mock,
              set_sum_timestamp_var_value(kCumulativeProposalTime, set_time))
      .Times(1);

  xcom_stats_storage_if->add_proposal_time(set_time);
}

TEST_F(XcomStatisticsStorageImplTest, AddThreePhasePaxosTest) {
  EXPECT_CALL(stats_mgr_mock, set_count_var_value(kFullProposalCount)).Times(1);

  xcom_stats_storage_if->add_three_phase_paxos();
}

TEST_F(XcomStatisticsStorageImplTest, AddMessageTest) {
  EXPECT_CALL(stats_mgr_mock, set_count_var_value(kMessagesSent)).Times(1);

  xcom_stats_storage_if->add_message();
}

TEST_F(XcomStatisticsStorageImplTest, AddBytesReceivedTest) {
  uint64_t received_bytes = 23456;

  EXPECT_CALL(stats_mgr_mock,
              set_sum_var_value(kMessageBytesReceived, received_bytes))
      .Times(1);

  xcom_stats_storage_if->add_bytes_received(received_bytes);
}

TEST_F(XcomStatisticsStorageImplTest, SetLastProposalTimeTest) {
  long long set_time = 22334455;

  EXPECT_CALL(stats_mgr_mock,
              set_timestamp_var_value(kLastProposalRoundTime, set_time))
      .Times(1);

  xcom_stats_storage_if->set_last_proposal_time(set_time);
}

}  // namespace gcs_xcom_statistics_storage_impl_unittest