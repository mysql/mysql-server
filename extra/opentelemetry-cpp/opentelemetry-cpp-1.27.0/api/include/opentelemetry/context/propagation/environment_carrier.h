// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace context
{
namespace propagation
{

// EnvironmentCarrier is a TextMapCarrier that reads from and writes to environment variables.
//
// This carrier enables context propagation across process boundaries using environment variables
// as specified in the OpenTelemetry specification:
// https://opentelemetry.io/docs/specs/otel/context/env-carriers/
//
// The carrier supports two usage scenarios:
//
// 1. Extract (default constructor): Reads context from environment variables.
//    Get() reads from TRACEPARENT, TRACESTATE, BAGGAGE environment variables.
//    Set() is a no-op. Values are cached on first access for lifetime management.
//
// 2. Inject (shared_ptr constructor): Writes context to a provided map.
//    Set() writes to the provided std::map. Keys are automatically converted
//    from lowercase header names to uppercase environment variable names.

class EnvironmentCarrier : public TextMapCarrier
{
public:
  // Constructs an EnvironmentCarrier for Extract operations.
  EnvironmentCarrier() noexcept = default;

  // Constructs an EnvironmentCarrier for Inject operations.
  explicit EnvironmentCarrier(std::shared_ptr<std::map<std::string, std::string>> env_map) noexcept
      : env_map_ptr_(std::move(env_map))
  {}

  // Returns the value associated with the passed key.
  // Always reads from process environment variables (with caching).
  // The key is automatically converted to uppercase.
  nostd::string_view Get(nostd::string_view key) const noexcept override
  {
    std::string env_name = ToEnvName(key);

    // Check cache first
    auto cache_it = cache_.find(std::string(key));
    if (cache_it != cache_.end())
    {
      return cache_it->second;
    }

    // Read from environment
    const char *value = std::getenv(env_name.c_str());
    if (value != nullptr)
    {
      // Cache for lifetime management (string_view requires stable storage)
      cache_[std::string(key)] = std::string(value);
      return cache_[std::string(key)];
    }
    return "";
  }

  // Stores the key-value pair in the map if one was provided at construction.
  // Otherwise, this operation is a no-op.
  // The key is automatically converted to uppercase.
  void Set(nostd::string_view key, nostd::string_view value) noexcept override
  {
    if (!env_map_ptr_)
    {
      return;
    }

    std::string env_name               = ToEnvName(key);
    env_map_ptr_->operator[](env_name) = std::string(value);
  }

private:
  std::shared_ptr<std::map<std::string, std::string>> env_map_ptr_;
  mutable std::map<std::string, std::string> cache_;

  // Converts a header name to an environment variable name.
  // e.g., "traceparent" -> "TRACEPARENT", "my-key" -> "MY_KEY",
  //        "my.complex.key" -> "MY_COMPLEX_KEY"
  static std::string ToEnvName(nostd::string_view key)
  {
    std::string env_name(key);
    std::transform(env_name.begin(), env_name.end(), env_name.begin(), [](unsigned char c) {
      return static_cast<char>(std::isalnum(c) ? std::toupper(c) : '_');
    });
    return env_name;
  }
};

}  // namespace propagation
}  // namespace context
OPENTELEMETRY_END_NAMESPACE
