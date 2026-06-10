// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/view_selector_configuration.h"
#include "opentelemetry/sdk/configuration/view_stream_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: View
class ViewConfiguration
{
public:
  std::unique_ptr<ViewSelectorConfiguration> selector;
  std::unique_ptr<ViewStreamConfiguration> stream;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
