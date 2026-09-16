/* Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <stddef.h>
#include <array>
#include <string_view>
#include <vector>
#include "decimal.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/com_data.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "mysys_util.h"
#include "sql-common/my_decimal.h"
#include "sql/mysqld.h"
#include "sql/protocol_classic.h"
#include "sql/sql_class.h"
#include "sql/sql_prepare.h"
#include "sql_string.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/test_utils.h"

namespace protocol_classic_unittest {

/**
 * Initializes a Protocol_classic instance before a microbenchmark.
 */
static void SetupProtocolForBenchmark(Protocol_classic *protocol) {
  // Simulate sending results to a client that expects UTF-8 strings.
  protocol->set_result_character_set(&my_charset_utf8mb4_0900_ai_ci);

  // Make sure there is room for a row in the packet buffer without further
  // allocations.
  protocol->get_output_packet()->reserve(1024);
}

static void BM_Protocol_binary_store_date(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_binary *const protocol = initializer.thd()->protocol_binary.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const Date_val date(2020, 2, 29);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_date(date);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_binary_store_date)

static void BM_Protocol_binary_store_time(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_binary *const protocol = initializer.thd()->protocol_binary.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const Time_val time(false, 123, 59, 59, 670000);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_time(time, 6);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_binary_store_time)

static void BM_Protocol_binary_store_datetime(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_binary *const protocol = initializer.thd()->protocol_binary.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const MysqlTime datetime(2020, 2, 29, 23, 59, 59, 670000);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_datetime(datetime, 6);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_binary_store_datetime)

static void BM_Protocol_binary_store_decimal(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_binary *const protocol = initializer.thd()->protocol_binary.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const char decimal_string[] =
      "12345678901234567890123456789012345678901234567890123456789012345";
  my_decimal decimal;
  str2my_decimal(E_DEC_FATAL_ERROR, decimal_string, sizeof(decimal_string) - 1,
                 &my_charset_utf8mb4_bin, &decimal);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_decimal(&decimal, 0, 0);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_binary_store_decimal)

static void BM_Protocol_text_store_tiny(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_text *const protocol = initializer.thd()->protocol_text.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const int value = 123;

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_tiny(value, 0);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_text_store_tiny)

static void BM_Protocol_text_store_longlong(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_text *const protocol = initializer.thd()->protocol_text.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const int64_t value = 1234567890123456789;

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_longlong(value, false, 0);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_text_store_longlong)

static void BM_Protocol_text_store_date(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_text *const protocol = initializer.thd()->protocol_text.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const Date_val date(2020, 2, 29);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_date(date);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_text_store_date)

static void BM_Protocol_text_store_time(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_text *const protocol = initializer.thd()->protocol_text.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const Time_val time(false, 123, 59, 59, 670000);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_time(time, 6);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_text_store_time)

static void BM_Protocol_text_store_datetime(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  Protocol_text *const protocol = initializer.thd()->protocol_text.get();
  SetupProtocolForBenchmark(protocol);
  String *const packet = protocol->get_output_packet();

  const MysqlTime datetime(2020, 2, 29, 23, 59, 59, 670000);

  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    packet->length(0);
    protocol->store_datetime(datetime, 6);
  }

  StopBenchmarkTiming();
  initializer.TearDown();
}
BENCHMARK(BM_Protocol_text_store_datetime)

struct Temporal_parameter_length {
  enum_field_types type;
  uchar length;
};

class Prepared_statement_limit {
 public:
  Prepared_statement_limit() : m_saved_limit(max_prepared_stmt_count) {
    max_prepared_stmt_count = 1;
  }
  ~Prepared_statement_limit() { max_prepared_stmt_count = m_saved_limit; }

 private:
  const ulong m_saved_limit;
};

static std::vector<uchar> MakeQueryWithTemporalAttribute(enum_field_types type,
                                                         uchar length) {
  const uchar type_code = static_cast<uchar>(type);
  std::vector<uchar> packet{/* parameter count */ 1,
                            /* parameter set count */ 1,
                            /* null bitmap */ 0,
                            /* new types follow */ 1,
                            type_code,
                            0,
                            /* attribute name */ 2,
                            'q',
                            'a',
                            length};
  packet.insert(packet.end(), length, 0);
  constexpr char query[] = "SELECT 1";
  packet.insert(packet.end(), query, query + sizeof(query) - 1);
  return packet;
}

static std::vector<uchar> MakeStmtExecuteWithLengthEncodedParameter(
    ulong statement_id, enum_field_types type, uchar declared_length,
    uchar payload_length = 0) {
  if (payload_length == 0) payload_length = declared_length;
  const uchar type_code = static_cast<uchar>(type);
  std::vector<uchar> packet{static_cast<uchar>(statement_id),
                            static_cast<uchar>(statement_id >> 8),
                            static_cast<uchar>(statement_id >> 16),
                            static_cast<uchar>(statement_id >> 24),
                            /* flags */ 0,
                            /* iteration count */ 1,
                            0,
                            0,
                            0,
                            /* null bitmap */ 0,
                            /* new types follow */ 1,
                            type_code,
                            /* unsigned */ 0,
                            declared_length};
  packet.insert(packet.end(), payload_length, 0);
  return packet;
}

TEST(ProtocolClassicTest, AcceptsValidTemporalQueryAttributeLengths) {
  constexpr std::array valid_lengths{
      Temporal_parameter_length{MYSQL_TYPE_DATE, 0},
      Temporal_parameter_length{MYSQL_TYPE_DATE, 4},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 0},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 8},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 12},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 0},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 4},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 7},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 11},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 13},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 0},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 4},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 7},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 11},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 13}};

  for (const auto [type, length] : valid_lengths) {
    SCOPED_TRACE(testing::Message() << "type=" << static_cast<int>(type)
                                    << ", length=" << static_cast<int>(length));
    my_testing::Server_initializer initializer;
    initializer.SetUp();
    Protocol_classic *protocol = initializer.thd()->get_protocol_classic();
    protocol->set_client_capabilities(CLIENT_QUERY_ATTRIBUTES);
    std::vector<uchar> packet = MakeQueryWithTemporalAttribute(type, length);
    COM_DATA command_data{};

    EXPECT_FALSE(protocol->create_command(&command_data, COM_QUERY,
                                          packet.data(), packet.size()));
    EXPECT_EQ(command_data.com_query.parameter_count, 1U);
    ASSERT_NE(command_data.com_query.parameters, nullptr);
    EXPECT_EQ(command_data.com_query.parameters[0].length, length);
    EXPECT_EQ(std::string_view(command_data.com_query.query,
                               command_data.com_query.length),
              "SELECT 1");
    initializer.TearDown();
  }
}

TEST(ProtocolClassicTest, RejectsInvalidTemporalQueryAttributeLengths) {
  constexpr std::array invalid_lengths{
      Temporal_parameter_length{MYSQL_TYPE_DATE, 1},
      Temporal_parameter_length{MYSQL_TYPE_DATE, 3},
      Temporal_parameter_length{MYSQL_TYPE_DATE, 5},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 1},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 7},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 9},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 10},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 11},
      Temporal_parameter_length{MYSQL_TYPE_TIME, 13},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 1},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 3},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 5},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 8},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 12},
      Temporal_parameter_length{MYSQL_TYPE_DATETIME, 14},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 1},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 3},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 5},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 8},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 12},
      Temporal_parameter_length{MYSQL_TYPE_TIMESTAMP, 14}};

  for (const auto [type, length] : invalid_lengths) {
    SCOPED_TRACE(testing::Message() << "type=" << static_cast<int>(type)
                                    << ", length=" << static_cast<int>(length));
    my_testing::Server_initializer initializer;
    initializer.SetUp();
    Protocol_classic *protocol = initializer.thd()->get_protocol_classic();
    protocol->set_client_capabilities(CLIENT_QUERY_ATTRIBUTES);
    initializer.set_expected_error(ER_MALFORMED_PACKET);
    std::vector<uchar> packet = MakeQueryWithTemporalAttribute(type, length);
    COM_DATA command_data{};

    EXPECT_TRUE(protocol->create_command(&command_data, COM_QUERY,
                                         packet.data(), packet.size()));
    initializer.TearDown();
  }
}

TEST(ProtocolClassicTest, AcceptsValidTemporalStmtExecuteTimeLengths) {
  constexpr std::array valid_lengths{uchar{0}, uchar{8}, uchar{12}};

  for (const uchar length : valid_lengths) {
    SCOPED_TRACE(testing::Message() << "length=" << static_cast<int>(length));
    my_testing::Server_initializer initializer;
    initializer.SetUp();
    Prepared_statement_limit prepared_statement_limit;
    auto *statement = new Prepared_statement(initializer.thd());
    statement->m_param_count = 1;
    ASSERT_EQ(initializer.thd()->stmt_map.insert(statement), 0);
    Protocol_classic *protocol = initializer.thd()->get_protocol_classic();
    std::vector<uchar> packet = MakeStmtExecuteWithLengthEncodedParameter(
        statement->id(), MYSQL_TYPE_TIME, length);
    COM_DATA command_data{};

    EXPECT_FALSE(protocol->create_command(&command_data, COM_STMT_EXECUTE,
                                          packet.data(), packet.size()));
    EXPECT_EQ(command_data.com_stmt_execute.parameter_count, 1U);
    ASSERT_NE(command_data.com_stmt_execute.parameters, nullptr);
    EXPECT_EQ(command_data.com_stmt_execute.parameters[0].length, length);
    initializer.TearDown();
  }
}

TEST(ProtocolClassicTest, RejectsInvalidTemporalStmtExecuteTimeLengths) {
  constexpr std::array invalid_lengths{uchar{9}, uchar{10}, uchar{11}};

  for (const uchar length : invalid_lengths) {
    SCOPED_TRACE(testing::Message() << "length=" << static_cast<int>(length));
    my_testing::Server_initializer initializer;
    initializer.SetUp();
    Prepared_statement_limit prepared_statement_limit;
    auto *statement = new Prepared_statement(initializer.thd());
    statement->m_param_count = 1;
    ASSERT_EQ(initializer.thd()->stmt_map.insert(statement), 0);
    Protocol_classic *protocol = initializer.thd()->get_protocol_classic();
    initializer.set_expected_error(ER_MALFORMED_PACKET);
    std::vector<uchar> packet = MakeStmtExecuteWithLengthEncodedParameter(
        statement->id(), MYSQL_TYPE_TIME, length);
    COM_DATA command_data{};

    EXPECT_TRUE(protocol->create_command(&command_data, COM_STMT_EXECUTE,
                                         packet.data(), packet.size()));
    initializer.TearDown();
  }
}

TEST(ProtocolClassicTest, RejectsTruncatedDecimalStmtExecuteParameters) {
  constexpr std::array decimal_types{MYSQL_TYPE_DECIMAL, MYSQL_TYPE_NEWDECIMAL};

  for (const auto type : decimal_types) {
    SCOPED_TRACE(testing::Message() << "type=" << static_cast<int>(type));
    my_testing::Server_initializer initializer;
    initializer.SetUp();
    Prepared_statement_limit prepared_statement_limit;
    auto *statement = new Prepared_statement(initializer.thd());
    statement->m_param_count = 1;
    ASSERT_EQ(initializer.thd()->stmt_map.insert(statement), 0);
    Protocol_classic *protocol = initializer.thd()->get_protocol_classic();
    initializer.set_expected_error(ER_MALFORMED_PACKET);
    std::vector<uchar> packet = MakeStmtExecuteWithLengthEncodedParameter(
        statement->id(), type, /* declared_length */ 4, /* payload_length */ 3);
    COM_DATA command_data{};

    EXPECT_TRUE(protocol->create_command(&command_data, COM_STMT_EXECUTE,
                                         packet.data(), packet.size()));
    initializer.TearDown();
  }
}

}  // namespace protocol_classic_unittest
