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

#include "my_config.h"

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gcs_interface.h"
#include "gcs_state_exchange.h"

#include "gcs_control_interface.h"
#include "gcs_communication_interface.h"
#include "gcs_corosync_communication_interface.h"

#include "gcs_corosync_utils.h"

#include <vector>

using ::testing::_;
using std::vector;

namespace gcs_corosync_state_exchange_unittest {

class mock_gcs_control_interface : public Gcs_control_interface {
 public:
  MOCK_METHOD0(join,
      bool());
  MOCK_METHOD0(leave,
      bool());
  MOCK_METHOD0(belongs_to_group,
      bool());
  MOCK_METHOD0(get_current_view,
      Gcs_view*());
  MOCK_METHOD0(get_local_information,
      Gcs_member_identifier*());
  MOCK_METHOD1(add_event_listener,
      int(Gcs_control_event_listener* event_listener));
  MOCK_METHOD1(remove_event_listener,
      void(int event_listener_handle));
  MOCK_METHOD1(set_exchangeable_data,
               void(vector<uchar> *data));
  MOCK_METHOD1(add_data_exchange_event_listener,
      int(Gcs_control_data_exchange_event_listener* event_listener));
  MOCK_METHOD1(remove_data_exchange_event_listener,
      void(int event_listener_handle));
};

class mock_gcs_corosync_communication_interface : public Gcs_corosync_communication_interface {
 public:
  MOCK_METHOD1(send_message,
      bool(Gcs_message *message_to_send));
  MOCK_METHOD1(add_event_listener,
      int(Gcs_communication_event_listener *event_listener));
  MOCK_METHOD1(remove_event_listener,
      void(int event_listener_handle));
  MOCK_METHOD1(send_binding_message,
      long(Gcs_message *message_to_send));
  MOCK_METHOD5(deliver_message,
      void(const struct cpg_name *name, uint32_t nodeid, uint32_t pid, void *data, size_t len));
};

class CorosyncStateExchangeTest : public ::testing::Test
{
protected:
  CorosyncStateExchangeTest() { };

  virtual void SetUp()
  {
    control_mock= new mock_gcs_control_interface();
    comm_mock= new mock_gcs_corosync_communication_interface();

    state_exchange= new Gcs_corosync_state_exchange(comm_mock);
  }

  virtual void TearDown()
  {
    delete control_mock;
    delete comm_mock;
    delete state_exchange;
  }

  Gcs_corosync_state_exchange* state_exchange;
  mock_gcs_control_interface* control_mock;
  mock_gcs_corosync_communication_interface* comm_mock;
};

TEST_F(CorosyncStateExchangeTest, StateExchangeBroadcastJoinerTest){

  //Setting expectations
  EXPECT_CALL(*comm_mock, send_binding_message(_))
             .Times(1);

  //Set up parameters
  size_t total_members_size= 2;

  cpg_address total_members[2];
  total_members[0].nodeid= 28;
  total_members[0].pid= getpid();
  total_members[0].reason= 1;

  total_members[1].nodeid= 29;
  total_members[1].pid= getpid();
  total_members[1].reason= 1;

  size_t joined_members_size= 1;
  cpg_address joined_members[1];
  joined_members[0].nodeid= 28;
  joined_members[0].pid= getpid();
  joined_members[0].reason= 1;

  size_t left_members_size= 0;
  cpg_address left_members[0];

  vector<uchar> data_to_exchange;

  string group_name("group_name");

  Gcs_member_identifier* mi= Gcs_corosync_utils
                            ::build_corosync_member_id(28,
                                                       getpid());

  bool leaving= state_exchange->state_exchange(total_members,
                                               total_members_size,
                                               left_members,
                                               left_members_size,
                                               joined_members,
                                               joined_members_size,
                                               &group_name,
                                               &data_to_exchange,
                                               NULL,
                                               mi);

  ASSERT_FALSE(leaving);

  delete mi;
}

TEST_F(CorosyncStateExchangeTest, StateExchangeProcessStatesPhase){
  //Set up parameters
  size_t total_members_size= 2;

  cpg_address total_members[2];
  total_members[0].nodeid= 28;
  total_members[0].pid= getpid();
  total_members[0].reason= 1;

  total_members[1].nodeid= 29;
  total_members[1].pid= getpid();
  total_members[1].reason= 1;

  size_t joined_members_size= 1;
  cpg_address joined_members[1];
  joined_members[0].nodeid= 28;
  joined_members[0].pid= getpid();
  joined_members[0].reason= CPG_REASON_JOIN;

  size_t left_members_size= 0;
  cpg_address left_members[0];

  vector<uchar> data_to_exchange;

  string group_name("group_name");

  Gcs_member_identifier* mi= Gcs_corosync_utils
                               ::build_corosync_member_id(28,
                                                          getpid());

  state_exchange->state_exchange(total_members, total_members_size,
                                 left_members, left_members_size,
                                 joined_members, joined_members_size,
                                 &group_name,
                                 &data_to_exchange,
                                 NULL,
                                 mi);

  Gcs_corosync_view_identifier* view_id
                      = new Gcs_corosync_view_identifier(99999, 1);

  Member_state *state1= new Member_state(view_id, &data_to_exchange);
  Member_state *state2= new Member_state(view_id, &data_to_exchange);

  Gcs_member_identifier* member_id_1=
                     Gcs_corosync_utils::build_corosync_member_id(28,getpid());
  bool can_install=
      state_exchange->process_member_state(state1,
                                           *member_id_1);

  ASSERT_FALSE(can_install);

  Gcs_member_identifier* member_id_2=
                     Gcs_corosync_utils::build_corosync_member_id(29,getpid());
  can_install=
      state_exchange->process_member_state(state2,
                                           *member_id_2);

  ASSERT_TRUE(can_install);

  delete member_id_1;
  delete member_id_2;
  delete view_id;
  delete mi;
}

}
