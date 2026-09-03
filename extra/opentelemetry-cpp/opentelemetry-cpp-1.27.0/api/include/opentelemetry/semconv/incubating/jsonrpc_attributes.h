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
namespace jsonrpc
{

/**
  Protocol version, as specified in the @code jsonrpc @endcode property of the request and its
  corresponding response.
 */
static constexpr const char *kJsonrpcProtocolVersion = "jsonrpc.protocol.version";

/**
  A string representation of the @code id @endcode property of the request and its corresponding
  response. <p> Under the <a href="https://www.jsonrpc.org/specification">JSON-RPC
  specification</a>, the @code id @endcode property may be a string, number, null, or omitted
  entirely. When omitted, the request is treated as a notification. Using @code null @endcode is not
  equivalent to omitting the @code id @endcode, but it is discouraged. Instrumentations SHOULD NOT
  capture this attribute when the @code id @endcode is @code null @endcode or omitted.
 */
static constexpr const char *kJsonrpcRequestId = "jsonrpc.request.id";

}  // namespace jsonrpc
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
