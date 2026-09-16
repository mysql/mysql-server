// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <string>

#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/logs/event_logger.h"           // IWYU pragma: keep
#include "opentelemetry/logs/event_logger_provider.h"  // IWYU pragma: keep
#include "opentelemetry/logs/logger.h"                 // IWYU pragma: keep
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"

#if OPENTELEMETRY_ABI_VERSION_NO < 2
using opentelemetry::logs::EventLogger;
using opentelemetry::logs::EventLoggerProvider;
using opentelemetry::logs::NoopEventLoggerProvider;
#endif
using opentelemetry::logs::Logger;
using opentelemetry::logs::LoggerProvider;
using opentelemetry::logs::NoopLoggerProvider;
using opentelemetry::logs::Provider;
using opentelemetry::nostd::shared_ptr;
namespace nostd = opentelemetry::nostd;

class TestProvider : public LoggerProvider
{
  nostd::shared_ptr<Logger> GetLogger(
      nostd::string_view /* logger_name */,
      nostd::string_view /* library_name */,
      nostd::string_view /* library_version */,
      nostd::string_view /* schema_url */,
      const opentelemetry::common::KeyValueIterable & /* attributes */) override
  {
    return shared_ptr<Logger>(nullptr);
  }
};

TEST(Provider, GetLoggerProviderDefault)
{
  auto tf = Provider::GetLoggerProvider();
  EXPECT_NE(nullptr, tf);
}

TEST(Provider, SetLoggerProvider)
{
  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  Provider::SetLoggerProvider(tf);
  ASSERT_EQ(tf, Provider::GetLoggerProvider());
}

TEST(Provider, MultipleLoggerProviders)
{
  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  Provider::SetLoggerProvider(tf);
  auto tf2 = shared_ptr<LoggerProvider>(new TestProvider());
  Provider::SetLoggerProvider(tf2);

  ASSERT_NE(Provider::GetLoggerProvider(), tf);
}

TEST(Provider, GetLogger)
{
  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  // tests GetLogger(name, version, schema)
  const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
  auto logger = tf->GetLogger("logger1", "opentelelemtry_library", "", schema_url);
  EXPECT_EQ(nullptr, logger);

  // tests GetLogger(name, arguments)

  auto logger2 = tf->GetLogger("logger2", "opentelelemtry_library", "", schema_url);
  EXPECT_EQ(nullptr, logger2);
}

TEST(NoopLoggerProvider, CreateNoopLogger)
{
  NoopLoggerProvider provider;
  auto logger = provider.GetLogger(
      "test", "lib", "1.0", "schema_url",
      opentelemetry::common::KeyValueIterableView<std::map<std::string, int>>({}));
  ASSERT_TRUE(logger != nullptr);
  EXPECT_EQ(logger->GetName(), "noop logger");
}

#if OPENTELEMETRY_ABI_VERSION_NO < 2

/*
 * opentelemetry::logs::Provider::GetEventLoggerProvider() is deprecated.
 * opentelemetry::logs::Provider::SetEventLoggerProvider() is deprecated.
 * Suppress warnings in tests, to have a clean build and coverage.
 */

#  if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#  elif defined(__GNUC__) && !defined(__clang__) && !defined(__apple_build_version__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  elif defined(__clang__) || defined(__apple_build_version__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#  endif

class TestEventLoggerProvider : public EventLoggerProvider
{
public:
  nostd::shared_ptr<EventLogger> CreateEventLogger(
      nostd::shared_ptr<Logger> /*delegate_logger*/,
      nostd::string_view /*event_domain*/) noexcept override
  {
    return nostd::shared_ptr<EventLogger>(nullptr);
  }
};

TEST(Provider, GetEventLoggerProviderDefault)
{
  auto tf = Provider::GetEventLoggerProvider();
  EXPECT_NE(nullptr, tf);
}

TEST(Provider, SetEventLoggerProvider)
{
  auto tf = nostd::shared_ptr<EventLoggerProvider>(new TestEventLoggerProvider());
  Provider::SetEventLoggerProvider(tf);
  ASSERT_EQ(tf, Provider::GetEventLoggerProvider());
}

TEST(Provider, MultipleEventLoggerProviders)
{
  auto tf = nostd::shared_ptr<EventLoggerProvider>(new TestEventLoggerProvider());
  Provider::SetEventLoggerProvider(tf);
  auto tf2 = nostd::shared_ptr<EventLoggerProvider>(new TestEventLoggerProvider());
  Provider::SetEventLoggerProvider(tf2);

  ASSERT_NE(Provider::GetEventLoggerProvider(), tf);
}

TEST(Provider, CreateEventLogger)
{
  auto tf     = nostd::shared_ptr<TestEventLoggerProvider>(new TestEventLoggerProvider());
  auto logger = tf->CreateEventLogger(nostd::shared_ptr<Logger>(nullptr), "domain");

  EXPECT_EQ(nullptr, logger);
}

TEST(NoopEventLoggerProvider, CreateNoopEventLogger)
{
  NoopEventLoggerProvider provider;
  auto event_logger = provider.CreateEventLogger(nostd::shared_ptr<Logger>(nullptr), "domain");
  ASSERT_TRUE(event_logger != nullptr);
  EXPECT_EQ(event_logger->GetName(), "noop event logger");
}

#  if defined(_MSC_VER)
#    pragma warning(pop)
#  elif defined(__GNUC__) && !defined(__clang__) && !defined(__apple_build_version__)
#    pragma GCC diagnostic pop
#  elif defined(__clang__) || defined(__apple_build_version__)
#    pragma clang diagnostic pop
#  endif

#endif
