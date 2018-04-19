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
#include "test_logger.h"

namespace gcs_xcom_stages_unittest {

class XcomStagesTest : public GcsBaseTestNoLogging {
 protected:
  XcomStagesTest(){};

  virtual void SetUp() {
    lz4_stage =
        new Gcs_message_stage_lz4(Gcs_message_stage_lz4::DEFAULT_THRESHOLD);
  }

  virtual void TearDown() { delete lz4_stage; }

  Gcs_message_stage_lz4 *lz4_stage;

 public:
  static const unsigned long long LARGE_PAYLOAD_LEN;
  static const unsigned long long SMALL_PAYLOAD_LEN;
};

const unsigned long long XcomStagesTest::LARGE_PAYLOAD_LEN =
    Gcs_message_stage_lz4::DEFAULT_THRESHOLD +
    Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;
const unsigned long long XcomStagesTest::SMALL_PAYLOAD_LEN =
    Gcs_message_stage_lz4::DEFAULT_THRESHOLD -
    Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;

TEST_F(XcomStagesTest, DoNotCompressMessage) {
  Gcs_internal_message_header gcs_hd(
      Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE);
  unsigned long long payload_len = SMALL_PAYLOAD_LEN;
  unsigned long long size =
      payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;
  // create a packet to send
  Gcs_packet p(size);
  unsigned char *control = (unsigned char *)malloc(payload_len);

  // mess around with the payload
  memset(p.get_buffer() + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  // create the header
  gcs_hd.set_msg_length(size +
                        Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_msg_length(size);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::CT_USER_DATA);

  // insert the fixed header in the packet
  p.reload_header(gcs_hd);

  unsigned long long before_length = p.get_payload_length();

  // this must not apply, since the payload is less than the threshold
  lz4_stage->apply(p);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_EQ(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(), (unsigned int)0);
  ASSERT_EQ(p.get_payload() - p.get_buffer(),
            Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  EXPECT_TRUE(strncmp((const char *)p.get_payload(), (const char *)control,
                      SMALL_PAYLOAD_LEN) == 0);

  free((void *)p.get_buffer());
  free(control);
}

TEST_F(XcomStagesTest, CompressMessage) {
  Gcs_internal_message_header gcs_hd(
      Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE);
  unsigned long long payload_len = LARGE_PAYLOAD_LEN;
  unsigned long long size =
      payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;
  // create a packet to send
  Gcs_packet p(size);
  unsigned char *control = (unsigned char *)malloc(payload_len);

  // mess around with the payload
  memset(p.get_buffer() + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  // create the header
  gcs_hd.set_msg_length(size +
                        Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_msg_length(size);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::CT_USER_DATA);
  gcs_hd.encode(p.get_buffer());
  p.reload_header(gcs_hd);

  unsigned long long before_length = p.get_payload_length();

  // this must not apply, since the payload is less than the threshold
  lz4_stage->apply(p);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_NE(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(),
            static_cast<unsigned int>(
                Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
                Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
                Gcs_message_stage_lz4::WIRE_HD_UNCOMPRESSED_SIZE));
  EXPECT_FALSE(strncmp((const char *)p.get_payload(), (const char *)control,
                       LARGE_PAYLOAD_LEN) == 0);

  free((void *)p.get_buffer());
  free(control);
}

TEST_F(XcomStagesTest, CompressDecompressMessage) {
  Gcs_internal_message_header gcs_hd(
      Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE);
  unsigned long long payload_len = LARGE_PAYLOAD_LEN;
  unsigned long long size =
      payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;
  // create a packet to send
  Gcs_packet p(size);
  unsigned char *control = (unsigned char *)malloc(payload_len);
  unsigned int size_of_lz4_stage_header =
      Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_UNCOMPRESSED_SIZE;

  lz4_stage->set_threshold(1);  // all messages are compressed always

  // mess around with the payload
  memset(p.get_buffer() + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  // create the header
  gcs_hd.set_msg_length(size +
                        Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_msg_length(size);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::CT_USER_DATA);
  gcs_hd.encode(p.get_buffer());

  // reload the header information into the packet
  p.reload_header(gcs_hd);

  unsigned long long before_length = p.get_payload_length();

  // this must apply, since the payload is more than the threshold
  bool error = lz4_stage->apply(p);
  ASSERT_FALSE(error);

  error = lz4_stage->apply(p);
  ASSERT_FALSE(error);

  // there are two headers in the packet
  ASSERT_EQ(p.get_dyn_headers_length(), 2 * size_of_lz4_stage_header);

  error = lz4_stage->revert(p);
  ASSERT_FALSE(error);

  // there is still one header to remove
  ASSERT_EQ(p.get_dyn_headers_length(), size_of_lz4_stage_header);

  error = lz4_stage->revert(p);
  ASSERT_FALSE(error);

  // this tests that the revert operation does not crash if there is no
  // header left to decode
  error = lz4_stage->revert(p);
  ASSERT_FALSE(error);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_EQ(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(),
            (unsigned int)0);  // all headers processed
  EXPECT_TRUE(strncmp((const char *)p.get_payload(), (const char *)control,
                      LARGE_PAYLOAD_LEN) == 0);

  free((void *)p.get_buffer());
  free(control);
}

/**
 This is the test case for BUG#22973628 .

 We were calculating wrong the alignment for Gcs_packet::BLOCK_SIZE, since
 the size of the destination buffer was considering the size of the dynamic
 header length, which was wrong. In this test, we calculate a payload size
 that is somwehere between the window

    Gcs_packet::BLOCK_SIZE -
    Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE +
    hd_len

  So that when decompressing the allocated buffer is
  Gcs_packet::BLOCK_SIZE + 8 -
  Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;

  Before the fix for BUG#22973628, this would raise a valgrind
 warning and sysbench with GR would occasionally segfault.
 */
TEST_F(XcomStagesTest, CompressDecompressMessageBoundary) {
  Gcs_internal_message_header gcs_hd(
      Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE);

  // We will create a payload that is just below the threshold
  unsigned long long payload_len =
      Gcs_packet::BLOCK_SIZE -
      Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE +
      8 /* uncompress allocates a buffer that is larger than BLOCK_SIZE */;

  unsigned long long size =
      payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE;

  // create a packet to send
  Gcs_packet p(size);
  unsigned char *control = (unsigned char *)malloc(payload_len);
  unsigned int size_of_lz4_stage_header =
      Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_UNCOMPRESSED_SIZE;

  lz4_stage->set_threshold(1);  // all messages are compressed always

  // mess around with the payload
  memset(p.get_buffer() + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  // create the header
  gcs_hd.set_msg_length(size +
                        Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_msg_length(size);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::CT_USER_DATA);
  gcs_hd.encode(p.get_buffer());

  // reload the header information into the packet
  p.reload_header(gcs_hd);

  unsigned long long before_length = p.get_payload_length();

  // threshold was set to 1, thence this applies.
  bool error = lz4_stage->apply(p);
  ASSERT_FALSE(error);

  // there is one header in the packet
  ASSERT_EQ(p.get_dyn_headers_length(), size_of_lz4_stage_header);

  // remove the header and decompress
  error = lz4_stage->revert(p);
  ASSERT_FALSE(error);

  // there are no more headers in the packet
  ASSERT_EQ(p.get_dyn_headers_length(), (unsigned)0);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_EQ(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(),
            (unsigned int)0);  // all headers processed
  EXPECT_TRUE(strncmp((const char *)p.get_payload(), (const char *)control,
                      payload_len) == 0);

  free((void *)p.get_buffer());
  free(control);
}

TEST_F(XcomStagesTest, CannotCompressPayloadTooBig) {
  std::string error_message(
      "Gcs_packet's payload is too big. Only the packets "
      "smaller than 2113929216 bytes can be compressed.");
  // Don't need to allocate memory, apply() will not access its payload.
  Gcs_packet p(0);
  bool error;

  test_logger.clear_event();

  /*
    Set payload's length bigger than LZ4_MAX_INPUT_SIZE.
    It causes LZ4_compressBound returns 0. And apply() will return
    true and log an error if LZ4_compressBound return 0.
  */
  p.set_payload_length(2113929216 + 1);
  error = lz4_stage->apply(p);

  ASSERT_TRUE(error);
  test_logger.assert_error(error_message);

  test_logger.clear_event();

  /*
    Set payload's length bigger than uint32. apply() should return true and
    log an error when payload is bigger than uint32. It must be tested because
    length bigger than uint32 is  handled different from length in uint32 range.
    For detail, see the comment in gcs_message_stage_lz4::apply().
  */
  p.set_payload_length((1ULL << 32) + 1);
  error = lz4_stage->apply(p);

  ASSERT_TRUE(error);
  test_logger.assert_error(error_message);
}

}  // namespace gcs_xcom_stages_unittest
