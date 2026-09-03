// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace plugin
{
/**
 * Manage the ownership of a dynamically loaded library.
 */
class DynamicLibraryHandle
{
public:
  DynamicLibraryHandle()                                        = default;
  DynamicLibraryHandle(const DynamicLibraryHandle &)            = delete;
  DynamicLibraryHandle(DynamicLibraryHandle &&)                 = delete;
  DynamicLibraryHandle &operator=(const DynamicLibraryHandle &) = delete;
  DynamicLibraryHandle &operator=(DynamicLibraryHandle &&)      = delete;
  virtual ~DynamicLibraryHandle()                               = default;
};
}  // namespace plugin
OPENTELEMETRY_END_NAMESPACE
