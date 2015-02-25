/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "gcs_message.h"
#include "gcs_group_identifier.h"
#include "gcs_member_identifier.h"

#include <vector>

using std::vector;

namespace gcs_message_unittest {

class MessageTest : public ::testing::Test
{
protected:
  MessageTest() {};

  virtual void SetUp()
  {
    member_id= new Gcs_member_identifier(string("member"));
    group_id= new Gcs_group_identifier(string("group"));

    message= new Gcs_message(*member_id, *group_id, NO_ORDER);
  }

  virtual void TearDown()
  {
    delete member_id;
    delete group_id;
    delete message;
  }

  Gcs_member_identifier* member_id;
  Gcs_group_identifier *group_id;
  Gcs_message* message;
};

TEST_F(MessageTest, AppendtoHeaderTest)
{
  string test_data("to_append");

  message->append_to_header((uchar*)test_data.c_str(),
                            test_data.length());

  EXPECT_EQ(test_data.length(), message->get_header_length());
  EXPECT_EQ((size_t)0, message->get_payload_length());
}

TEST_F(MessageTest, AppendtoPayloadTest)
{
  string test_data("to_append");

  message->append_to_payload((uchar*)test_data.c_str(),
                             test_data.length());

  EXPECT_EQ(test_data.length(), message->get_payload_length());
  EXPECT_EQ((size_t)0, message->get_header_length());

}

TEST_F(MessageTest, EncodeTest)
{
  string test_header("header");
  string test_payload("payload");

  message->append_to_header((uchar*)test_header.c_str(),
                             test_header.length());

  message->append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length());

  vector<uchar> *result= message->encode();

  EXPECT_TRUE(result != NULL);

  EXPECT_EQ( GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE + ( 2*GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH )
             + test_header.length() + test_payload.length(),
            result->size());
}

TEST_F(MessageTest, DecodeTest)
{
  string test_header("header");
  string test_payload("payload");

  message->append_to_header((uchar*)test_header.c_str(),
                             test_header.length() + 1);

  message->append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length() + 1);

  vector<uchar> *result= message->encode();

  EXPECT_TRUE(result != NULL);
  EXPECT_EQ( GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE + (2*GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH)
              + test_header.length() + test_payload.length() + 2,
             result->size());

  Gcs_message to_decode(*member_id, *group_id, (gcs_message_delivery_guarantee)0);

  uchar* data_ptr= &result->front();

  to_decode.decode(data_ptr, result->size());

  EXPECT_EQ(test_header.length() + 1, to_decode.get_header_length());
  EXPECT_EQ(test_payload.length() + 1, to_decode.get_payload_length());

  string returned_header((char*)to_decode.get_header());
  EXPECT_EQ(test_header, returned_header);

  string returned_payload((char*)to_decode.get_payload());
  EXPECT_EQ(test_payload, returned_payload);
}

}