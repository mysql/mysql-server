# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

include(${PROJECT_SOURCE_DIR}/cmake/pkgconfig.cmake)

#
# opentelemetry_exporter_ostream_span
#

add_library(opentelemetry_exporter_ostream_span src/span_exporter.cc
                                                src/span_exporter_factory.cc)

set_target_properties(opentelemetry_exporter_ostream_span
                      PROPERTIES EXPORT_NAME ostream_span_exporter)
set_target_version(opentelemetry_exporter_ostream_span)

target_include_directories(
  opentelemetry_exporter_ostream_span
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")

target_link_libraries(opentelemetry_exporter_ostream_span
                      PUBLIC opentelemetry_trace)

list(APPEND OPENTELEMETRY_OSTREAM_TARGETS opentelemetry_exporter_ostream_span)

if(BUILD_TESTING)
  add_executable(ostream_span_test test/ostream_span_test.cc)
  target_link_libraries(ostream_span_test ${GTEST_BOTH_LIBRARIES}
                        opentelemetry_exporter_ostream_span)
  gtest_add_tests(
    TARGET ostream_span_test
    TEST_PREFIX exporter.
    TEST_LIST ostream_span_test)
endif() # BUILD_TESTING

#
# opentelemetry_exporter_ostream_span_builder
#

add_library(opentelemetry_exporter_ostream_span_builder
            src/console_span_builder.cc)

set_target_properties(opentelemetry_exporter_ostream_span_builder
                      PROPERTIES EXPORT_NAME ostream_span_exporter_builder)
set_target_version(opentelemetry_exporter_ostream_span_builder)

target_include_directories(
  opentelemetry_exporter_ostream_span_builder
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

target_link_libraries(opentelemetry_exporter_ostream_span_builder
                      PUBLIC opentelemetry_exporter_ostream_span)

list(APPEND OPENTELEMETRY_OSTREAM_BUILDER_TARGETS
     opentelemetry_exporter_ostream_span_builder)

#
# opentelemetry_exporter_ostream_metrics
#

add_library(opentelemetry_exporter_ostream_metrics
            src/metric_exporter.cc src/metric_exporter_factory.cc)
set_target_properties(opentelemetry_exporter_ostream_metrics
                      PROPERTIES EXPORT_NAME ostream_metrics_exporter)
set_target_version(opentelemetry_exporter_ostream_metrics)

target_include_directories(
  opentelemetry_exporter_ostream_metrics
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")
target_link_libraries(opentelemetry_exporter_ostream_metrics
                      PUBLIC opentelemetry_metrics)

list(APPEND OPENTELEMETRY_OSTREAM_TARGETS
     opentelemetry_exporter_ostream_metrics)

if(BUILD_TESTING)
  add_executable(ostream_metric_test test/ostream_metric_test.cc)
  target_link_libraries(
    ostream_metric_test ${GTEST_BOTH_LIBRARIES}
    opentelemetry_exporter_ostream_metrics opentelemetry_resources)
  gtest_add_tests(
    TARGET ostream_metric_test
    TEST_PREFIX exporter.
    TEST_LIST ostream_metric_test)
endif()

#
# opentelemetry_exporter_ostream_metrics_builder
#

add_library(opentelemetry_exporter_ostream_metrics_builder
            src/console_push_metric_builder.cc)
set_target_properties(opentelemetry_exporter_ostream_metrics_builder
                      PROPERTIES EXPORT_NAME ostream_metrics_exporter_builder)
set_target_version(opentelemetry_exporter_ostream_metrics_builder)

target_include_directories(
  opentelemetry_exporter_ostream_metrics_builder
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")
target_link_libraries(opentelemetry_exporter_ostream_metrics_builder
                      PUBLIC opentelemetry_exporter_ostream_metrics)

list(APPEND OPENTELEMETRY_OSTREAM_BUILDER_TARGETS
     opentelemetry_exporter_ostream_metrics_builder)

#
# opentelemetry_exporter_ostream_logs
#

add_library(opentelemetry_exporter_ostream_logs
            src/log_record_exporter.cc src/log_record_exporter_factory.cc)
set_target_properties(opentelemetry_exporter_ostream_logs
                      PROPERTIES EXPORT_NAME ostream_log_record_exporter)
set_target_version(opentelemetry_exporter_ostream_logs)

target_include_directories(
  opentelemetry_exporter_ostream_logs
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")
target_link_libraries(opentelemetry_exporter_ostream_logs
                      PUBLIC opentelemetry_logs)
list(APPEND OPENTELEMETRY_OSTREAM_TARGETS opentelemetry_exporter_ostream_logs)

if(BUILD_TESTING)
  add_executable(ostream_log_test test/ostream_log_test.cc)
  target_link_libraries(ostream_log_test ${GTEST_BOTH_LIBRARIES}
                        opentelemetry_exporter_ostream_logs)
  gtest_add_tests(
    TARGET ostream_log_test
    TEST_PREFIX exporter.
    TEST_LIST ostream_log_test)
endif()

#
# opentelemetry_exporter_ostream_logs_builder
#

add_library(opentelemetry_exporter_ostream_logs_builder
            src/console_log_record_builder.cc)
set_target_properties(
  opentelemetry_exporter_ostream_logs_builder
  PROPERTIES EXPORT_NAME ostream_log_record_exporter_builder)
set_target_version(opentelemetry_exporter_ostream_logs_builder)

target_include_directories(
  opentelemetry_exporter_ostream_logs_builder
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")
target_link_libraries(opentelemetry_exporter_ostream_logs_builder
                      PUBLIC opentelemetry_exporter_ostream_logs)
list(APPEND OPENTELEMETRY_OSTREAM_BUILDER_TARGETS
     opentelemetry_exporter_ostream_logs_builder)

otel_add_component(
  COMPONENT
  exporters_ostream
  TARGETS
  ${OPENTELEMETRY_OSTREAM_TARGETS}
  FILES_DIRECTORY
  "include/opentelemetry/exporters/ostream"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h"
  PATTERN
  "console_log_record_builder.h"
  EXCLUDE
  PATTERN
  "console_push_metric_builder.h"
  EXCLUDE
  PATTERN
  "console_span_builder.h"
  EXCLUDE)

otel_add_component(
  COMPONENT
  exporters_ostream_builder
  TARGETS
  ${OPENTELEMETRY_OSTREAM_BUILDER_TARGETS}
  FILES_DIRECTORY
  "include/opentelemetry/exporters/ostream"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "console_log_record_builder.h"
  PATTERN
  "console_push_metric_builder.h"
  PATTERN
  "console_span_builder.h")

if(OPENTELEMETRY_INSTALL)
  opentelemetry_add_pkgconfig(
    exporter_ostream_span "OpenTelemetry Exporter - OStream Span"
    "Exports trace spans to output stream." "opentelemetry_trace")

  opentelemetry_add_pkgconfig(
    exporter_ostream_span_builder
    "OpenTelemetry Exporter - OStream Span Builder"
    "Builder for OStream span exporter." "opentelemetry_exporter_ostream_span")

  opentelemetry_add_pkgconfig(
    exporter_ostream_metrics "OpenTelemetry Exporter - OStream Metrics"
    "Exports metrics to output stream." "opentelemetry_metrics")

  opentelemetry_add_pkgconfig(
    exporter_ostream_metrics_builder
    "OpenTelemetry Exporter - OStream Metrics Builder"
    "Builder for OStream metrics exporter."
    "opentelemetry_exporter_ostream_metrics")

  opentelemetry_add_pkgconfig(
    exporter_ostream_logs "OpenTelemetry Exporter - OStream Logs"
    "Exports log records to output stream." "opentelemetry_logs")

  opentelemetry_add_pkgconfig(
    exporter_ostream_logs_builder
    "OpenTelemetry Exporter - OStream Logs Builder"
    "Builder for OStream log record exporter."
    "opentelemetry_exporter_ostream_logs")
endif()
