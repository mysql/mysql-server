// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace logs
{

class EventLogger;
class Logger;

#if OPENTELEMETRY_ABI_VERSION_NO < 2
/**
 * Creates new EventLogger instances.
 * @deprecated
 */
class EventLoggerProvider
{
public:
  EventLoggerProvider()                                           = default;
  EventLoggerProvider(const EventLoggerProvider &)                = default;
  EventLoggerProvider(EventLoggerProvider &&) noexcept            = default;
  EventLoggerProvider &operator=(const EventLoggerProvider &)     = default;
  EventLoggerProvider &operator=(EventLoggerProvider &&) noexcept = default;
  virtual ~EventLoggerProvider()                                  = default;

  /**
   * Creates a named EventLogger instance.
   *
   */

  virtual nostd::shared_ptr<EventLogger> CreateEventLogger(
      nostd::shared_ptr<Logger> delegate_logger,
      nostd::string_view event_domain) noexcept = 0;
};
#endif
}  // namespace logs
OPENTELEMETRY_END_NAMESPACE
