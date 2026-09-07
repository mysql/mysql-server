// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/metric_producer_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionMetricProducerConfiguration : public MetricProducerConfiguration
{
public:
  void Accept(MetricProducerConfigurationVisitor *visitor) const override
  {
    visitor->VisitExtension(this);
  }

  std::string name;
  std::unique_ptr<DocumentNode> node;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
