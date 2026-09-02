// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <chrono>
#include <string>
#include <utility>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/context/context_value.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/logs/event_id.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
namespace trace_api = opentelemetry::trace;
namespace common    = opentelemetry::common;
namespace context   = opentelemetry::context;
namespace nostd     = opentelemetry::nostd;

namespace
{
nostd::string_view GetEventName(const opentelemetry::logs::EventId &event_id) noexcept
{
  return event_id.name_ != nullptr ? nostd::string_view{event_id.name_.get()}
                                   : nostd::string_view{};
}

bool IsAllowedByTraceBasedFiltering(const context::Context &context,
                                    const LoggerConfig &logger_config) noexcept
{
  if (!logger_config.IsTraceBased())
  {
    return true;
  }

  const trace_api::SpanContext span_context = trace_api::GetSpan(context)->GetContext();

  if (!span_context.span_id().IsValid())
  {
    return true;
  }

  return span_context.trace_flags().IsSampled();
}
}  // namespace

opentelemetry::logs::NoopLogger Logger::kNoopLogger = opentelemetry::logs::NoopLogger();

Logger::Logger(
    opentelemetry::nostd::string_view name,
    std::shared_ptr<LoggerContext> context,
    std::unique_ptr<instrumentationscope::InstrumentationScope> instrumentation_scope) noexcept
    : logger_name_(std::string(name)),
      instrumentation_scope_(std::move(instrumentation_scope)),
      context_(std::move(context)),
      logger_config_(context_->GetLoggerConfigurator().ComputeConfig(*instrumentation_scope_))
{
  SetMinimumSeverity(logger_config_.IsEnabled()
                         ? static_cast<uint8_t>(logger_config_.GetMinimumSeverity())
                         : opentelemetry::logs::kMaxSeverity);
}

const opentelemetry::nostd::string_view Logger::GetName() noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.GetName();
  }
  return logger_name_;
}

opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> Logger::CreateLogRecord() noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.CreateLogRecord();
  }

  auto recordable = context_->GetProcessor().MakeRecordable();

  recordable->SetObservedTimestamp(std::chrono::system_clock::now());

  // Get the current span metadata from the runtime context
  const auto current_context = context::RuntimeContext::GetCurrent();

  if (current_context.HasKey(trace_api::kSpanKey))
  {
    const context::ContextValue context_value = current_context.GetValue(trace_api::kSpanKey);

    const trace_api::SpanContext span_context = [&context_value]() {
      // Get the span metadata from the active span in the runtime context
      if (const nostd::shared_ptr<trace_api::Span> *maybe_span =
              nostd::get_if<nostd::shared_ptr<trace_api::Span>>(&context_value))
      {
        const nostd::shared_ptr<trace_api::Span> &span = *maybe_span;
        return span->GetContext();
      }
      // Get the span metadata directly from a SpanContext in the runtime context.
      // TODO: This path is unused and may be removed in the future.
      else if (const nostd::shared_ptr<trace_api::SpanContext> *maybe_span_context =
                   nostd::get_if<nostd::shared_ptr<trace_api::SpanContext>>(&context_value))
      {
        const nostd::shared_ptr<trace_api::SpanContext> &span_context = *maybe_span_context;
        return *span_context;
      }
      return trace_api::SpanContext::GetInvalid();
    }();

    if (span_context.IsValid())
    {
      recordable->SetTraceId(span_context.trace_id());
      recordable->SetTraceFlags(span_context.trace_flags());
      recordable->SetSpanId(span_context.span_id());
    }
  }

  return opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord>(recordable.release());
}

void Logger::EmitLogRecord(
    opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.EmitLogRecord(std::move(log_record));
  }

  if (!log_record)
  {
    return;
  }

  std::unique_ptr<Recordable> recordable =
      std::unique_ptr<Recordable>(static_cast<Recordable *>(log_record.release()));
  recordable->SetResource(context_->GetResource());
  recordable->SetInstrumentationScope(GetInstrumentationScope());

  auto &processor = context_->GetProcessor();

  // TODO: Sampler (should include check for minSeverity)

  // Send the log recordable to the processor
  processor.OnEmit(std::move(recordable));
}

bool Logger::EnabledImplementation(opentelemetry::logs::Severity severity,
                                   const opentelemetry::logs::EventId &event_id) const noexcept
{
  const auto &current = context::RuntimeContext::GetCurrent();
  if (!IsAllowedByTraceBasedFiltering(current, logger_config_))
  {
    return false;
  }

  return context_->GetProcessor().Enabled(current, GetInstrumentationScope(), severity,
                                          GetEventName(event_id));
}

bool Logger::EnabledImplementation(opentelemetry::logs::Severity severity,
                                   int64_t /*event_id*/) const noexcept
{
  const auto &current = context::RuntimeContext::GetCurrent();
  if (!IsAllowedByTraceBasedFiltering(current, logger_config_))
  {
    return false;
  }

  return context_->GetProcessor().Enabled(current, GetInstrumentationScope(), severity);
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
bool Logger::EnabledImplementation(const opentelemetry::context::Context &context,
                                   opentelemetry::logs::Severity severity) const noexcept
{
  if (!IsAllowedByTraceBasedFiltering(context, logger_config_))
  {
    return false;
  }

  return context_->GetProcessor().Enabled(context, GetInstrumentationScope(), severity);
}

bool Logger::EnabledImplementation(const opentelemetry::context::Context &context,
                                   opentelemetry::logs::Severity severity,
                                   const opentelemetry::logs::EventId &event_id) const noexcept
{
  if (!IsAllowedByTraceBasedFiltering(context, logger_config_))
  {
    return false;
  }

  return context_->GetProcessor().Enabled(context, GetInstrumentationScope(), severity,
                                          GetEventName(event_id));
}
#endif  // OPENTELEMETRY_ABI_VERSION_NO >= 2

const opentelemetry::sdk::instrumentationscope::InstrumentationScope &
Logger::GetInstrumentationScope() const noexcept
{
  return *instrumentation_scope_;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
