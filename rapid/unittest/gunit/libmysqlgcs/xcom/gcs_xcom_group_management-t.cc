/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/gcs/gcs_log_system.h"
#include "gcs_xcom_utils.h"
#include "gcs_xcom_group_management.h"
#include "gcs_xcom_communication_interface.h"

using ::testing::Return;
using ::testing::DoAll;
using ::testing::WithArgs;
using ::testing::SaveArgPointee;
using ::testing::Invoke;
using ::testing::_;
using ::testing::Eq;

namespace gcs_xcom_groupmanagement_unittest
{

class mock_gcs_xcom_view_change_control_interface
  : public Gcs_xcom_view_change_control_interface
{
public:
  mock_gcs_xcom_view_change_control_interface()
    :m_current_view(NULL)
  {
  }

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
  MOCK_METHOD0(belongs_to_group, bool());
  MOCK_METHOD1(set_belongs_to_group, void(bool));
  MOCK_METHOD1(set_unsafe_current_view, void(Gcs_view*));
  MOCK_METHOD0(get_unsafe_current_view, Gcs_view *());

  void set_current_view(Gcs_view *view)
  {
    m_current_view= view;
  }

  Gcs_view *get_current_view()
  {
    return m_current_view;
  }

private:
  Gcs_view *m_current_view;
};

class mock_gcs_xcom_proxy : public Gcs_xcom_proxy
{
public:
  mock_gcs_xcom_proxy()
  {
    ON_CALL(*this, xcom_open_handlers(_,_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_init(_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_exit(_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_close_handlers()).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_boot(_,_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_add_node(_,_,_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_send_data(_,_)).WillByDefault(Return(10));
    ON_CALL(*this, new_node_address_uuid(_,_,_)).WillByDefault(
            WithArgs<0,1,2>(Invoke(::new_node_address_uuid)));
    ON_CALL(*this, delete_node_address(_,_)).WillByDefault(
            WithArgs<0,1>(Invoke(::delete_node_address)));
  }

  MOCK_METHOD3(new_node_address_uuid, node_address *(unsigned int n, char *names[], blob uuid[]));
  MOCK_METHOD2(delete_node_address, void(unsigned int n, node_address *na));
  MOCK_METHOD3(xcom_client_add_node,
               int(connection_descriptor *fd, node_list *nl, uint32_t group_id));
  MOCK_METHOD3(xcom_client_remove_node,
               int(connection_descriptor *fd, node_list* nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_remove_node,
               int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_boot, int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_open_connection, connection_descriptor *(std::string, xcom_port port));
  MOCK_METHOD1(xcom_client_close_connection, int(connection_descriptor *fd));
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
  MOCK_METHOD1(xcom_release_handler, void(int fd));
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
};

class XcomGroupManagementTest : public ::testing::Test
{
protected:

  XcomGroupManagementTest() {};

  virtual void SetUp()
  {
    group_id= new Gcs_group_identifier("only_group");
    mock_vce= new mock_gcs_xcom_view_change_control_interface();
    xcom_group_mgmt_if=
      new Gcs_xcom_group_management(&proxy, mock_vce, *group_id);

    logger= new Gcs_simple_ext_logger_impl();
    Gcs_logger::initialize(logger);
  }

  virtual void TearDown()
  {
    delete xcom_group_mgmt_if;
    delete mock_vce;
    delete group_id;

    Gcs_logger::finalize();
    delete logger;
  }

  Gcs_group_identifier *group_id;
  Gcs_xcom_state_exchange *state_exchange;
  mock_gcs_xcom_view_change_control_interface *mock_vce;

  mock_gcs_xcom_proxy proxy;
  Gcs_xcom_group_management *xcom_group_mgmt_if;
  Ext_logger_interface *logger;
};

TEST_F(XcomGroupManagementTest, EmptyPeerNodes)
{
  EXPECT_CALL(proxy, xcom_client_force_config(_,_))
                      .Times(0);

  Gcs_interface_parameters forced_group;
  forced_group.add_parameter("peer_nodes", "");

  enum_gcs_error result= xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_NOK, result);
}

TEST_F(XcomGroupManagementTest, UnconfiguredPeerNodes)
{
  EXPECT_CALL(proxy, xcom_client_force_config(_,_)).Times(0);

  Gcs_interface_parameters forced_group;

  enum_gcs_error result= xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_NOK, result);
}

bool operator==(const node_list& first, const node_list& second)
{
  if(first.node_list_len != second.node_list_len)
    return false;

  for(unsigned int i= 0; i < first.node_list_len; i++)
  {
    if(strcmp(first.node_list_val[i].address,
              second.node_list_val[i].address) != 0)
      return false;
  }

  return true;
}


TEST_F(XcomGroupManagementTest, ErrorNoViewTest)
{
  Gcs_interface_parameters forced_group;
  forced_group.add_parameter("peer_nodes", "127.0.0.1:12345,127.0.0.1:123456");

  enum_gcs_error result= xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_NOK, result);
}


MATCHER_P(node_list_pointer_matcher, other_nl, "Derreference pointer")
                                                  { return other_nl == (*arg);}

TEST_F(XcomGroupManagementTest, TestListContent)
{
  node_list nl;
  nl.node_list_len= 2;
  const char *address_1= "127.0.0.1:12345";
  const char *address_2= "127.0.0.1:12346";
  const char *node_addrs[]= {address_1, address_2};
  Gcs_uuid uuid_1= Gcs_uuid::create_uuid();
  Gcs_uuid uuid_2= Gcs_uuid::create_uuid();
  blob blobs[] = {
     {{
       uuid_1.size,
       static_cast<char *>(malloc(uuid_1.size * sizeof(char *)))
     }},
     {{
       uuid_2.size,
       static_cast<char *>(malloc(uuid_2.size * sizeof(char *)))
     }}
  };
  uuid_1.encode(reinterpret_cast<uchar **>(&blobs[0].data.data_val));
  uuid_2.encode(reinterpret_cast<uchar **>(&blobs[1].data.data_val));


  Gcs_xcom_view_identifier view_id(0, 0);

  Gcs_member_identifier member_1(address_1, uuid_1);
  Gcs_member_identifier member_2(address_2, uuid_2);
  std::vector<Gcs_member_identifier> members;
  members.push_back(member_1);
  members.push_back(member_2);

  std::vector<Gcs_member_identifier> left_members;

  std::vector<Gcs_member_identifier> joined_members;
  joined_members.push_back(member_2);
  joined_members.push_back(member_2);

  // Create the new view
  Gcs_view *current_view=
    new Gcs_view(members, view_id, left_members, joined_members, *group_id);
  mock_vce->set_current_view(current_view);

  nl.node_list_val= ::new_node_address_uuid(nl.node_list_len, const_cast<char**>(node_addrs), blobs);

  EXPECT_CALL(proxy, xcom_client_force_config(node_list_pointer_matcher(nl),_))
                      .Times(1)
                      .WillOnce(Return(1));

  Gcs_interface_parameters forced_group;
  forced_group.add_parameter("peer_nodes", "127.0.0.1:12345,127.0.0.1:12346");

  enum_gcs_error result= xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_OK, result);
  ASSERT_EQ((unsigned)2, nl.node_list_len);
  ASSERT_STREQ("127.0.0.1:12345", nl.node_list_val[0].address);
  ASSERT_STREQ("127.0.0.1:12346", nl.node_list_val[1].address);

  ::delete_node_address(nl.node_list_len, nl.node_list_val);
}

}
