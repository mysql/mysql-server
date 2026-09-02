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
namespace gen_ai
{

/**
  GenAI operation duration.
  <p>
  histogram
 */
static constexpr const char *kMetricGenAiClientOperationDuration =
    "gen_ai.client.operation.duration";
static constexpr const char *descrMetricGenAiClientOperationDuration = "GenAI operation duration.";
static constexpr const char *unitMetricGenAiClientOperationDuration  = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiClientOperationDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiClientOperationDuration,
                                      descrMetricGenAiClientOperationDuration,
                                      unitMetricGenAiClientOperationDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiClientOperationDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiClientOperationDuration,
                                      descrMetricGenAiClientOperationDuration,
                                      unitMetricGenAiClientOperationDuration);
}

/**
  Time per output chunk, recorded for each chunk received after the first one, measured as the time
  elapsed from the end of the previous chunk to the end of the current chunk. <p> This metrics
  SHOULD be reported for streaming calls and SHOULD NOT be reported otherwise. <p> histogram
 */
static constexpr const char *kMetricGenAiClientOperationTimePerOutputChunk =
    "gen_ai.client.operation.time_per_output_chunk";
static constexpr const char *descrMetricGenAiClientOperationTimePerOutputChunk =
    "Time per output chunk, recorded for each chunk received after the first one, measured as the time elapsed from the end of the previous chunk to the end of the current chunk.
    ";
    static constexpr const char *unitMetricGenAiClientOperationTimePerOutputChunk = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiClientOperationTimePerOutputChunk(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiClientOperationTimePerOutputChunk,
                                      descrMetricGenAiClientOperationTimePerOutputChunk,
                                      unitMetricGenAiClientOperationTimePerOutputChunk);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiClientOperationTimePerOutputChunk(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiClientOperationTimePerOutputChunk,
                                      descrMetricGenAiClientOperationTimePerOutputChunk,
                                      unitMetricGenAiClientOperationTimePerOutputChunk);
}

/**
  Time to receive the first chunk, measured from when the client issues the generation request to
  when the first chunk is received in the response stream. <p> This metrics SHOULD be reported for
  streaming calls and SHOULD NOT be reported otherwise. <p> histogram
 */
static constexpr const char *kMetricGenAiClientOperationTimeToFirstChunk =
    "gen_ai.client.operation.time_to_first_chunk";
static constexpr const char *descrMetricGenAiClientOperationTimeToFirstChunk =
    "Time to receive the first chunk, measured from when the client issues the generation request "
    "to when the first chunk is received in the response stream.";
static constexpr const char *unitMetricGenAiClientOperationTimeToFirstChunk = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiClientOperationTimeToFirstChunk(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiClientOperationTimeToFirstChunk,
                                      descrMetricGenAiClientOperationTimeToFirstChunk,
                                      unitMetricGenAiClientOperationTimeToFirstChunk);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiClientOperationTimeToFirstChunk(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiClientOperationTimeToFirstChunk,
                                      descrMetricGenAiClientOperationTimeToFirstChunk,
                                      unitMetricGenAiClientOperationTimeToFirstChunk);
}

/**
  Number of input and output tokens used.
  <p>
  histogram
 */
static constexpr const char *kMetricGenAiClientTokenUsage = "gen_ai.client.token.usage";
static constexpr const char *descrMetricGenAiClientTokenUsage =
    "Number of input and output tokens used.";
static constexpr const char *unitMetricGenAiClientTokenUsage = "{token}";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiClientTokenUsage(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiClientTokenUsage,
                                      descrMetricGenAiClientTokenUsage,
                                      unitMetricGenAiClientTokenUsage);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiClientTokenUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiClientTokenUsage,
                                      descrMetricGenAiClientTokenUsage,
                                      unitMetricGenAiClientTokenUsage);
}

/**
  Generative AI server request duration such as time-to-last byte or last output token.
  <p>
  histogram
 */
static constexpr const char *kMetricGenAiServerRequestDuration = "gen_ai.server.request.duration";
static constexpr const char *descrMetricGenAiServerRequestDuration =
    "Generative AI server request duration such as time-to-last byte or last output token.";
static constexpr const char *unitMetricGenAiServerRequestDuration = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiServerRequestDuration(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiServerRequestDuration,
                                      descrMetricGenAiServerRequestDuration,
                                      unitMetricGenAiServerRequestDuration);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiServerRequestDuration(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiServerRequestDuration,
                                      descrMetricGenAiServerRequestDuration,
                                      unitMetricGenAiServerRequestDuration);
}

/**
  Time per output token generated after the first token for successful responses.
  <p>
  histogram
 */
static constexpr const char *kMetricGenAiServerTimePerOutputToken =
    "gen_ai.server.time_per_output_token";
static constexpr const char *descrMetricGenAiServerTimePerOutputToken =
    "Time per output token generated after the first token for successful responses.";
static constexpr const char *unitMetricGenAiServerTimePerOutputToken = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiServerTimePerOutputToken(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiServerTimePerOutputToken,
                                      descrMetricGenAiServerTimePerOutputToken,
                                      unitMetricGenAiServerTimePerOutputToken);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiServerTimePerOutputToken(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiServerTimePerOutputToken,
                                      descrMetricGenAiServerTimePerOutputToken,
                                      unitMetricGenAiServerTimePerOutputToken);
}

/**
  Time to generate first token for successful responses.
  <p>
  histogram
 */
static constexpr const char *kMetricGenAiServerTimeToFirstToken =
    "gen_ai.server.time_to_first_token";
static constexpr const char *descrMetricGenAiServerTimeToFirstToken =
    "Time to generate first token for successful responses.";
static constexpr const char *unitMetricGenAiServerTimeToFirstToken = "s";

static inline nostd::unique_ptr<metrics::Histogram<uint64_t>>
CreateSyncInt64MetricGenAiServerTimeToFirstToken(metrics::Meter *meter)
{
  return meter->CreateUInt64Histogram(kMetricGenAiServerTimeToFirstToken,
                                      descrMetricGenAiServerTimeToFirstToken,
                                      unitMetricGenAiServerTimeToFirstToken);
}

static inline nostd::unique_ptr<metrics::Histogram<double>>
CreateSyncDoubleMetricGenAiServerTimeToFirstToken(metrics::Meter *meter)
{
  return meter->CreateDoubleHistogram(kMetricGenAiServerTimeToFirstToken,
                                      descrMetricGenAiServerTimeToFirstToken,
                                      unitMetricGenAiServerTimeToFirstToken);
}

}  // namespace gen_ai
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
