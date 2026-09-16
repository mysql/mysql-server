/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace message
{

/**
  Deprecated, no replacement at this time.

  @deprecated
  {"note": "Deprecated, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMessageCompressedSize =
    "message.compressed_size";

/**
  Deprecated, no replacement at this time.

  @deprecated
  {"note": "Deprecated, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMessageId = "message.id";

/**
  Deprecated, no replacement at this time.

  @deprecated
  {"note": "Deprecated, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMessageType = "message.type";

/**
  Deprecated, no replacement at this time.

  @deprecated
  {"note": "Deprecated, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kMessageUncompressedSize =
    "message.uncompressed_size";

namespace MessageTypeValues
{

static constexpr const char *kSent = "SENT";

static constexpr const char *kReceived = "RECEIVED";

}  // namespace MessageTypeValues

}  // namespace message
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
