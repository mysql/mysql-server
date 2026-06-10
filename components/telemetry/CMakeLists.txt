# Copyright (c) 2022, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Allows to opt out, to speed up local builds in development.
OPTION(WITH_MYSQL_SERVER_TELEMETRY "Build server telemetry component" ON)

IF(NOT WITH_MYSQL_SERVER_TELEMETRY)
  MESSAGE(STATUS "Skipping server telemetry")
  RETURN()
ENDIF()

IF(TARGET opentelemetry_targets_seen)
  MESSAGE(STATUS "server telemetry opentelemetry_targets_seen")
ELSE()
  INCLUDE(${CMAKE_SOURCE_DIR}/cmake/mysql_telemetry.cmake)
ENDIF()

IF(NOT OPENTELEMETRY_CPP_AVAILABLE)
  MESSAGE(STATUS "Skipping server telemetry, not available on this platform")
  RETURN()
ENDIF()

INCLUDE_DIRECTORIES(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(BEFORE SYSTEM ${OPENTELEMETRY_CPP_INCLUDE_DIRS})

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="telemetry")

MESSAGE(STATUS
  "Compiling server telemetry with ${OTELCPP_USER_COMPILE_OPTIONS}")

# Silence warnings caused by opentelemetry-cpp
STRING(APPEND CMAKE_CXX_FLAGS "${OTELCPP_USER_COMPILE_OPTIONS}")
DISABLE_MISSING_PROFILE_WARNING()

MESSAGE(STATUS "server telemetry CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
MESSAGE(STATUS
  "server telemetry INCLUDE_DIRECTORIES ${OPENTELEMETRY_CPP_INCLUDE_DIRS}")

SET(TELEMETRY_SOURCES
  tm_component.cc
  tm_control.cc
  tm_global.cc
  tm_log.cc
  tm_notification.cc
  tm_option_usage.cc
  tm_otel_trace.cc
  tm_otel_metric.cc
  tm_otel_log.cc
  tm_mysql_metric.cc
  tm_ns.cc
  tm_propagation.cc
  tm_psi.cc
  tm_resource.cc
  tm_secret.cc
  tm_setup_otel.cc
  tm_slot.cc
  tm_status_variables.cc
  tm_system_variables.cc
  tm_thread_instrumentation.cc
  option_usage.cc
)

SET(TELEMETRY_LIBRARIES
  opentelemetry_version
  opentelemetry_trace
  opentelemetry_metrics
  opentelemetry_logs
  opentelemetry_common
  opentelemetry_resources
  opentelemetry_exporter_otlp_http
  opentelemetry_exporter_otlp_http_metric
  opentelemetry_exporter_otlp_http_log
  opentelemetry_otlp_recordable
  opentelemetry_exporter_otlp_http_client
  opentelemetry_http_client_curl
)

MYSQL_ADD_COMPONENT(telemetry
  ${TELEMETRY_SOURCES}
  LINK_LIBRARIES
  ${TELEMETRY_LIBRARIES}
  ${LINK_FLAG_NO_UNDEFINED}
  OpenSSL::SSL OpenSSL::Crypto library_mysys
  ${LIBRT}
  MODULE_ONLY
)

IF(WITH_PROTOBUF STREQUAL "bundled")
  ADD_DEPENDENCIES(component_telemetry libprotobuf)
ENDIF()

IF(UNIX_INSTALL_RPATH_ORIGIN_PRIV_LIBDIR)
  ADD_INSTALL_RPATH_FOR_PROTOBUF(component_telemetry)
ENDIF()

IF(MY_COMPILER_IS_GNU)
  TARGET_LINK_OPTIONS(component_telemetry PRIVATE -Wno-alloc-size-larger-than)
ENDIF()
