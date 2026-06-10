# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(
  opentelemetry_resource_detectors
  container_detector_utils.cc container_detector.cc process_detector.cc
  process_detector_utils.cc)

set_target_properties(opentelemetry_resource_detectors
                      PROPERTIES EXPORT_NAME resource_detectors)
set_target_version(opentelemetry_resource_detectors)

target_link_libraries(opentelemetry_resource_detectors
                      PUBLIC opentelemetry_resources)
target_include_directories(
  opentelemetry_resource_detectors
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

otel_add_component(
  COMPONENT
  resource_detectors
  TARGETS
  opentelemetry_resource_detectors
  FILES_DIRECTORY
  "include/opentelemetry/"
  FILES_DESTINATION
  "include/opentelemetry"
  FILES_MATCHING
  PATTERN
  "*.h"
  PATTERN
  "resource_detectors/detail/*"
  EXCLUDE)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
