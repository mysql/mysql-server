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

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gcs_message.h"

#include "gcs_corosync_statistics_interface.h"
#include "gcs_corosync_communication_interface.h"

using ::testing::Return;
using ::testing::_;

namespace gcs_corosync_communication_unittest {

class mock_gcs_corosync_view_change_control_interface
                          : public Gcs_corosync_view_change_control_interface {
 public:
  MOCK_METHOD0(start_view_exchange,
      void());
  MOCK_METHOD0(end_view_exchange,
      void());
  MOCK_METHOD0(wait_for_view_change_end,
      void());
};

class mock_gcs_corosync_statistics_updater :
                              public Gcs_corosync_statistics_updater {
 public:
  MOCK_METHOD1(update_message_sent, void(long message_lenght));
  MOCK_METHOD1(update_message_received, void(long message_lenght));
};

class mock_gcs_communication_event_listener :
                              public Gcs_communication_event_listener {
 public:
  MOCK_METHOD1(on_message_received, void(Gcs_message &message));
};

class mock_gcs_corosync_communication_proxy :
                              public Gcs_corosync_communication_proxy {
 public:
  MOCK_METHOD4(cpg_mcast_joined,
               cs_error_t(cpg_handle_t handle, cpg_guarantee_t guarantee,
                          const struct iovec *iovec, unsigned int iov_len));
};

class CorosyncCommunicationTest : public ::testing::Test
{
protected:

  virtual void SetUp()
  {
    handle= 1;
    mock_stats= new mock_gcs_corosync_statistics_updater();
    mock_proxy= new mock_gcs_corosync_communication_proxy();
    mock_vce= new mock_gcs_corosync_view_change_control_interface();

    corosync_comm_if= new Gcs_corosync_communication(handle,
                                                     mock_stats,
                                                     mock_proxy,
                                                     mock_vce);
  }

  virtual void TearDown()
  {
    delete mock_stats;
    delete mock_vce;
    delete mock_proxy;
    delete corosync_comm_if;
  }

  Gcs_corosync_communication* corosync_comm_if;
  cpg_handle_t handle;
  mock_gcs_corosync_statistics_updater* mock_stats;
  mock_gcs_corosync_communication_proxy* mock_proxy;
  mock_gcs_corosync_view_change_control_interface* mock_vce;
};

TEST_F(CorosyncCommunicationTest, SetEventListenerTest)
{
  Gcs_communication_event_listener* comm_listener
                    = new mock_gcs_communication_event_listener();

  int reference= corosync_comm_if->add_event_listener(comm_listener);

  ASSERT_NE(0, reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->size());

  delete comm_listener;
}

TEST_F(CorosyncCommunicationTest, SetEventListenersTest)
{
  Gcs_communication_event_listener* comm_listener
                    = new mock_gcs_communication_event_listener();

  Gcs_communication_event_listener* another_comm_listener
                    = new mock_gcs_communication_event_listener();

  int reference= corosync_comm_if->add_event_listener(comm_listener);
  int another_reference= corosync_comm_if->add_event_listener
                                                  (another_comm_listener);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)2,
            corosync_comm_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete comm_listener;
  delete another_comm_listener;
}

TEST_F(CorosyncCommunicationTest, RemoveEventListenerTest)
{
  Gcs_communication_event_listener* comm_listener
                    = new mock_gcs_communication_event_listener();

  Gcs_communication_event_listener* another_comm_listener
                    = new mock_gcs_communication_event_listener();

  int reference= corosync_comm_if->add_event_listener(comm_listener);
  int another_reference= corosync_comm_if->add_event_listener
                                                  (another_comm_listener);

  corosync_comm_if->remove_event_listener(reference);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)0,
            corosync_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_comm_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete comm_listener;
  delete another_comm_listener;
}

TEST_F(CorosyncCommunicationTest, SendMessageTest)
{
  //Test Expectations
  EXPECT_CALL(*mock_proxy, cpg_mcast_joined(_,_,_,_))
             .Times(1)
             .WillOnce(Return (CS_OK));
  EXPECT_CALL(*mock_stats, update_message_sent(_))
              .Times(1);

  EXPECT_CALL(*mock_vce, wait_for_view_change_end())
              .Times(1);

  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");

  Gcs_message message( member_id, group_id,
                       (gcs_message_delivery_guarantee)0);

  string test_header("header");
  string test_payload("payload");

  message.append_to_header((uchar*)test_header.c_str(),
                           test_header.length());

  message.append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length());

  corosync_comm_if->send_message(&message);
}

TEST_F(CorosyncCommunicationTest, SendMessageTestWithRetryAndSuceed)
{
  //Test Expectations
  EXPECT_CALL(*mock_proxy, cpg_mcast_joined(_,_,_,_))
             .Times(2)
             .WillOnce(Return (CS_ERR_TRY_AGAIN))
             .WillOnce(Return (CS_OK));
  EXPECT_CALL(*mock_stats, update_message_sent(_))
              .Times(1);

  EXPECT_CALL(*mock_vce, wait_for_view_change_end())
              .Times(1);

  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");

  Gcs_message message( member_id, group_id,
                       (gcs_message_delivery_guarantee)0);

  string test_header("header");
  string test_payload("payload");

  message.append_to_header((uchar*)test_header.c_str(),
                           test_header.length());

  message.append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length());

  long message_result= corosync_comm_if->send_message(&message);

  ASSERT_EQ(0, message_result);
}

TEST_F(CorosyncCommunicationTest, SendMessageTestWithRetryAndFail)
{
  //Test Expectations
  EXPECT_CALL(*mock_proxy, cpg_mcast_joined(_,_,_,_))
             .Times(3)
             .WillRepeatedly(Return (CS_ERR_TRY_AGAIN));

  EXPECT_CALL(*mock_vce, wait_for_view_change_end())
              .Times(1);

  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");

  Gcs_message message( member_id, group_id,
                       (gcs_message_delivery_guarantee)0);

  string test_header("header");
  string test_payload("payload");

  message.append_to_header((uchar*)test_header.c_str(),
                           test_header.length());

  message.append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length());

  long message_result= corosync_comm_if->send_message(&message);

  ASSERT_EQ(1, message_result);
}

TEST_F(CorosyncCommunicationTest, ReceiveMessageTest)
{
  mock_gcs_communication_event_listener *ev_listener=
                        new mock_gcs_communication_event_listener();
  //Test Expectations
  EXPECT_CALL(*ev_listener, on_message_received(_))
              .Times(1);

  EXPECT_CALL(*mock_stats, update_message_received(_))
              .Times(1);

  //Test
  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");

  Gcs_message message( member_id, group_id,
                       (gcs_message_delivery_guarantee)0 );

  string test_header("header");
  string test_payload("payload");

  message.append_to_header((uchar*)test_header.c_str(),
                             test_header.length());

  message.append_to_payload((uchar*)test_payload.c_str(),
                             test_payload.length());

  vector<uchar> *encoded_message= message.encode();
  uchar* message_data = &encoded_message->front();

  cpg_name group_name;
  group_name.length=1;
  group_name.value[0]= 'a';

  int listener_ref= corosync_comm_if->add_event_listener(ev_listener);
  corosync_comm_if->deliver_message(&group_name,
                                    42,
                                    42,
                                    (void*)message_data,
                                    (size_t)encoded_message->size());

  corosync_comm_if->remove_event_listener(listener_ref);

  delete ev_listener;
}

}
