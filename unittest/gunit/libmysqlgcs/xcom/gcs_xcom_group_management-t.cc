/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_communication_interface.h"
#include "gcs_xcom_group_management.h"
#include "gcs_xcom_utils.h"

namespace gcs_xcom_groupmanagement_unittest {

class mock_gcs_xcom_proxy : public Gcs_xcom_proxy_base {
 public:
  mock_gcs_xcom_proxy() {
    ON_CALL(*this, xcom_open_handlers(_, _)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_init(_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_exit(_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_close_handlers()).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_boot(_, _)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_add_node(_, _, _)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_send_data(_, _)).WillByDefault(Return(10));
    ON_CALL(*this, new_node_address_uuid(_, _, _))
        .WillByDefault(WithArgs<0, 1, 2>(Invoke(::new_node_address_uuid)));
    ON_CALL(*this, delete_node_address(_, _))
        .WillByDefault(WithArgs<0, 1>(Invoke(::delete_node_address)));
  }

  MOCK_METHOD3(new_node_address_uuid,
               node_address *(unsigned int n, char *names[], blob uuid[]));
  MOCK_METHOD2(delete_node_address, void(unsigned int n, node_address *na));
  MOCK_METHOD3(xcom_client_add_node, int(connection_descriptor *fd,
                                         node_list *nl, uint32_t group_id));
  MOCK_METHOD3(xcom_client_remove_node, int(connection_descriptor *fd,
                                            node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_remove_node, int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_get_event_horizon,
               bool(uint32_t group_id, xcom_event_horizon &event_horizon));
  MOCK_METHOD2(xcom_client_set_event_horizon,
               bool(uint32_t group_id, xcom_event_horizon event_horizon));
  MOCK_METHOD2(xcom_client_boot, int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_open_connection,
               connection_descriptor *(std::string, xcom_port port));
  MOCK_METHOD1(xcom_client_close_connection, int(connection_descriptor *fd));
  MOCK_METHOD2(xcom_client_send_data, int(unsigned long long size, char *data));
  MOCK_METHOD0(xcom_client_send_die, int());
  MOCK_METHOD1(xcom_init, int(xcom_port listen_port));
  MOCK_METHOD1(xcom_exit, int(bool xcom_handlers_open));
  MOCK_METHOD0(xcom_set_cleanup, void());
  MOCK_METHOD1(xcom_get_ssl_mode, int(const char *mode));
  MOCK_METHOD1(xcom_set_ssl_mode, int(int mode));
  MOCK_METHOD1(xcom_get_ssl_fips_mode, int(const char *mode));
  MOCK_METHOD1(xcom_set_ssl_fips_mode, int(int mode));
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
  MOCK_METHOD1(xcom_wait_for_xcom_comms_status_change, void(int &status));
  MOCK_METHOD0(xcom_has_comms_status_changed, bool());
  MOCK_METHOD1(xcom_set_comms_status, void(int status));
  MOCK_METHOD1(xcom_signal_comms_status_changed, void(int status));
  MOCK_METHOD0(xcom_wait_exit, enum_gcs_error());
  MOCK_METHOD0(xcom_is_exit, bool());
  MOCK_METHOD1(xcom_set_exit, void(bool));
  MOCK_METHOD0(xcom_signal_exit, void());
  MOCK_METHOD3(xcom_client_force_config, int(connection_descriptor *fd,
                                             node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_force_config, int(node_list *nl, uint32_t group_id));

  MOCK_METHOD0(get_should_exit, bool());
  MOCK_METHOD1(set_should_exit, void(bool should_exit));
};

class XcomGroupManagementTest : public GcsBaseTest {
 protected:
  XcomGroupManagementTest(){};

  virtual void SetUp() {
    group_id = new Gcs_group_identifier("only_group");
    xcom_group_mgmt_if = new Gcs_xcom_group_management(&proxy, *group_id);
  }

  virtual void TearDown() {
    delete xcom_group_mgmt_if;
    delete group_id;
  }

  Gcs_group_identifier *group_id;
  Gcs_xcom_state_exchange *state_exchange;

  mock_gcs_xcom_proxy proxy;
  Gcs_xcom_group_management *xcom_group_mgmt_if;
};

TEST_F(XcomGroupManagementTest, EmptyPeerNodes) {
  EXPECT_CALL(proxy, xcom_client_force_config(_, _)).Times(0);

  Gcs_interface_parameters forced_group;
  forced_group.add_parameter("peer_nodes", "");

  enum_gcs_error result =
      xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_NOK, result);
}

TEST_F(XcomGroupManagementTest, UnconfiguredPeerNodes) {
  EXPECT_CALL(proxy, xcom_client_force_config(_, _)).Times(0);

  Gcs_interface_parameters forced_group;

  enum_gcs_error result =
      xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_NOK, result);
}

bool operator==(const node_list &first, const node_list &second) {
  if (first.node_list_len != second.node_list_len) return false;

  for (unsigned int i = 0; i < first.node_list_len; i++) {
    if (strcmp(first.node_list_val[i].address,
               second.node_list_val[i].address) != 0)
      return false;
  }

  return true;
}

MATCHER_P(node_list_pointer_matcher, other_nl, "Derreference pointer") {
  return other_nl == (*arg);
}

TEST_F(XcomGroupManagementTest, TestListContent) {
  Gcs_xcom_node_information node_1("127.0.0.1:12345");
  Gcs_xcom_node_information node_2("127.0.0.1:12346");

  Gcs_xcom_nodes nodes;
  nodes.add_node(node_1);
  nodes.add_node(node_2);

  node_list nl;
  nl.node_list_len = 2;
  const char *node_addrs[] = {node_1.get_member_id().get_member_id().c_str(),
                              node_2.get_member_id().get_member_id().c_str()};
  blob blobs[] = {{{0, static_cast<char *>(malloc(
                           node_1.get_member_uuid().actual_value.size()))}},
                  {{0, static_cast<char *>(malloc(
                           node_2.get_member_uuid().actual_value.size()))}}};
  node_1.get_member_uuid().encode(
      reinterpret_cast<uchar **>(&blobs[0].data.data_val),
      &blobs[0].data.data_len);
  node_2.get_member_uuid().encode(
      reinterpret_cast<uchar **>(&blobs[1].data.data_val),
      &blobs[1].data.data_len);

  nl.node_list_val = ::new_node_address_uuid(
      nl.node_list_len, const_cast<char **>(node_addrs), blobs);

  EXPECT_CALL(proxy, xcom_client_force_config(node_list_pointer_matcher(nl), _))
      .Times(1)
      .WillOnce(Return(1));

  Gcs_interface_parameters forced_group;
  forced_group.add_parameter("peer_nodes", "127.0.0.1:12345,127.0.0.1:12346");

  xcom_group_mgmt_if->set_xcom_nodes(nodes);
  enum_gcs_error result =
      xcom_group_mgmt_if->modify_configuration(forced_group);

  ASSERT_EQ(GCS_OK, result);
  ASSERT_EQ((unsigned)2, nl.node_list_len);
  ASSERT_STREQ("127.0.0.1:12345", nl.node_list_val[0].address);
  ASSERT_STREQ("127.0.0.1:12346", nl.node_list_val[1].address);

  ::delete_node_address(nl.node_list_len, nl.node_list_val);

  free(blobs[0].data.data_val);
  free(blobs[1].data.data_val);
}

}  // namespace gcs_xcom_groupmanagement_unittest
