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
#include <ctime>
#include <gtest/gtest.h>

#include "gcs_corosync_statistics_interface.h"

namespace gcs_corosync_statistics_unittest {

class CorosyncStatisticsTest : public ::testing::Test
{
protected:
  CorosyncStatisticsTest() { };

  virtual void SetUp()
  {
    corosync_stats_if= new Gcs_corosync_statistics();
  }

  virtual void TearDown()
  {
    delete corosync_stats_if;
  }

  Gcs_corosync_statistics* corosync_stats_if;
};

TEST_F(CorosyncStatisticsTest, UpdateMessageSentTest)
{
  long message_length= 1000;

  corosync_stats_if->update_message_sent(message_length);

  ASSERT_EQ(message_length, corosync_stats_if->get_total_bytes_sent());
  ASSERT_EQ(1, corosync_stats_if->get_total_messages_sent());
}

TEST_F(CorosyncStatisticsTest, UpdateMessagesSentTest)
{
  long message_length= 1000;

  corosync_stats_if->update_message_sent(message_length);
  corosync_stats_if->update_message_sent(message_length);

  EXPECT_EQ(message_length*2, corosync_stats_if->get_total_bytes_sent());
  EXPECT_EQ(2, corosync_stats_if->get_total_messages_sent());
}

TEST_F(CorosyncStatisticsTest, UpdateMessageReceivedTest)
{
  long message_length= 1000;

  corosync_stats_if->update_message_received(message_length);

  EXPECT_EQ(message_length, corosync_stats_if->get_total_bytes_received());
  EXPECT_EQ(1, corosync_stats_if->get_total_messages_received());
  EXPECT_GE(time(0), corosync_stats_if->get_last_message_timestamp());
  EXPECT_EQ(message_length, corosync_stats_if->get_max_message_length());
  EXPECT_EQ(message_length, corosync_stats_if->get_min_message_length());
}

TEST_F(CorosyncStatisticsTest, UpdateMessagesReceivedTest)
{
  long message_length_big= 1000;
  long message_length_small= 1000;

  corosync_stats_if->update_message_received(message_length_big);
  corosync_stats_if->update_message_received(message_length_small);

  EXPECT_EQ(message_length_big + message_length_small,
            corosync_stats_if->get_total_bytes_received());

  EXPECT_EQ(2, corosync_stats_if->get_total_messages_received());
  EXPECT_GE(time(0), corosync_stats_if->get_last_message_timestamp());
  EXPECT_EQ(message_length_big, corosync_stats_if->get_max_message_length());
  EXPECT_EQ(message_length_small, corosync_stats_if->get_min_message_length());
}

}
