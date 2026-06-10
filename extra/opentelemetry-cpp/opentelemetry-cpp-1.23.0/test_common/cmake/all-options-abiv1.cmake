# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Enable all options with ABI V1 except preview features

set(WITH_ABI_VERSION_1 ON CACHE BOOL "" FORCE)
set(WITH_ABI_VERSION_2 OFF CACHE BOOL "" FORCE)

set(ENABLE_PREVIEW OFF)
include(${CMAKE_CURRENT_LIST_DIR}/all-options.cmake)
