/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_metrics-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace mcp
{

/**
  The duration of the MCP request or notification as observed on the sender from the time it was
  sent until the response or ack is received. <p> histogram
 */
static constexpr const char *kMetricMcpClientOperationDuration = "mcp.client.operation.duration";
static constexpr const char *descrMetricMcpClientOperationDuration =
    "The duration of the MCP request or notification as observed on the sender from the time it was sent until the response or ack is received.
    ";
    static constexpr const char *unitMetricMcpClientOperationDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricMcpClientOperationDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricMcpClientOperationDuration,
                                      descrMetricMcpClientOperationDuration,
                                      unitMetricMcpClientOperationDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricMcpClientOperationDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricMcpClientOperationDuration,
                                      descrMetricMcpClientOperationDuration,
                                      unitMetricMcpClientOperationDuration);
}

/**
  The duration of the MCP session as observed on the MCP client.
  <p>
  histogram
 */
static constexpr const char *kMetricMcpClientSessionDuration = "mcp.client.session.duration";
static constexpr const char *descrMetricMcpClientSessionDuration =
    "The duration of the MCP session as observed on the MCP client.";
static constexpr const char *unitMetricMcpClientSessionDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricMcpClientSessionDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricMcpClientSessionDuration,
                                      descrMetricMcpClientSessionDuration,
                                      unitMetricMcpClientSessionDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricMcpClientSessionDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricMcpClientSessionDuration,
                                      descrMetricMcpClientSessionDuration,
                                      unitMetricMcpClientSessionDuration);
}

/**
  MCP request or notification duration as observed on the receiver from the time it was received
  until the result or ack is sent. <p> histogram
 */
static constexpr const char *kMetricMcpServerOperationDuration = "mcp.server.operation.duration";
static constexpr const char *descrMetricMcpServerOperationDuration =
    "MCP request or notification duration as observed on the receiver from the time it was received until the result or ack is sent.
    ";
    static constexpr const char *unitMetricMcpServerOperationDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricMcpServerOperationDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricMcpServerOperationDuration,
                                      descrMetricMcpServerOperationDuration,
                                      unitMetricMcpServerOperationDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricMcpServerOperationDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricMcpServerOperationDuration,
                                      descrMetricMcpServerOperationDuration,
                                      unitMetricMcpServerOperationDuration);
}

/**
  The duration of the MCP session as observed on the MCP server.
  <p>
  histogram
 */
static constexpr const char *kMetricMcpServerSessionDuration = "mcp.server.session.duration";
static constexpr const char *descrMetricMcpServerSessionDuration =
    "The duration of the MCP session as observed on the MCP server.";
static constexpr const char *unitMetricMcpServerSessionDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricMcpServerSessionDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricMcpServerSessionDuration,
                                      descrMetricMcpServerSessionDuration,
                                      unitMetricMcpServerSessionDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricMcpServerSessionDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricMcpServerSessionDuration,
                                      descrMetricMcpServerSessionDuration,
                                      unitMetricMcpServerSessionDuration);
}

}  // namespace mcp
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
