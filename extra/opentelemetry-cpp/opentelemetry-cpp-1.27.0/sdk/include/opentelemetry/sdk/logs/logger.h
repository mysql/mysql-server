// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "logger_config.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

class Logger final : public opentelemetry::logs::Logger
{
public:
  /**
   * Initialize a new logger.
   * @param name The name of this logger instance
   * @param context The logger provider that owns this logger.
   * @param instrumentation_scope The instrumentation scope for this logger.
   */
  explicit Logger(
      opentelemetry::nostd::string_view name,
      std::shared_ptr<LoggerContext> context,
      std::unique_ptr<instrumentationscope::InstrumentationScope> instrumentation_scope =
          instrumentationscope::InstrumentationScope::Create("")) noexcept;

  /**
   * Returns the name of this logger.
   */
  const opentelemetry::nostd::string_view GetName() noexcept override;

  nostd::unique_ptr<opentelemetry::logs::LogRecord> CreateLogRecord() noexcept override;

  using opentelemetry::logs::Logger::EmitLogRecord;

  void EmitLogRecord(
      nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept override;

  /** Returns the associated instrumentation scope */
  const opentelemetry::sdk::instrumentationscope::InstrumentationScope &GetInstrumentationScope()
      const noexcept;

  OPENTELEMETRY_DEPRECATED_MESSAGE("Please use GetInstrumentationScope instead")
  const opentelemetry::sdk::instrumentationscope::InstrumentationScope &GetInstrumentationLibrary()
      const noexcept
  {
    return GetInstrumentationScope();
  }

private:
  bool EnabledImplementation(opentelemetry::logs::Severity severity,
                             const opentelemetry::logs::EventId &event_id) const noexcept override;

  bool EnabledImplementation(opentelemetry::logs::Severity severity,
                             int64_t event_id) const noexcept override;

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  bool EnabledImplementation(const opentelemetry::context::Context &context,
                             opentelemetry::logs::Severity severity) const noexcept override;

  bool EnabledImplementation(const opentelemetry::context::Context &context,
                             opentelemetry::logs::Severity severity,
                             const opentelemetry::logs::EventId &event_id) const noexcept override;
#endif  // OPENTELEMETRY_ABI_VERSION_NO >= 2
  // The name of this logger
  std::string logger_name_;

  // order of declaration is important here - instrumentation scope should destroy after
  // logger-context.
  std::unique_ptr<instrumentationscope::InstrumentationScope> instrumentation_scope_;
  std::shared_ptr<LoggerContext> context_;
  LoggerConfig logger_config_;
  static opentelemetry::logs::NoopLogger kNoopLogger;
};

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
