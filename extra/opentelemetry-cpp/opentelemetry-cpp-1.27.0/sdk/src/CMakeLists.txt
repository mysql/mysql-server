# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(common)
add_subdirectory(trace)
add_subdirectory(metrics)
add_subdirectory(logs)
add_subdirectory(version)
add_subdirectory(resource)

if(WITH_CONFIGURATION)
  add_subdirectory(configuration)
endif()
