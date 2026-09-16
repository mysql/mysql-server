# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import the benchmark target (benchmark::benchmark).
# 1. Find an installed benchmark package
# 2. Use FetchContent to build benchmark from a git submodule
# 3. Use FetchContent to fetch and build benchmark from GitHub

find_package(benchmark CONFIG QUIET)
set(benchmark_PROVIDER "find_package")

if(NOT benchmark_FOUND)
  set(_BENCHMARK_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/benchmark")
  if(EXISTS "${_BENCHMARK_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
      "benchmark"
      SOURCE_DIR "${_BENCHMARK_SUBMODULE_DIR}"
      )
    set(benchmark_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "benchmark"
      GIT_REPOSITORY "https://github.com/google/benchmark.git"
      GIT_TAG "${benchmark_GIT_TAG}"
      )
    set(benchmark_PROVIDER "fetch_repository")
  endif()

  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(benchmark)

  # Set the benchmark_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" benchmark_VERSION "${benchmark_GIT_TAG}")
endif()

if(NOT TARGET benchmark::benchmark)
  message(FATAL_ERROR "The required benchmark::benchmark target was not imported")
endif()
