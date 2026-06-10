/*
  Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "dest_metadata_cache.h"

#include <stdexcept>
#include <string>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>

#include "destination.h"
#include "mysql/harness/destination.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/metadata_cache.h"
#include "router_test_helpers.h"  // ASSERT_THROW_LIKE
#include "test/helpers.h"         // init_test_logger

using metadata_cache::ServerMode;
using metadata_cache::ServerRole;
using InstanceVector = std::vector<metadata_cache::ManagedInstance>;

using ::testing::_;
using ::testing::ElementsAre;

using namespace std::chrono_literals;

constexpr auto GR = mysqlrouter::InstanceType::GroupMember;

class MetadataCacheAPIStub : public metadata_cache::MetadataCacheAPIBase {
 public:
  metadata_cache::cluster_nodes_list_t get_cluster_nodes() override {
    if (cluster_topology_.clusters_data.size() == 0) return {};

    return cluster_topology_.clusters_data[0].members;
  }

  metadata_cache::ClusterTopology get_cluster_topology() override {
    return cluster_topology_;
  }

  void add_state_listener(
      metadata_cache::ClusterStateListenerInterface *listener) override {
    instances_change_listener_ = listener;
  }

  void remove_state_listener(
      metadata_cache::ClusterStateListenerInterface *) override {
    instances_change_listener_ = nullptr;
  }

  MOCK_METHOD(void, add_acceptor_handler_listener,
              (metadata_cache::AcceptorUpdateHandlerInterface *), (override));
  MOCK_METHOD(void, remove_acceptor_handler_listener,
              (metadata_cache::AcceptorUpdateHandlerInterface *), (override));
  MOCK_METHOD(void, add_md_refresh_listener,
              (metadata_cache::MetadataRefreshListenerInterface *), (override));
  MOCK_METHOD(void, remove_md_refresh_listener,
              (metadata_cache::MetadataRefreshListenerInterface *), (override));

  MOCK_METHOD(void, enable_fetch_auth_metadata, (), (override));
  MOCK_METHOD(void, force_cache_update, (), (override));
  MOCK_METHOD(void, check_auth_metadata_timers, (), (const, override));

  MOCK_METHOD((std::pair<bool, std::pair<std::string, rapidjson::Document>>),
              get_rest_user_auth_data, (const std::string &),
              (const, override));

  MOCK_METHOD(bool, wait_primary_failover,
              (const std::string &, const std::chrono::seconds &), (override));

  MOCK_METHOD(void, handle_sockets_acceptors_on_md_refresh, (), (override));

  // cannot mock it as it has more than 10 parameters
  void cache_init(
      const mysqlrouter::ClusterType /*cluster_type*/, unsigned /*router_id*/,
      const std::string & /*clusterset_id*/,
      const std::vector<mysql_harness::TcpDestination> & /*metadata_servers*/,
      const metadata_cache::MetadataCacheTTLConfig & /*ttl_config*/,
      const mysqlrouter::SSLOptions & /*ssl_options*/,
      const mysqlrouter::TargetCluster & /*target_cluster*/,
      const metadata_cache::MetadataCacheMySQLSessionConfig
          & /*session_config*/,
      const metadata_cache::RouterAttributes &,
      size_t /*thread_stack_size*/ =
          mysql_harness::kDefaultStackSizeInKiloBytes,
      bool /*use_gr_notifications*/ = false, uint64_t /*view_id*/ = 0,
      bool /* close_connection_after_refresh */ = false) override {}

  mysqlrouter::ClusterType cluster_type() const override {
    return mysqlrouter::ClusterType::GR_V2;
  }

  MOCK_METHOD(void, cache_start, (), (override));

  void cache_stop() noexcept override {}  // no easy way to mock noexcept method
  bool is_initialized() noexcept override { return true; }

  void instance_name(const std::string &) override {}
  std::string instance_name() const override { return "foo"; }
  mysqlrouter::TargetCluster target_cluster() const override {
    return {mysqlrouter::TargetCluster::TargetType::ByName, "foo"};
  }
  std::chrono::milliseconds ttl() const override { return {}; }

  RefreshStatus get_refresh_status() override { return {}; }

  MOCK_METHOD(void, set_instance_factory, (metadata_factory_t cb), (override));

  MOCK_METHOD(void, add_routing_guidelines_update_callbacks,
              (update_routing_guidelines_callback_t,
               on_routing_guidelines_change_callback_t),
              (override));
  MOCK_METHOD(void, clear_routing_guidelines_update_callbacks, (), (override));
  MOCK_METHOD(void, add_router_info_update_callback,
              (update_router_info_callback_t), (override));
  MOCK_METHOD(void, clear_router_info_update_callback, (), (override));

 public:
  void fill_instance_vector(const InstanceVector &iv) {
    metadata_cache::metadata_servers_list_t md_servers;
    for (const auto &instance : iv) {
      md_servers.emplace_back(instance.host, instance.port);
    }

    metadata_cache::ManagedCluster cluster{"cluster-uuid", "cluster-name", iv,
                                           true};

    cluster_topology_ =
        metadata_cache::ClusterTopology{{cluster}, 0, md_servers};
  }

  void trigger_instances_change_callback(
      const bool md_servers_reachable = true) {
    if (!instances_change_listener_) return;

    instances_change_listener_->notify_instances_changed(md_servers_reachable,
                                                         0);
  }

  metadata_cache::ClusterTopology cluster_topology_;
  metadata_cache::ClusterStateListenerInterface *instances_change_listener_{
      nullptr};
};

class DestMetadataCacheTest : public ::testing::Test {
 public:
  DestMetadataCacheTest() {
    conf_.protocol = Protocol::Type::kClassicProtocol;
    conf_.bind_address = mysql_harness::TcpDestination{"", 3306};
  }

 protected:
  void fill_instance_vector(const InstanceVector &iv) {
    metadata_cache_api_.fill_instance_vector(iv);
  }
  RoutingConfig conf_;
  MySQLRoutingContext routing_ctx_{conf_, "routing_name", {}, {}, nullptr};
  ::testing::StrictMock<MetadataCacheAPIStub> metadata_cache_api_;
  net::io_context io_ctx_;
};

/*****************************************/
/*ALLOWED NODES CALLBACK TESTS          */
/*****************************************/

/**
 * @test verifies that when the metadata changes and there is no primary node,
 *       then allowed_nodes that gets passed to read-write destination is empty
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesNoPrimary) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      DestMetadataCacheManager::ServerRole::Primary, &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  });

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // new metadata - no primary
  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  });

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    // no primaries so we expect empty set as we are role=PRIMARY
    ASSERT_EQ(0u, nodes.size());
    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata change", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback();

  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test verifies that when the metadata changes and there are 2 r/w nodes,
 *       then allowed_nodes that gets passed to read-write destination has both
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodes2Primaries) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      DestMetadataCacheManager::ServerRole::Primary, &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  };

  fill_instance_vector(instances);

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // new metadata - 2 primaries
  instances[1].mode = metadata_cache::ServerMode::ReadWrite;
  fill_instance_vector(instances);

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    // 2 primaries and we are role=PRIMARY
    ASSERT_THAT(
        nodes,
        ::testing::ElementsAre(
            ::testing::Field(&AvailableDestination::destination,
                             mysql_harness::TcpDestination{instances[0].host,
                                                           instances[0].port}),
            ::testing::Field(&AvailableDestination::destination,
                             mysql_harness::TcpDestination{
                                 instances[1].host, instances[1].port})));

    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata change", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback();

  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test verifies that when the metadata changes and there is only single r/w
 * node, then allowed_nodes that gets passed to read-only destination observer
 * has this node (it should as by default disconnect_on_promoted_to_primary=no)
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesNoSecondaries) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      DestMetadataCacheManager::ServerRole::Secondary, &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  };

  fill_instance_vector(instances);

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // remove last node, leaving only the one primary
  instances.pop_back();
  fill_instance_vector(instances);

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    // no secondaries and we are role=SECONDARY
    // by default we allow existing connections to the primary so it should
    // be in the allowed nodes
    ASSERT_THAT(nodes, ::testing::ElementsAre(::testing::Field(
                           &AvailableDestination::destination,
                           mysql_harness::TcpDestination{instances[0].host,
                                                         instances[0].port})));
    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata change", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback();

  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test verifies that for the read-only destination r/w node is not among
 * allowed_nodes if disconnect_on_promoted_to_primary=yes is configured
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesSecondaryDisconnectToPromoted) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI(
          "metadata-cache://cache-name/"
          "default?role=SECONDARY&disconnect_on_promoted_to_primary=yes")
          .query,
      DestMetadataCacheManager::ServerRole::Secondary, &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  };

  fill_instance_vector(instances);

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // let's stick to the 'old' md so we have single primary and single secondary

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    // one secondary and we are role=SECONDARY
    // we have disconnect_on_promoted_to_primary=yes configured so primary is
    // not allowed
    ASSERT_THAT(nodes, ::testing::ElementsAre(::testing::Field(
                           &AvailableDestination::destination,
                           mysql_harness::TcpDestination{instances[1].host,
                                                         instances[1].port})));
    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata change", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback();

  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test
 *      Verify that if disconnect_on_promoted_to_primary is used more than once,
 * then the last stated value is used, e.g.
 *
 *      &disconnect_on_promoted_to_primary=no&disconnect_on_promoted_to_primary=yes
 *
 * is considered the same as
 *
 *      &disconnect_on_promoted_to_primary=yes
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesSecondaryDisconnectToPromotedTwice) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI("metadata-cache://cache-name/"
                       "default?role=SECONDARY&disconnect_on_promoted_to_"
                       "primary=no&disconnect_on_promoted_to_primary=yes")
          .query,
      DestMetadataCacheManager::ServerRole::Secondary, &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  };

  fill_instance_vector(instances);

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // let's stick to the 'old' md so we have single primary and single secondary
  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    // one secondary and we are role=SECONDARY
    // disconnect_on_promoted_to_primary=yes overrides previous value in
    // configuration so primary is not allowed
    ASSERT_THAT(nodes, ::testing::ElementsAre(::testing::Field(
                           &AvailableDestination::destination,
                           mysql_harness::TcpDestination{instances[1].host,
                                                         instances[1].port})));
    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata change", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback();

  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test verifies that when metadata becomes unavailable the change notifier is
 * not called (because by default disconnect_on_metadata_unavailable=no)
 *
 */
TEST_F(DestMetadataCacheTest,
       AllowedNodesEmptyKeepConnectionsIfMetadataUnavailable) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI("metadata-cache://cache-name/"
                       "default?role=SECONDARY")
          .query,
      DestMetadataCacheManager::ServerRole::Secondary, &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  });

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // new empty metadata
  fill_instance_vector({});

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    ASSERT_EQ(0u, nodes.size());
    ASSERT_FALSE(disconnect);
    ASSERT_STREQ("metadata unavailable", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback(
      /*md_servers_reachable=*/false);

  // the metadata has changed but we got the notification that this is triggered
  // because md servers are not reachable as disconnect_on_metadata_unavailable
  // is set to 'no' (by default) we are not expected to force the disconnects
  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/**
 * @test verifies that when metadata becomes unavailable the change notifier is
 *       called with empyt allowed_nodes set when
 * disconnect_on_metadata_unavailable=yes is configured
 *
 */
TEST_F(DestMetadataCacheTest,
       AllowedNodesEmptyDisconnectConnectionsIfMetadataUnavailable) {
  RoutingConfig conf;
  conf.protocol = Protocol::Type::kClassicProtocol;
  MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};
  DestMetadataCacheManager dest_mc_group(
      io_ctx_, routing_ctx, "cache-name",
      mysqlrouter::URI(
          "metadata-cache://cache-name/"
          "default?role=SECONDARY&disconnect_on_metadata_unavailable=yes")
          .query,
      DestMetadataCacheManager::ServerRole::Secondary, &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060, "label"},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070, "label"},
  });

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // new empty metadata
  fill_instance_vector({});

  bool callback_called{false};
  auto check_nodes = [&](const AllowedNodes &nodes, const AllowedNodes &,
                         const bool disconnect,
                         const std::string &disconnect_reason) -> void {
    ASSERT_EQ(0u, nodes.size());
    ASSERT_TRUE(disconnect);
    ASSERT_STREQ("metadata unavailable", disconnect_reason.c_str());
    callback_called = true;
  };
  dest_mc_group.register_allowed_nodes_change_callback(check_nodes);
  metadata_cache_api_.trigger_instances_change_callback(
      /*md_servers_reachable=*/false);

  // the metadata has changed and we got the notification that this is triggered
  // because md servers are not reachable as
  // disconnect_on_metadata_unavailable=yes we are expected to call the users
  // (routing) callbacks to force the disconnects
  ASSERT_TRUE(callback_called);
  EXPECT_CALL(metadata_cache_api_, remove_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, remove_md_refresh_listener(_));
}

/*****************************************/
/*URI parsing tests                      */
/*****************************************/
TEST_F(DestMetadataCacheTest,
       MetadataCacheGroupAllowPrimaryReadsNoLongerSupported) {
  {
    RecordProperty("Worklog", "15872");
    RecordProperty("RequirementId", "FR1");
    RecordProperty("Description",
                   "Checks that the Router logs a proper error message when "
                   "allow_primary_reads parameter is used in the "
                   "[routing].destinations URI");

    mysqlrouter::URI uri(
        "metadata-cache://test/default?allow_primary_reads=yes&role=SECONDARY");
    ASSERT_THROW_LIKE(
        DestMetadataCacheManager dest(
            io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
            DestMetadataCacheManager::ServerRole::Secondary),
        std::runtime_error,
        "allow_primary_reads is no longer supported, use "
        "role=PRIMARY_AND_SECONDARY instead");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheGroupDisconnectOnPromotedToPrimary) {
  // yes valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=yes");
    ASSERT_NO_THROW(DestMetadataCacheManager dest(
        io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
        DestMetadataCacheManager::ServerRole::Secondary));
  }

  // no valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=no");
    ASSERT_NO_THROW(DestMetadataCacheManager dest(
        io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
        DestMetadataCacheManager::ServerRole::Secondary));
  }

  // invalid option
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=invalid");
    ASSERT_THROW_LIKE(
        DestMetadataCacheManager dest(
            io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
            DestMetadataCacheManager::ServerRole::Secondary),
        std::runtime_error,
        "Invalid value for option 'disconnect_on_promoted_to_primary'. Allowed "
        "are 'yes' and 'no'");
  }

  // incompatible role, valid only for secondary
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=PRIMARY&disconnect_on_promoted_to_primary=invalid");
    ASSERT_THROW_LIKE(
        DestMetadataCacheManager dest(
            io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
            DestMetadataCacheManager::ServerRole::Primary),
        std::runtime_error,
        "Option 'disconnect_on_promoted_to_primary' is valid "
        "only for role=SECONDARY");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheDisconnectOnMetadataUnavailable) {
  // yes valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=yes");
    ASSERT_NO_THROW(DestMetadataCacheManager dest(
        io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
        DestMetadataCacheManager::ServerRole::Secondary));
  }

  // no valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=no");
    ASSERT_NO_THROW(DestMetadataCacheManager dest(
        io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
        DestMetadataCacheManager::ServerRole::Secondary));
  }

  // invalid option
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=invalid");
    ASSERT_THROW_LIKE(
        DestMetadataCacheManager dest(
            io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
            DestMetadataCacheManager::ServerRole::Secondary),
        std::runtime_error,
        "Invalid value for option 'disconnect_on_metadata_unavailable'. "
        "Allowed are 'yes' and 'no'");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheGroupUnknownParam) {
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/default?role=SECONDARY&xxx=yyy,metadata-cache://"
        "test2/default?role=SECONDARY");
    ASSERT_THROW_LIKE(
        DestMetadataCacheManager dest(
            io_ctx_, routing_ctx_, "metadata_cache_name", uri.query,
            DestMetadataCacheManager::ServerRole::Secondary),
        std::runtime_error,
        "Unsupported 'metadata-cache' parameter in URI: 'xxx'");
  }
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
