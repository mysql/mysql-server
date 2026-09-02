// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/logs/severity.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
/**
 * LoggerConfig defines various configurable aspects of a Logger's behavior.
 * This class should not be used directly to configure a Logger's behavior, instead a
 * ScopeConfigurator should be used to compute the desired LoggerConfig which can then be used to
 * configure a Logger.
 */
class OPENTELEMETRY_EXPORT LoggerConfig
{
public:
  bool operator==(const LoggerConfig &other) const noexcept;

  /**
   * Returns if the Logger is enabled or disabled. Loggers are enabled by default.
   * @return a boolean indicating if the Logger is enabled. Defaults to true.
   */
  bool IsEnabled() const noexcept;

  /**
   * Returns the configured minimum severity for the Logger.
   * @return the minimum severity. Defaults to Severity::kInvalid.
   */
  opentelemetry::logs::Severity GetMinimumSeverity() const noexcept;

  /**
   * Returns if trace-based filtering is enabled for the Logger.
   * @return a boolean indicating if trace-based filtering is enabled. Defaults to false.
   */
  bool IsTraceBased() const noexcept;

  /**
   * Returns a LoggerConfig with the provided settings.
   * @param enabled if false, the Logger behaves like a no-op logger.
   * @param minimum_severity the minimum severity required for filtering.
   * @param trace_based if true, trace-based filtering is enabled.
   * @return a LoggerConfig with the provided settings.
   */
  static LoggerConfig Create(bool enabled,
                             opentelemetry::logs::Severity minimum_severity,
                             bool trace_based) noexcept;

  /**
   * Returns a LoggerConfig that represents an enabled Logger.
   * @return a static constant LoggerConfig that represents an enabled logger.
   */
  static LoggerConfig Enabled();

  /**
   * Returns a LoggerConfig that represents a disabled Logger. A disabled logger behaves like a
   * no-op logger.
   * @return a static constant LoggerConfig that represents a disabled logger.
   */
  static LoggerConfig Disabled();

  /**
   * Returns a LoggerConfig that represents a Logger configured with the default behavior.
   * The default behavior is guided by the OpenTelemetry specification.
   * @return a static constant LoggerConfig that represents a logger configured with default
   * behavior.
   */
  static LoggerConfig Default();

private:
  explicit LoggerConfig(
      bool enabled                                   = true,
      opentelemetry::logs::Severity minimum_severity = opentelemetry::logs::Severity::kInvalid,
      bool trace_based                               = false) noexcept
      : enabled_(enabled), minimum_severity_(minimum_severity), trace_based_(trace_based)
  {}

  bool enabled_;
  opentelemetry::logs::Severity minimum_severity_;
  bool trace_based_;
};
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
