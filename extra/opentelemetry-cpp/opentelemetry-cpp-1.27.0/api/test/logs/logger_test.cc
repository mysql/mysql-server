// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/event_id.h"
#include "opentelemetry/logs/event_logger.h"           // IWYU pragma: keep
#include "opentelemetry/logs/event_logger_provider.h"  // IWYU pragma: keep
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
#  include "opentelemetry/context/context.h"
#  include "opentelemetry/nostd/variant.h"
#endif

#if OPENTELEMETRY_ABI_VERSION_NO < 2
using opentelemetry::logs::NoopEventLogger;
#endif

using opentelemetry::logs::EventId;
using opentelemetry::logs::Logger;
using opentelemetry::logs::LoggerProvider;
using opentelemetry::logs::NoopLogger;
using opentelemetry::logs::Provider;
using opentelemetry::logs::Severity;
using opentelemetry::nostd::shared_ptr;
using opentelemetry::nostd::string_view;
namespace common = opentelemetry::common;
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
namespace context = opentelemetry::context;
#endif
namespace nostd = opentelemetry::nostd;
namespace trace = opentelemetry::trace;

// Check that the default logger is a noop logger instance
TEST(Logger, GetLoggerDefault)
{
  auto lp = Provider::GetLoggerProvider();
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = lp->GetLogger("TestLogger", "opentelelemtry_library", "", schema_url);
  EXPECT_NE(nullptr, logger);
  auto name = logger->GetName();
  EXPECT_EQ(name, "noop logger");
  auto record = logger->CreateLogRecord();
  EXPECT_NE(nullptr, record);
}

// Test the two additional overloads for GetLogger()
TEST(Logger, GetNoopLoggerNameWithArgs)
{
  auto lp = Provider::GetLoggerProvider();

  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  lp->GetLogger("NoopLoggerWithArgs", "opentelelemtry_library", "", schema_url);

  lp->GetLogger("NoopLoggerWithOptions", "opentelelemtry_library", "", schema_url);
}

TEST(NoopLogger, NoopLoggerUsage)
{
  NoopLogger logger;
  auto record = logger.CreateLogRecord();
  ASSERT_TRUE(record != nullptr);
  logger.EmitLogRecord(std::move(record));
}

// Test the EmitLogRecord() overloads
TEST(Logger, LogMethodOverloads)
{
  auto lp = Provider::GetLoggerProvider();
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = lp->GetLogger("TestLogger", "opentelelemtry_library", "", schema_url);

  EventId trace_event_id{0x1, "TraceEventId"};
  EventId debug_event_id{0x2, "DebugEventId"};
  EventId info_event_id{0x3, "InfoEventId"};
  EventId warn_event_id{0x4, "WarnEventId"};
  EventId error_event_id{0x5, "ErrorEventId"};
  EventId fatal_event_id{0x6, "FatalEventId"};

  // Create a map to test the logs with
  std::map<std::string, std::string> m = {{"key1", "value1"}};

  // EmitLogRecord overloads
  logger->EmitLogRecord(Severity::kTrace, "Test log message");
  logger->EmitLogRecord(Severity::kInfo, "Test log message");
  logger->EmitLogRecord(Severity::kDebug, m);
  logger->EmitLogRecord(Severity::kWarn, "Logging a map", m);
  logger->EmitLogRecord(Severity::kError,
                        opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->EmitLogRecord(Severity::kFatal, "Logging an initializer list",
                        opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->EmitLogRecord(Severity::kDebug, opentelemetry::common::MakeAttributes(m));
  logger->EmitLogRecord(Severity::kDebug,
                        common::KeyValueIterableView<std::map<std::string, std::string>>(m));
  std::pair<nostd::string_view, common::AttributeValue> array[] = {{"key1", "value1"}};
  logger->EmitLogRecord(Severity::kDebug, opentelemetry::common::MakeAttributes(array));
  std::vector<std::pair<std::string, std::string>> vec = {{"key1", "value1"}};
  logger->EmitLogRecord(Severity::kDebug, opentelemetry::common::MakeAttributes(vec));

  // Severity methods
  logger->Trace("Test log message");
  logger->Trace("Test log message", m);
  logger->Trace("Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Trace(m);
  logger->Trace(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Trace(trace_event_id, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Trace(trace_event_id.id_, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));

  logger->Debug("Test log message");
  logger->Debug("Test log message", m);
  logger->Debug("Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Debug(m);
  logger->Debug(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Debug(debug_event_id, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Debug(debug_event_id.id_, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));

  logger->Info("Test log message");
  logger->Info("Test log message", m);
  logger->Info("Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Info(m);
  logger->Info(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Info(info_event_id, "Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Info(info_event_id.id_, "Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));

  logger->Warn("Test log message");
  logger->Warn("Test log message", m);
  logger->Warn("Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Warn(m);
  logger->Warn(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Warn(warn_event_id, "Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Warn(warn_event_id.id_, "Test log message",
               opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));

  logger->Error("Test log message");
  logger->Error("Test log message", m);
  logger->Error("Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Error(m);
  logger->Error(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Error(error_event_id, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Error(error_event_id.id_, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));

  logger->Fatal("Test log message");
  logger->Fatal("Test log message", m);
  logger->Fatal("Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Fatal(m);
  logger->Fatal(opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Fatal(fatal_event_id, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  logger->Fatal(fatal_event_id.id_, "Test log message",
                opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
}

#if OPENTELEMETRY_ABI_VERSION_NO < 2

/*
 * opentelemetry::logs::Provider::GetLoggerProvider() is deprecated.
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

TEST(NoopEventLogger, NoopEventLoggerUsage)
{
  NoopEventLogger event_logger;

  auto logger = event_logger.GetDelegateLogger();
  ASSERT_TRUE(logger != nullptr);
  auto record = logger->CreateLogRecord();
  ASSERT_TRUE(record != nullptr);
  event_logger.EmitEvent("event_name", std::move(record));
}

TEST(Logger, EventLogMethodOverloads)
{
  auto lp = Provider::GetLoggerProvider();
  const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = lp->GetLogger("TestLogger", "opentelelemtry_library", "", schema_url);

  auto elp          = Provider::GetEventLoggerProvider();
  auto event_logger = elp->CreateEventLogger(logger, "otel-cpp.test");

  std::map<std::string, std::string> m = {{"key1", "value1"}};

  event_logger->EmitEvent("event name", Severity::kTrace, "Test log message");
  event_logger->EmitEvent("event name", Severity::kInfo, "Test log message");
  event_logger->EmitEvent("event name", Severity::kDebug, m);
  event_logger->EmitEvent("event name", Severity::kWarn, "Logging a map", m);
  event_logger->EmitEvent(
      "event name", Severity::kError,
      opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  event_logger->EmitEvent(
      "event name", Severity::kFatal, "Logging an initializer list",
      opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 2}}));
  event_logger->EmitEvent("event name", Severity::kDebug, opentelemetry::common::MakeAttributes(m));
  event_logger->EmitEvent("event name", Severity::kDebug,
                          common::KeyValueIterableView<std::map<std::string, std::string>>(m));
  std::pair<nostd::string_view, common::AttributeValue> array[] = {{"key1", "value1"}};
  event_logger->EmitEvent("event name", Severity::kDebug,
                          opentelemetry::common::MakeAttributes(array));
  std::vector<std::pair<std::string, std::string>> vec = {{"key1", "value1"}};
  event_logger->EmitEvent("event name", Severity::kDebug,
                          opentelemetry::common::MakeAttributes(vec));
}

#  if defined(_MSC_VER)
#    pragma warning(pop)
#  elif defined(__GNUC__) && !defined(__clang__) && !defined(__apple_build_version__)
#    pragma GCC diagnostic pop
#  elif defined(__clang__) || defined(__apple_build_version__)
#    pragma clang diagnostic pop
#  endif

#endif

// Define a basic Logger class
class TestLogger : public Logger
{
  const nostd::string_view GetName() noexcept override { return "test logger"; }

  nostd::unique_ptr<opentelemetry::logs::LogRecord> CreateLogRecord() noexcept override
  {
    return nullptr;
  }

  using Logger::EmitLogRecord;

  void EmitLogRecord(
      nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept override
  {
    auto log_record_ptr = std::move(log_record);
  }
};

class EnablementAwareTestLogRecord : public opentelemetry::logs::LogRecord
{
public:
  void SetTimestamp(common::SystemTimestamp /*timestamp*/) noexcept override {}

  void SetObservedTimestamp(common::SystemTimestamp /*timestamp*/) noexcept override {}

  void SetSeverity(Severity severity) noexcept override { severity_ = severity; }

  void SetBody(const common::AttributeValue & /*message*/) noexcept override
  {
    body_was_set_ = true;
  }

  void SetAttribute(nostd::string_view /*key*/,
                    const common::AttributeValue & /*value*/) noexcept override
  {}

  void SetEventId(int64_t id, nostd::string_view name = {}) noexcept override
  {
    event_id_           = id;
    event_id_was_set_   = true;
    event_name_was_set_ = !name.empty();
  }

  void SetTraceId(const trace::TraceId & /*trace_id*/) noexcept override {}

  void SetSpanId(const trace::SpanId & /*span_id*/) noexcept override {}

  void SetTraceFlags(const trace::TraceFlags & /*trace_flags*/) noexcept override {}

  Severity severity_{Severity::kInvalid};
  int64_t event_id_{-1};
  bool body_was_set_{false};
  bool event_id_was_set_{false};
  bool event_name_was_set_{false};
};

class EnablementAwareTestLogger : public Logger
{
public:
  explicit EnablementAwareTestLogger(Severity minimum_severity,
                                     bool event_id_enabled = false) noexcept
      : event_id_enabled_(event_id_enabled)
  {
    SetMinimumSeverity(static_cast<uint8_t>(minimum_severity));
  }

  const nostd::string_view GetName() noexcept override { return "enablement-aware logger"; }

  nostd::unique_ptr<opentelemetry::logs::LogRecord> CreateLogRecord() noexcept override
  {
    ++create_log_record_calls_;
    return nostd::unique_ptr<opentelemetry::logs::LogRecord>(new EnablementAwareTestLogRecord());
  }

  using Logger::EmitLogRecord;

  void EmitLogRecord(
      nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept override
  {
    auto owned_log_record = std::move(log_record);
    if (owned_log_record)
    {
      ++emit_log_record_calls_;
      last_emitted_record_.reset(
          static_cast<EnablementAwareTestLogRecord *>(owned_log_record.release()));
    }
  }

  void SetEventIdEnabled(bool enabled) noexcept { event_id_enabled_ = enabled; }

  size_t create_log_record_calls_{0};
  size_t emit_log_record_calls_{0};
  mutable size_t enabled_calls_{0};
  mutable size_t enabled_with_event_id_calls_{0};
  mutable Severity last_enabled_severity_{Severity::kInvalid};
  mutable int64_t last_enabled_event_id_{-1};
  mutable bool last_enabled_context_has_test_key_{false};
  mutable bool last_enabled_context_test_key_value_{false};
  nostd::unique_ptr<EnablementAwareTestLogRecord> last_emitted_record_;

protected:
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  bool EnabledImplementation(const context::Context &context,
                             Severity severity) const noexcept override
  {
    ++enabled_calls_;
    last_enabled_severity_ = severity;
    auto value             = context.GetValue("test-key");
    if (const bool *maybe_value = nostd::get_if<bool>(&value))
    {
      last_enabled_context_has_test_key_   = true;
      last_enabled_context_test_key_value_ = *maybe_value;
    }
    else
    {
      last_enabled_context_has_test_key_   = false;
      last_enabled_context_test_key_value_ = false;
    }
    return true;
  }

  bool EnabledImplementation(const context::Context &context,
                             Severity severity,
                             const EventId &event_id) const noexcept override
  {
    ++enabled_with_event_id_calls_;
    last_enabled_severity_ = severity;
    last_enabled_event_id_ = event_id.id_;
    auto value             = context.GetValue("test-key");
    if (const bool *maybe_value = nostd::get_if<bool>(&value))
    {
      last_enabled_context_has_test_key_   = true;
      last_enabled_context_test_key_value_ = *maybe_value;
    }
    else
    {
      last_enabled_context_has_test_key_   = false;
      last_enabled_context_test_key_value_ = false;
    }
    return event_id_enabled_;
  }
#endif  // OPENTELEMETRY_ABI_VERSION_NO >= 2

  bool EnabledImplementation(Severity severity, const EventId &event_id) const noexcept override
  {
    ++enabled_with_event_id_calls_;
    last_enabled_severity_ = severity;
    last_enabled_event_id_ = event_id.id_;
    return event_id_enabled_;
  }

  bool EnabledImplementation(Severity severity, int64_t event_id) const noexcept override
  {
    return EnabledImplementation(severity, EventId{event_id});
  }

private:
  bool event_id_enabled_;
};

// Define a basic LoggerProvider class that returns an instance of the logger class defined above
class TestProvider : public LoggerProvider
{
  nostd::shared_ptr<Logger> GetLogger(nostd::string_view /* logger_name */,
                                      nostd::string_view /* library_name */,
                                      nostd::string_view /* library_version */,
                                      nostd::string_view /* schema_url */,
                                      const common::KeyValueIterable & /* attributes */) override
  {
    return nostd::shared_ptr<Logger>(new TestLogger());
  }
};

TEST(Logger, PushLoggerImplementation)
{
  // Push the new loggerprovider class into the global singleton
  auto test_provider = shared_ptr<LoggerProvider>(new TestProvider());
  Provider::SetLoggerProvider(test_provider);

  auto lp = Provider::GetLoggerProvider();

  // Check that the implementation was pushed by calling TestLogger's GetName()
  nostd::string_view schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto logger = lp->GetLogger("TestLogger", "opentelelemtry_library", "", schema_url);
  ASSERT_EQ("test logger", logger->GetName());
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
TEST(Logger, EnabledWithExplicitContextUsesContextAwareImplementation)
{
  EnablementAwareTestLogger logger(Severity::kTrace);

  context::Context test_context{"test-key", true};

  EXPECT_TRUE(logger.Enabled(test_context, Severity::kInfo));
  EXPECT_EQ(logger.enabled_calls_, 1u);
  EXPECT_EQ(logger.last_enabled_severity_, Severity::kInfo);
  EXPECT_TRUE(logger.last_enabled_context_has_test_key_);
  EXPECT_TRUE(logger.last_enabled_context_test_key_value_);
}
#endif  // OPENTELEMETRY_ABI_VERSION_NO >= 2
