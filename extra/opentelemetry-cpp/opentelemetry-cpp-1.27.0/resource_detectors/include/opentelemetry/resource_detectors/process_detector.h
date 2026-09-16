// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace resource_detector
{

/**
 * ProcessResourceDetector to detect resource attributes when running in a process.
 * This detector extracts metadata such as process ID, executable path, and command line arguments
 * and sets attributes like process.pid, process.executable.path, and process.command following
 * the OpenTelemetry semantic conventions.
 */
class ProcessResourceDetector : public opentelemetry::sdk::resource::ResourceDetector
{
public:
  /**
   * Detect retrieves the resource attributes for the current process.
   * It reads:
   * - process.pid from the current process ID
   * - process.executable.path from the executable path of the current process
   * - process.command from the command used to launch the process
   * and returns a Resource with these attributes set.
   */
  opentelemetry::sdk::resource::Resource Detect() noexcept override;
};

}  // namespace resource_detector
OPENTELEMETRY_END_NAMESPACE
