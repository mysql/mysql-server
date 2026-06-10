// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionSamplerConfiguration : public SamplerConfiguration
{
public:
  void Accept(SamplerConfigurationVisitor *visitor) const override
  {
    visitor->VisitExtension(this);
  }

  std::string name;
  std::unique_ptr<DocumentNode> node;
  std::size_t depth{0};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
