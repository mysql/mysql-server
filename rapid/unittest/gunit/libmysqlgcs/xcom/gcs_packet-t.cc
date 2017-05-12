/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ctime>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "gcs_internal_message.h"
#include "gcs_message_stages.h"
#include "gcs_xcom_statistics_interface.h"
#include "mysql/gcs/gcs_message.h"
#include "gcs_message_stage_lz4.h"
#include "mysql/gcs/gcs_log_system.h"

namespace gcs_xcom_packet_unittest
{

class GcsPacketTest : public ::testing::Test
{
protected:

  GcsPacketTest() {};

  virtual void SetUp()
  {
    logger= new Gcs_simple_ext_logger_impl();
    Gcs_logger::initialize(logger);
    lz4_stage= new Gcs_message_stage_lz4(1024);
  }

  virtual void TearDown()
  {
    delete lz4_stage;
    Gcs_logger::finalize();
    logger->finalize();
    delete logger;
  }

  Gcs_simple_ext_logger_impl *logger;
  Gcs_message_stage_lz4 *lz4_stage;
public:
  static const unsigned long long LARGE_PAYLOAD_LEN;
  static const unsigned long long SMALL_PAYLOAD_LEN;
};

const unsigned long long  GcsPacketTest::LARGE_PAYLOAD_LEN= 1024+Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;
const unsigned long long  GcsPacketTest::SMALL_PAYLOAD_LEN= 1024-Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;


TEST_F(GcsPacketTest, PacketInit)
{
  char content[]= "OLA123";
  unsigned int content_len= sizeof(content);

  Gcs_member_identifier origin(std::string("luis"));
  Gcs_message msg(origin, new Gcs_message_data(0, content_len));
  Gcs_internal_message_header::enum_cargo_type cargo= Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE;
  msg.get_message_data().append_to_payload((const unsigned char*)content, content_len);

  Gcs_message_data &msg_data= msg.get_message_data();
  unsigned long long len= msg_data.get_header_length() +
                          msg_data.get_payload_length();
  Gcs_packet p(len);
  uint64_t buffer_size= p.get_capacity();
  unsigned long long payload_len;
  Gcs_internal_message_header gcs_hd;

  ASSERT_NE(p.get_buffer(), (void*)NULL);

  // insert the payload
  msg_data.encode(p.get_buffer()+Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
                  &buffer_size);

  payload_len= buffer_size;

  // fix the header
  gcs_hd.set_msg_length(payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_cargo_type(cargo);
  gcs_hd.encode(p.get_buffer());

  // set the headers
  p.reload_header(gcs_hd);

  ASSERT_EQ(p.get_payload_length(), payload_len);
  ASSERT_EQ(p.get_length(), payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  ASSERT_GE(p.get_capacity(), p.BLOCK_SIZE);

  Gcs_message_data msg_decoded(p.get_payload_length());
  msg_decoded.decode(p.get_payload(), p.get_payload_length());

  ASSERT_TRUE(strncmp((const char*)msg_decoded.get_payload(), (const char*)content, content_len) == 0);

  free(p.get_buffer());
}

}
