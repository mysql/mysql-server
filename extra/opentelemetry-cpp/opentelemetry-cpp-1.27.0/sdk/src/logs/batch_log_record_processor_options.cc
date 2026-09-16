// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "opentelemetry/sdk/common/env_variables.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
namespace batch_log_record_processor_options_env
{

// Environment variable names
static constexpr const char *kMaxQueueSizeEnv       = "OTEL_BLRP_MAX_QUEUE_SIZE";
static constexpr const char *kScheduleDelayEnv      = "OTEL_BLRP_SCHEDULE_DELAY";
static constexpr const char *kExportTimeoutEnv      = "OTEL_BLRP_EXPORT_TIMEOUT";
static constexpr const char *kMaxExportBatchSizeEnv = "OTEL_BLRP_MAX_EXPORT_BATCH_SIZE";

// Default values
static constexpr size_t kDefaultMaxQueueSize       = 2048;
static constexpr size_t kDefaultMaxExportBatchSize = 512;

size_t GetMaxQueueSizeFromEnv()
{
  std::uint32_t value{};
  if (!opentelemetry::sdk::common::GetUintEnvironmentVariable(kMaxQueueSizeEnv, value))
  {
    return kDefaultMaxQueueSize;
  }
  return static_cast<size_t>(value);
}

std::chrono::milliseconds GetScheduleDelayFromEnv()
{
  static const std::chrono::milliseconds kDefaultScheduleDelay{1000};
  std::chrono::system_clock::duration duration{0};
  if (!opentelemetry::sdk::common::GetDurationEnvironmentVariable(kScheduleDelayEnv, duration))
  {
    return kDefaultScheduleDelay;
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}

std::chrono::milliseconds GetExportTimeoutFromEnv()
{
  static const std::chrono::milliseconds kDefaultExportTimeout{30000};
  std::chrono::system_clock::duration duration{0};
  if (!opentelemetry::sdk::common::GetDurationEnvironmentVariable(kExportTimeoutEnv, duration))
  {
    return kDefaultExportTimeout;
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}

size_t GetMaxExportBatchSizeFromEnv()
{
  std::uint32_t value{};
  if (!opentelemetry::sdk::common::GetUintEnvironmentVariable(kMaxExportBatchSizeEnv, value))
  {
    return kDefaultMaxExportBatchSize;
  }
  return static_cast<size_t>(value);
}

}  // namespace batch_log_record_processor_options_env
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
