// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/logger_provider.json
// YAML-NODE: BatchLogRecordProcessor
class BatchLogRecordProcessorConfiguration : public LogRecordProcessorConfiguration
{
public:
  void Accept(LogRecordProcessorConfigurationVisitor *visitor) const override
  {
    visitor->VisitBatch(this);
  }

  std::size_t schedule_delay{0};
  std::size_t export_timeout{0};
  std::size_t max_queue_size{0};
  std::size_t max_export_batch_size{0};
  std::unique_ptr<LogRecordExporterConfiguration> exporter;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
