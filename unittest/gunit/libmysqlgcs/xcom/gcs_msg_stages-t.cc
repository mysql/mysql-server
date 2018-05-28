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

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>

#include "gcs_base_test.h"

#include "gcs_internal_message.h"
#include "gcs_message_stage_lz4.h"
#include "gcs_message_stages.h"
#include "gcs_xcom_statistics_interface.h"
#include "mysql/gcs/xplatform/byteorder.h"
#include "test_logger.h"

namespace gcs_xcom_stages_unittest {

class XcomStagesTest : public GcsBaseTestNoLogging {
 protected:
  XcomStagesTest(){};

  virtual void SetUp() {
    lz4_stage = new Gcs_message_stage_lz4(
        true, Gcs_message_stage_lz4::DEFAULT_THRESHOLD);
  }

  virtual void TearDown() { delete lz4_stage; }

  Gcs_message_stage_lz4 *lz4_stage;

 public:
  static const unsigned long long LARGE_PAYLOAD_LEN;
  static const unsigned long long SMALL_PAYLOAD_LEN;
};

const unsigned long long XcomStagesTest::LARGE_PAYLOAD_LEN =
    Gcs_message_stage_lz4::DEFAULT_THRESHOLD +
    Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE;
const unsigned long long XcomStagesTest::SMALL_PAYLOAD_LEN =
    Gcs_message_stage_lz4::DEFAULT_THRESHOLD -
    Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE;

TEST_F(XcomStagesTest, DoNotCompressMessage) {
  Gcs_internal_message_header gcs_hd;
  unsigned long long payload_len = SMALL_PAYLOAD_LEN;

  // create the header
  gcs_hd.set_version(Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_payload_length(payload_len);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::cargo_type::CT_USER_DATA);

  // create a packet to send
  Gcs_packet p(gcs_hd);
  unsigned char *control = (unsigned char *)malloc(payload_len);

  // mess around with the payload
  memset(p.get_buffer() +
             Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);
  unsigned long long before_length = p.get_payload_length();
  gcs_hd.encode(p.get_buffer());

  // this must not apply, since the payload is less than the threshold
  lz4_stage->apply(p);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_EQ(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(), (unsigned int)0);
  ASSERT_EQ(p.get_payload() - p.get_buffer(),
            Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE);
  EXPECT_TRUE(strncmp((const char *)p.get_payload(), (const char *)control,
                      SMALL_PAYLOAD_LEN) == 0);

  free((void *)p.get_buffer());
  free(control);
}

TEST_F(XcomStagesTest, CompressMessage) {
  Gcs_internal_message_header gcs_hd;
  unsigned long long payload_len = LARGE_PAYLOAD_LEN;

  // create the header
  gcs_hd.set_version(Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION);
  gcs_hd.set_payload_length(payload_len);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::cargo_type::CT_USER_DATA);

  // create a packet to send
  Gcs_packet p(gcs_hd);
  unsigned char *control = (unsigned char *)malloc(payload_len);

  // mess around with the payload
  memset(p.get_buffer() +
             Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  unsigned long long before_length = p.get_payload_length();

  gcs_hd.encode(p.get_buffer());
  // this must apply, since the payload is greater than the threshold
  lz4_stage->apply(p);

  unsigned long long after_length = p.get_payload_length();

  ASSERT_NE(before_length, after_length);
  ASSERT_EQ(p.get_dyn_headers_length(),
            static_cast<unsigned int>(
                Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
                Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
                Gcs_message_stage_lz4::WIRE_HD_PAYLOAD_LEN_SIZE));
  EXPECT_FALSE(strncmp((const char *)p.get_payload(), (const char *)control,
                       LARGE_PAYLOAD_LEN) == 0);

  free((void *)p.get_buffer());
  free(control);
}

TEST_F(XcomStagesTest, CompressDecompressMessage) {
  Gcs_internal_message_header gcs_hd;
  unsigned long long payload_len = LARGE_PAYLOAD_LEN;

  // create the header
  gcs_hd.set_version(Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_payload_length(payload_len);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::cargo_type::CT_USER_DATA);

  // create a packet to send
  Gcs_packet p(gcs_hd);
  unsigned char *control = (unsigned char *)malloc(payload_len);
  unsigned int size_of_lz4_stage_header =
      Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_PAYLOAD_LEN_SIZE;

  lz4_stage->set_threshold(1);  // all messages are compressed always

  // mess around with the payload
  memset(p.get_buffer() +
             Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);

  unsigned long long before_length = p.get_payload_length();

  gcs_hd.encode(p.get_buffer());

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
  ASSERT_TRUE(error);

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
  Gcs_internal_message_header gcs_hd;

  // We will create a payload that is just below the threshold
  unsigned long long payload_len =
      Gcs_packet::BLOCK_SIZE -
      Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE +
      8 /* uncompress allocates a buffer that is larger than BLOCK_SIZE */;

  // create the header
  gcs_hd.set_version(Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION);
  gcs_hd.set_dynamic_headers_length(0);
  gcs_hd.set_payload_length(payload_len);
  gcs_hd.set_cargo_type(Gcs_internal_message_header::cargo_type::CT_USER_DATA);

  // create a packet to send
  Gcs_packet p(gcs_hd);
  unsigned char *control = (unsigned char *)malloc(payload_len);
  unsigned int size_of_lz4_stage_header =
      Gcs_message_stage_lz4::WIRE_HD_LEN_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_TYPE_SIZE +
      Gcs_message_stage_lz4::WIRE_HD_PAYLOAD_LEN_SIZE;

  lz4_stage->set_threshold(1);  // all messages are compressed always

  // mess around with the payload
  memset(p.get_buffer() +
             Gcs_internal_message_header::WIRE_TOTAL_FIXED_HEADER_SIZE,
         0x61, payload_len);
  memset(control, 0x61, payload_len);
  unsigned long long before_length = p.get_payload_length();
  gcs_hd.encode(p.get_buffer());

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
  // Don't need to allocate memory, apply() will not access its payload.
  Gcs_internal_message_header gcs_hd;
  gcs_hd.set_total_length(0);

  Gcs_packet p(gcs_hd);
  bool error;

  test_logger.clear_event();

  /*
    Set payload's length bigger than
    Gcs_message_stage_lz4::max_input_compression(). It causes LZ4_compressBound
    returns 0. And apply() will return true and log an error if
    LZ4_compressBound return 0.
  */
  p.set_payload_length(2113929216 + 1);
  error = lz4_stage->apply(p);

  ASSERT_TRUE(error);
  std::stringstream error_message_1;
  error_message_1
      << "Gcs_packet's payload is too big. Only packets smaller than "
      << Gcs_message_stage_lz4::max_input_compression()
      << " bytes can be compressed. "
      << "Payload size is " << p.get_payload_length() << ".";
  test_logger.assert_error(error_message_1);

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
  std::stringstream error_message_2;
  error_message_2
      << "Gcs_packet's payload is too big. Only packets smaller than "
      << Gcs_message_stage_lz4::max_input_compression()
      << " bytes can be compressed. "
      << "Payload size is " << p.get_payload_length() << ".";
  test_logger.assert_error(error_message_2);
}

/*
 New class that implements the Gcs_message_stage to test the multiple
 stages.
 */
class Gcs_new_stage_1 : public Gcs_message_stage {
 public:
  explicit Gcs_new_stage_1() : m_id(std::rand()) {}

  virtual ~Gcs_new_stage_1() {}

  virtual stage_code get_stage_code() const { return my_stage_code(); }

  static stage_code my_stage_code() { return static_cast<stage_code>(10); }

 private:
  int m_id{0};

  static const unsigned short MESSAGE_ID_SIZE{8};

  int64_t get_id() { return m_id; }

  virtual stage_status skip_apply(const Gcs_packet &packet) const {
    bool result = (packet.get_payload_length() != 0);
    return result ? stage_status::apply : stage_status::abort;
  }

  virtual stage_status skip_revert(const Gcs_packet &packet) const {
    bool result = (packet.get_payload_length() != 0);
    return result ? stage_status::apply : stage_status::abort;
  }

  virtual unsigned long long calculate_payload_length(
      Gcs_packet &packet) const {
    return packet.get_payload_length() + MESSAGE_ID_SIZE;
  }

  virtual std::pair<bool, unsigned long long> transform_payload_apply(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length) {
    assert(version == version);
    assert(new_payload_length == (old_payload_length + MESSAGE_ID_SIZE));

    int64_t id = htole64(get_id());
    memcpy(new_payload_ptr, &id, MESSAGE_ID_SIZE);

    memcpy(new_payload_ptr + MESSAGE_ID_SIZE, old_payload_ptr,
           old_payload_length);

    return std::make_pair(false, new_payload_length);
  }

  virtual std::pair<bool, unsigned long long> transform_payload_revert(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length) {
    assert(version == version);
    assert(new_payload_length == (old_payload_length - MESSAGE_ID_SIZE));

    int64_t id = 0;
    memcpy(&id, old_payload_ptr, MESSAGE_ID_SIZE);
    id = le64toh(id);
    assert(get_id() == id);

    memcpy(new_payload_ptr, old_payload_ptr + MESSAGE_ID_SIZE,
           new_payload_length);

    return std::make_pair(false, new_payload_length);
  }
};

const unsigned short Gcs_new_stage_1::MESSAGE_ID_SIZE;

/*
 New class that implements the Gcs_message_stage to test the multiple
 stages.
 */
class Gcs_new_stage_2 : public Gcs_new_stage_1 {
 public:
  explicit Gcs_new_stage_2() {}

  virtual ~Gcs_new_stage_2() {}

  virtual stage_code get_stage_code() const { return my_stage_code(); }

  static stage_code my_stage_code() { return static_cast<stage_code>(11); }
};

class Gcs_new_stage_3 : public Gcs_new_stage_1 {
 public:
  explicit Gcs_new_stage_3() {}

  virtual ~Gcs_new_stage_3() {}

  virtual stage_code get_stage_code() const { return my_stage_code(); }

  static stage_code my_stage_code() { return static_cast<stage_code>(12); }
};

class XcomMultipleStagesTest : public GcsBaseTest {
 protected:
  XcomMultipleStagesTest(){};

  virtual void SetUp() {}

  virtual void TearDown() {}

 public:
  Gcs_message_pipeline pipeline{};
};

TEST_F(XcomMultipleStagesTest, MultipleStagesCheckConfigure) {
  /*
   The following configuration is perfectly fine as all stages have
   different type codes but it will fail because none of the stages
   were registered.
   */
  ASSERT_EQ(
      pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                                  {2, {Gcs_new_stage_2::my_stage_code()}},
                                  {3, {Gcs_new_stage_3::my_stage_code()}}}),
      true);

  pipeline.register_stage<Gcs_new_stage_1>();

  /*
   The following configuration is perfectly fine as all stages have
   different type codes but it will fail because there are stages
   that were not registered.
   */
  ASSERT_EQ(
      pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                                  {2, {Gcs_new_stage_2::my_stage_code()}},
                                  {3, {Gcs_new_stage_3::my_stage_code()}}}),
      true);

  pipeline.register_stage<Gcs_new_stage_1>();
  pipeline.register_stage<Gcs_new_stage_2>();
  pipeline.register_stage<Gcs_new_stage_3>();

  /*
   Handlers were not defined and the configuration should fail.
   */
  ASSERT_EQ(pipeline.register_pipeline({
                {1, {Gcs_new_stage_1::my_stage_code()}},
                {2, {Gcs_message_stage::stage_code::ST_UNKNOWN}},
            }),
            true);

  /*
   There are handlers with the same type code in different pipeline
   versions.
   */
  ASSERT_EQ(pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                                        {2,
                                         {Gcs_new_stage_1::my_stage_code(),
                                          Gcs_new_stage_2::my_stage_code()}}}),
            true);

  /*
   The following configuration is perfectly fine as all stages have
   different type codes.
   */
  ASSERT_EQ(
      pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                                  {2, {Gcs_new_stage_2::my_stage_code()}},
                                  {3, {Gcs_new_stage_3::my_stage_code()}}}),
      false);

  /*
   If we want to redefine the pipeline, we have to clean it up first.
   */
  pipeline.cleanup();

  pipeline.register_stage<Gcs_new_stage_1>();
  pipeline.register_stage<Gcs_new_stage_2>();
  pipeline.register_stage<Gcs_new_stage_3>();

  ASSERT_EQ(
      pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                                  {2, {Gcs_new_stage_2::my_stage_code()}},
                                  {3, {Gcs_new_stage_3::my_stage_code()}}}),
      false);
}

TEST_F(XcomMultipleStagesTest, MultipleStagesCheckVersion) {
  pipeline.register_stage<Gcs_new_stage_1>();
  pipeline.register_stage<Gcs_new_stage_2>();
  pipeline.register_stage<Gcs_message_stage_lz4>();

  /*
   Configure the pipeline with the set of supported versions.
   */
  pipeline.register_pipeline({{1, {Gcs_new_stage_1::my_stage_code()}},
                              {2, {Gcs_new_stage_2::my_stage_code()}},
                              {3, {Gcs_message_stage::stage_code::ST_LZ4}}});
  /*
   Check properties when the different versions are set up and they are
   increasing.
   */
  std::vector<int> requested_inc_versions = {0, 1, 2, 3, 5};
  std::vector<int> configured_inc_versions = {1, 1, 2, 3, 3};
  std::vector<int> configured_inc_success = {false, true, true, true, false};
  std::vector<int> outcome_inc_versions{};
  std::vector<bool> outcome_inc_success{};
  for (const auto &version : requested_inc_versions) {
    /*
     Setting the protocol version to be used.
     */
    outcome_inc_success.push_back(!pipeline.set_version(version));
    outcome_inc_versions.push_back(pipeline.get_version());
  }
  ASSERT_EQ(
      std::equal(configured_inc_versions.begin(), configured_inc_versions.end(),
                 outcome_inc_versions.begin()),
      true);
  ASSERT_EQ(
      std::equal(configured_inc_success.begin(), configured_inc_success.end(),
                 outcome_inc_success.begin()),
      true);

  /*
   Check properties when the different versions are set up and they are
   increasing.
   */
  std::vector<int> requested_dec_versions = {5, 3, 2, 1};
  std::vector<int> configured_dec_versions = {3, 3, 2, 1};
  std::vector<int> configured_dec_success = {false, true, true, true};
  std::vector<int> outcome_dec_versions{};
  std::vector<bool> outcome_dec_success{};
  for (const auto &version : requested_dec_versions) {
    /*
     Setting the protocol version to be used.
     */
    outcome_dec_success.push_back(!pipeline.set_version(version));
    outcome_dec_versions.push_back(pipeline.get_version());
  }
  ASSERT_EQ(
      std::equal(configured_dec_versions.begin(), configured_dec_versions.end(),
                 outcome_dec_versions.begin()),
      true);
  ASSERT_EQ(
      std::equal(configured_dec_success.begin(), configured_dec_success.end(),
                 outcome_dec_success.begin()),
      true);
}

TEST_F(XcomMultipleStagesTest, MultipleStagesCheckData) {
  /*
   Configure the pipeline with the set of supported versions.
   */
  pipeline.register_stage<Gcs_new_stage_1>();
  pipeline.register_stage<Gcs_new_stage_2>();
  pipeline.register_stage<Gcs_new_stage_3>();
  pipeline.register_stage<Gcs_message_stage_lz4>();

  std::string sent_message("Message in a bottle.");
  pipeline.register_pipeline(
      {{1,
        {Gcs_new_stage_1::my_stage_code(), Gcs_new_stage_2::my_stage_code()}},
       {3,
        {Gcs_new_stage_3::my_stage_code(),
         Gcs_message_stage::stage_code::ST_LZ4}}});
  /*
   Check properties when the different versions are set up.
   */
  std::vector<int> requested_versions = {1, 3};
  for (const auto &version : requested_versions) {
    /*
     Setting the protocol version to be used.
     */
    pipeline.set_version(version);

    /*
     Calculate sizes of different bits and pieces.
     */
    unsigned long long payload_size = sent_message.size() + 1;
    unsigned long long dynamic_headers_size = 0;

    /*
     Set up the header information to be used in what follows.
     */
    Gcs_internal_message_header header;
    header.set_version(Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION);
    header.set_payload_length(payload_size);
    header.set_dynamic_headers_length(dynamic_headers_size);
    header.set_cargo_type(
        Gcs_internal_message_header::cargo_type::CT_USER_DATA);

    /*
     Set up the packet and copy the payload content: "Message in a bottle".
     */
    Gcs_packet packet(header);
    memcpy(packet.get_payload(), sent_message.c_str(), sent_message.size() + 1);

    /*
     Traverse all the stages and get an updated packet ready to be
     sent through the network.
     */
    ASSERT_EQ(pipeline.outgoing(header, packet), false);

    /*
     Traverse all the stages and get an updated packet ready to be
     consumed by the application
     */
    ASSERT_EQ(pipeline.incoming(packet), false);

    /*
     Check the payload content.
     */
    std::string received_message{
        reinterpret_cast<char *>(packet.get_payload())};
    ASSERT_EQ(sent_message.compare(received_message), 0);

    /*
     Free any created buffer.
     */
    packet.free_buffer();
  }
}
}  // namespace gcs_xcom_stages_unittest
