/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <stddef.h>

#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/protocol/encoders/encoding_xmessages.h"
#include "unittest/gunit/xplugin/xpl/protobuf_message.h"

namespace protocol {

namespace test {

class Encoder_validator {
 private:
  void check_size(const char *info) {
    if (m_buffer_left < 0)
      FAIL() << "Buffer underflow at " << info << "(field: " << m_field_number
             << "), please increase the buffer in begin_xmessage";
  }

  void summarize_buffer(const char *info) {
    if (m_buffer_left > 0) {
      FAIL() << "Buffer was not filled to its boundaries at " << info
             << ", not used space: " << m_buffer_left;
    }

    if (m_buffer_left < 0) {
      FAIL() << "Buffer is too small at " << info
             << ", we need additional: " << -m_buffer_left
             << " bytes, in total: " << -m_buffer_left + m_buffer_size;
    }
  }

 public:
  using Position = XProtocol_encoder::Position;

  template <uint32_t t>
  using Field_delimiter = XProtocol_encoder::Field_delimiter<t>;

  template <uint32_t id, uint32_t needed_size>
  Position begin_xmessage() {
    if (m_message_started) {
      ADD_FAILURE() << "Message already started";
      return {};
    }

    m_buffer_size = m_buffer_left = needed_size;
    m_buffer_left -= k_xmsg_header_size;

    return {};
  }

  void end_xmessage(const Position &) {
    if (m_message_ended) {
      FAIL() << "Message already finished";
    }

    summarize_buffer("end_xmessage");
  }

  template <uint32_t field_id>
  void encode_field_enum(const int32_t) {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint32_size);

    check_size("encode_field_enum");
  }

  template <uint32_t field_id>
  void encode_optional_field_var_uint64(const uint64_t *) {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("encode_optional_field_var_uint64");
  }

  template <uint32_t field_id>
  void encode_optional_field_var_uint32(const uint32_t *) {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint32_size);

    check_size("encode_optional_field_var_uint32");
  }

  template <uint32_t field_id>
  void encode_field_string(const std::string &) {
    ++m_field_number;
    summarize_buffer("encode_field_string");
    m_buffer_left = 0;
  }

  template <uint32_t field_id, uint64_t value>
  void encode_field_const_var_uint() {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("encode_field_const_var_uint");
  }

  template <uint32_t field_id, int64_t value>
  void encode_field_const_enum() {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("encode_field_const_enum");
  }

  template <uint32_t field_id>
  void encode_field_var_uint32(const uint32_t) {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("k_varint32_size");
  }

  template <uint32_t id, uint32_t delimiter_length = 1>
  Field_delimiter<delimiter_length> begin_delimited_field() {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("begin_delimited_field");

    return {};
  }

  template <uint32_t delimiter_length>
  void end_delimited_field(const Field_delimiter<delimiter_length> &) {}

  template <uint32_t field_id>
  void encode_field_var_uint64(const uint64_t) {
    ++m_field_number;
    m_buffer_left -= (k_varint_field_prefix_size + k_varint64_size);

    check_size("encode_field_var_uint64");
  }

  void encode_raw(const uint8_t *, uint32_t) { m_buffer_left = 0; }

  template <uint32_t id>
  void empty_xmessage() {
    if (m_message_started) {
      FAIL() << "Message already started";
    }

    if (m_message_ended) {
      FAIL() << "Message already ended";
    }

    m_message_started = true;
    m_message_ended = true;
    check_size("empty_xmessage");
  }

 private:
  const int k_varint_field_prefix_size = 10;
  const int k_varint32_size = 5;
  const int k_varint64_size = 10;
  const int k_xmsg_header_size = 5;

  bool m_message_started = false;
  bool m_message_ended = false;
  int64_t m_buffer_left = 0;
  int64_t m_buffer_size = 0;
  int m_field_number = 0;
};

class Encoder_validator_testsuite : public ::testing::Test {
 public:
  protocol::XMessage_encoder_base<Encoder_validator> m_encoder;
};

TEST_F(Encoder_validator_testsuite, encode_full_metadata) {
  ::ngs::Encode_column_info column_info;
  column_info.m_compact = false;

  m_encoder.encode_metadata(&column_info);
}

TEST_F(Encoder_validator_testsuite, encode_compact_metadata) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_metadata(&column_info);
}

TEST_F(Encoder_validator_testsuite, encode_compact_metadata_multiple_params) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_compact_metadata(0, nullptr, nullptr, nullptr, nullptr,
                                    nullptr);
}

TEST_F(Encoder_validator_testsuite, encode_full_metadata_multiple_params) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_full_metadata("", "", "", "", "", "", 0, nullptr, nullptr,
                                 nullptr, nullptr, nullptr);
}

TEST_F(Encoder_validator_testsuite, encode_notice_row_affected) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice_rows_affected(0);
}

TEST_F(Encoder_validator_testsuite, encode_notice_client_id) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice_client_id(0);
}

TEST_F(Encoder_validator_testsuite, encode_notice_expired) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice_expired();
}

TEST_F(Encoder_validator_testsuite, encode_notice_generated_insert_id) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice_generated_insert_id(0);
}

TEST_F(Encoder_validator_testsuite, encode_notice_text_message) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice_text_message("");
}

TEST_F(Encoder_validator_testsuite, encode_notice) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_notice(0, 0, "");
}

TEST_F(Encoder_validator_testsuite, encode_global_notice) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_global_notice(0, "");
}

TEST_F(Encoder_validator_testsuite, encode_fetch_more_resultsets) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_fetch_more_resultsets();
}

TEST_F(Encoder_validator_testsuite, encode_fetch_out_params) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_fetch_out_params();
}

TEST_F(Encoder_validator_testsuite, encode_fetch_suspended) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_fetch_suspended();
}

TEST_F(Encoder_validator_testsuite, encode_fetch_done) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_fetch_done();
}

TEST_F(Encoder_validator_testsuite, encode_stmt_execute_ok) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_stmt_execute_ok();
}

TEST_F(Encoder_validator_testsuite, encode_ok) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_ok();
}

TEST_F(Encoder_validator_testsuite, encode_ok_with_param) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_ok("");
}

TEST_F(Encoder_validator_testsuite, encode_error) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_error(0, 0, "", "");
}

TEST_F(Encoder_validator_testsuite, encode_xmessage) {
  ::ngs::Encode_column_info column_info;

  m_encoder.encode_xmessage<1>("");
}

}  // namespace test

}  // namespace protocol
