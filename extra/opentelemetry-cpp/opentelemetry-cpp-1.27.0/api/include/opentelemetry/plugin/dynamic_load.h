// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/plugin/factory.h"
#include "opentelemetry/version.h"

#ifdef _WIN32
#  include "opentelemetry/plugin/detail/dynamic_load_windows.h"  // IWYU pragma: export
#else
#  include "opentelemetry/plugin/detail/dynamic_load_unix.h"  // IWYU pragma: export
#endif

// Always print a warning

#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC warning "opentelemetry/plugin/ is deprecated, and will be removed. Do not use."
#elif defined(_MSC_VER)
#  pragma message( \
      "[WARNING]: opentelemetry/plugin/ is deprecated, and will be removed. Do not use.")
#endif

// And fail in deprecated-free builds

#ifdef OPENTELEMETRY_NO_DEPRECATED_CODE
#  error "header <opentelemetry/plugin/> is deprecated."
#endif

OPENTELEMETRY_BEGIN_NAMESPACE
namespace plugin
{

/**
 * Load an OpenTelemetry implementation as a plugin.
 * @param plugin the path to the plugin to load
 * @param error_message on failure this is set to an error message
 * @return a Factory that can be used to create OpenTelemetry objects or nullptr on failure.
 */
std::unique_ptr<Factory> LoadFactory(const char *plugin, std::string &error_message) noexcept;
}  // namespace plugin
OPENTELEMETRY_END_NAMESPACE
