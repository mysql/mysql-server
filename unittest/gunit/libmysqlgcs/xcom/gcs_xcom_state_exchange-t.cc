/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <vector>

#include "gcs_base_test.h"

#include "gcs_xcom_state_exchange.h"
#include "mysql/gcs/gcs_interface.h"

#include "gcs_xcom_communication_interface.h"
#include "mysql/gcs/gcs_communication_interface.h"
#include "mysql/gcs/gcs_control_interface.h"

#include "gcs_internal_message.h"
#include "gcs_xcom_utils.h"

#include "synode_no.h"

namespace gcs_xcom_state_exchange_unittest {

class mock_gcs_control_interface : public Gcs_control_interface {
 public:
  MOCK_METHOD0(join, enum_gcs_error());
  MOCK_METHOD0(leave, enum_gcs_error());
  MOCK_METHOD0(belongs_to_group, bool());
  MOCK_METHOD0(get_current_view, Gcs_view *());
  MOCK_CONST_METHOD0(get_local_member_identifier,
                     const Gcs_member_identifier());
  MOCK_METHOD0(get_minimum_write_concurrency, uint32_t());
  MOCK_METHOD0(get_maximum_write_concurrency, uint32_t());
  MOCK_METHOD1(get_write_concurrency,
               enum_gcs_error(uint32_t &write_concurrency));
  MOCK_METHOD1(set_write_concurrency,
               enum_gcs_error(uint32_t write_concurrency));
  MOCK_METHOD1(add_event_listener,
               int(const Gcs_control_event_listener &event_listener));
  MOCK_METHOD1(remove_event_listener, void(int event_listener_handle));
};

class mock_gcs_xcom_communication_interface
    : public Gcs_xcom_communication_interface {
 public:
  MOCK_METHOD1(send_message,
               enum_gcs_error(const Gcs_message &message_to_send));
  MOCK_METHOD1(add_event_listener,
               int(const Gcs_communication_event_listener &event_listener));
  MOCK_METHOD1(remove_event_listener, void(int event_listener_handle));
  MOCK_METHOD3(send_binding_message,
               enum_gcs_error(const Gcs_message &message_to_send,
                              unsigned long long *message_length,
                              Gcs_internal_message_header::cargo_type type));
  MOCK_METHOD1(xcom_receive_data, bool(Gcs_message *message));
  MOCK_METHOD1(buffer_message, void(Gcs_message *message));
  MOCK_METHOD0(deliver_buffered_messages, void());
  MOCK_METHOD0(cleanup_buffered_messages, void());
  MOCK_METHOD0(number_buffered_messages, size_t());
};

class XComStateExchangeTest : public GcsBaseTest {
 protected:
  XComStateExchangeTest(){};

  virtual void SetUp() {
    control_mock = new mock_gcs_control_interface();
    comm_mock = new mock_gcs_xcom_communication_interface();
    state_exchange = new Gcs_xcom_state_exchange(comm_mock);
  }

  virtual void TearDown() {
    delete state_exchange;
    delete comm_mock;
    delete control_mock;
  }

  Gcs_xcom_state_exchange *state_exchange;
  mock_gcs_control_interface *control_mock;
  mock_gcs_xcom_communication_interface *comm_mock;
};

TEST_F(XComStateExchangeTest, StateExchangeBroadcastJoinerTest) {
  // Setting expectations
  EXPECT_CALL(*comm_mock, send_binding_message(_, _, _))
      .Times(1)
      .WillOnce(Return(GCS_OK));

  std::string member_1_addr("127.0.0.1:12345");
  std::string member_2_addr("127.0.0.1:12346");

  // Set up parameters
  std::vector<Gcs_member_identifier *> total_members;
  total_members.push_back(new Gcs_member_identifier(member_1_addr));
  total_members.push_back(new Gcs_member_identifier(member_2_addr));

  std::vector<Gcs_member_identifier *> joined_members;
  joined_members.push_back(new Gcs_member_identifier(member_2_addr));

  std::vector<Gcs_member_identifier *> left_members;

  std::vector<Gcs_message_data *> data_to_exchange;

  std::string group_name("group_name");

  Gcs_member_identifier *mi = new Gcs_member_identifier(member_2_addr);
  synode_no configuration_id = null_synode;
  bool leaving = state_exchange->state_exchange(
      configuration_id, total_members, left_members, joined_members,
      data_to_exchange, NULL, &group_name, *mi);

  ASSERT_FALSE(leaving);

  delete mi;
}

uchar *copied_payload = NULL;
uint64_t copied_length = 0;
enum_gcs_error copy_message_content(const Gcs_message &msg) {
  copied_length = msg.get_message_data().get_payload_length();
  copied_payload = static_cast<uchar *>(malloc(sizeof(uchar) * copied_length));
  memcpy(copied_payload, msg.get_message_data().get_payload(), copied_length);

  return GCS_OK;
}

TEST_F(XComStateExchangeTest, StateExchangeProcessStatesPhase) {
  EXPECT_CALL(*comm_mock, send_binding_message(_, _, _))
      .WillOnce(WithArgs<0>(Invoke(copy_message_content)));

  /*
    Define that the first view delivered has two members, i.e.
    two members are simultaneously joining the view.
  */
  synode_no configuration_id = null_synode;

  std::string group_name("group_name");

  std::string member_1_addr("127.0.0.1:12345");
  Gcs_member_identifier *member_id_1 = new Gcs_member_identifier(member_1_addr);

  std::string member_2_addr("127.0.0.1:12346");
  Gcs_member_identifier *member_id_2 = new Gcs_member_identifier(member_2_addr);

  std::vector<Gcs_member_identifier *> total_members;
  total_members.push_back(new Gcs_member_identifier(member_1_addr));
  total_members.push_back(new Gcs_member_identifier(member_2_addr));

  std::vector<Gcs_member_identifier *> joined_members;
  joined_members.push_back(new Gcs_member_identifier(member_1_addr));
  joined_members.push_back(new Gcs_member_identifier(member_2_addr));

  std::vector<Gcs_member_identifier *> left_members;

  /*
    No application metadata shall be sent during the state exchange
    process.
  */
  std::vector<Gcs_message_data *> data_to_exchange;

  /*
    Send a state exchange message on behalf of member 1.
  */
  bool leaving = state_exchange->state_exchange(
      configuration_id, total_members, left_members, joined_members,
      data_to_exchange, NULL, &group_name, *member_id_1);
  ASSERT_FALSE(leaving);

  /*
    Check whether the state exchange message was properly sent
    and the state exchange state machine has the expected data.
  */
  Xcom_member_state *state_1 =
      new Xcom_member_state(copied_payload, copied_length);

  ASSERT_TRUE(state_1->get_view_id()->get_fixed_part() != 0);
  ASSERT_EQ(state_1->get_view_id()->get_monotonic_part(), 0u);
  ASSERT_EQ(state_1->get_data_size(), 0u);
  ASSERT_TRUE(synode_eq(state_1->get_configuration_id(), configuration_id));

  ASSERT_EQ(state_exchange->get_total()->size(), 2u);
  ASSERT_EQ(state_exchange->get_joined()->size(), 2u);
  ASSERT_EQ(state_exchange->get_left()->size(), 0u);
  ASSERT_EQ(*(state_exchange->get_group()), group_name);
  ASSERT_EQ(state_exchange->get_member_states()->size(), 0u);

  /*
    Simulate message received by member 1.
  */
  bool can_install =
      state_exchange->process_member_state(state_1, *member_id_1, 1);
  ASSERT_FALSE(can_install);
  ASSERT_EQ(state_exchange->get_member_states()->size(), 1u);

  /*
    Simulate message received by member 2.
  */
  const Gcs_xcom_view_identifier view_id_2(99999, 0);
  Xcom_member_state *state_2 =
      new Xcom_member_state(view_id_2, configuration_id, NULL, 0);
  can_install = state_exchange->process_member_state(state_2, *member_id_2, 1);
  ASSERT_TRUE(can_install);
  ASSERT_EQ(state_exchange->get_member_states()->size(), 2u);

  /*
    Simulate how the view is calculated.
  */
  Gcs_xcom_view_identifier *new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(view_id_2.get_fixed_part(), new_view_id->get_fixed_part());
  ASSERT_EQ(view_id_2.get_monotonic_part(), new_view_id->get_monotonic_part());

  delete member_id_1;
  delete member_id_2;
  free(copied_payload);
}

TEST_F(XComStateExchangeTest, StateExchangeChoosingView) {
  /*
    Prepare configuration to simulate state exchanges and
    calculate the new view.
  */
  synode_no configuration_id = null_synode;

  std::string member_1_addr("127.0.0.1:12345");
  Gcs_member_identifier *member_id_1 = new Gcs_member_identifier(member_1_addr);

  std::string member_2_addr("127.0.0.1:12348");
  Gcs_member_identifier *member_id_2 = new Gcs_member_identifier(member_2_addr);

  std::string member_3_addr("127.0.0.1:12346");
  Gcs_member_identifier *member_id_3 = new Gcs_member_identifier(member_3_addr);

  std::string member_4_addr("127.0.0.1:12347");
  Gcs_member_identifier *member_id_4 = new Gcs_member_identifier(member_4_addr);

  /*
    Check the map between member identifiers and states is empty.
  */
  std::map<Gcs_member_identifier, Xcom_member_state *> *member_states =
      state_exchange->get_member_states();
  ASSERT_EQ(member_states->size(), 0u);

  /*
    If there is one view, there is no much choice and the view is picked.
  */
  Gcs_xcom_view_identifier *new_view_id = NULL;

  Gcs_xcom_view_identifier view_id_1(99999, 0);
  Xcom_member_state *state_1 =
      new Xcom_member_state(view_id_1, configuration_id, NULL, 0);
  (*member_states)[*member_id_1] = state_1;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 1u);
  ASSERT_EQ(view_id_1.get_fixed_part(), new_view_id->get_fixed_part());
  ASSERT_EQ(view_id_1.get_monotonic_part(), new_view_id->get_monotonic_part());

  /*
    If there is two views where all the monotonic parts are zero, the one
    with the greater member identifier is picked.
  */
  Gcs_xcom_view_identifier view_id_2(88888, 0);
  Xcom_member_state *state_2 =
      new Xcom_member_state(view_id_2, configuration_id, NULL, 0);
  (*member_states)[*member_id_2] = state_2;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 2u);
  ASSERT_TRUE(*member_id_1 < *member_id_2);
  ASSERT_EQ(view_id_2.get_fixed_part(), new_view_id->get_fixed_part());
  ASSERT_EQ(view_id_2.get_monotonic_part(), new_view_id->get_monotonic_part());

  /*
    If there are n views where their monotonic parts are zero, the one
    with the greater member identifier is picked.
  */
  Gcs_xcom_view_identifier view_id_3(66666, 0);
  Xcom_member_state *state_3 =
      new Xcom_member_state(view_id_3, configuration_id, NULL, 0);
  (*member_states)[*member_id_3] = state_3;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 3u);
  ASSERT_TRUE(*member_id_1 < *member_id_2);
  ASSERT_TRUE(*member_id_3 < *member_id_2);
  ASSERT_EQ(view_id_2.get_fixed_part(), new_view_id->get_fixed_part());
  ASSERT_EQ(view_id_2.get_monotonic_part(), new_view_id->get_monotonic_part());

  /*
    If there are views where their monotonic parts are not zero, the first
    one where the monotonic part is not zero is picked. The system must
    guarantee that all elements that have the monotonic part different from
    zero has the same value.

    This basically means that a previous view has been installed and all
    the members that are part of the previous view must have the same
    view identifier.
  */
  Gcs_xcom_view_identifier view_id_4(77777, 1);
  Xcom_member_state *state_4 =
      new Xcom_member_state(view_id_4, configuration_id, NULL, 0);
  (*member_states)[*member_id_4] = state_4;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 4u);
  ASSERT_TRUE(*member_id_1 < *member_id_2);
  ASSERT_TRUE(*member_id_3 < *member_id_2);
  ASSERT_TRUE(*member_id_4 < *member_id_2);
  ASSERT_EQ(view_id_4.get_fixed_part(), new_view_id->get_fixed_part());
  ASSERT_EQ(view_id_4.get_monotonic_part(), new_view_id->get_monotonic_part());

  delete member_id_1;
  delete member_id_2;
  delete member_id_3;
  delete member_id_4;
}

TEST_F(XComStateExchangeTest, StateExchangeWrongAssumptionsView) {
  /*
    This test requires that all debug modes are set but it is not safe to
    set it only here because if it fails, the system may start logging
    messages that are not supposed to do so.
  */
  if (Gcs_debug_manager::get_current_debug_options() != GCS_DEBUG_ALL) {
    /* purecov: begin deadcode */
    return;
    /* purecov: end */
  }

  /*
    Prepare configuration to simulate state exchanges when there
    is a bug in the state exchange messages and members are not
    proposing the correct views.
  */
  Gcs_xcom_view_identifier *new_view_id = NULL;
  std::map<Gcs_member_identifier, Xcom_member_state *>::iterator state_it;

  std::string member_1_addr("127.0.0.1:12345");
  Gcs_member_identifier *member_id_1 = new Gcs_member_identifier(member_1_addr);

  std::string member_2_addr("127.0.0.1:12348");
  Gcs_member_identifier *member_id_2 = new Gcs_member_identifier(member_2_addr);

  std::string member_3_addr("127.0.0.1:12346");
  Gcs_member_identifier *member_id_3 = new Gcs_member_identifier(member_3_addr);

  std::string member_4_addr("127.0.0.1:12347");
  Gcs_member_identifier *member_id_4 = new Gcs_member_identifier(member_4_addr);

  /*
    Check the map between member identifiers and states is empty.
  */
  std::map<Gcs_member_identifier, Xcom_member_state *> *member_states =
      state_exchange->get_member_states();
  ASSERT_EQ(member_states->size(), 0u);

  /*
    Two views where the monotonic part in each view is different from
    zero but the fixed parts don't match. This situation cannot happen
    in practice.
  */
  synode_no configuration_id = null_synode;
  Gcs_xcom_view_identifier view_id_1(99999, 1);
  Xcom_member_state *state_1 =
      new Xcom_member_state(view_id_1, configuration_id, NULL, 0);
  (*member_states)[*member_id_1] = state_1;

  Gcs_xcom_view_identifier view_id_2(88888, 1);
  Xcom_member_state *state_2 =
      new Xcom_member_state(view_id_2, configuration_id, NULL, 0);
  (*member_states)[*member_id_2] = state_2;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 2u);
  ASSERT_TRUE(new_view_id == NULL);

  for (state_it = member_states->begin(); state_it != member_states->end();
       state_it++)
    delete (*state_it).second;
  member_states->clear();

  /*
    Two views where the monotonic part in each view is different from
    zero but they don't match. This situation cannot happen in practice.
  */
  Gcs_xcom_view_identifier view_id_3(99999, 1);
  Xcom_member_state *state_3 =
      new Xcom_member_state(view_id_3, configuration_id, NULL, 0);
  (*member_states)[*member_id_3] = state_3;

  Gcs_xcom_view_identifier view_id_4(99999, 2);
  Xcom_member_state *state_4 =
      new Xcom_member_state(view_id_4, configuration_id, NULL, 0);
  (*member_states)[*member_id_4] = state_4;
  new_view_id = state_exchange->get_new_view_id();
  ASSERT_EQ(member_states->size(), 2u);
  ASSERT_TRUE(new_view_id == NULL);
  (void)new_view_id;

  for (state_it = member_states->begin(); state_it != member_states->end();
       state_it++)
    delete (*state_it).second;
  member_states->clear();

  delete member_id_1;
  delete member_id_2;
  delete member_id_3;
  delete member_id_4;
}

TEST_F(XComStateExchangeTest, StateExchangeDiscardSynodes) {
  EXPECT_CALL(*comm_mock, send_binding_message(_, _, _))
      .WillOnce(Return(GCS_OK));

  /*
    Define that the first view delivered has two members, i.e.
    two members are simultaneously joining the view.
  */
  synode_no configuration_id = null_synode;

  synode_no invalid_configuration_id = null_synode;
  invalid_configuration_id.group_id = 0;
  invalid_configuration_id.msgno = 1;
  invalid_configuration_id.node = 0;

  std::string group_name("group_name");

  std::string member_1_addr("127.0.0.1:12345");
  Gcs_member_identifier *member_id_1 = new Gcs_member_identifier(member_1_addr);

  std::vector<Gcs_member_identifier *> total_members;
  total_members.push_back(new Gcs_member_identifier(member_1_addr));

  std::vector<Gcs_member_identifier *> joined_members;
  joined_members.push_back(new Gcs_member_identifier(member_1_addr));

  std::vector<Gcs_member_identifier *> left_members;

  /*
    No application metadata shall be sent during the state exchange
    process.
  */
  std::vector<Gcs_message_data *> data_to_exchange;

  /*
    Send a state exchange message on behalf of member 1.
  */
  state_exchange->state_exchange(configuration_id, total_members, left_members,
                                 joined_members, data_to_exchange, NULL,
                                 &group_name, *member_id_1);

  /*
    If the synode does not match, the state exchange message is
    ignored.
  */
  const Gcs_xcom_view_identifier view_id_1(99999, 0);
  Xcom_member_state *state_1 =
      new Xcom_member_state(view_id_1, invalid_configuration_id, NULL, 0);
  bool can_install =
      state_exchange->process_member_state(state_1, *member_id_1, 1);
  ASSERT_FALSE(can_install);
  ASSERT_EQ(state_exchange->get_member_states()->size(), 0u);

  delete member_id_1;
}

}  // namespace gcs_xcom_state_exchange_unittest
