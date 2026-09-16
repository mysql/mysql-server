// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstddef>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{

namespace logs
{

namespace batch_log_record_processor_options_env
{

/// @brief Returns the max queue size from the OTEL_BLRP_MAX_QUEUE_SIZE environment variable
/// or the default value (2048) if not set.
OPENTELEMETRY_EXPORT size_t GetMaxQueueSizeFromEnv();

/// @brief Returns the schedule delay from the OTEL_BLRP_SCHEDULE_DELAY environment variable
/// or the default value (1000ms) if not set.
OPENTELEMETRY_EXPORT std::chrono::milliseconds GetScheduleDelayFromEnv();

/// @brief Returns the export timeout from the OTEL_BLRP_EXPORT_TIMEOUT environment variable
/// or the default value (30000ms) if not set.
OPENTELEMETRY_EXPORT std::chrono::milliseconds GetExportTimeoutFromEnv();

/// @brief Returns the max export batch size from the OTEL_BLRP_MAX_EXPORT_BATCH_SIZE environment
/// variable or the default value (512) if not set.
OPENTELEMETRY_EXPORT size_t GetMaxExportBatchSizeFromEnv();

}  // namespace batch_log_record_processor_options_env

/**
 * Struct to hold batch LogRecordProcessor options.
 *
 * This is an aggregate type that supports C++20 designated initializers.
 * Default values are read from environment variables when an instance is created:
 * - OTEL_BLRP_MAX_QUEUE_SIZE (default: 2048)
 * - OTEL_BLRP_SCHEDULE_DELAY (default: 1000ms)
 * - OTEL_BLRP_EXPORT_TIMEOUT (default: 30000ms)
 * - OTEL_BLRP_MAX_EXPORT_BATCH_SIZE (default: 512)
 *
 * Usage notes:
 * - If you use default initialization (e.g., `BatchLogRecordProcessorOptions opts{}`), all fields
 *   are set by reading the environment variables (or hardcoded defaults if unset).
 * - If you use aggregate initialization with explicit values (positional or designated),
 *   those values override the environment variable defaults for the specified fields.
 * - With C++20 designated initializers, you can override only specific fields; unspecified
 *   fields will use environment variables or hardcoded defaults.
 */
struct OPENTELEMETRY_EXPORT BatchLogRecordProcessorOptions
{
  /**
   * The maximum buffer/queue size. After the size is reached, log records are
   * dropped.
   */
  size_t max_queue_size = batch_log_record_processor_options_env::GetMaxQueueSizeFromEnv();

  /* The time interval between two consecutive exports. */
  std::chrono::milliseconds schedule_delay_millis =
      batch_log_record_processor_options_env::GetScheduleDelayFromEnv();

  /**
   * The maximum time allowed to export data.
   * It is not currently used by the SDK and the parameter is ignored.
   * TODO: Implement the parameter in BatchLogRecordProcessor
   */
  std::chrono::milliseconds export_timeout_millis =
      batch_log_record_processor_options_env::GetExportTimeoutFromEnv();

  /**
   * The maximum batch size of every export. It must be smaller or
   * equal to max_queue_size.
   */
  size_t max_export_batch_size =
      batch_log_record_processor_options_env::GetMaxExportBatchSizeFromEnv();
};

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
