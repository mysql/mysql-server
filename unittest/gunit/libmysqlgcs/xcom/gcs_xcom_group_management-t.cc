/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
    ON_CALL(*this, xcom_client_boot(_, _)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_add_node(_, _, _)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_send_data(_, _)).WillByDefault(Return(10));
    ON_CALL(*this, new_node_address_uuid(_, _, _))
        .WillByDefault(Invoke(::new_node_address_uuid));
    ON_CALL(*this, delete_node_address(_, _))
        .WillByDefault(Invoke(::delete_node_address));
  }

  MOCK_METHOD(node_address *, new_node_address_uuid,
              (unsigned int n, char const *names[], blob uuids[]), (override));
  MOCK_METHOD(void, delete_node_address, (unsigned int n, node_address *na),
              (override));
  MOCK_METHOD(bool, xcom_client_add_node,
              (connection_descriptor * fd, node_list *nl, uint32_t group_id),
              (override));
  MOCK_METHOD(bool, xcom_client_remove_node,
              (node_list * nl, uint32_t group_id), (override));
  MOCK_METHOD(bool, xcom_client_remove_node,
              (connection_descriptor * fd, node_list *nl, uint32_t group_id),
              (override));
  MOCK_METHOD(bool, xcom_client_get_event_horizon,
              (uint32_t group_id, xcom_event_horizon &event_horizon),
              (override));
  MOCK_METHOD(bool, xcom_client_set_event_horizon,
              (uint32_t group_id, xcom_event_horizon event_horizon),
              (override));
  MOCK_METHOD(bool, xcom_client_set_max_leaders,
              (uint32_t group_id, node_no max_leaders), (override));
  MOCK_METHOD(bool, xcom_client_set_leaders,
              (uint32_t group_id, u_int n, char const *names[],
               node_no max_nr_leaders),
              (override));
  MOCK_METHOD(bool, xcom_client_get_leaders,
              (uint32_t gid, leader_info_data &leaders), (override));
  MOCK_METHOD(bool, xcom_client_get_synode_app_data,
              (connection_descriptor * con, uint32_t group_id_hash,
               synode_no_array &synodes, synode_app_data_array &reply),
              (override));
  MOCK_METHOD(bool, xcom_client_set_cache_size, (uint64_t size), (override));
  MOCK_METHOD(bool, xcom_client_boot, (node_list * nl, uint32_t group_id),
              (override));
  MOCK_METHOD(connection_descriptor *, xcom_client_open_connection,
              (std::string, xcom_port port), (override));
  MOCK_METHOD(bool, xcom_client_close_connection, (connection_descriptor * fd),
              (override));
  MOCK_METHOD(bool, xcom_client_send_data,
              (unsigned long long size, char *data), (override));
  MOCK_METHOD(void, xcom_init, (xcom_port listen_port), (override));
  MOCK_METHOD(void, xcom_exit, (), (override));
  MOCK_METHOD(void, xcom_set_cleanup, (), (override));
  MOCK_METHOD(int, xcom_get_ssl_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_set_ssl_mode, (int mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_fips_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_set_ssl_fips_mode, (int mode), (override));
  MOCK_METHOD(bool, xcom_init_ssl, ());
  MOCK_METHOD(void, xcom_destroy_ssl, (), (override));
  MOCK_METHOD(bool, xcom_use_ssl, (), (override));
  MOCK_METHOD(void, xcom_set_ssl_parameters,
              (ssl_parameters ssl, tls_parameters tls), (override));
  MOCK_METHOD(site_def const *, find_site_def, (synode_no synode), (override));
  MOCK_METHOD(enum_gcs_error, xcom_wait_ready, (), (override));
  MOCK_METHOD(bool, xcom_is_ready, (), (override));
  MOCK_METHOD(void, xcom_set_ready, (bool value), (override));
  MOCK_METHOD(void, xcom_signal_ready, (), (override));
  MOCK_METHOD(void, xcom_wait_for_xcom_comms_status_change, (int &status),
              (override));
  MOCK_METHOD(bool, xcom_has_comms_status_changed, (), (override));
  MOCK_METHOD(void, xcom_set_comms_status, (int status), (override));
  MOCK_METHOD(void, xcom_signal_comms_status_changed, (int status), (override));
  MOCK_METHOD(enum_gcs_error, xcom_wait_exit, (), (override));
  MOCK_METHOD(bool, xcom_is_exit, (), (override));
  MOCK_METHOD(void, xcom_set_exit, vd(bool), (override));
  MOCK_METHOD(void, xcom_signal_exit, (), (override));
  MOCK_METHOD(int, xcom_client_force_config,
              (connection_descriptor * fd, node_list *nl, uint32_t group_id),
              (override));
  MOCK_METHOD(bool, xcom_client_force_config,
              (node_list * nl, uint32_t group_id), (override));

  MOCK_METHOD(bool, get_should_exit, (), (override));
  MOCK_METHOD(void, set_should_exit, (bool should_exit), (override));

  MOCK_METHOD(bool, xcom_input_connect,
              (std::string const &address, xcom_port port), (override));
  MOCK_METHOD(void, xcom_input_disconnect, (), (override));
  MOCK_METHOD(bool, xcom_input_try_push, (app_data_ptr data), (override));
  /* Mocking fails compilation on Windows. It attempts to copy the std::future
   * which is non-copyable. */
  Gcs_xcom_input_queue::future_reply xcom_input_try_push_and_get_reply(
      app_data_ptr) override {
    return std::future<std::unique_ptr<Gcs_xcom_input_queue::Reply>>();
  }
  MOCK_METHOD(xcom_input_request_ptr, xcom_input_try_pop, (), (override));
};

class mock_gcs_xcom_view_change_control_interface
    : public Gcs_xcom_view_change_control_interface {
 public:
  MOCK_METHOD(void, start_view_exchange, (), (override));
  MOCK_METHOD(void, end_view_exchange, (), (override));
  MOCK_METHOD(void, wait_for_view_change_end, (), (override));
  MOCK_METHOD(bool, is_view_changing, (), (override));
  MOCK_METHOD(bool, start_leave, (), (override));
  MOCK_METHOD(void, end_leave, (), (override));
  MOCK_METHOD(bool, is_leaving, (), (override));
  MOCK_METHOD(bool, start_join, (), (override));
  MOCK_METHOD(void, end_join, (), (override));
  MOCK_METHOD(bool, is_joining, (), (override));

  MOCK_METHOD(void, set_current_view, (Gcs_view *), (override));
  MOCK_METHOD(Gcs_view *, get_current_view, (), (override));
  MOCK_METHOD(bool, belongs_to_group, (), (override));
  MOCK_METHOD(void, set_belongs_to_group, (bool), (override));
  MOCK_METHOD(void, set_unsafe_current_view, (Gcs_view *), (override));
  MOCK_METHOD(Gcs_view *, get_unsafe_current_view, (), (override));

  MOCK_METHOD(void, finalize, (), (override));
  MOCK_METHOD(bool, is_finalized, (), (override));
};

class XcomGroupManagementTest : public GcsBaseTest {
 protected:
  XcomGroupManagementTest() = default;

  void SetUp() override {
    group_id = new Gcs_group_identifier("only_group");
    xcom_group_mgmt_if = new Gcs_xcom_group_management(&proxy, *group_id, &vce);
  }

  void TearDown() override {
    delete xcom_group_mgmt_if;
    delete group_id;
  }

  Gcs_group_identifier *group_id;

  mock_gcs_xcom_proxy proxy;
  mock_gcs_xcom_view_change_control_interface vce;
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
  Gcs_xcom_node_information node_3("127.0.0.1:12347");

  Gcs_xcom_nodes nodes;
  nodes.add_node(node_1);
  nodes.add_node(node_2);
  nodes.add_node(node_3);

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

  nl.node_list_val =
      ::new_node_address_uuid(nl.node_list_len, node_addrs, blobs);

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

TEST_F(XcomGroupManagementTest, DisallowForcingSameMembership) {
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

  nl.node_list_val =
      ::new_node_address_uuid(nl.node_list_len, node_addrs, blobs);

  EXPECT_CALL(proxy, xcom_client_force_config(node_list_pointer_matcher(nl), _))
      .Times(0);

  xcom_group_mgmt_if->set_xcom_nodes(nodes);

  Gcs_interface_parameters forced_group_1;
  forced_group_1.add_parameter("peer_nodes", "127.0.0.1:12346,127.0.0.1:12345");
  enum_gcs_error result_1 =
      xcom_group_mgmt_if->modify_configuration(forced_group_1);
  ASSERT_EQ(GCS_NOK, result_1);

  Gcs_interface_parameters forced_group_2;
  forced_group_2.add_parameter("peer_nodes", "127.0.0.1:12345,127.0.0.1:12346");
  enum_gcs_error result_2 =
      xcom_group_mgmt_if->modify_configuration(forced_group_2);
  ASSERT_EQ(GCS_NOK, result_2);

  ASSERT_EQ((unsigned)2, nl.node_list_len);
  ASSERT_STREQ("127.0.0.1:12345", nl.node_list_val[0].address);
  ASSERT_STREQ("127.0.0.1:12346", nl.node_list_val[1].address);

  ::delete_node_address(nl.node_list_len, nl.node_list_val);

  free(blobs[0].data.data_val);
  free(blobs[1].data.data_val);
}

TEST_F(XcomGroupManagementTest, getWriteConcurrencyGroupLeaving) {
  EXPECT_CALL(vce, is_leaving()).Times(1).WillOnce(Return(true));

  uint32_t out_value;
  enum_gcs_error result = xcom_group_mgmt_if->get_write_concurrency(out_value);

  ASSERT_EQ(GCS_NOK, result);
}

TEST_F(XcomGroupManagementTest, getWriteConcurrencyNoGroup) {
  EXPECT_CALL(vce, is_leaving()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(proxy, xcom_is_exit()).Times(1).WillOnce(Return(true));

  uint32_t out_value;
  enum_gcs_error result = xcom_group_mgmt_if->get_write_concurrency(out_value);

  ASSERT_EQ(GCS_NOK, result);
}

}  // namespace gcs_xcom_groupmanagement_unittest
