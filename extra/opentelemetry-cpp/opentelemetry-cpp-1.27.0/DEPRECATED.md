# Deprecated

This document lists all the items currently deprecated in opentelemetry-cpp.

Deprecated items will be removed in the future.

## Guidelines

### Maintainer guidelines

See the [deprecation-process](./docs/deprecation-process.md)

## [TEMPLATE]

### New Deprecation Title (Template)

#### Announcement (Template)

#### Motivation (Template)

#### Scope (Template)

#### Mitigation (Template)

#### Planned removal (Template)

## [Platforms]

N/A

## [Compilers]

N/A

## [Third party dependencies]

N/A

## [Build tools]

N/A

## [Build scripts]

### CMake WITH_OTLP_RETRY_PREVIEW

#### Announcement (WITH_OTLP_RETRY_PREVIEW)

The CMake compile flag WITH_OTLP_RETRY_PREVIEW is deprecated by:

* Enable WITH_OTLP_RETRY_PREVIEW by default
  [#3953](https://github.com/open-telemetry/opentelemetry-cpp/pull/3953)

#### Motivation (WITH_OTLP_RETRY_PREVIEW)

Flags like `WITH_OTLP_RETRY_PREVIEW` are used to conditionally compile
new features, when they are introduced in the code base.

Now that this feature is stable, conditional compilation is no longer
necessary.

#### Scope (WITH_OTLP_RETRY_PREVIEW)

Remove compilation flag WITH_OTLP_RETRY_PREVIEW in CMake.

Remove ifdef ENABLE_OTLP_RETRY_PREVIEW in C++ .

#### Mitigation (WITH_OTLP_RETRY_PREVIEW)

Applications built with `WITH_OTLP_RETRY_PREVIEW=ON` need to:

* remove the WITH_OTLP_RETRY_PREVIEW flag from CMake scripts

Applications built with `WITH_OTLP_RETRY_PREVIEW=OFF` need to:

* remove the WITH_OTLP_RETRY_PREVIEW flag from CMake scripts
* use `retry_policy_max_attempts` = 0 in the OTLP exporter options.

For Bazel, no compilation option exists,
the bazel build enables unconditionally ENABLE_OTLP_RETRY_PREVIEW.

Make sure to properly initialize `retry_policy_max_attempts`
to enable or disable the retry feature.

#### Planned removal (WITH_OTLP_RETRY_PREVIEW)

This compilation flag will be removed after October 1st, 2026.

### CMake WITH_OTLP_GRPC_SSL_MTLS_PREVIEW

#### Announcement (WITH_OTLP_GRPC_SSL_MTLS_PREVIEW)

The CMake compile flag WITH_OTLP_GRPC_SSL_MTLS_PREVIEW is deprecated by:

* Enable WITH_OTLP_GRPC_SSL_MTLS_PREVIEW by default
  [#3970](https://github.com/open-telemetry/opentelemetry-cpp/pull/3970)

#### Motivation (WITH_OTLP_GRPC_SSL_MTLS_PREVIEW)

Flags like `WITH_OTLP_GRPC_SSL_MTLS_PREVIEW` are used to conditionally compile
new features, when they are introduced in the code base.

Now that this feature is stable, conditional compilation is no longer
necessary.

#### Scope (WITH_OTLP_GRPC_SSL_MTLS_PREVIEW)

Remove compilation flag WITH_OTLP_GRPC_SSL_MTLS_PREVIEW in CMake.

Remove ifdef ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW in C++ .

#### Mitigation (WITH_OTLP_GRPC_SSL_MTLS_PREVIEW)

Applications built with `WITH_OTLP_GRPC_SSL_MTLS_PREVIEW=ON` need to:

* remove the WITH_OTLP_GRPC_SSL_MTLS_PREVIEW flag from CMake scripts

Applications built with `WITH_OTLP_GRPC_SSL_MTLS_PREVIEW=OFF` need to:

* remove the WITH_OTLP_GRPC_SSL_MTLS_PREVIEW flag from CMake scripts
* initialize ssl client properties in the OTLP GRPC exporter options,
  either with real KEY and CERT data, or with empty strings.

See:

* struct `OtlpGrpcClientOptions`
* struct `OtlpGrpcLogRecordExporterOptions`
* struct `OtlpGrpcMetricExporterOptions`
* struct `OtlpGrpcExporterOptions`

For Bazel, no compilation flag exists, the code is always built using
ENABLE_OTLP_GRPC_SSL_MTLS_PREVIEW.

Make sure to properly initialize ssl client properties
to enable or disable the feature.

#### Planned removal (WITH_OTLP_GRPC_SSL_MTLS_PREVIEW)

This compilation flag will be removed after October 1st, 2026.

## [opentelemetry-cpp API]

### Deprecation of EventLogger

#### Announcement (EventLogger)

The `EventLogger` API was deprecated by:

* [API] Deprecate event logger
  [#3285](https://github.com/open-telemetry/opentelemetry-cpp/pull/3285)

on Feb 25, 2025, as published in release 1.20.

#### Motivation (EventLogger)

The specification for `EventLogger` was deprecated by:

* Deprecate Event API and SDK in favor of Emit Event in the Log API
  [#4319](https://github.com/open-telemetry/opentelemetry-specification/pull/4319)

The opentelemetry-cpp API and SDK deprecates the EventLogger to be aligned
with the opentelemetry specification.

#### Scope (EventLogger)

The following classes or methods are deprecated in ABI version 1:

* API
  * class opentelemetry::logs::EventLogger
  * class opentelemetry::logs::EventLoggerProvider
  * class opentelemetry::logs::NoopEventLogger
  * class opentelemetry::logs::NoopEventLoggerProvider
  * method opentelemetry::logs::Provider::GetEventLoggerProvider()
  * method opentelemetry::logs::Provider::SetEventLoggerProvider()
* SDK
  * class opentelemetry::sdk::logs::EventLogger
  * class opentelemetry::sdk::logs::EventLoggerProvider
  * class opentelemetry::sdk::logs::EventLoggerProviderFactory

The same are removed from ABI version 2.

#### Mitigation (EventLogger)

In respective order, the following should be used in place of `EventLogger`:

* API
  * class opentelemetry::logs::Logger
  * class opentelemetry::logs::LoggerProvider
  * class opentelemetry::logs::NoopLogger
  * class opentelemetry::logs::NoopLoggerProvider
  * method opentelemetry::logs::Provider::GetLoggerProvider()
  * method opentelemetry::logs::Provider::SetLoggerProvider()
* SDK
  * class opentelemetry::sdk::logs::Logger
  * class opentelemetry::sdk::logs::LoggerProvider
  * class opentelemetry::sdk::logs::LoggerProviderFactory

Instrumented applications, that currently use
`opentelemetry::logs::Provider::GetEventLoggerProvider()`,
are strongly encouraged to migrate and use
`opentelemetry::logs::Provider::GetLoggerProvider()` instead.

Main applications, when installing and configuring an SDK, that currently use
`opentelemetry::logs::Provider::SetEventLoggerProvider()`,
are strongly encouraged to migrate and use
`opentelemetry::logs::Provider::SetLoggerProvider()` instead.

This migration can be implemented now,
as `Logger` is available already in ABI version 1.

This migration is mandatory and a pre requisite for adoption of ABI version 2 later.

#### Planned removal (EventLogger)

All the deprecated code is already removed from ABI version 2, since Feb
2025.

While EventLogger will not be removed from ABI version 1,
the ABI version 1 itself will be deprecated and decommissioned in the long
term, after ABI version 2 is declared stable.

By the time only ABI version 2 is available and supported,
any code still using the deprecated `EventLogger` will break.

### Deprecation of plugin

#### Announcement (plugin)

The `plugin` API was deprecated by:

* [API] Deprecate plugin
  [#4021](https://github.com/open-telemetry/opentelemetry-cpp/pull/4021)

on Apr 21, 2026.

#### Motivation (plugin)

All the code in namespace `opentelemetry::plugin` is a framework designed
to help loading code from shared libraries.

This framework was never used.

Keeping the code in the repository has a maintenance cost
(include-what-you-use, cpp-check, clang-tidy, CI),
and also increases confusion.

This unused code is to be removed.

#### Scope (plugin)

The following files are to be removed from the API:

* `opentelemetry/plugin/*.h`
* `opentelemetry/plugin/detail/*.h`

The following files are to be removed from examples:

* `examples/plugin/*`

#### Mitigation (plugin)

None.

This code is unused, there is no replacement.

#### Planned removal (plugin)

API header files under the `opentelemetry::plugin` namespace
will be removed after October 1st, 2026.

## [opentelemetry-cpp SDK]

N/A

## [opentelemetry-cpp Exporter]

N/A

## [Documentation]

N/A

## Semantic conventions

N/A
