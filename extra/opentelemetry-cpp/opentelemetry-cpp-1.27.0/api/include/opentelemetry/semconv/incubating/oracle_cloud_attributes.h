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
namespace oracle_cloud
{

/**
  The OCI realm identifier that indicates the isolated partition in which the tenancy and its
  resources reside. <p> See <a
  href="https://docs.oracle.com/iaas/Content/General/Concepts/regions.htm">OCI documentation on
  realms</a>
 */
static constexpr const char *kOracleCloudRealm = "oracle_cloud.realm";

}  // namespace oracle_cloud
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
