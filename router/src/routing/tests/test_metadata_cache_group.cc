/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
#include "mysqlrouter/destination.h"
#include "mysqlrouter/metadata_cache.h"
#include "router_test_helpers.h"  // ASSERT_THROW_LIKE
#include "tcp_address.h"
#include "test/helpers.h"  // init_test_logger

using metadata_cache::ServerMode;
using metadata_cache::ServerRole;
using InstanceVector = std::vector<metadata_cache::ManagedInstance>;

using ::testing::_;

using namespace std::chrono_literals;

constexpr auto GR = metadata_cache::InstanceType::GroupMember;

bool operator==(const std::unique_ptr<Destination> &a, const Destination &b) {
  return a->hostname() == b.hostname() && a->port() == b.port();
}

std::ostream &operator<<(std::ostream &os, const Destination &v) {
  os << "(host: " << v.hostname() << ", port: " << v.port() << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const std::unique_ptr<Destination> &v) {
  os << *(v.get());
  return os;
}

std::ostream &operator<<(std::ostream &os, const Destinations &v) {
  for (const auto &dest : v) {
    os << dest;
  }
  return os;
}

MATCHER(IsGoodEq, "") {
  return ::testing::ExplainMatchResult(
      ::testing::Property(&Destination::good, std::get<1>(arg)),
      std::get<0>(arg).get(), result_listener);
}

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

  MOCK_METHOD1(add_acceptor_handler_listener,
               void(metadata_cache::AcceptorUpdateHandlerInterface *));
  MOCK_METHOD1(remove_acceptor_handler_listener,
               void(metadata_cache::AcceptorUpdateHandlerInterface *));
  MOCK_METHOD1(add_md_refresh_listener,
               void(metadata_cache::MetadataRefreshListenerInterface *));
  MOCK_METHOD1(remove_md_refresh_listener,
               void(metadata_cache::MetadataRefreshListenerInterface *));

  MOCK_METHOD0(enable_fetch_auth_metadata, void());
  MOCK_METHOD0(force_cache_update, void());
  MOCK_CONST_METHOD0(check_auth_metadata_timers, void());

  MOCK_CONST_METHOD1(
      get_rest_user_auth_data,
      std::pair<bool, std::pair<std::string, rapidjson::Document>>(
          const std::string &));

  MOCK_METHOD2(wait_primary_failover,
               bool(const std::string &, const std::chrono::seconds &));

  MOCK_METHOD0(handle_sockets_acceptors_on_md_refresh, void());

  // cannot mock it as it has more than 10 parameters
  void cache_init(
      const mysqlrouter::ClusterType /*cluster_type*/, unsigned /*router_id*/,
      const std::string & /*clusterset_id*/,
      const std::vector<mysql_harness::TCPAddress> & /*metadata_servers*/,
      const metadata_cache::MetadataCacheTTLConfig & /*ttl_config*/,
      const mysqlrouter::SSLOptions & /*ssl_options*/,
      const mysqlrouter::TargetCluster & /*target_cluster*/,
      const metadata_cache::MetadataCacheMySQLSessionConfig
          & /*session_config*/,
      const metadata_cache::RouterAttributes &,
      size_t /*thread_stack_size*/ =
          mysql_harness::kDefaultStackSizeInKiloBytes,
      bool /*use_gr_notifications*/ = false,
      uint64_t /*view_id*/ = 0) override {}

  mysqlrouter::ClusterType cluster_type() const override {
    return mysqlrouter::ClusterType::GR_V1;
  }

  MOCK_METHOD0(cache_start, void());

  void cache_stop() noexcept override {}  // no easy way to mock noexcept method
  bool is_initialized() noexcept override { return true; }
  bool fetch_whole_topology() const override { return false; }

  void fetch_whole_topology(bool /*val*/) override {}

  void instance_name(const std::string &) override {}
  std::string instance_name() const override { return "foo"; }
  mysqlrouter::TargetCluster target_cluster() const override {
    return {mysqlrouter::TargetCluster::TargetType::ByName, "foo"};
  }
  std::chrono::milliseconds ttl() const override { return {}; }

  RefreshStatus get_refresh_status() override { return {}; }

  MOCK_METHOD1(set_instance_factory, void(metadata_factory_t cb));

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

    instances_change_listener_->notify_instances_changed(
        cluster_topology_, md_servers_reachable, 0);
  }

  metadata_cache::ClusterTopology cluster_topology_;
  metadata_cache::ClusterStateListenerInterface *instances_change_listener_{
      nullptr};
};

class DestMetadataCacheTest : public ::testing::Test {
 protected:
  void fill_instance_vector(const InstanceVector &iv) {
    metadata_cache_api_.fill_instance_vector(iv);
  }

  ::testing::StrictMock<MetadataCacheAPIStub> metadata_cache_api_;
  net::io_context io_ctx_;
};

/*****************************************/
/*STRATEGY FIRST AVAILABLE               */
/*****************************************/
TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnPrimaries) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnSinglePrimary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // only one PRIMARY
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnNoPrimary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // no PRIMARY
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnSecondaries) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // two SECONDARY's
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnSingleSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // one SECONDARY
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailableOnNoSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadWrite, ServerRole::Primary, "3308", 3308,
       33062},
  });

  // no SECONDARY
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }
}

TEST_F(DestMetadataCacheTest, StrategyFirstAvailablePrimaryAndSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
      mysqlrouter::URI(
          "metadata-cache://cache-name/default?role=PRIMARY_AND_SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // all nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // first available should not change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinWithFallbackUnavailableServer) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobinWithFallback,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::Unavailable, ServerRole::Unavailable, "3306",
       3306, 33060},
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3308", 3308,
       33062},
  });

  // all available nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

/*****************************************/
/*STRATEGY ROUND ROBIN                   */
/*****************************************/
TEST_F(DestMetadataCacheTest, StrategyRoundRobinOnPrimaries) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadWrite, ServerRole::Primary, "3308", 3308,
       33062},
      {GR, "uuid4", ServerMode::ReadOnly, ServerRole::Secondary, "3309", 3309,
       33063},
  });

  // all PRIMARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinOnSinglePrimary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // the one PRIMARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }

  // still the same
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinPrimaryMissing) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // no PRIMARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }

  // ... still the same
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinOnSecondaries) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
      {GR, "uuid4", ServerMode::ReadOnly, ServerRole::Secondary, "3309", 3309,
       33063},
  });

  // all SECONDARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3309", "3309", 3309),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinOnSingleSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // the one SECONDARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }

  // still the same
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinSecondaryMissing) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3308", 3308,
       33062},
  });

  // no SECONDARY nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }

  // ... still the same
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinPrimaryAndSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
      mysqlrouter::URI(
          "metadata-cache://cache-name/default?role=PRIMARY_AND_SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3309", 3309,
       33063},
  });

  // all nodes
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3309", "3309", 3309),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3309", "3309", 3309)));
  }
}

/*****************************************/
/*STRATEGY ROUND ROBIN_WITH_FALLBACK     */
/*****************************************/
TEST_F(DestMetadataCacheTest, StrategyRoundRobinWithFallbackBasicScenario) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobinWithFallback,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // we have 2 SECONDARIES up so we expect round robin on them
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinWithFallbackSingleSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobinWithFallback,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // we do not fallback to PRIMARIES as long as there is at least single
  // SECONDARY available
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, StrategyRoundRobinWithFallbackNoSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobinWithFallback,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
  });

  // no SECONDARY available so we expect round-robin on PRIAMRIES
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin-with-fallback should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3306", "3306", 3306)));
  }
}

TEST_F(DestMetadataCacheTest,
       StrategyRoundRobinWithFallbackPrimaryAndSecondary) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest(
          io_ctx_, "cache-name",
          routing::RoutingStrategy::kRoundRobinWithFallback,
          mysqlrouter::URI(
              "metadata-cache://cache-name/default?role=PRIMARY_AND_SECONDARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
          &metadata_cache_api_),
      std::runtime_error,
      "Strategy 'round-robin-with-fallback' is supported only for SECONDARY "
      "routing");
}

/*****************************************/
/*allow_primary_reads=yes                */
/*****************************************/
TEST_F(DestMetadataCacheTest, AllowPrimaryReadsBasic) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/"
                       "default?role=SECONDARY&allow_primary_reads=yes")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // we expect round-robin on all the servers (PRIMARY and SECONDARY)
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // round-robin should change the order.
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, AllowPrimaryReadsNoSecondary) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/"
                       "default?role=SECONDARY&allow_primary_reads=yes")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
  });

  // we expect the PRIMARY being used
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }

  // ... no change
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306)));
  }
}

/*****************************************/
/*DEFAULT_STRATEGIES                     */
/*****************************************/
TEST_F(DestMetadataCacheTest, PrimaryDefault) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadWrite, ServerRole::Primary, "3307", 3307,
       33061},
  });

  // default for PRIMARY should be round-robin on ReadWrite servers
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // .. rotate
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3306", "3306", 3306)));
  }

  // ... and back
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }
}

TEST_F(DestMetadataCacheTest, SecondaryDefault) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // default for SECONDARY should be round-robin on ReadOnly servers
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // .. rotate
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3307", "3307", 3307)));
  }

  // ... and back
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

TEST_F(DestMetadataCacheTest, PrimaryAndSecondaryDefault) {
  DestMetadataCacheGroup dest(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI(
          "metadata-cache://cache-name/default?role=PRIMARY_AND_SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33061},
      {GR, "uuid3", ServerMode::ReadOnly, ServerRole::Secondary, "3308", 3308,
       33062},
  });

  // default for PRIMARY_AND_SECONDARY should be round-robin on ReadOnly and
  // ReadWrite servers
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }

  // .. rotate
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306)));
  }

  // ... rotate
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3308", "3308", 3308),
                                       Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307)));
  }

  // ... and back
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual,
                ::testing::ElementsAre(Destination("3306", "3306", 3306),
                                       Destination("3307", "3307", 3307),
                                       Destination("3308", "3308", 3308)));
  }
}

/*****************************************/
/*ALLOWED NODES CALLBACK TESTS          */
/*****************************************/

/**
 * @test verifies that when the metadata changes and there is no primary node,
 *       then allowed_nodes that gets passed to read-write destination is empty
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesNoPrimary) {
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
  });

  EXPECT_CALL(metadata_cache_api_, add_acceptor_handler_listener(_));
  EXPECT_CALL(metadata_cache_api_, add_md_refresh_listener(_));
  dest_mc_group.start(nullptr);

  // new metadata - no primary
  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadOnly, ServerRole::Secondary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
      &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
            ::testing::Field(&AvailableDestination::address,
                             mysql_harness::TCPAddress{instances[0].host,
                                                       instances[0].port}),
            ::testing::Field(&AvailableDestination::address,
                             mysql_harness::TCPAddress{instances[1].host,
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
 * @test verifies that when the metadata changes and there is only single r/w
 * node, then allowed_nodes that gets passed to read-only destination observer
 * has this node (it should as by default disconnect_on_promoted_to_primary=no)
 *
 */
TEST_F(DestMetadataCacheTest, AllowedNodesNoSecondaries) {
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
    ASSERT_THAT(
        nodes,
        ::testing::ElementsAre(::testing::Field(
            &AvailableDestination::address,
            mysql_harness::TCPAddress{instances[0].host, instances[0].port})));
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
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI(
          "metadata-cache://cache-name/"
          "default?role=SECONDARY&disconnect_on_promoted_to_primary=yes")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
    ASSERT_THAT(
        nodes,
        ::testing::ElementsAre(::testing::Field(
            &AvailableDestination::address,
            mysql_harness::TCPAddress{instances[1].host, instances[1].port})));
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
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/"
                       "default?role=SECONDARY&disconnect_on_promoted_to_"
                       "primary=no&disconnect_on_promoted_to_primary=yes")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  InstanceVector instances{
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
    ASSERT_THAT(
        nodes,
        ::testing::ElementsAre(::testing::Field(
            &AvailableDestination::address,
            mysql_harness::TCPAddress{instances[1].host, instances[1].port})));
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
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
  DestMetadataCacheGroup dest_mc_group(
      io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
      mysqlrouter::URI(
          "metadata-cache://cache-name/"
          "default?role=SECONDARY&disconnect_on_metadata_unavailable=yes")
          .query,
      BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
      &metadata_cache_api_);

  fill_instance_vector({
      {GR, "uuid1", ServerMode::ReadWrite, ServerRole::Primary, "3306", 3306,
       33060},
      {GR, "uuid2", ServerMode::ReadOnly, ServerRole::Secondary, "3307", 3307,
       33070},
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
/*ERROR SCENARIOS                        */
/*****************************************/
TEST_F(DestMetadataCacheTest, InvalidServerNodeRole) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
          mysqlrouter::URI("metadata-cache://cache-name/default?role=INVALID")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
          &metadata_cache_api_),
      std::runtime_error,
      "The role in '?role=INVALID' does not contain one of the valid role "
      "names: PRIMARY, SECONDARY, PRIMARY_AND_SECONDARY");
}

TEST_F(DestMetadataCacheTest, UnsupportedRoutingStrategy) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name",
          routing::RoutingStrategy::kNextAvailable,  // this one is not
                                                     // supported for metadata
                                                     // cache
          mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
          &metadata_cache_api_),
      std::runtime_error, "Unsupported routing strategy: next-available");
}

TEST_F(DestMetadataCacheTest, AllowPrimaryReadsWithPrimaryRouting) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
          mysqlrouter::URI("metadata-cache://cache-name/"
                           "default?role=PRIMARY&allow_primary_reads=yes")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
          &metadata_cache_api_),
      std::runtime_error,
      "allow_primary_reads is supported only for SECONDARY routing");
}

TEST_F(DestMetadataCacheTest, AllowPrimaryReadsWithRoutingStrategy) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kRoundRobin,
          mysqlrouter::URI("metadata-cache://cache-name/"
                           "default?role=SECONDARY&allow_primary_reads=yes")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
          &metadata_cache_api_),
      std::runtime_error,
      "allow_primary_reads is only supported for backward compatibility: "
      "without routing_strategy but with mode defined, use "
      "role=PRIMARY_AND_SECONDARY instead");
}

TEST_F(DestMetadataCacheTest, RoundRobinWitFallbackStrategyWithPrimaryRouting) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name",
          routing::RoutingStrategy::kRoundRobinWithFallback,
          mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kUndefined,
          &metadata_cache_api_),
      std::runtime_error,
      "Strategy 'round-robin-with-fallback' is supported only for SECONDARY "
      "routing");
}

TEST_F(DestMetadataCacheTest, ModeWithStrategy) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kFirstAvailable,
          mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
          &metadata_cache_api_),
      std::runtime_error,
      "option 'mode' is not allowed together with 'routing_strategy' option");
}

TEST_F(DestMetadataCacheTest, RolePrimaryWrongMode) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
          mysqlrouter::URI("metadata-cache://cache-name/default?role=PRIMARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadOnly,
          &metadata_cache_api_),
      std::runtime_error, "mode 'read-only' is not valid for 'role=PRIMARY'");
}

TEST_F(DestMetadataCacheTest, RoleSecondaryWrongMode) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
          mysqlrouter::URI("metadata-cache://cache-name/default?role=SECONDARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
          &metadata_cache_api_),
      std::runtime_error,
      "mode 'read-write' is not valid for 'role=SECONDARY'");
}

TEST_F(DestMetadataCacheTest, RolePrimaryAndSecondaryWrongMode) {
  ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest_mc_group(
          io_ctx_, "cache-name", routing::RoutingStrategy::kUndefined,
          mysqlrouter::URI(
              "metadata-cache://cache-name/default?role=PRIMARY_AND_SECONDARY")
              .query,
          BaseProtocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
          &metadata_cache_api_),
      std::runtime_error,
      "mode 'read-write' is not valid for 'role=PRIMARY_AND_SECONDARY'");
}

/*****************************************/
/*URI parsing tests                      */
/*****************************************/
TEST_F(DestMetadataCacheTest, MetadataCacheGroupAllowPrimaryReads) {
  // yes
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/default?allow_primary_reads=yes&role=SECONDARY");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // no
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/default?allow_primary_reads=no&role=SECONDARY");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // invalid value
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?allow_primary_reads=yes,xxx&role=SECONDARY");
    ASSERT_THROW_LIKE(
        DestMetadataCacheGroup dest(io_ctx_, "metadata_cache_name",
                                    routing::RoutingStrategy::kUndefined,
                                    uri.query,
                                    Protocol::Type::kClassicProtocol),
        std::runtime_error,
        "Invalid value for allow_primary_reads option: 'yes,xxx'");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheGroupMultipleUris) {
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/default?role=SECONDARY,metadata-cache://test2/"
        "default?role=SECONDARY");
    ASSERT_THROW_LIKE(
        DestMetadataCacheGroup dest(io_ctx_, "metadata_cache_name",
                                    routing::RoutingStrategy::kUndefined,
                                    uri.query,
                                    Protocol::Type::kClassicProtocol),
        std::runtime_error,
        "The role in '?role=SECONDARY,metadata-cache://test2/default?role' "
        "does not contain one of the valid role names: PRIMARY, SECONDARY, "
        "PRIMARY_AND_SECONDARY");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheGroupDisconnectOnPromotedToPrimary) {
  // yes valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=yes");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // no valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=no");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // invalid option
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_promoted_to_primary=invalid");
    ASSERT_THROW_LIKE(
        DestMetadataCacheGroup dest(io_ctx_, "metadata_cache_name",
                                    routing::RoutingStrategy::kUndefined,
                                    uri.query,
                                    Protocol::Type::kClassicProtocol),
        std::runtime_error,
        "Invalid value for option 'disconnect_on_promoted_to_primary'. Allowed "
        "are 'yes' and 'no'");
  }

  // incompatible mode, valid only for secondary
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=PRIMARY&disconnect_on_promoted_to_primary=invalid");
    ASSERT_THROW_LIKE(DestMetadataCacheGroup dest(
                          io_ctx_, "metadata_cache_name",
                          routing::RoutingStrategy::kUndefined, uri.query,
                          Protocol::Type::kClassicProtocol),
                      std::runtime_error,
                      "Option 'disconnect_on_promoted_to_primary' is valid "
                      "only for mode=SECONDARY");
  }
}

TEST_F(DestMetadataCacheTest, MetadataCacheDisconnectOnMetadataUnavailable) {
  // yes valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=yes");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // no valid
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=no");
    ASSERT_NO_THROW(DestMetadataCacheGroup dest(
        io_ctx_, "metadata_cache_name", routing::RoutingStrategy::kUndefined,
        uri.query, Protocol::Type::kClassicProtocol));
  }

  // invalid option
  {
    mysqlrouter::URI uri(
        "metadata-cache://test/"
        "default?role=SECONDARY&disconnect_on_metadata_unavailable=invalid");
    ASSERT_THROW_LIKE(
        DestMetadataCacheGroup dest(io_ctx_, "metadata_cache_name",
                                    routing::RoutingStrategy::kUndefined,
                                    uri.query,
                                    Protocol::Type::kClassicProtocol),
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
    ASSERT_THROW_LIKE(DestMetadataCacheGroup dest(
                          io_ctx_, "metadata_cache_name",
                          routing::RoutingStrategy::kUndefined, uri.query,
                          Protocol::Type::kClassicProtocol),
                      std::runtime_error,
                      "Unsupported 'metadata-cache' parameter in URI: 'xxx'");
  }
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
