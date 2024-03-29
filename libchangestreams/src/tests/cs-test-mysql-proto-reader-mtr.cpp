/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <set>
#include <sstream>
#include <string>

#include "libchangestreams/include/mysql/cs/reader/binary/mysqlproto.h"
#include "libchangestreams/src/lib/mysql/cs/codec/pb/reader_state_codec_pb.h"

namespace cs::reader::binary::inttests {

static char **argv;
static int argc;

/**
 * @brief This is a test case that is called from an MTR test case.
 *
 * This test leverages GTest framework to implement assertions and
 * run client side tests for binlog events.
 */
class TestMysqlProtoReader : public ::testing::Test {
 public:
  const static inline std::string S_UUID0{
      "00000000-0000-0000-0000-000000000000"};
  const static inline std::string S_UUID1{
      "11111111-1111-1111-1111-111111111111"};
  MYSQL *mysql1{nullptr};
  MYSQL *mysql2{nullptr};
  std::shared_ptr<cs::reader::State> state1{nullptr};
  std::shared_ptr<cs::reader::State> state2{nullptr};
  cs::reader::binary::Mysql_protocol *reader1{nullptr};
  cs::reader::binary::Mysql_protocol *reader2{nullptr};
  binary_log::gtids::Uuid uuid0;
  binary_log::gtids::Uuid uuid1;
  binary_log::gtids::Gtid gtid0_1{uuid0, 1};
  binary_log::gtids::Gtid gtid0_2{uuid0, 2};
  binary_log::gtids::Gtid gtid1_1{uuid1, 1};
  binary_log::gtids::Gtid gtid1_2{uuid1, 2};

  void SetUp() override {
    if (argc != 5) {
      GTEST_SKIP();
    }
    // ASSERT_EQ(argc, 5);
    auto host = argv[1];
    auto user = argv[2];
    auto pass = argv[3];
    uint32_t port = std::atoi(argv[4]);
    ASSERT_FALSE(uuid0.parse(S_UUID0.c_str(), S_UUID0.size()));
    ASSERT_FALSE(uuid1.parse(S_UUID1.c_str(), S_UUID1.size()));
    gtid0_1 = {uuid0, 1};
    gtid0_2 = {uuid0, 2};
    gtid1_1 = {uuid1, 1};
    gtid1_2 = {uuid1, 2};

    uint32_t flags =
        cs::reader::binary::Mysql_protocol::COM_BINLOG_DUMP_FLAG_NON_BLOCKING;

    // setup reader1
    MYSQL *mysql1 = mysql_init(nullptr);
    ASSERT_TRUE(mysql1);
    ASSERT_TRUE(mysql_real_connect(mysql1, host, user, pass, nullptr, port,
                                   nullptr, 0));
    reader1 = new cs::reader::binary::Mysql_protocol(mysql1, 24844, flags);

    // setup reader2
    MYSQL *mysql2 = mysql_init(nullptr);
    ASSERT_TRUE(mysql2);
    ASSERT_TRUE(mysql_real_connect(mysql2, host, user, pass, nullptr, port,
                                   nullptr, 0));
    reader2 = new cs::reader::binary::Mysql_protocol(mysql2, 24844, flags);
  }

  void TearDown() override {
    if (reader2 != nullptr) mysql_close(reader2->get_mysql_connection());
    if (reader1 != nullptr) mysql_close(reader1->get_mysql_connection());

    delete reader1;
    delete reader2;
  }
};

TEST_F(TestMysqlProtoReader, ReadEmptyState) {
  std::vector<uint8_t> buffer;
  std::set<std::string> expected_gtids{gtid0_1.to_string(), gtid0_2.to_string(),
                                       gtid1_1.to_string(),
                                       gtid1_2.to_string()};

  std::set<std::string> received_gtids{};

  // At this point we have four transactions in the log UUID0:1, UUID0:2,
  // UUID1:1, UUID1:2
  ASSERT_FALSE(reader1->open(std::make_shared<cs::reader::State>()));

  while (!reader1->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);

  reader1->close();
}

TEST_F(TestMysqlProtoReader, ReadUpdatedState) {
  std::vector<uint8_t> buffer;
  // we will receive the other two (gtid1_1, gtid1_2), since we
  // are adding gtid0_1 and gitd0_2 to the state.
  std::set<std::string> expected_gtids{gtid1_1.to_string(),
                                       gtid1_2.to_string()};
  std::set<std::string> received_gtids{};

  state1 = std::make_shared<cs::reader::State>();
  binary_log::gtids::Gtid gtid0_1{uuid0, 1};
  binary_log::gtids::Gtid gtid0_2{uuid0, 2};

  // lets add both gtids to the state
  state1->add_gtid(gtid0_1);
  state1->add_gtid(gtid0_2);

  ASSERT_FALSE(reader1->open(state1));

  while (!reader1->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);

  reader1->close();
}

TEST_F(TestMysqlProtoReader, RereadUpdatedState) {
  std::vector<uint8_t> buffer;

  /* *****************************************************************************
   * * First Connection *
   * *****************************************************************************
   */
  std::set<std::string> expected_gtids{gtid0_1.to_string(), gtid0_2.to_string(),
                                       gtid1_1.to_string(),
                                       gtid1_2.to_string()};
  std::set<std::string> received_gtids{};

  state1 = std::make_shared<cs::reader::State>();
  ASSERT_FALSE(reader1->open(state1));

  while (!reader1->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);
  reader1->close();

  /* *****************************************************************************
   * * Second Connection *
   * *****************************************************************************
   */
  expected_gtids.clear();
  // we will add the ':1' terminated to the state, so we expect the rest (':2')
  expected_gtids.insert(gtid0_2.to_string());
  expected_gtids.insert(gtid1_2.to_string());
  received_gtids.clear();

  state2 = std::make_shared<cs::reader::State>();
  state2->add_gtid(gtid0_1);
  state2->add_gtid(gtid1_1);
  ASSERT_FALSE(reader2->open(state2));

  while (!reader2->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  // at this point, we have only received uuid0:2, uuid1:2
  ASSERT_EQ(received_gtids, expected_gtids);

  reader2->close();
}

TEST_F(TestMysqlProtoReader, RereadSerializedState) {
  std::vector<uint8_t> buffer;

  /* *****************************************************************************
   * * First Connection *
   * *****************************************************************************
   */

  std::set<std::string> expected_gtids{gtid0_1.to_string(), gtid0_2.to_string(),
                                       gtid1_1.to_string(),
                                       gtid1_2.to_string()};
  std::set<std::string> received_gtids{};
  state1 = std::make_shared<cs::reader::State>();
  ASSERT_FALSE(reader1->open(state1));

  while (!reader1->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);
  reader1->close();

  /* *****************************************************************************
   * * Second Connection *
   * *****************************************************************************
   */
  expected_gtids.clear();
  received_gtids.clear();

  // emulate that we are loading the state from some storage and rereading
  cs::reader::codec::pb::example::stringstream pb_ss;
  state2 = std::make_shared<cs::reader::State>();

  // serialize the old state
  pb_ss << *reader1->get_state();

  // load the old state and reread
  pb_ss >> *state2;

  // there shall not be any new transaction transferred, since we had
  // transferred everything in the first readion

  ASSERT_FALSE(reader2->open(state2));

  while (!reader2->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      /* purecov: begin inspected */
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
      /* purecov: end */
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);

  reader2->close();
}

TEST_F(TestMysqlProtoReader, ReadUpdateImplicitState) {
  std::vector<uint8_t> buffer;
  state1 = nullptr;

  // since state1 is null, the reader1->open will create an empty state
  // and it will pull binary logs from the start
  std::set<std::string> expected_gtids{gtid0_1.to_string(), gtid0_2.to_string(),
                                       gtid1_1.to_string(),
                                       gtid1_2.to_string()};
  std::set<std::string> received_gtids{};

  ASSERT_FALSE(reader1->open(state1));

  while (!reader1->read(buffer)) {
    auto evt_type = (binary_log::Log_event_type)buffer[EVENT_TYPE_OFFSET];
    if (evt_type == binary_log::GTID_LOG_EVENT) {
      binary_log::Format_description_event fde{BINLOG_VERSION, "8.0.26"};
      auto char_buffer = reinterpret_cast<const char *>(buffer.data());
      binary_log::Gtid_event gev(char_buffer, &fde);
      binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
      received_gtids.insert(gtid.to_string());
    }
  }

  ASSERT_EQ(received_gtids, expected_gtids);

  reader1->close();
}

}  // namespace cs::reader::binary::inttests

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  cs::reader::binary::inttests::argv = argv;
  cs::reader::binary::inttests::argc = argc;
  auto res = RUN_ALL_TESTS();
  mysql_library_end();  // TODO: check this
  return res;
}