// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class PrometheusPullMetricExporterConfiguration;
class ExtensionPullMetricExporterConfiguration;

class PullMetricExporterConfigurationVisitor
{
public:
  PullMetricExporterConfigurationVisitor()                                               = default;
  PullMetricExporterConfigurationVisitor(PullMetricExporterConfigurationVisitor &&)      = default;
  PullMetricExporterConfigurationVisitor(const PullMetricExporterConfigurationVisitor &) = default;
  PullMetricExporterConfigurationVisitor &operator=(PullMetricExporterConfigurationVisitor &&) =
      default;
  PullMetricExporterConfigurationVisitor &operator=(
      const PullMetricExporterConfigurationVisitor &other) = default;
  virtual ~PullMetricExporterConfigurationVisitor()        = default;

  virtual void VisitPrometheus(const PrometheusPullMetricExporterConfiguration *model) = 0;
  virtual void VisitExtension(const ExtensionPullMetricExporterConfiguration *model)   = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
