// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/logger_config.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

OPENTELEMETRY_EXPORT bool LoggerConfig::operator==(const LoggerConfig &other) const noexcept
{
  return enabled_ == other.enabled_ && minimum_severity_ == other.minimum_severity_ &&
         trace_based_ == other.trace_based_;
}

OPENTELEMETRY_EXPORT bool LoggerConfig::IsEnabled() const noexcept
{
  return enabled_;
}

OPENTELEMETRY_EXPORT opentelemetry::logs::Severity LoggerConfig::GetMinimumSeverity() const noexcept
{
  return minimum_severity_;
}

OPENTELEMETRY_EXPORT bool LoggerConfig::IsTraceBased() const noexcept
{
  return trace_based_;
}

OPENTELEMETRY_EXPORT LoggerConfig
LoggerConfig::Create(bool enabled,
                     opentelemetry::logs::Severity minimum_severity,
                     bool trace_based) noexcept
{
  return LoggerConfig(enabled, minimum_severity, trace_based);
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Enabled()
{
  return Default();
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Disabled()
{
  static const auto kDisabledConfig = Create(false, opentelemetry::logs::Severity::kInvalid, false);
  return kDisabledConfig;
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Default()
{
  static const auto kDefaultConfig = Create(true, opentelemetry::logs::Severity::kInvalid, false);
  return kDefaultConfig;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
