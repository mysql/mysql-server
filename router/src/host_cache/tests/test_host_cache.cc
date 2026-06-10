/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "host_cache.h"
#include "mock/resolver_interface.h"
#include "mysql/harness/resolver/error_code.h"
#include "mysql/harness/resolver/registry.h"
#include "mysql/harness/utility/wait_variable.h"
#include "mysqlrouter/host_cache_config.h"

#define C_SIZE(X) X
#define C_USE(X) X
#define C_INSERT(X) X

using ResolvedAddresses = mysql_harness::resolver::ResolvedAddresses;
using ResolveHostResult = mysql_harness::resolver::ResolveHostResult;

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrEq;

const uint32_t k_max_entries{3};
const uint32_t k_ttl_ok{100};
const uint32_t k_ttl_fail{50};
const double k_jitter{0};

struct SutCounters {
  uint64_t temporary_size{0};
  uint64_t cache_hits{0};
  uint64_t cache_size{0};
  uint64_t cache_inserts{0};
  uint64_t cache_drops{0};
  uint64_t cache_used{0};
  uint64_t cache_expired{0};
};

const std::vector<ResolvedAddresses> k_resolved_address{
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.1.1")}, {10}},
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.2.1")}, {}},
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.3.1")}, {200}},
};

// The TTL values in k_ttl_ok_resolved_address differ from k_resolved_address.
// If not specified, the cache-level TTL will be used or the one with the
// lesser value.
const std::vector<ResolvedAddresses> k_ttl_ok_resolved_address{
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.1.1")}, {10}},
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.2.1")}, {k_ttl_ok}},
    ResolvedAddresses{{*net::ip::make_address_v4("127.0.3.1")}, {k_ttl_ok}},
};

MATCHER_P(RawHost, expected_value, "") {
  if (!arg) {
    *result_listener << "The value is an error_code:" << arg.error().message();
    return false;
  }

  if (arg->ttl.has_value() != expected_value.ttl.has_value()) {
    *result_listener << (expected_value.ttl.has_value()
                             ? "TTL is expected but missing"
                             : "TTL is unexpected but present");
    return false;
  }
  if (arg->ttl.has_value()) {
    if (*arg->ttl != *expected_value.ttl) {
      *result_listener << "Expected value of TTL is " << *expected_value.ttl
                       << " but received " << *arg->ttl;
      return false;
    }
  }

  if (arg->addresses.size() != expected_value.addresses.size()) {
    *result_listener << "Number of addresses doesn't match "
                     << expected_value.addresses.size() << " but received "
                     << arg->addresses.size();
    return false;
  }

  for (std::size_t i = 0; i < arg->addresses.size(); ++i) {
    if (expected_value.addresses[i] != arg->addresses[i]) {
      *result_listener << "Addresses " << i << " doesn't match "
                       << expected_value.addresses[i] << " but received "
                       << arg->addresses[i];
      return false;
    }
  }

  return true;
}

MATCHER_P(RawHostAddresses, expected_value, "") {
  if (!arg) {
    *result_listener << "The value is an error_code:" << arg.error().message();
    return false;
  }

  if (arg->addresses.size() != expected_value.addresses.size()) {
    *result_listener << "Number of addresses doesn't match "
                     << expected_value.addresses.size() << " but received "
                     << arg->addresses.size();
    return false;
  }

  for (std::size_t i = 0; i < arg->addresses.size(); ++i) {
    if (expected_value.addresses[i] != arg->addresses[i]) {
      *result_listener << "Addresses " << i << " doesn't match "
                       << expected_value.addresses[i] << " but received "
                       << arg->addresses[i];
      return false;
    }
  }

  return true;
}

MATCHER(OkHost1, "") {
  if (!arg) {
    *result_listener << "The value is an error_code:" << arg.error().message();
    return false;
  }
  if (!arg->ttl.has_value()) {
    *result_listener << "The value doesn't have ttl set";
    return false;
  }
  if (arg->ttl.value() != k_ttl_ok) {
    *result_listener << "The value has wrong TTL:" << arg->ttl.value();
    return false;
  }
  if (arg->addresses.size() != 1) {
    *result_listener << "The value unexpected number of addresses: "
                     << arg->addresses.size();
    return false;
  }
  return true;
}

MATCHER(RawHostNotFound, "") {
  if (arg) {
    *result_listener << "Was expecing error_code but it returned addresses: "
                     << arg->addresses.size();
    return false;
  }

  if (arg.error() !=
      mysql_harness::resolver::make_error_code(
          mysql_harness::resolver::ErrcResolveResult::NotFound)) {
    *result_listener << "Was expecing not-found error, but received:"
                     << arg.error().message();
    return false;
  }

  return true;
}

static HostCacheConfig g_config{k_max_entries, k_ttl_ok, k_ttl_fail, k_jitter,
                                false};

class DisabledHostCacheTest : public ::testing::Test {
 public:
  using CachePolicy = mysql_harness::resolver::CachePolicy;

  static auto &get_resolver_reg() {
    return mysql_harness::resolver::Registry::get_instance();
  }

  virtual std::shared_ptr<HostCacheConfig> get_config() {
    return std::make_shared<HostCacheConfig>(g_config);
  }

  void setup_sut() {
    resolver_mock_ =
        std::make_shared<mysql_harness::resolver::MockResolverInterface>();
    config_ = get_config();
    host_cache_ = std::make_unique<HostCache>(config_);
    get_resolver_reg().set(CachePolicy::Bypass, resolver_mock_);
  }

  void SetUp() override { setup_sut(); }

  void TearDown() override { get_resolver_reg().clear(); }

  void check_sut_counters(const SutCounters &expected) {
    auto stat = host_cache_->get_statistics();
    EXPECT_EQ(expected.cache_drops, stat->get_cache_drops());
    EXPECT_EQ(expected.cache_expired, stat->get_cache_expired());
    EXPECT_EQ(expected.cache_hits, stat->get_cache_hits());
    EXPECT_EQ(expected.cache_inserts, stat->get_cache_inserts());
    EXPECT_EQ(expected.cache_size, stat->get_cache_size());
    EXPECT_EQ(expected.cache_used, stat->get_cache_used());
    EXPECT_EQ(expected.temporary_size, stat->get_temporary_size());
  }

  auto not_found() {
    return stdx::unexpected(mysql_harness::resolver::make_error_code(
        mysql_harness::resolver::ErrcResolveResult::NotFound));
  }

  std::shared_ptr<mysql_harness::resolver::MockResolverInterface>
      resolver_mock_;
  std::shared_ptr<HostCacheConfig> config_;
  std::unique_ptr<HostCache> host_cache_;
};

class EnabledHostCacheTest : public DisabledHostCacheTest {
 public:
  std::shared_ptr<HostCacheConfig> get_config() override {
    auto cfg = DisabledHostCacheTest::get_config();
    cfg->enabled_ = true;
    return cfg;
  }
};

TEST_F(DisabledHostCacheTest, all_fill_requests_are_forwarded_to_raw_resolver) {
  const std::string k_host1{"first_host"};
  const std::string k_host2{"second_host"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(5)
      .WillRepeatedly(Return(k_resolved_address[1]));

  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host2), CachePolicy::Bypass))
      .Times(6)
      .WillRepeatedly(Return(k_resolved_address[2]));

  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::FillOnSuccess),
                RawHost(k_resolved_address[1]));
  }

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::FillOnSuccess),
                RawHost(k_resolved_address[2]));
  }
}

TEST_F(DisabledHostCacheTest, all_use_requests_are_forwarded_to_raw_resolver) {
  const std::string k_host1{"first_host"};
  const std::string k_host2{"second_host"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(5)
      .WillRepeatedly(Return(k_resolved_address[1]));
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host2), CachePolicy::Bypass))
      .Times(6)
      .WillRepeatedly(Return(k_resolved_address[2]));

  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::UseIfPresent),
                RawHost(k_resolved_address[1]));
  }

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::UseIfPresent),
                RawHost(k_resolved_address[2]));
  }
}

TEST_F(EnabledHostCacheTest, all_fill_requests_are_forwarded_to_raw_resolver) {
  const std::string k_host1{"first_host"};
  const std::string k_host2{"second_host"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(5)
      .WillRepeatedly(Return(k_resolved_address[1]));
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host2), CachePolicy::Bypass))
      .Times(6)
      .WillRepeatedly(Return(k_resolved_address[2]));

  // min(ttl_ok, lower_layer::ttl),
  // please note that current impl of raw resolver doesn't provide the ttl
  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::FillOnSuccess),
                RawHost(k_ttl_ok_resolved_address[1]));
  }

  SutCounters cnt;
  cnt.cache_size = 1;
  cnt.cache_used = 5;
  cnt.cache_inserts = 5;
  check_sut_counters(cnt);

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::FillOnSuccess),
                RawHost(k_ttl_ok_resolved_address[2]));
  }
  cnt.cache_size += 1;
  cnt.cache_used += 6;
  cnt.cache_inserts += 6;
  check_sut_counters(cnt);
}

TEST_F(EnabledHostCacheTest, use_requests_generate_one_call_to_raw_resolver) {
  const std::string k_host1{"first_host"};
  const std::string k_host2{"second_host"};
  const std::string k_host3{"third"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_address[0]));
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host2), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_address[1]));
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host3), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_address[2]));

  SutCounters cnt;
  // Do first round on all three hosts
  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[0]));
  }

  cnt.cache_size = 1;
  cnt.cache_used = 5;
  cnt.cache_hits = 4;
  cnt.cache_inserts = 1;
  check_sut_counters(cnt);

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[1]));
  }

  cnt.cache_size += 1;
  cnt.cache_used += 6;
  cnt.cache_hits += 5;
  cnt.cache_inserts += 1;
  check_sut_counters(cnt);

  for (int i = 0; i < 3; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host3, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[2]));
  }

  cnt.cache_size += 1;
  cnt.cache_used += 3;
  cnt.cache_hits += 2;
  cnt.cache_inserts += 1;
  check_sut_counters(cnt);

  // Do second round on all three hosts
  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[0]));
  }

  cnt.cache_used += 5;
  cnt.cache_hits += 5;
  check_sut_counters(cnt);

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[1]));
  }

  cnt.cache_used += 6;
  cnt.cache_hits += 6;
  check_sut_counters(cnt);

  for (int i = 0; i < 3; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host3, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[2]));
  }

  cnt.cache_used += 3;
  cnt.cache_hits += 3;
  check_sut_counters(cnt);
}

TEST_F(EnabledHostCacheTest,
       sequence_fill_use_generate_only_one_call_to_resolver) {
  const std::string k_host1{"first_host"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_address[1]));

  ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::FillOnSuccess),
              RawHost(k_ttl_ok_resolved_address[1]));

  SutCounters cnt;
  cnt.cache_size = 1;
  cnt.cache_inserts = cnt.cache_used = 1;
  check_sut_counters(cnt);
  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::UseIfPresent),
                RawHost(k_ttl_ok_resolved_address[1]));
  }
  cnt.cache_used += 6;
  cnt.cache_hits += 6;
  check_sut_counters(cnt);
}

TEST_F(EnabledHostCacheTest, full_cache_eject_one_item) {
  const std::vector<std::string> k_hosts{"first_host", "second", "third",
                                         "fourth"};
  // Specify the TTL, that the cache entry will use the same.
  const std::vector<ResolvedAddresses> k_resolved{
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.1.1")}, {10}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.2.1")}, {11}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.3.1")}, {12}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.4.1")}, {13}},
  };

  const std::vector<ResolvedAddresses> k_resolved_refetch{
      ResolvedAddresses{{*net::ip::make_address_v4("128.0.1.1")}, {20}},
      ResolvedAddresses{{*net::ip::make_address_v4("128.0.2.1")}, {21}},
      ResolvedAddresses{{*net::ip::make_address_v4("128.0.3.1")}, {22}},
      ResolvedAddresses{{*net::ip::make_address_v4("128.0.4.1")}, {23}},
  };

  ASSERT_EQ(k_hosts.size(), k_resolved.size());
  SutCounters cnt;

  for (size_t i = 0; i < k_hosts.size(); ++i) {
    EXPECT_CALL(*resolver_mock_,
                resolve_host(StrEq(k_hosts[i]), CachePolicy::Bypass))
        .Times(1)
        .WillOnce(Return(k_resolved[i]));
  }

  for (size_t i = 0; i < k_hosts.size(); ++i) {
    ASSERT_THAT(
        host_cache_->resolve_host(k_hosts[i], CachePolicy::UseIfPresent),
        RawHost(k_resolved[i]));
  }

  cnt.cache_size = 3;
  cnt.cache_inserts = cnt.cache_used = k_hosts.size();
  cnt.cache_drops = 1;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  // Fetch items that are in cache and
  // check that they do not resolve.

  //  We start at 1, because 0 should have been removed.
  for (size_t i = 1; i < k_hosts.size(); ++i) {
    ASSERT_THAT(
        host_cache_->resolve_host(k_hosts[i], CachePolicy::UseIfPresent),
        RawHost(k_resolved[i]));
  }

  cnt.cache_used += (k_hosts.size() - 1);
  cnt.cache_hits += (k_hosts.size() - 1);
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  // Fetch 0 and see that it generates resolve request,
  // its not inside the cache.
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_hosts[0]), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_refetch[0]));

  ASSERT_THAT(host_cache_->resolve_host(k_hosts[0], CachePolicy::UseIfPresent),
              RawHost(k_resolved_refetch[0]));

  cnt.cache_used += 1;
  cnt.cache_inserts += 1;
  cnt.cache_drops += 1;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  // Fetch items that are in cache and
  // check that they do not resolve.

  // Do 0 as last, 2 should be dopped next.
  // Skip 1, its not cached.
  for (size_t i = 2; i < k_hosts.size(); ++i) {
    ASSERT_THAT(
        host_cache_->resolve_host(k_hosts[i], CachePolicy::UseIfPresent),
        RawHost(k_resolved[i]));
  }

  ASSERT_THAT(host_cache_->resolve_host(k_hosts[0], CachePolicy::UseIfPresent),
              RawHost(k_resolved_refetch[0]));

  cnt.cache_used += k_hosts.size() - 1;
  cnt.cache_hits += k_hosts.size() - 1;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());
  // Fetch 1 and see that it generates resolve request,
  // its not inside the cache.
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_hosts[1]), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved_refetch[1]));

  ASSERT_THAT(host_cache_->resolve_host(k_hosts[1], CachePolicy::UseIfPresent),
              RawHost(k_resolved_refetch[1]));

  cnt.cache_used += 1;
  cnt.cache_inserts += 1;
  cnt.cache_drops += 1;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  // Fetch items that are in cache and
  // check that they do not resolve.

  //  Skip 2, its not cached
  ASSERT_THAT(host_cache_->resolve_host(k_hosts[0], CachePolicy::UseIfPresent),
              RawHost(k_resolved_refetch[0]));
  ASSERT_THAT(host_cache_->resolve_host(k_hosts[1], CachePolicy::UseIfPresent),
              RawHost(k_resolved_refetch[1]));

  ASSERT_THAT(host_cache_->resolve_host(k_hosts[3], CachePolicy::UseIfPresent),
              RawHost(k_resolved[3]));

  cnt.cache_used += 3;
  cnt.cache_hits += 3;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());
}

TEST_F(EnabledHostCacheTest, not_found_is_also_cached_with_different_ttl) {
  const std::string k_host1{"first_host"};
  const std::string k_host2{"second_host"};
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host1), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(not_found()));
  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq(k_host2), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(not_found()));

  // In this case cache doesn't return TTL, its held internally.
  for (int i = 0; i < 5; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host1, CachePolicy::UseIfPresent),
                RawHostNotFound());
  }

  SutCounters cnt;
  cnt.cache_size = 1;
  cnt.cache_inserts = 1;
  cnt.cache_used = 5;
  cnt.cache_hits = 4;

  check_sut_counters(cnt);

  for (int i = 0; i < 6; ++i) {
    ASSERT_THAT(host_cache_->resolve_host(k_host2, CachePolicy::UseIfPresent),
                RawHostNotFound());
  }

  cnt.cache_size += 1;
  cnt.cache_inserts += 1;
  cnt.cache_used += 6;
  cnt.cache_hits += 5;

  check_sut_counters(cnt);

  // Verify the TTL in internal data (look comment above).
  auto stat = host_cache_->get_statistics();
  auto entries = stat->get_entries();

  ASSERT_EQ(2, entries.size());
  for (const auto &e : entries) {
    ASSERT_EQ(k_ttl_fail, e.ttl_.count());
  }
}

class TimeoutHostCacheTest : public EnabledHostCacheTest {
 public:
  std::shared_ptr<HostCacheConfig> get_config() override {
    auto cfg = EnabledHostCacheTest::get_config();
    cfg->max_entries_ = 10;
    return cfg;
  }
};

TEST_F(TimeoutHostCacheTest, verify_that_items_are_dropped_after_timeout) {
  const std::vector<std::string> k_hosts{"h1", "h2", "h3", "h4", "h5",
                                         "h6", "h7", "h8", "h9", "h10"};
  // The TTL is set in a way, that half of the list should be purged
  // at next use.
  const std::vector<ResolvedAddresses> k_resolved{
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.1.1")}, {8}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.2.1")}, {30}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.3.1")}, {6}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.4.1")}, {40}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.5.1")}, {5}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.6.1")}, {50}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.7.1")}, {5}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.8.1")}, {}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.9.1")}, {5}},
      ResolvedAddresses{{*net::ip::make_address_v4("127.0.0.1")}, {}},
  };

  ASSERT_EQ(k_hosts.size(), k_resolved.size());
  SutCounters cnt;

  for (size_t i = 0; i < k_hosts.size(); ++i) {
    EXPECT_CALL(*resolver_mock_,
                resolve_host(StrEq(k_hosts[i]), CachePolicy::Bypass))
        .Times(1)
        .WillOnce(Return(k_resolved[i]));
  }

  for (size_t i = 0; i < k_hosts.size(); ++i) {
    ASSERT_THAT(
        host_cache_->resolve_host(k_hosts[i], CachePolicy::UseIfPresent),
        RawHostAddresses(k_resolved[i]));
  }

  cnt.cache_size = cnt.cache_inserts = cnt.cache_used = k_hosts.size();
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  // Sleep to make entries expired.
  std::this_thread::sleep_for(std::chrono::seconds(10));

  // The entries are not removed right away, they will at next cache access
  check_sut_counters(cnt);

  // Fetch item that is in cache and
  // check that they do not resolve and do not drop expired entries.

  ASSERT_THAT(host_cache_->resolve_host(k_hosts[9], CachePolicy::UseIfPresent),
              RawHostAddresses(k_resolved[9]));

  ++cnt.cache_used;
  ++cnt.cache_hits;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());

  EXPECT_CALL(*resolver_mock_,
              resolve_host(StrEq("NewHost"), CachePolicy::Bypass))
      .Times(1)
      .WillOnce(Return(k_resolved[0]));

  ASSERT_THAT(host_cache_->resolve_host("NewHost", CachePolicy::UseIfPresent),
              RawHostAddresses(k_resolved[0]));

  ++cnt.cache_used;
  ++cnt.cache_inserts;
  cnt.cache_size -= 5; /* 5 items expired and one newly inserted */
  cnt.cache_size += 1;
  cnt.cache_expired += 5;
  check_sut_counters(cnt);

  Mock::VerifyAndClearExpectations(resolver_mock_.get());
}

class TemporaryHostCacheTest : public EnabledHostCacheTest {
 public:
};

TEST_F(TemporaryHostCacheTest, temporary_item) {
  enum ThreadTempState { TtsInit, TtsReady, TtsEnding };

  mysql_harness::utility::WaitableVariable<ThreadTempState> state{TtsInit};
  const auto k_addr = k_resolved_address[0];

  EXPECT_CALL(*resolver_mock_, resolve_host(_, _))
      .Times(1)
      .WillOnce([&](const std::string &, CachePolicy) -> ResolveHostResult {
        if (!state.exchange(TtsInit, TtsReady)) {
          throw std::invalid_argument("Unexpected state");
        }
        state.wait(TtsEnding);
        return k_addr;
      });

  int thx_idx = 0;
  std::array<ResolveHostResult, 6> results;
  std::thread t(
      [&](int idx) {
        results[idx] =
            host_cache_->resolve_host("test_host", CachePolicy::UseIfPresent);
      },
      thx_idx++);

  state.wait(TtsReady);

  std::array<std::thread, 5> threads;
  for (auto &t : threads) {
    t = std::thread(
        [&](int idx) {
          results[idx] =
              host_cache_->resolve_host("test_host", CachePolicy::UseIfPresent);
        },
        thx_idx++);
  }

  //  pulling number of threads waiting in temp request:
  auto stats = host_cache_->get_statistics();
  ASSERT_EQ(1, stats->get_temporary_size());

  while (true) {
    auto entries = stats->get_temporary_entries();
    ASSERT_EQ(1, entries.size());
    const auto &item = entries[0];

    if (item->resolve_waiters_peak_.load() == threads.size() + 1) break;
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  state.set(TtsEnding);

  for (auto &t : threads) {
    t.join();
  }
  t.join();

  for (size_t idx = 0; idx < results.size(); ++idx) {
    ASSERT_TRUE(results[idx]);

    ASSERT_THAT(results[idx], RawHost(k_addr));
  }
}
