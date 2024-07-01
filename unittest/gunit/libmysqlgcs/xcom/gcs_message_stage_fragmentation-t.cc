/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <atomic>
#include <thread>
#include "gcs_base_test.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_split.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/network_provider_manager.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_manager.h"

namespace gcs_message_stage_fragmentation_unittest {

bool mock_xcom_client_send_data(unsigned long long, char *data) {
  std::free(data);
  return true;
}

class Mock_gcs_xcom_proxy : public Gcs_xcom_proxy_base {
 public:
  Mock_gcs_xcom_proxy() {
    ON_CALL(*this, xcom_client_send_data(_, _))
        .WillByDefault(Invoke(&mock_xcom_client_send_data));
  }

  MOCK_METHOD(node_address *, new_node_address_uuid,
              (unsigned int n, char const *names[], blob uuids[]), (override));
  MOCK_METHOD(void, delete_node_address, (unsigned int n, node_address *na),
              (override));
  MOCK_METHOD(bool, xcom_client_add_node,
              (connection_descriptor * con, node_list *nl, uint32_t group_id),
              (override));
  MOCK_METHOD(bool, xcom_client_remove_node,
              (node_list * nl, uint32_t group_id), (override));
  MOCK_METHOD(bool, xcom_client_remove_node,
              (connection_descriptor * con, node_list *nl, uint32_t group_id),
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
  MOCK_METHOD(bool, xcom_client_set_cache_size, (uint64_t), (override));
  MOCK_METHOD(bool, xcom_client_boot, (node_list * nl, uint32_t group_id),
              (override));
  MOCK_METHOD(connection_descriptor *, xcom_client_open_connection,
              (std::string, xcom_port port), (override));
  MOCK_METHOD(bool, xcom_client_close_connection, (connection_descriptor * con),
              (override));
  MOCK_METHOD(bool, client_send_data, (unsigned long long size, char *data),
              (override));
  MOCK_METHOD(void, xcom_init, (xcom_port listen_port), (override));
  MOCK_METHOD(void, xcom_exit, (), (override));
  MOCK_METHOD(void, xcom_set_cleanup, (), (override));
  MOCK_METHOD(int, xcom_get_ssl_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_set_ssl_mode, (int mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_fips_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_set_ssl_fips_mode, (int mode), (override));
  MOCK_METHOD(bool, xcom_init_ssl, (), (override));
  MOCK_METHOD(void, xcom_destroy_ssl, (), (override));
  MOCK_METHOD(bool, xcom_use_ssl, (), (override));
  MOCK_METHOD(void, xcom_set_ssl_parameters,
              (ssl_parameters ssl, tls_parameters tls), (override));
  MOCK_METHOD(site_def const *, find_site_def, (synode_no synode), (override));
  MOCK_METHOD(bool, xcom_open_handlers, (std::string saddr, xcom_port port),
              (override));
  MOCK_METHOD(bool, xcom_close_handlers, (), (override));
  MOCK_METHOD(int, xcom_acquire_handler, (), (override));
  MOCK_METHOD(void, xcom_release_handler, (int index), (override));
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
  MOCK_METHOD(void, xcom_set_exit, (bool), (override));
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
      app_data_ptr) {
    return std::future<std::unique_ptr<Gcs_xcom_input_queue::Reply>>();
  }
  MOCK_METHOD(xcom_input_request_ptr, xcom_input_try_pop, (), (override));
};

class Mock_gcs_xcom_view_change_control_interface
    : public Gcs_xcom_view_change_control_interface {
 public:
  Mock_gcs_xcom_view_change_control_interface() {
    ON_CALL(*this, belongs_to_group()).WillByDefault(Return(true));
  }
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

class Mock_gcs_communication_event_listener
    : public Gcs_communication_event_listener {
 public:
  MOCK_METHOD(void, on_message_received, (const Gcs_message &message),
              (const, override));
};

class Mock_gcs_network_provider_management_interface
    : public Network_provider_management_interface {
 public:
  MOCK_METHOD(bool, initialize, (), (override));
  MOCK_METHOD(bool, finalize, (), (override));
  MOCK_METHOD(void, set_running_protocol, (enum_transport_protocol new_value),
              (override));
  MOCK_METHOD(void, add_network_provider,
              (std::shared_ptr<Network_provider> provider), (override));
  MOCK_METHOD(enum_transport_protocol, get_running_protocol, (),
              (const, override));
  MOCK_METHOD(enum_transport_protocol, get_incoming_connections_protocol, (),
              (const, override));
  MOCK_METHOD(int, is_xcom_using_ssl, (), (const, override));
  MOCK_METHOD(int, xcom_set_ssl_mode, (int mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_mode, (), (override));
  MOCK_METHOD(int, xcom_set_ssl_fips_mode, (int mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_fips_mode, (const char *mode), (override));
  MOCK_METHOD(int, xcom_get_ssl_fips_mode, (), (override));
  MOCK_METHOD(void, cleanup_secure_connections_context, (), (override));
  MOCK_METHOD(void, delayed_cleanup_secure_connections_context, (), (override));
  MOCK_METHOD(void, finalize_secure_connections_context, (), (override));
  MOCK_METHOD(void, remove_all_network_provider, (), (override));
  MOCK_METHOD(void, remove_network_provider, (enum_transport_protocol),
              (override));
};

class Mock_gcs_xcom_statistics_manager
    : public Gcs_xcom_statistics_manager_interface {
  MOCK_METHOD(uint64_t, get_sum_var_value,
              (Gcs_cumulative_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_sum_var_value,
              (Gcs_cumulative_statistics_enum to_set, uint64_t to_add),
              (override));

  // COUNT VARS
  MOCK_METHOD(uint64_t, get_count_var_value,
              (Gcs_counter_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_count_var_value, (Gcs_counter_statistics_enum to_set),
              (override));

  // TIMESTAMP VALUES
  MOCK_METHOD(unsigned long long, get_timestamp_var_value,
              (Gcs_time_statistics_enum to_get), (const, override));
  MOCK_METHOD(void, set_timestamp_var_value,
              (Gcs_time_statistics_enum to_set, unsigned long long new_value),
              (override));
  MOCK_METHOD(void, set_sum_timestamp_var_value,
              (Gcs_time_statistics_enum to_set, unsigned long long to_add),
              (override));

  // ALL OTHER VARS
  MOCK_METHOD(std::vector<Gcs_node_suspicious>, get_all_suspicious, (),
              (const, override));
  MOCK_METHOD(void, add_suspicious_for_a_node, (std::string node_id),
              (override));
};

class GcsMessageStageFragmentationTest : public GcsBaseTest {
 protected:
  Gcs_xcom_engine m_engine;
  Gcs_group_identifier m_mock_gid{"mock_group"};
  Gcs_xcom_node_address m_mock_xcom_address{"127.0.0.1:12345"};
  Mock_gcs_xcom_statistics_manager m_mock_stats;
  Mock_gcs_xcom_proxy m_mock_proxy;
  Mock_gcs_xcom_view_change_control_interface m_mock_vce;

  Gcs_xcom_communication m_xcom_comm_if{
      &m_mock_stats,
      &m_mock_proxy,
      &m_mock_vce,
      &m_engine,
      m_mock_gid,
      std::make_unique<Mock_gcs_network_provider_management_interface>()};
  Gcs_message_stage_split_v2 *m_fragmentation_stage{nullptr};

 public:
  GcsMessageStageFragmentationTest() {
    static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface())
        ->set_xcom_group_information(m_mock_gid.get_group_id());
    static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface())
        ->set_node_address(m_mock_xcom_address.get_member_address());
  }

  void configure_pipeline(bool const fragmentation_enabled,
                          unsigned long long const fragmentation_threshold) {
    Gcs_message_pipeline &pipeline = m_xcom_comm_if.get_msg_pipeline();
    pipeline.register_stage<Gcs_message_stage_split_v2>(
        fragmentation_enabled, fragmentation_threshold);
    // clang-format off
    pipeline.register_pipeline({
      {
        Gcs_protocol_version::HIGHEST_KNOWN, { Stage_code::ST_SPLIT_V2 }
      }
    });
    // clang-format on
    m_fragmentation_stage = static_cast<Gcs_message_stage_split_v2 *>(
        &pipeline.get_stage(Stage_code::ST_SPLIT_V2));
  }
};

/* Verify that the reassembly of fragments whose delivery crosses views works.
 */
TEST_F(GcsMessageStageFragmentationTest, ReassemblyOfFragmentsThatCrossViews) {
  Mock_gcs_communication_event_listener ev_listener;
  int listener_ref = m_xcom_comm_if.add_event_listener(ev_listener);
  EXPECT_CALL(ev_listener, on_message_received(_)).Times(1);

  std::string payload("payload!");

  bool constexpr FRAGMENT = true;
  unsigned long long constexpr FRAGMENT_THRESHOLD = 10;
  configure_pipeline(FRAGMENT, FRAGMENT_THRESHOLD);

  /* Current view:
       0 -> some other guy
       1 -> me */
  std::unique_ptr<Gcs_xcom_nodes> xcom_nodes_first_view(new Gcs_xcom_nodes());
  xcom_nodes_first_view->add_node(
      Gcs_xcom_node_information("127.0.0.1:54321", Gcs_xcom_uuid(), 0, true));
  xcom_nodes_first_view->add_node(Gcs_xcom_node_information(
      m_mock_xcom_address.get_member_address(), Gcs_xcom_uuid(), 1, true));
  m_fragmentation_stage->update_members_information(
      Gcs_member_identifier(m_mock_xcom_address.get_member_address()),
      *xcom_nodes_first_view);

  Gcs_message_data *message_data = new Gcs_message_data(0, payload.size());
  message_data->append_to_payload(
      reinterpret_cast<uchar const *>(payload.c_str()), payload.size());
  ASSERT_GT(message_data->get_encode_size(), FRAGMENT_THRESHOLD);

  /* Get the serialized fragments. */
  bool error = true;
  std::vector<Gcs_packet> packets_out;
  std::tie(error, packets_out) =
      m_xcom_comm_if.get_msg_pipeline().process_outgoing(
          *message_data, Cargo_type::CT_USER_DATA);
  ASSERT_FALSE(error);
  ASSERT_EQ(packets_out.size(), 2);

  /* Mock sending the packets to affect the protocol changer. */
  Gcs_message message(
      Gcs_member_identifier(m_mock_xcom_address.get_member_address()),
      m_mock_gid, message_data);
  enum_gcs_error message_result = m_xcom_comm_if.send_message(message);
  ASSERT_EQ(GCS_OK, message_result);

  /* Receive first fragment in one view. */
  // I am currently node 1.
  synode_no synod_first_fragment;
  synod_first_fragment.group_id =
      Gcs_xcom_utils::build_xcom_group_id(m_mock_gid);
  synod_first_fragment.msgno = 0;
  synod_first_fragment.node = 1;
  Gcs_packet::buffer_ptr buffer_pointer = nullptr;
  unsigned long long buffer_size = 0;
  std::tie(buffer_pointer, buffer_size) = packets_out.at(0).serialize();
  auto first_fragment = Gcs_packet::make_incoming_packet(
      std::move(buffer_pointer), buffer_size, synod_first_fragment,
      synod_first_fragment, m_xcom_comm_if.get_msg_pipeline());
  m_xcom_comm_if.process_user_data_packet(std::move(first_fragment),
                                          std::move(xcom_nodes_first_view));

  /* Receive last fragment in other view: 0 -> me. */
  std::unique_ptr<Gcs_xcom_nodes> xcom_nodes_last_view(new Gcs_xcom_nodes());
  xcom_nodes_last_view->add_node(Gcs_xcom_node_information(
      m_mock_xcom_address.get_member_address(), Gcs_xcom_uuid(), 0, true));
  m_fragmentation_stage->update_members_information(
      Gcs_member_identifier(m_mock_xcom_address.get_member_address()),
      *xcom_nodes_last_view);
  // I am currently node 0.
  synode_no synod_last_fragment;
  synod_last_fragment.group_id =
      Gcs_xcom_utils::build_xcom_group_id(m_mock_gid);
  synod_last_fragment.msgno = 1;
  synod_last_fragment.node = 0;
  std::tie(buffer_pointer, buffer_size) = packets_out.at(1).serialize();
  auto last_fragment = Gcs_packet::make_incoming_packet(
      std::move(buffer_pointer), buffer_size, synod_last_fragment,
      synod_last_fragment, m_xcom_comm_if.get_msg_pipeline());
  Gcs_view mock_view(
      {Gcs_member_identifier(m_mock_xcom_address.get_member_address())},
      Gcs_xcom_view_identifier(0, 0), {}, {}, m_mock_gid);
  EXPECT_CALL(m_mock_vce, get_unsafe_current_view())
      .Times(1)
      .WillOnce(Return(&mock_view));
  m_xcom_comm_if.process_user_data_packet(std::move(last_fragment),
                                          std::move(xcom_nodes_last_view));

  m_xcom_comm_if.remove_event_listener(listener_ref);
}

}  // namespace gcs_message_stage_fragmentation_unittest
