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

#include "gcs_interface.h"
#include "gcs_corosync_interface.h"
#include <iostream>

#include <ctime>

using ::testing::_;

namespace gcs_corosync_integratedtest
{

  class mock_gcs_control_event_listener : public Gcs_control_event_listener
  {
  public:
    MOCK_METHOD1(on_view_changed,
                 void(Gcs_view *new_view));
  } ;

  class mock_gcs_communication_event_listener :
  public Gcs_communication_event_listener
  {
  public:
    MOCK_METHOD1(on_message_received, void(Gcs_message &message));
  } ;

  class CorosyncInterfaceTest : public ::testing::Test
  {
  protected:

    CorosyncInterfaceTest()
    {
    };

    virtual void SetUp()
    {
      mock_ev_listener= new mock_gcs_control_event_listener();
      mock_msg_listener= new mock_gcs_communication_event_listener();

      group_id= new Gcs_group_identifier("groupname");
      corosync_if= Gcs_corosync_interface::get_interface();
    }

    virtual void TearDown()
    {
      delete group_id;
      delete mock_ev_listener;
      delete mock_msg_listener;
    }

    mock_gcs_control_event_listener *mock_ev_listener;
    mock_gcs_communication_event_listener *mock_msg_listener;
    Gcs_group_identifier* group_id;
    Gcs_interface* corosync_if;
  } ;

  /**
    This is an integration test. It is meant to run in a machine that has
    Corosync installed and running.
   */
  TEST_F(CorosyncInterfaceTest, IntegrationTest)
  {
    /*
    Set Expectations

    In the future, consider augmenting of this mock in order for it to
    wait on the arrival of the view or the message to proceed, instead of
    sleeping a determined number of seconds.
     */
    EXPECT_CALL(*mock_ev_listener, on_view_changed(_))
            .Times(2);

    EXPECT_CALL(*mock_msg_listener, on_message_received(_))
            .Times(1);

    corosync_if->initialize();

    Gcs_control_interface* control_if=
            corosync_if->get_control_session(*group_id);
    ASSERT_TRUE(control_if != NULL);

    Gcs_communication_interface* comm_if=
            corosync_if->get_communication_session(*group_id);
    ASSERT_TRUE(comm_if != NULL);

    int ev_listener_ref= control_if->add_event_listener(mock_ev_listener);
    int msg_listener_ref= comm_if->add_event_listener(mock_msg_listener);

    bool join_error= control_if->join();
    ASSERT_FALSE(join_error);

    sleep(4);

    ASSERT_TRUE(control_if->belongs_to_group());

    Gcs_view* join_view= control_if->get_current_view();

    ASSERT_TRUE(join_view != NULL);

    Gcs_view_identifier* join_view_id = join_view->get_view_id();

    ASSERT_EQ(typeid(Gcs_corosync_view_identifier),
              typeid(*join_view_id));

    Gcs_corosync_view_identifier* corosync_view_id
                     = static_cast<Gcs_corosync_view_identifier*>
                                                                 (join_view_id);

    ASSERT_EQ(1, corosync_view_id->get_monotonic_part());
    ASSERT_EQ((size_t) 1, join_view->get_members()->size());

    Gcs_member_identifier* member_id= control_if->get_local_information();
    ASSERT_TRUE(member_id != NULL);

    Gcs_message *to_send= new Gcs_message(*control_if->get_local_information(),
                                          *group_id,
                                          (gcs_message_delivery_guarantee) 0);

    string test_header("header");
    string test_payload("payload");

    to_send->append_to_header((uchar*) test_header.c_str(),
                              test_header.length());

    to_send->append_to_payload((uchar*) test_payload.c_str(),
                               test_payload.length());

    bool message_error= comm_if->send_message(to_send);

    ASSERT_FALSE(message_error);

    sleep(2);

    bool leave_error= control_if->leave();

    ASSERT_FALSE(leave_error);

    sleep(2);

    join_view= control_if->get_current_view();

    ASSERT_TRUE(join_view != NULL);

    join_view_id = join_view->get_view_id();

    ASSERT_TRUE(join_view_id != NULL);

    ASSERT_EQ(typeid(Gcs_corosync_view_identifier),
              typeid(*join_view_id));

    corosync_view_id = static_cast<Gcs_corosync_view_identifier*>
                                                                 (join_view_id);

    ASSERT_EQ(2, corosync_view_id->get_monotonic_part());
    ASSERT_EQ((size_t) 0, join_view->get_members()->size());

    control_if->remove_event_listener(ev_listener_ref);
    comm_if->remove_event_listener(msg_listener_ref);

    bool finalize_error= corosync_if->finalize();

    ASSERT_FALSE(finalize_error);
  }
}
