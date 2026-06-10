// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class AlwaysOffSamplerConfiguration;
class AlwaysOnSamplerConfiguration;
class JaegerRemoteSamplerConfiguration;
class ParentBasedSamplerConfiguration;
class TraceIdRatioBasedSamplerConfiguration;
class ExtensionSamplerConfiguration;

class SamplerConfigurationVisitor
{
public:
  SamplerConfigurationVisitor()                                                    = default;
  SamplerConfigurationVisitor(SamplerConfigurationVisitor &&)                      = default;
  SamplerConfigurationVisitor(const SamplerConfigurationVisitor &)                 = default;
  SamplerConfigurationVisitor &operator=(SamplerConfigurationVisitor &&)           = default;
  SamplerConfigurationVisitor &operator=(const SamplerConfigurationVisitor &other) = default;
  virtual ~SamplerConfigurationVisitor()                                           = default;

  virtual void VisitAlwaysOff(const AlwaysOffSamplerConfiguration *model)                 = 0;
  virtual void VisitAlwaysOn(const AlwaysOnSamplerConfiguration *model)                   = 0;
  virtual void VisitJaegerRemote(const JaegerRemoteSamplerConfiguration *model)           = 0;
  virtual void VisitParentBased(const ParentBasedSamplerConfiguration *model)             = 0;
  virtual void VisitTraceIdRatioBased(const TraceIdRatioBasedSamplerConfiguration *model) = 0;
  virtual void VisitExtension(const ExtensionSamplerConfiguration *model)                 = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
