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
namespace rpc
{

/**
  Measures the duration of an outgoing Remote Procedure Call (RPC).
  <p>
  When this metric is reported alongside an RPC client span, the metric value
  SHOULD be the same as the RPC client span duration.
  <p>
  histogram
 */
static constexpr const char *kMetricRpcClientCallDuration = "rpc.client.call.duration";
static constexpr const char *descrMetricRpcClientCallDuration =
    "Measures the duration of an outgoing Remote Procedure Call (RPC).";
static constexpr const char *unitMetricRpcClientCallDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientCallDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientCallDuration,
                                      descrMetricRpcClientCallDuration,
                                      unitMetricRpcClientCallDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientCallDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientCallDuration,
                                      descrMetricRpcClientCallDuration,
                                      unitMetricRpcClientCallDuration);
}

/**
  Deprecated, use @code rpc.client.call.duration @endcode instead. Note: the unit also changed from
  @code ms @endcode to @code s @endcode.

  @deprecated
  {"note": "Replaced by @code rpc.client.call.duration @endcode with unit @code s @endcode.",
  "reason": "uncategorized"} <p> While streaming RPCs may record this metric as start-of-batch to
  end-of-batch, it's hard to interpret in practice. <p> <strong>Streaming</strong>: N/A. <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcClientDuration =
    "rpc.client.duration";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcClientDuration =
    "Deprecated, use `rpc.client.call.duration` instead. Note: the unit also changed from `ms` to "
    "`s`.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcClientDuration = "ms";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientDuration, descrMetricRpcClientDuration,
                                      unitMetricRpcClientDuration);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientDuration, descrMetricRpcClientDuration,
                                      unitMetricRpcClientDuration);
}

/**
  Measures the size of RPC request messages (uncompressed).

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  <strong>Streaming</strong>: Recorded per message in a streaming batch
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcClientRequestSize =
    "rpc.client.request.size";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcClientRequestSize =
    "Measures the size of RPC request messages (uncompressed).";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcClientRequestSize = "By";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientRequestSize(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientRequestSize, descrMetricRpcClientRequestSize,
                                      unitMetricRpcClientRequestSize);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientRequestSize(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientRequestSize, descrMetricRpcClientRequestSize,
                                      unitMetricRpcClientRequestSize);
}

/**
  Measures the number of messages received per RPC.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  Should be 1 for all non-streaming RPCs.
  <p>
  <strong>Streaming</strong>: This metric is required for server and client streaming RPCs
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcClientRequestsPerRpc =
    "rpc.client.requests_per_rpc";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcClientRequestsPerRpc =
    "Measures the number of messages received per RPC.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcClientRequestsPerRpc = "{count}";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientRequestsPerRpc(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientRequestsPerRpc,
                                      descrMetricRpcClientRequestsPerRpc,
                                      unitMetricRpcClientRequestsPerRpc);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientRequestsPerRpc(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientRequestsPerRpc,
                                      descrMetricRpcClientRequestsPerRpc,
                                      unitMetricRpcClientRequestsPerRpc);
}

/**
  Measures the size of RPC response messages (uncompressed).

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  <strong>Streaming</strong>: Recorded per response in a streaming batch
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcClientResponseSize =
    "rpc.client.response.size";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcClientResponseSize =
    "Measures the size of RPC response messages (uncompressed).";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcClientResponseSize = "By";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientResponseSize(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientResponseSize,
                                      descrMetricRpcClientResponseSize,
                                      unitMetricRpcClientResponseSize);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientResponseSize(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientResponseSize,
                                      descrMetricRpcClientResponseSize,
                                      unitMetricRpcClientResponseSize);
}

/**
  Measures the number of messages sent per RPC.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  Should be 1 for all non-streaming RPCs.
  <p>
  <strong>Streaming</strong>: This metric is required for server and client streaming RPCs
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcClientResponsesPerRpc =
    "rpc.client.responses_per_rpc";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcClientResponsesPerRpc =
    "Measures the number of messages sent per RPC.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcClientResponsesPerRpc =
    "{count}";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcClientResponsesPerRpc(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcClientResponsesPerRpc,
                                      descrMetricRpcClientResponsesPerRpc,
                                      unitMetricRpcClientResponsesPerRpc);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcClientResponsesPerRpc(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcClientResponsesPerRpc,
                                      descrMetricRpcClientResponsesPerRpc,
                                      unitMetricRpcClientResponsesPerRpc);
}

/**
  Measures the duration of an incoming Remote Procedure Call (RPC).
  <p>
  When this metric is reported alongside an RPC server span, the metric value
  SHOULD be the same as the RPC server span duration.
  <p>
  histogram
 */
static constexpr const char *kMetricRpcServerCallDuration = "rpc.server.call.duration";
static constexpr const char *descrMetricRpcServerCallDuration =
    "Measures the duration of an incoming Remote Procedure Call (RPC).";
static constexpr const char *unitMetricRpcServerCallDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerCallDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerCallDuration,
                                      descrMetricRpcServerCallDuration,
                                      unitMetricRpcServerCallDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerCallDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerCallDuration,
                                      descrMetricRpcServerCallDuration,
                                      unitMetricRpcServerCallDuration);
}

/**
  Deprecated, use @code rpc.server.call.duration @endcode instead. Note: the unit also changed from
  @code ms @endcode to @code s @endcode.

  @deprecated
  {"note": "Replaced by @code rpc.server.call.duration @endcode with unit @code s @endcode.",
  "reason": "uncategorized"} <p> While streaming RPCs may record this metric as start-of-batch to
  end-of-batch, it's hard to interpret in practice. <p> <strong>Streaming</strong>: N/A. <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcServerDuration =
    "rpc.server.duration";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcServerDuration =
    "Deprecated, use `rpc.server.call.duration` instead. Note: the unit also changed from `ms` to "
    "`s`.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcServerDuration = "ms";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerDuration, descrMetricRpcServerDuration,
                                      unitMetricRpcServerDuration);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerDuration, descrMetricRpcServerDuration,
                                      unitMetricRpcServerDuration);
}

/**
  Measures the size of RPC request messages (uncompressed).

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  <strong>Streaming</strong>: Recorded per message in a streaming batch
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcServerRequestSize =
    "rpc.server.request.size";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcServerRequestSize =
    "Measures the size of RPC request messages (uncompressed).";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcServerRequestSize = "By";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerRequestSize(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerRequestSize, descrMetricRpcServerRequestSize,
                                      unitMetricRpcServerRequestSize);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerRequestSize(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerRequestSize, descrMetricRpcServerRequestSize,
                                      unitMetricRpcServerRequestSize);
}

/**
  Measures the number of messages received per RPC.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  Should be 1 for all non-streaming RPCs.
  <p>
  <strong>Streaming</strong> : This metric is required for server and client streaming RPCs
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcServerRequestsPerRpc =
    "rpc.server.requests_per_rpc";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcServerRequestsPerRpc =
    "Measures the number of messages received per RPC.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcServerRequestsPerRpc = "{count}";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerRequestsPerRpc(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerRequestsPerRpc,
                                      descrMetricRpcServerRequestsPerRpc,
                                      unitMetricRpcServerRequestsPerRpc);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerRequestsPerRpc(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerRequestsPerRpc,
                                      descrMetricRpcServerRequestsPerRpc,
                                      unitMetricRpcServerRequestsPerRpc);
}

/**
  Measures the size of RPC response messages (uncompressed).

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  <strong>Streaming</strong>: Recorded per response in a streaming batch
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcServerResponseSize =
    "rpc.server.response.size";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcServerResponseSize =
    "Measures the size of RPC response messages (uncompressed).";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcServerResponseSize = "By";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerResponseSize(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerResponseSize,
                                      descrMetricRpcServerResponseSize,
                                      unitMetricRpcServerResponseSize);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerResponseSize(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerResponseSize,
                                      descrMetricRpcServerResponseSize,
                                      unitMetricRpcServerResponseSize);
}

/**
  Measures the number of messages sent per RPC.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
  <p>
  Should be 1 for all non-streaming RPCs.
  <p>
  <strong>Streaming</strong>: This metric is required for server and client streaming RPCs
  <p>
  histogram
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMetricRpcServerResponsesPerRpc =
    "rpc.server.responses_per_rpc";
OPENTELEMETRY_DEPRECATED static constexpr const char *descrMetricRpcServerResponsesPerRpc =
    "Measures the number of messages sent per RPC.";
OPENTELEMETRY_DEPRECATED static constexpr const char *unitMetricRpcServerResponsesPerRpc =
    "{count}";

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricRpcServerResponsesPerRpc(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricRpcServerResponsesPerRpc,
                                      descrMetricRpcServerResponsesPerRpc,
                                      unitMetricRpcServerResponsesPerRpc);
}

OPENTELEMETRY_DEPRECATED static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricRpcServerResponsesPerRpc(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricRpcServerResponsesPerRpc,
                                      descrMetricRpcServerResponsesPerRpc,
                                      unitMetricRpcServerResponsesPerRpc);
}

}  // namespace rpc
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
