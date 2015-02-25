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

#include "gcs_corosync_control_interface.h"
#include "gcs_group_identifier.h"
#include "gcs_corosync_utils.h"

using ::testing::SetArgPointee;
using ::testing::Return;
using ::testing::_;

using std::string;

namespace gcs_corosync_control_unittest {

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

class mock_gcs_corosync_state_exchange_interface
                      : public Gcs_corosync_state_exchange_interface {
 public:
  MOCK_METHOD0(init,
      void());
  MOCK_METHOD0(reset,
      void());
  MOCK_METHOD10(state_exchange,
      bool(const cpg_address *total, size_t total_entries,
           const struct cpg_address *left, size_t left_entries,
           const struct cpg_address *joined, size_t joined_entries,
           string* group, vector<uchar> *data, Gcs_view* current_view,
           Gcs_member_identifier* local_info));
  MOCK_METHOD2(process_member_state,
      bool(Member_state *ms_info, Gcs_member_identifier p_id));
  MOCK_METHOD1(is_state_exchange_message,
      bool(Gcs_message *to_verify));
  MOCK_METHOD0(get_new_view_id,
      Gcs_corosync_view_identifier*());
  MOCK_METHOD0(get_joined,
      set<Gcs_member_identifier*>*());
  MOCK_METHOD0(get_left,
      set<Gcs_member_identifier*>*());
  MOCK_METHOD0(get_total,
      set<Gcs_member_identifier*>*());
  MOCK_METHOD0(get_group,
      string*());
};

class mock_gcs_corosync_control_proxy : public Gcs_corosync_control_proxy
{
 public:
  MOCK_METHOD2(cpg_join,
      cs_error_t(cpg_handle_t handle, const struct cpg_name *group));
  MOCK_METHOD2(cpg_leave,
      cs_error_t(cpg_handle_t handle, const struct cpg_name *group));
  MOCK_METHOD2(cpg_local_get,
      cs_error_t(cpg_handle_t handle, unsigned int *local_nodeid));
};

class mock_gcs_control_event_listener : public Gcs_control_event_listener
{
 public:
  MOCK_METHOD1(on_view_changed,
               void(Gcs_view *new_view));
};

class mock_gcs_control_data_exchange_event_listener :
                        public Gcs_control_data_exchange_event_listener {
 public:
  MOCK_METHOD1(on_data,
               int(vector<uchar>* exchanged_data));
};

class CorosyncControlTest : public ::testing::Test
{
protected:
  CorosyncControlTest() { };

  virtual void SetUp()
  {
    mock_ev_listener= new mock_gcs_control_event_listener();
    mock_se= new mock_gcs_corosync_state_exchange_interface();
    mock_vce= new mock_gcs_corosync_view_change_control_interface();

    handle= 1;
    group_id= new Gcs_group_identifier("group");
    corosync_control_if= new Gcs_corosync_control(handle,
                                                  *group_id,
                                                  &control_proxy,
                                                  mock_se,
                                                  mock_vce);
  }

  virtual void TearDown()
  {
    delete group_id;
    delete corosync_control_if;
    delete mock_se;
    delete mock_vce;
    delete mock_ev_listener;
  }

  cpg_handle_t handle;
  Gcs_group_identifier* group_id;
  Gcs_corosync_control* corosync_control_if;
  mock_gcs_corosync_control_proxy control_proxy;
  mock_gcs_control_event_listener *mock_ev_listener;
  mock_gcs_corosync_state_exchange_interface *mock_se;
  mock_gcs_corosync_view_change_control_interface *mock_vce;
};

TEST_F(CorosyncControlTest, JoinTest)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_join(_,_))
             .Times(1)
             .WillOnce(Return(CS_OK));

  bool result= corosync_control_if->join();

  ASSERT_FALSE(corosync_control_if->belongs_to_group());
  ASSERT_FALSE(result);
}

TEST_F(CorosyncControlTest, JoinTestWithRetryAndSucess)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_join(_,_))
             .Times(2)
             .WillOnce(Return(CS_ERR_TRY_AGAIN))
             .WillOnce(Return(CS_OK));

  bool result= corosync_control_if->join();

  ASSERT_FALSE(corosync_control_if->belongs_to_group());
  ASSERT_FALSE(result);
}

TEST_F(CorosyncControlTest, JoinTestWithRetryAndFail)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_join(_,_))
             .Times(3)
             .WillRepeatedly(Return (CS_ERR_TRY_AGAIN));

  bool result= corosync_control_if->join();

  ASSERT_FALSE(corosync_control_if->belongs_to_group());
  ASSERT_TRUE(result);
}

TEST_F(CorosyncControlTest, LeaveTest)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_leave(_,_))
             .Times(1)
             .WillOnce(Return(CS_OK));

  bool result= corosync_control_if->leave();

  ASSERT_FALSE(corosync_control_if->belongs_to_group());
  ASSERT_FALSE(result);
}

TEST_F(CorosyncControlTest, GetLocalInformationTest)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_local_get(_,_))
             .Times(1)
             .WillOnce(DoAll(SetArgPointee<1>(28),
                             Return(CS_OK)));

  Gcs_member_identifier *result= corosync_control_if->get_local_information();

  ASSERT_TRUE(result != NULL);
  ASSERT_NE(string::npos, result->get_member_id()->find("28"));
}

TEST_F(CorosyncControlTest, GetLocalInformationErrorTest)
{
  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_local_get(_,_))
             .WillRepeatedly(DoAll(SetArgPointee<1>(0),
                                   Return(CS_ERR_TRY_AGAIN)));

  Gcs_member_identifier *result= corosync_control_if->get_local_information();

  ASSERT_TRUE(result == NULL);
}

Gcs_message* create_state_exchange_msg(int local_id,
                                       string* grp_id)
{
  uchar* header_buffer= new uchar[STATE_EXCHANGE_HEADER_CODE_LENGTH];
  memcpy(header_buffer,
         &Gcs_corosync_state_exchange::state_exchange_header_code,
         STATE_EXCHANGE_HEADER_CODE_LENGTH);

  vector<uchar> *dummy= new vector<uchar>();
  dummy->push_back((uchar)1);
  dummy->push_back((uchar)1);
  dummy->push_back((uchar)1);

  Gcs_corosync_view_identifier* view_id
           = new Gcs_corosync_view_identifier(999999,
                                                         1);
  Member_state member_state(view_id,
                            dummy);

  vector<uchar> encoded_state;
  member_state.encode(&encoded_state);

  Gcs_group_identifier group_id(*grp_id);

  Gcs_member_identifier* member_id=
                Gcs_corosync_utils::build_corosync_member_id(local_id,getpid());

  Gcs_message *msg= new Gcs_message(*member_id,
                                    group_id,
                                    UNIFORM);

  msg->append_to_header(header_buffer, STATE_EXCHANGE_HEADER_CODE_LENGTH);
  msg->append_to_payload(&(encoded_state.front()),
                        encoded_state.size());

  delete dummy;
  delete member_id;
  delete view_id;

  return msg;
}

TEST_F(CorosyncControlTest, ViewChangedJoiningTest)
{
  //Common unit test data
  string group_name_str("a");
  int node1= 28, node2= 29;
  Gcs_corosync_view_identifier* view_id
                   = new Gcs_corosync_view_identifier(999999, 27);
  int pid= getpid();

  Gcs_member_identifier* node1_member_id=
                 Gcs_corosync_utils::build_corosync_member_id(node1, getpid());

  Gcs_member_identifier* node2_member_id=
                 Gcs_corosync_utils::build_corosync_member_id(node2, getpid());

  set<Gcs_member_identifier*>* total_set= new set<Gcs_member_identifier*>();
  set<Gcs_member_identifier*>* join_set= new set<Gcs_member_identifier*>();
  set<Gcs_member_identifier*>* left_set= new set<Gcs_member_identifier*>();

  total_set->insert(node1_member_id);
  total_set->insert(node2_member_id);

  join_set->insert(node2_member_id);

  //Setting Expectations and Return Values
  EXPECT_CALL(control_proxy, cpg_local_get(_,_))
             .Times(1)
             .WillOnce(DoAll(SetArgPointee<1>(28),
                             Return(CS_OK)));

  //Set Expectations...
  EXPECT_CALL(*mock_ev_listener, on_view_changed(_))
             .Times(1);

  EXPECT_CALL(*mock_se, state_exchange(_,_,_,_,_,_,_,_,_,_))
             .Times(1)
             .WillOnce(Return(false));

  EXPECT_CALL(*mock_se, is_state_exchange_message(_))
             .Times(2)
             .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_se, process_member_state(_,_))
             .Times(2)
             .WillOnce(Return(false))
             .WillOnce(Return(true));

  EXPECT_CALL(*mock_se, get_new_view_id())
             .Times(1)
             .WillOnce(Return(view_id));

  EXPECT_CALL(*mock_se, get_group())
             .Times(1)
             .WillOnce(Return(&group_name_str));

  EXPECT_CALL(*mock_se, get_joined())
             .Times(1)
             .WillOnce(Return(join_set));

  EXPECT_CALL(*mock_se, get_left())
             .Times(1)
             .WillOnce(Return(left_set));

  EXPECT_CALL(*mock_se, get_total())
             .Times(1)
             .WillOnce(Return(total_set));

  EXPECT_CALL(*mock_vce, start_view_exchange())
             .Times(1);

  EXPECT_CALL(*mock_vce, end_view_exchange())
             .Times(1);

  cpg_name group_name;
  group_name.length= 1;
  group_name.value[0]= group_name_str.c_str()[0];

  size_t total_members_size= 2;

  cpg_address total_members[2];
  total_members[0].nodeid= node1;
  total_members[0].pid= pid;
  total_members[0].reason= 1;

  total_members[1].nodeid= node2;
  total_members[1].pid= pid;
  total_members[1].reason= 1;

  size_t joined_members_size= 1;
  cpg_address joined_members[1];
  joined_members[0].nodeid= node2;
  joined_members[0].pid= pid;
  joined_members[0].reason= 1;

  size_t left_members_size= 0;
  cpg_address left_members[0];

  corosync_control_if->add_event_listener(mock_ev_listener);

  ASSERT_FALSE(corosync_control_if->belongs_to_group());
  ASSERT_TRUE(corosync_control_if->get_current_view() == NULL);

  corosync_control_if->view_changed(&group_name,
                                    total_members, total_members_size,
                                    left_members, left_members_size,
                                    joined_members, joined_members_size);

  Gcs_message* state_message1=
                  create_state_exchange_msg(node1, &group_name_str);
  Gcs_message* state_message2=
                  create_state_exchange_msg(node2, &group_name_str);

  corosync_control_if->process_possible_control_message(state_message1);
  corosync_control_if->process_possible_control_message(state_message2);

  ASSERT_TRUE(corosync_control_if->belongs_to_group());
  ASSERT_TRUE(corosync_control_if->get_current_view() != NULL);

  Gcs_view_identifier* current_view_id=
                        corosync_control_if->get_current_view()->get_view_id();

  ASSERT_EQ(typeid(Gcs_corosync_view_identifier),
            typeid(*current_view_id));

  Gcs_corosync_view_identifier* corosync_view_id
                    = static_cast<Gcs_corosync_view_identifier*>
                                                              (current_view_id);

  ASSERT_EQ(view_id->get_fixed_part(),
            corosync_view_id->get_fixed_part());

  ASSERT_EQ(view_id->get_monotonic_part() + 1,
            corosync_view_id->get_monotonic_part());

  ASSERT_EQ((size_t)2,
            corosync_control_if->get_current_view()->get_members()->size());

  ASSERT_EQ((size_t)1,
            corosync_control_if->get_current_view()
                               ->get_joined_members()->size());

  delete view_id;
}

TEST_F(CorosyncControlTest, SetEventListenerTest)
{
  Gcs_control_event_listener* control_listener
                    = new mock_gcs_control_event_listener();

  int reference= corosync_control_if->add_event_listener(control_listener);

  ASSERT_NE(0, reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->size());

  delete control_listener;
}

TEST_F(CorosyncControlTest, SetEventListenersTest)
{
  Gcs_control_event_listener* control_listener
                    = new mock_gcs_control_event_listener();

  Gcs_control_event_listener* another_control_listener
                          = new mock_gcs_control_event_listener();

  int reference= corosync_control_if->add_event_listener(control_listener);
  int another_reference= corosync_control_if->add_event_listener
                                                  (another_control_listener);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)2,
            corosync_control_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete control_listener;
  delete another_control_listener;
}

TEST_F(CorosyncControlTest, RemoveEventListenerTest)
{
  Gcs_control_event_listener* control_listener
                    = new mock_gcs_control_event_listener();

  Gcs_control_event_listener* another_control_listener
                          = new mock_gcs_control_event_listener();

  int reference= corosync_control_if->add_event_listener(control_listener);
  int another_reference= corosync_control_if->add_event_listener
                                                  (another_control_listener);

  corosync_control_if->remove_event_listener(reference);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)0,
            corosync_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete control_listener;
  delete another_control_listener;
}

TEST_F(CorosyncControlTest, SetDataExchangeListenerTest)
{
  Gcs_control_data_exchange_event_listener* data_listener
                    = new mock_gcs_control_data_exchange_event_listener();

  int reference= corosync_control_if
                            ->add_data_exchange_event_listener(data_listener);

  ASSERT_NE(0, reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()->size());

  delete data_listener;
}

TEST_F(CorosyncControlTest, SetDataExchangeListenersTest)
{
  Gcs_control_data_exchange_event_listener* data_listener
                    = new mock_gcs_control_data_exchange_event_listener();

  Gcs_control_data_exchange_event_listener* another_data_listener
                          = new mock_gcs_control_data_exchange_event_listener();

  int reference= corosync_control_if
                        ->add_data_exchange_event_listener(data_listener);
  int another_reference= corosync_control_if->add_data_exchange_event_listener
                                                  (another_data_listener);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()
                                                  ->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()
                                                  ->count(another_reference));
  ASSERT_EQ((long unsigned int)2,
            corosync_control_if->get_data_exchange_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete data_listener;
  delete another_data_listener;
}

TEST_F(CorosyncControlTest, RemoveDataExchangeListenerTest)
{
  Gcs_control_data_exchange_event_listener* data_listener
                    = new mock_gcs_control_data_exchange_event_listener();

  Gcs_control_data_exchange_event_listener* another_data_listener
                          = new mock_gcs_control_data_exchange_event_listener();

  int reference= corosync_control_if
                        ->add_data_exchange_event_listener(data_listener);
  int another_reference= corosync_control_if->add_data_exchange_event_listener
                                                        (another_data_listener);

  corosync_control_if->remove_data_exchange_event_listener(reference);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)0,
            corosync_control_if->get_data_exchange_listeners()
                                                  ->count(reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()
                                                  ->count(another_reference));
  ASSERT_EQ((long unsigned int)1,
            corosync_control_if->get_data_exchange_listeners()->size());
  ASSERT_NE(reference, another_reference);

  delete data_listener;
  delete another_data_listener;
}

}
