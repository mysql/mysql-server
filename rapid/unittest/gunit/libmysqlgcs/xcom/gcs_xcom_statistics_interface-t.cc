/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_statistics_interface.h"

namespace gcs_xcom_statistics_unittest
{

class XcomStatisticsTest : public ::testing::Test
{
protected:

  XcomStatisticsTest() {};


  virtual void SetUp()
  {
    xcom_stats_if= new Gcs_xcom_statistics();
  }


  virtual void TearDown()
  {
    delete xcom_stats_if;
  }


  Gcs_xcom_statistics *xcom_stats_if;
};


TEST_F(XcomStatisticsTest, UpdateMessageSentTest)
{
  long message_length= 1000;

  xcom_stats_if->update_message_sent(message_length);

  ASSERT_EQ(message_length, xcom_stats_if->get_total_bytes_sent());
  ASSERT_EQ(1, xcom_stats_if->get_total_messages_sent());
}


TEST_F(XcomStatisticsTest, UpdateMessagesSentTest)
{
  long message_length= 1000;

  xcom_stats_if->update_message_sent(message_length);
  xcom_stats_if->update_message_sent(message_length);

  EXPECT_EQ(message_length * 2, xcom_stats_if->get_total_bytes_sent());
  EXPECT_EQ(2, xcom_stats_if->get_total_messages_sent());
}


TEST_F(XcomStatisticsTest, UpdateMessageReceivedTest)
{
  long message_length= 1000;

  xcom_stats_if->update_message_received(message_length);

  EXPECT_EQ(message_length, xcom_stats_if->get_total_bytes_received());
  EXPECT_EQ(1, xcom_stats_if->get_total_messages_received());
  EXPECT_GE(time(0), xcom_stats_if->get_last_message_timestamp());
  EXPECT_EQ(message_length, xcom_stats_if->get_max_message_length());
  EXPECT_EQ(message_length, xcom_stats_if->get_min_message_length());
}


TEST_F(XcomStatisticsTest, UpdateMessagesReceivedTest)
{
  long message_length_big=   1000;
  long message_length_small= 1000;

  xcom_stats_if->update_message_received(message_length_big);
  xcom_stats_if->update_message_received(message_length_small);

  EXPECT_EQ(message_length_big + message_length_small,
            xcom_stats_if->get_total_bytes_received());

  EXPECT_EQ(2, xcom_stats_if->get_total_messages_received());
  EXPECT_GE(time(0), xcom_stats_if->get_last_message_timestamp());
  EXPECT_EQ(message_length_big, xcom_stats_if->get_max_message_length());
  EXPECT_EQ(message_length_small, xcom_stats_if->get_min_message_length());
}

}
