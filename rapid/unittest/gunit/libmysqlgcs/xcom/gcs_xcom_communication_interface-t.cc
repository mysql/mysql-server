/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_base_test.h"

#include "mysql/gcs/gcs_message.h"
#include "gcs_xcom_statistics_interface.h"
#include "gcs_xcom_communication_interface.h"

namespace gcs_xcom_communication_unittest {

class mock_gcs_xcom_view_change_control_interface
  : public Gcs_xcom_view_change_control_interface
{
public:
  MOCK_METHOD0(start_view_exchange, void());
  MOCK_METHOD0(end_view_exchange, void());
  MOCK_METHOD0(wait_for_view_change_end, void());
  MOCK_METHOD0(is_view_changing, bool());
  MOCK_METHOD0(start_leave, bool());
  MOCK_METHOD0(end_leave, void());
  MOCK_METHOD0(is_leaving, bool());
  MOCK_METHOD0(start_join, bool());
  MOCK_METHOD0(end_join, void());
  MOCK_METHOD0(is_joining, bool());
  MOCK_METHOD1(set_current_view, void(Gcs_view*));
  MOCK_METHOD0(get_current_view, Gcs_view *());
  MOCK_METHOD0(belongs_to_group, bool());
  MOCK_METHOD1(set_belongs_to_group, void(bool));
  MOCK_METHOD1(set_unsafe_current_view, void(Gcs_view*));
  MOCK_METHOD0(get_unsafe_current_view, Gcs_view *());
};

class mock_gcs_xcom_statistics_updater : public Gcs_xcom_statistics_updater
{
public:
  MOCK_METHOD1(update_message_sent, void(unsigned long long message_lenght));
  MOCK_METHOD1(update_message_received, void(long message_lenght));
};

class mock_gcs_communication_event_listener
  : public Gcs_communication_event_listener
{
public:
  MOCK_CONST_METHOD1(on_message_received, void(const Gcs_message &message));
};

class mock_gcs_xcom_proxy : public Gcs_xcom_proxy_base
{
public:

  bool deallocate_msg(unsigned int size MY_ATTRIBUTE((unused)), char *data)
  {
    free(data);
    return false;
  }

  mock_gcs_xcom_proxy()
  {
    ON_CALL(*this, xcom_open_handlers(_,_)).WillByDefault(Return(false));
    ON_CALL(*this, xcom_close_handlers()).WillByDefault(Return(false));
    ON_CALL(*this, xcom_client_add_node(_,_,_)).WillByDefault(Return(false));
    ON_CALL(*this, xcom_client_send_data(_,_)).WillByDefault(Invoke(this, &mock_gcs_xcom_proxy::deallocate_msg));
  }

  MOCK_METHOD3(new_node_address_uuid, node_address *(unsigned int n, char *names[], blob uuids[]));
  MOCK_METHOD2(delete_node_address, void(unsigned int n, node_address *na));
  MOCK_METHOD3(xcom_client_add_node,
               int(connection_descriptor* con, node_list *nl, uint32_t group_id));
  MOCK_METHOD3(xcom_client_remove_node,
               int(connection_descriptor* con, node_list* nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_remove_node,
               int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_boot, int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_open_connection, connection_descriptor* (std::string, xcom_port port));
  MOCK_METHOD1(xcom_client_close_connection, int(connection_descriptor* con));
  MOCK_METHOD2(xcom_client_send_data, int(unsigned long long size, char *data));
  MOCK_METHOD1(xcom_init, int(xcom_port listen_port));
  MOCK_METHOD1(xcom_exit, int(bool xcom_handlers_open));
  MOCK_METHOD0(xcom_set_cleanup, void());
  MOCK_METHOD1(xcom_get_ssl_mode, int(const char* mode));
  MOCK_METHOD1(xcom_set_ssl_mode, int(int mode));
  MOCK_METHOD0(xcom_init_ssl, int());
  MOCK_METHOD0(xcom_destroy_ssl, void());
  MOCK_METHOD0(xcom_use_ssl, int());
  MOCK_METHOD10(xcom_set_ssl_parameters,
                    void(const char *server_key_file, const char *server_cert_file,
                    const char *client_key_file, const char *client_cert_file,
                    const char *ca_file, const char *ca_path,
                    const char *crl_file, const char *crl_path,
                    const char *cipher, const char *tls_version));
  MOCK_METHOD1(find_site_def, site_def const *(synode_no synode));
  MOCK_METHOD2(xcom_open_handlers, bool(std::string saddr, xcom_port port));
  MOCK_METHOD0(xcom_close_handlers, bool());
  MOCK_METHOD0(xcom_acquire_handler, int());
  MOCK_METHOD1(xcom_release_handler, void(int index));
  MOCK_METHOD0(xcom_wait_ready, enum_gcs_error());
  MOCK_METHOD0(xcom_is_ready, bool());
  MOCK_METHOD1(xcom_set_ready, void(bool value));
  MOCK_METHOD0(xcom_signal_ready, void());
  MOCK_METHOD1(xcom_wait_for_xcom_comms_status_change,
      void(int& status));
  MOCK_METHOD0(xcom_has_comms_status_changed,
      bool());
  MOCK_METHOD1(xcom_set_comms_status,
      void(int status));
  MOCK_METHOD1(xcom_signal_comms_status_changed,
      void(int status));
  MOCK_METHOD0(xcom_wait_exit, enum_gcs_error());
  MOCK_METHOD0(xcom_is_exit, bool());
  MOCK_METHOD1(xcom_set_exit, void(bool));
  MOCK_METHOD0(xcom_signal_exit, void());
  MOCK_METHOD3(xcom_client_force_config, int(connection_descriptor *fd, node_list *nl,
                                             uint32_t group_id));
  MOCK_METHOD2(xcom_client_force_config, int(node_list *nl,
                                             uint32_t group_id));

  MOCK_METHOD0(get_should_exit, bool());
  MOCK_METHOD1(set_should_exit,void(bool should_exit));
};


class XComCommunicationTest : public GcsBaseTest
{
protected:

  virtual void SetUp()
  {
    mock_stats=      new mock_gcs_xcom_statistics_updater();
    mock_proxy=      new mock_gcs_xcom_proxy();
    mock_vce=        new mock_gcs_xcom_view_change_control_interface();
    xcom_comm_if=    new Gcs_xcom_communication(mock_stats,
                                                mock_proxy,
                                                mock_vce);
  }


  virtual void TearDown()
  {
    delete mock_stats;
    delete mock_vce;
    delete mock_proxy;
    delete xcom_comm_if;
  }


  Gcs_xcom_communication                       *xcom_comm_if;
  mock_gcs_xcom_statistics_updater             *mock_stats;
  mock_gcs_xcom_proxy                          *mock_proxy;
  mock_gcs_xcom_view_change_control_interface  *mock_vce;
};


TEST_F(XComCommunicationTest, SetEventListenerTest)
{
  mock_gcs_communication_event_listener comm_listener;

  int reference= xcom_comm_if->add_event_listener(comm_listener);

  ASSERT_NE(0, reference);
  ASSERT_EQ((long unsigned int)1,
            xcom_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1, xcom_comm_if->get_event_listeners()->size());
}


TEST_F(XComCommunicationTest, SetEventListenersTest)
{
  mock_gcs_communication_event_listener comm_listener;
  mock_gcs_communication_event_listener another_comm_listener;

  int reference=         xcom_comm_if->add_event_listener(comm_listener);
  int another_reference=
    xcom_comm_if->add_event_listener(another_comm_listener);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)1,
            xcom_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_comm_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)2, xcom_comm_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);
}


TEST_F(XComCommunicationTest, RemoveEventListenerTest)
{
  mock_gcs_communication_event_listener comm_listener;
  mock_gcs_communication_event_listener another_comm_listener;

  int reference= xcom_comm_if->add_event_listener(comm_listener);
  int another_reference=
    xcom_comm_if->add_event_listener(another_comm_listener);

  xcom_comm_if->remove_event_listener(reference);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)0,
            xcom_comm_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_comm_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)1, xcom_comm_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);
}


TEST_F(XComCommunicationTest, SendMessageTest)
{
  //Test Expectations
  EXPECT_CALL(*mock_proxy, xcom_client_send_data(_,_)).Times(1);
  EXPECT_CALL(*mock_stats, update_message_sent(_)).Times(1);
  EXPECT_CALL(*mock_vce, belongs_to_group()).Times(1).WillOnce(Return(true));

  std::string test_header("header");
  std::string test_payload("payload");
  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");
  Gcs_message_data *message_data=
    new Gcs_message_data(test_header.length(), test_payload.length());

  Gcs_message message(member_id, group_id, message_data);

  message.get_message_data().append_to_header((uchar *)test_header.c_str(),
                                              test_header.length());

  message.get_message_data().append_to_payload((uchar *)test_payload.c_str(),
                                               test_payload.length());

  enum_gcs_error message_result= xcom_comm_if->send_message(message);
  ASSERT_EQ(GCS_OK, message_result);
}


TEST_F(XComCommunicationTest, ReceiveMessageTest)
{
  mock_gcs_communication_event_listener ev_listener;

  //Test Expectations
  EXPECT_CALL(ev_listener, on_message_received(_))
              .Times(1);

  EXPECT_CALL(*mock_stats, update_message_received(_))
              .Times(1);

  ON_CALL(*mock_vce, belongs_to_group()).WillByDefault(Return(true));
  ON_CALL(*mock_vce, is_view_changing()).WillByDefault(Return(false));

  //Test
  std::string test_header("header");
  std::string test_payload("payload");
  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");
  Gcs_message_data *message_data=
    new Gcs_message_data(test_header.length(), test_payload.length());

  Gcs_message *message= new Gcs_message(member_id, group_id, message_data);

  message->get_message_data().append_to_header((uchar *)test_header.c_str(),
                                               test_header.length());

  message->get_message_data().append_to_payload((uchar *)test_payload.c_str(),
                                               test_payload.length());

  int listener_ref= xcom_comm_if->add_event_listener(ev_listener);

  xcom_comm_if->xcom_receive_data(message);

  xcom_comm_if->remove_event_listener(listener_ref);
}


TEST_F(XComCommunicationTest, BufferMessageTest)
{
  mock_gcs_communication_event_listener ev_listener;

  //Test Expectations
  EXPECT_CALL(ev_listener, on_message_received(_))
              .Times(1);

  EXPECT_CALL(*mock_stats, update_message_received(_))
              .Times(1);


  // Set up the environment.
  std::string test_header("header");
  std::string test_payload("payload");
  Gcs_member_identifier member_id("member");
  Gcs_group_identifier group_id("group");
  int listener_ref= xcom_comm_if->add_event_listener(ev_listener);
  Gcs_message_data *message_data= NULL;
  Gcs_message *message= NULL;

  /*
     Try to send a message when the view is not installed. They are
     buffered but we flush them out.
  */
  ON_CALL(*mock_vce, belongs_to_group()).WillByDefault(Return(false));
  ON_CALL(*mock_vce, is_view_changing()).WillByDefault(Return(true));
  message_data=
    new Gcs_message_data(test_header.length(), test_payload.length());
  message= new Gcs_message(member_id, group_id, message_data);
  message->get_message_data().append_to_header((uchar *)test_header.c_str(),
                                               test_header.length());
  message->get_message_data().append_to_payload((uchar *)test_payload.c_str(),
                                               test_payload.length());
  xcom_comm_if->xcom_receive_data(message);
  ON_CALL(*mock_vce, belongs_to_group()).WillByDefault(Return(true));
  ON_CALL(*mock_vce, is_view_changing()).WillByDefault(Return(false));
  xcom_comm_if->deliver_buffered_messages();

  xcom_comm_if->remove_event_listener(listener_ref);
}

}
