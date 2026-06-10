# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# ZLIB must be found as an installed package for now.
# Fetching ZLIB and building in-tree is not supported.
# Protobuf, gRPC, prometheus-cpp, civetweb, CURL, and other dependencies require ZLIB and import its target.
# When ZLIB::ZLIB is an alias of the shared library then inconsistent linking may occur.

find_package(ZLIB REQUIRED)
set(ZLIB_PROVIDER "find_package")

# Set the ZLIB_VERSION from the legacy ZLIB_VERSION_STRING Required for CMake
# versions below 3.26
if(NOT ZLIB_VERSION AND ZLIB_VERSION_STRING)
  set(ZLIB_VERSION ${ZLIB_VERSION_STRING})
endif()

if(NOT TARGET ZLIB::ZLIB)
  message(FATAL_ERROR "The required zlib target (ZLIB::ZLIB) was not imported.")
endif()
