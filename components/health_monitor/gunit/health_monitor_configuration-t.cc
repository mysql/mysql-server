/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include <string>

#include "../health_monitor_configuration.h"
#include "../health_monitor_monitored_path.h"

namespace hm::configuration {
namespace {

TEST(HealthMonitorConfiguration, UsesCommunityEditionDefaults) {
  EXPECT_STREQ("", k_disk_monitored_default);
  EXPECT_FALSE(k_disk_fallback_enable_default);
}

TEST(HealthMonitorConfiguration, ConvertsReportMinutesToSeconds) {
  EXPECT_EQ(60, report_interval_seconds(1));
  EXPECT_EQ(14400, report_interval_seconds(240));
  EXPECT_EQ(604800, report_interval_seconds(10080));
}

TEST(HealthMonitorConfiguration, ValidatesMonitoredPaths) {
  EXPECT_TRUE(valid_monitored_path("", 0));
  EXPECT_TRUE(valid_monitored_path("/tmp/health-monitor", 19));
  EXPECT_TRUE(valid_monitored_path("/tmp/%s", 7));

  const std::string relative{"tmp/health-monitor"};
  EXPECT_FALSE(valid_monitored_path(relative.data(), relative.size()));

  const std::string embedded_null{"/tmp/a\0b", 8};
  EXPECT_FALSE(
      valid_monitored_path(embedded_null.data(), embedded_null.size()));

  const std::string maximum =
      "/" + std::string(k_max_monitored_path_length - 2, 'a');
  const std::string too_long =
      "/" + std::string(k_max_monitored_path_length - 1, 'a');
  EXPECT_TRUE(valid_monitored_path(maximum.data(), maximum.size()));
  EXPECT_FALSE(valid_monitored_path(too_long.data(), too_long.size()));
  EXPECT_FALSE(valid_monitored_path(nullptr, 0));
}

TEST(HealthMonitorConfiguration, RetriesAutomaticPathAfterLookupFailure) {
  Monitored_path_cache cache;
  std::string path;
  uint64_t generation{0};

  EXPECT_TRUE(cache.begin_refresh());
  cache.store(std::nullopt);
  EXPECT_FALSE(cache.select("", path, generation));

  EXPECT_TRUE(cache.begin_refresh());
  cache.store(std::string{"/var/lib/mysql"});
  EXPECT_TRUE(cache.select("", path, generation));
  EXPECT_EQ("/var/lib/mysql", path);
}

TEST(HealthMonitorConfiguration, ExplicitPathBypassesAutomaticLookup) {
  Monitored_path_cache cache;
  std::string path;
  uint64_t generation{0};

  EXPECT_TRUE(cache.select("/srv/mysql", path, generation));
  EXPECT_EQ("/srv/mysql", path);
  EXPECT_TRUE(cache.begin_refresh());
}

TEST(HealthMonitorConfiguration, PreservesRefreshRequestedDuringLookup) {
  Monitored_path_cache cache;

  EXPECT_TRUE(cache.begin_refresh());
  cache.request_refresh();
  cache.store(std::string{"/var/lib/mysql"});
  EXPECT_TRUE(cache.begin_refresh());
}

}  // namespace
}  // namespace hm::configuration
