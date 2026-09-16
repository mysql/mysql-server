// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/resource_detectors/container_detector.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/resource_detectors/detail/container_detector_utils.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "opentelemetry/semconv/incubating/container_attributes.h"
#include "opentelemetry/version.h"

#include <string>
#include <unordered_map>
#include <utility>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace resource_detector
{

/**
 * This is the file path from where we can get container.id
 */
constexpr const char *kCGroupPath = "/proc/self/cgroup";

opentelemetry::sdk::resource::Resource ContainerResourceDetector::Detect() noexcept
{
  std::string container_id =
      opentelemetry::resource_detector::detail::GetContainerIDFromCgroup(kCGroupPath);
  if (container_id.empty())
  {
    return ResourceDetector::Create({});
  }

  opentelemetry::sdk::resource::ResourceAttributes attributes;

  attributes[semconv::container::kContainerId] = std::move(container_id);
  return ResourceDetector::Create(attributes);
}

}  // namespace resource_detector
OPENTELEMETRY_END_NAMESPACE
