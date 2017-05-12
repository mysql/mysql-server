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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gcs_xcom_utils.h"
#include "test_logger.h"

#include <vector>
#include <string>

using ::testing::_;
using ::testing::Return;

namespace gcs_xcom_utils_unittest
{
TEST(GcsXComUtils, BuildXcomMemberIdSmokeTest)
{
  std::string peer_nodes_str("127.0.0.1:12345,127.0.0.1:12346");
  std::vector<std::string> processed_peers;
  Gcs_xcom_utils::process_peer_nodes(&peer_nodes_str,
                                     processed_peers);

  ASSERT_EQ((unsigned)2, processed_peers.size());
  ASSERT_STREQ("127.0.0.1:12345", processed_peers.at(0).c_str());
  ASSERT_STREQ("127.0.0.1:12346", processed_peers.at(1).c_str());
}

TEST(GcsXComUtils, BuildXcomMemberIdSpacesAtBegin)
{
  std::string peer_nodes_str("    127.0.0.1:12345,127.0.0.1:12346");
  std::vector<std::string> processed_peers;
  Gcs_xcom_utils::process_peer_nodes(&peer_nodes_str,
                                     processed_peers);

  ASSERT_EQ((unsigned)2, processed_peers.size());
  ASSERT_STREQ("127.0.0.1:12345", processed_peers.at(0).c_str());
  ASSERT_STREQ("127.0.0.1:12346", processed_peers.at(1).c_str());
}

TEST(GcsXComUtils, BuildXcomMemberIdSpacesAtBeginAndEnd)
{
  std::string peer_nodes_str("    127.0.0.1:12345,127.0.0.1:12346      ");
  std::vector<std::string> processed_peers;
  Gcs_xcom_utils::process_peer_nodes(&peer_nodes_str,
                                     processed_peers);

  ASSERT_EQ((unsigned)2, processed_peers.size());
  ASSERT_STREQ("127.0.0.1:12345", processed_peers.at(0).c_str());
  ASSERT_STREQ("127.0.0.1:12346", processed_peers.at(1).c_str());
}

TEST(GcsXComUtils, BuildXcomMemberIdSpacesSpacesEverywhere)
{
  std::string peer_nodes_str(" 127.0.  0.1:12   345,    127.0   .0.1:12346   ");
  std::vector<std::string> processed_peers;
  Gcs_xcom_utils::process_peer_nodes(&peer_nodes_str,
                                     processed_peers);

  ASSERT_EQ((unsigned)2, processed_peers.size());
  ASSERT_STREQ("127.0.0.1:12345", processed_peers.at(0).c_str());
  ASSERT_STREQ("127.0.0.1:12346", processed_peers.at(1).c_str());
}

TEST(GcsXcomProxyImpl, XcomClientSendDataBiggerThanUINT32)
{
  std::stringstream error_message;
  Gcs_xcom_proxy_impl xcom_proxy;

  /*
    xcom_client_send_data cannot send a message bigger than uint32.
    It should log and error and return non-zero value if the message
    is bigger than uint32.
  */
  test_logger.clear_event();
  int ret= xcom_proxy.xcom_client_send_data(1ULL<<32, NULL);

  ASSERT_NE(ret, 0);

  error_message << "The data is too big. Data length should "
                << "not exceed "
                <<  std::numeric_limits<unsigned int>::max()
                <<  " bytes.";
  test_logger.assert_error(error_message.str());
}

}
