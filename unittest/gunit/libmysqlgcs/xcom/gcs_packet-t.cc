/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <ctime>

#include "gcs_base_test.h"

#include "gcs_internal_message.h"
#include "gcs_message_stage_lz4.h"
#include "gcs_message_stages.h"
#include "gcs_xcom_statistics_interface.h"
#include "mysql/gcs/gcs_message.h"

namespace gcs_xcom_packet_unittest {

class GcsPacketTest : public GcsBaseTest {
 protected:
  GcsPacketTest() {}

  virtual void SetUp() { lz4_stage = new Gcs_message_stage_lz4(true, 1024); }

  virtual void TearDown() { delete lz4_stage; }

  Gcs_message_stage_lz4 *lz4_stage;

 public:
  static const unsigned long long LARGE_PAYLOAD_LEN;
  static const unsigned long long SMALL_PAYLOAD_LEN;
};

const unsigned long long GcsPacketTest::LARGE_PAYLOAD_LEN =
    1024 + Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE;
const unsigned long long GcsPacketTest::SMALL_PAYLOAD_LEN =
    1024 - Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE;

TEST_F(GcsPacketTest, PacketCapacity) {
  Gcs_internal_message_header gcs_hd;

  /*
   Verify if the packet is allocating the right capacity.
   */
  gcs_hd.set_payload_length(1023 - gcs_hd.get_fixed_header_length());
  Gcs_packet p1(gcs_hd);
  ASSERT_EQ(p1.get_capacity(), Gcs_packet::BLOCK_SIZE);

  gcs_hd.set_payload_length(1023 + gcs_hd.get_fixed_header_length());
  Gcs_packet p2(gcs_hd);
  ASSERT_EQ(p2.get_capacity(), 2 * Gcs_packet::BLOCK_SIZE);

  /*
   Verify if the capacity is being calculated properly.
   */
  ASSERT_EQ(p1.calculate_capacity(Gcs_packet::BLOCK_SIZE - 1),
            Gcs_packet::BLOCK_SIZE);
  ASSERT_EQ(p1.calculate_capacity(Gcs_packet::BLOCK_SIZE),
            Gcs_packet::BLOCK_SIZE);
  ASSERT_EQ(p1.calculate_capacity(2 * Gcs_packet::BLOCK_SIZE - 1),
            2 * Gcs_packet::BLOCK_SIZE);

  ASSERT_EQ(
      p1.calculate_capacity(p1.calculate_capacity(Gcs_packet::BLOCK_SIZE - 1)),
      Gcs_packet::BLOCK_SIZE);
  ASSERT_EQ(
      p1.calculate_capacity(p1.calculate_capacity(Gcs_packet::BLOCK_SIZE)),
      Gcs_packet::BLOCK_SIZE);
  ASSERT_EQ(p1.calculate_capacity(
                p1.calculate_capacity(2 * Gcs_packet::BLOCK_SIZE - 1)),
            2 * Gcs_packet::BLOCK_SIZE);

  /*
   Free allocated buffers.
   */
  p1.free_buffer();
  p2.free_buffer();
}

TEST_F(GcsPacketTest, PacketInit) {
  const char content[] = "OLA123";
  unsigned int content_len = sizeof(content);

  /*
   Simulate a message that was prepared by an upper layer such as group
   replication.
   */
  Gcs_member_identifier origin(std::string("luis"));
  Gcs_message msg(origin, new Gcs_message_data(0, content_len));
  Gcs_internal_message_header::cargo_type cargo =
      Gcs_internal_message_header::cargo_type::CT_INTERNAL_STATE_EXCHANGE;
  msg.get_message_data().append_to_payload((const unsigned char *)content,
                                           content_len);

  /*
   Create an internal gcs message that will be eventually delivered to
   the group communication layer.
   */
  Gcs_message_data &msg_data = msg.get_message_data();
  unsigned long long payload_length = msg_data.get_encode_size();

  Gcs_internal_message_header gcs_hd;
  gcs_hd.set_payload_length(payload_length);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_cargo_type(cargo);

  Gcs_packet p(gcs_hd);
  uint64_t buffer_size = p.get_capacity();

  ASSERT_NE(p.get_buffer(), (void *)NULL);

  /*
   Encode the payload encapsulated in the group replication message into
   the gcs message and write the header information into the gcs message.
   */
  msg_data.encode(p.get_payload(), &buffer_size);
  gcs_hd.encode(p.get_buffer());

  ASSERT_EQ(p.get_payload_length(), payload_length);
  ASSERT_EQ(p.get_total_length(),
            payload_length +
                Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE);
  ASSERT_GE(p.get_capacity(), p.BLOCK_SIZE);

  /*
   Decode the payload from the gcs message into the group replication
   message.
   */
  Gcs_message_data msg_decoded(p.get_payload_length());
  msg_decoded.decode(p.get_payload(), p.get_payload_length());

  ASSERT_TRUE(strncmp((const char *)msg_decoded.get_payload(),
                      (const char *)content, content_len) == 0);

  free(p.get_buffer());
}

}  // namespace gcs_xcom_packet_unittest
