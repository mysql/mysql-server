// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0/

#pragma once

#if OPENTELEMETRY_ABI_VERSION_NO < 2
#  include "opentelemetry/logs/event_logger.h"
#  include "opentelemetry/logs/event_logger_provider.h"
#  include "opentelemetry/logs/logger.h"
#  include "opentelemetry/nostd/shared_ptr.h"
#  include "opentelemetry/nostd/string_view.h"
#endif

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
/**
 * Sdk implementation of EventLoggerProvider.
 * @deprecated
 */
class OPENTELEMETRY_EXPORT EventLoggerProvider final
    : public opentelemetry::logs::EventLoggerProvider
{
public:
  EventLoggerProvider() noexcept;

  EventLoggerProvider(const EventLoggerProvider &)            = delete;
  EventLoggerProvider(EventLoggerProvider &&)                 = delete;
  EventLoggerProvider &operator=(const EventLoggerProvider &) = delete;
  EventLoggerProvider &operator=(EventLoggerProvider &&)      = delete;

  ~EventLoggerProvider() override;

  nostd::shared_ptr<opentelemetry::logs::EventLogger> CreateEventLogger(
      nostd::shared_ptr<opentelemetry::logs::Logger> delegate_logger,
      nostd::string_view event_domain) noexcept override;
};
#endif
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
