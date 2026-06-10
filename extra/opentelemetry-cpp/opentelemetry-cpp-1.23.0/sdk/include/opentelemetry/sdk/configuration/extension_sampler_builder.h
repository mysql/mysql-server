// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/extension_sampler_configuration.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionSamplerBuilder
{
public:
  ExtensionSamplerBuilder()                                                = default;
  ExtensionSamplerBuilder(ExtensionSamplerBuilder &&)                      = default;
  ExtensionSamplerBuilder(const ExtensionSamplerBuilder &)                 = default;
  ExtensionSamplerBuilder &operator=(ExtensionSamplerBuilder &&)           = default;
  ExtensionSamplerBuilder &operator=(const ExtensionSamplerBuilder &other) = default;
  virtual ~ExtensionSamplerBuilder()                                       = default;

  virtual std::unique_ptr<opentelemetry::sdk::trace::Sampler> Build(
      const opentelemetry::sdk::configuration::ExtensionSamplerConfiguration *model) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
