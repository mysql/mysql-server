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
OPTION(WITH_MYSQL_CLIENT_TELEMETRY "Build telemetry_client plugin" ON)

IF(NOT WITH_MYSQL_CLIENT_TELEMETRY)
  MESSAGE(STATUS "Skipping telemetry_client")
  RETURN()
ENDIF()

IF(TARGET opentelemetry_targets_seen)
  MESSAGE(STATUS "telemetry_client opentelemetry_targets_seen")
ELSE()
  INCLUDE(${CMAKE_SOURCE_DIR}/cmake/mysql_telemetry.cmake)
ENDIF()

IF(NOT OPENTELEMETRY_CPP_AVAILABLE)
  MESSAGE(STATUS "Skipping telemetry_client, not available on this platform")
  RETURN()
ENDIF()

INCLUDE_DIRECTORIES(BEFORE SYSTEM ${OPENTELEMETRY_CPP_INCLUDE_DIRS})

MESSAGE(STATUS
  "Compiling telemetry_client with ${OTELCPP_USER_COMPILE_OPTIONS}")

# Silence warnings caused by opentelemetry-cpp
STRING(APPEND CMAKE_CXX_FLAGS "${OTELCPP_USER_COMPILE_OPTIONS}")
DISABLE_MISSING_PROFILE_WARNING()

MESSAGE(STATUS "telemetry_client CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")

SET(TELEMETRY_LIBRARIES
  opentelemetry_version
  opentelemetry_trace
  opentelemetry_common
  opentelemetry_resources
  opentelemetry_exporter_otlp_http
  opentelemetry_otlp_recordable
  opentelemetry_exporter_otlp_http_client
  opentelemetry_http_client_curl
)

MYSQL_ADD_PLUGIN(telemetry_client
  tm_client_options.cc
  tm_client_plugin.cc
  CLIENT_ONLY
  MODULE_ONLY
  LINK_LIBRARIES
  ${TELEMETRY_LIBRARIES}
  mysys
  OpenSSL::SSL OpenSSL::Crypto
  ${LIBRT}
)
DOWNGRADE_STRINGOP_WARNINGS(telemetry_client)

IF(WITH_PROTOBUF STREQUAL "bundled")
  ADD_DEPENDENCIES(telemetry_client libprotobuf)
ENDIF()

IF(UNIX_INSTALL_RPATH_ORIGIN_PRIV_LIBDIR)
  ADD_INSTALL_RPATH_FOR_PROTOBUF(telemetry_client)
ENDIF()

IF(MY_COMPILER_IS_GNU)
  TARGET_LINK_OPTIONS(telemetry_client PRIVATE -Wno-alloc-size-larger-than)
ENDIF()
