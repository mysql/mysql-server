# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Enable all options with ABI V2 including preview features

set(WITH_ABI_VERSION_1 OFF CACHE BOOL "" FORCE)
set(WITH_ABI_VERSION_2 ON CACHE BOOL "" FORCE)

set(ENABLE_PREVIEW ON)
include(${CMAKE_CURRENT_LIST_DIR}/all-options.cmake)
