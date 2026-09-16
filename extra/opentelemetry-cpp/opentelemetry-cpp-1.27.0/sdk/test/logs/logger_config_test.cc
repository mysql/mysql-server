// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/logger_config.h"
#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"

namespace logs_sdk              = opentelemetry::sdk::logs;
namespace instrumentation_scope = opentelemetry::sdk::instrumentationscope;
namespace logs_api              = opentelemetry::logs;

/** Test to verify the basic behavior of logs_sdk::LoggerConfig */

TEST(LoggerConfig, CheckDisabledWorksAsExpected)
{
  logs_sdk::LoggerConfig disabled_config = logs_sdk::LoggerConfig::Disabled();
  ASSERT_FALSE(disabled_config.IsEnabled());
  ASSERT_EQ(disabled_config.GetMinimumSeverity(), logs_api::Severity::kInvalid);
  ASSERT_FALSE(disabled_config.IsTraceBased());
}

TEST(LoggerConfig, CheckEnabledWorksAsExpected)
{
  logs_sdk::LoggerConfig enabled_config = logs_sdk::LoggerConfig::Enabled();
  ASSERT_TRUE(enabled_config.IsEnabled());
  ASSERT_EQ(enabled_config.GetMinimumSeverity(), logs_api::Severity::kInvalid);
  ASSERT_FALSE(enabled_config.IsTraceBased());
}

TEST(LoggerConfig, CheckDefaultConfigWorksAccToSpec)
{
  logs_sdk::LoggerConfig default_config = logs_sdk::LoggerConfig::Default();
  ASSERT_TRUE(default_config.IsEnabled());
  ASSERT_EQ(default_config.GetMinimumSeverity(), logs_api::Severity::kInvalid);
  ASSERT_FALSE(default_config.IsTraceBased());
}

TEST(LoggerConfig, CheckCreateWorksAsExpected)
{
  logs_sdk::LoggerConfig custom_config =
      logs_sdk::LoggerConfig::Create(true, logs_api::Severity::kWarn, true);

  ASSERT_TRUE(custom_config.IsEnabled());
  ASSERT_EQ(custom_config.GetMinimumSeverity(), logs_api::Severity::kWarn);
  ASSERT_TRUE(custom_config.IsTraceBased());
}

/** Tests to verify the behavior of logs_sdk::LoggerConfig::Default */

static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr1 = {
    "accept_single_attr", true};
static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr2 = {
    "accept_second_attr", "some other attr"};
static std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> attr3 = {
    "accept_third_attr", 3};

static instrumentation_scope::InstrumentationScope test_scope_1 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_1");
static instrumentation_scope::InstrumentationScope test_scope_2 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_2", "1.0");
static instrumentation_scope::InstrumentationScope test_scope_3 =
    *instrumentation_scope::InstrumentationScope::Create(
        "test_scope_3",
        "0",
        "https://opentelemetry.io/schemas/v1.18.0");
static instrumentation_scope::InstrumentationScope test_scope_4 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_4",
                                                         "0",
                                                         "https://opentelemetry.io/schemas/v1.18.0",
                                                         {attr1});
static instrumentation_scope::InstrumentationScope test_scope_5 =
    *instrumentation_scope::InstrumentationScope::Create("test_scope_5",
                                                         "0",
                                                         "https://opentelemetry.io/schemas/v1.18.0",
                                                         {attr1, attr2, attr3});

// This array could also directly contain the reference types, but that  leads to 'uninitialized
// value was created by heap allocation' errors in Valgrind memcheck. This is a bug in Googletest
// library, see https://github.com/google/googletest/issues/3805#issuecomment-1397301790 for more
// details. Using pointers is a workaround to prevent the Valgrind warnings.
const std::array<instrumentation_scope::InstrumentationScope *, 5> instrumentation_scopes = {
    &test_scope_1, &test_scope_2, &test_scope_3, &test_scope_4, &test_scope_5,
};

// Test fixture for VerifyDefaultConfiguratorBehavior
class DefaultLoggerConfiguratorTestFixture
    : public ::testing::TestWithParam<instrumentation_scope::InstrumentationScope *>
{};

// Verifies that the default configurator always returns the default logger config.
TEST_P(DefaultLoggerConfiguratorTestFixture, VerifyDefaultConfiguratorBehavior)
{
  instrumentation_scope::InstrumentationScope *scope = GetParam();
  instrumentation_scope::ScopeConfigurator<logs_sdk::LoggerConfig> default_configurator =
      instrumentation_scope::ScopeConfigurator<logs_sdk::LoggerConfig>::Builder(
          logs_sdk::LoggerConfig::Default())
          .Build();

  ASSERT_EQ(default_configurator.ComputeConfig(*scope), logs_sdk::LoggerConfig::Default());
}

TEST(LoggerConfig, ScopeConfiguratorPreservesCustomConfig)
{
  logs_sdk::LoggerConfig default_config =
      logs_sdk::LoggerConfig::Create(false, logs_api::Severity::kError, true);
  logs_sdk::LoggerConfig matching_config =
      logs_sdk::LoggerConfig::Create(true, logs_api::Severity::kWarn, false);

  instrumentation_scope::ScopeConfigurator<logs_sdk::LoggerConfig> configurator =
      instrumentation_scope::ScopeConfigurator<logs_sdk::LoggerConfig>::Builder(default_config)
          .AddConditionNameEquals("test_scope_1", matching_config)
          .Build();

  ASSERT_EQ(configurator.ComputeConfig(test_scope_1), matching_config);
  ASSERT_EQ(configurator.ComputeConfig(test_scope_2), default_config);
}

INSTANTIATE_TEST_SUITE_P(InstrumentationScopes,
                         DefaultLoggerConfiguratorTestFixture,
                         ::testing::ValuesIn(instrumentation_scopes));
