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
namespace oracle
{

/**
  The database domain associated with the connection.
  <p>
  This attribute SHOULD be set to the value of the @code DB_DOMAIN @endcode initialization
  parameter, as exposed in @code v$parameter @endcode. @code DB_DOMAIN @endcode defines the domain
  portion of the global database name and SHOULD be configured when a database is, or may become,
  part of a distributed environment. Its value consists of one or more valid identifiers
  (alphanumeric ASCII characters) separated by periods.
 */
static constexpr const char *kOracleDbDomain = "oracle.db.domain";

/**
  The instance name associated with the connection in an Oracle Real Application Clusters
  environment. <p> There can be multiple instances associated with a single database service. It
  indicates the unique instance name to which the connection is currently bound. For non-RAC
  databases, this value defaults to the @code oracle.db.name @endcode.
 */
static constexpr const char *kOracleDbInstanceName = "oracle.db.instance.name";

/**
  The database name associated with the connection.
  <p>
  This attribute SHOULD be set to the value of the parameter @code DB_NAME @endcode exposed in @code
  v$parameter @endcode.
 */
static constexpr const char *kOracleDbName = "oracle.db.name";

/**
  The pluggable database (PDB) name associated with the connection.
  <p>
  This attribute SHOULD reflect the PDB that the session is currently connected to.
  If instrumentation cannot reliably obtain the active PDB name for each operation
  without issuing an additional query (such as @code SELECT SYS_CONTEXT @endcode), it is
  RECOMMENDED to fall back to the PDB name specified at connection establishment.
 */
static constexpr const char *kOracleDbPdb = "oracle.db.pdb";

/**
  The service name currently associated with the database connection.
  <p>
  The effective service name for a connection can change during its lifetime,
  for example after executing sql, @code ALTER SESSION @endcode. If an instrumentation cannot
  reliably obtain the current service name for each operation without issuing an additional query
  (such as @code SELECT SYS_CONTEXT @endcode), it is RECOMMENDED to fall back to the service name
  originally provided at connection establishment.
 */
static constexpr const char *kOracleDbService = "oracle.db.service";

}  // namespace oracle
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
