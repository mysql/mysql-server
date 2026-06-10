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

## =======================================================================
##
## OVERVIEW OF DEPENDENCIES
##
## MySQL telemetry_client plugin --> opentelemetry-cpp
## MySQL telemetry component --> opentelemetry-cpp
##
## opentelemetry-cpp --> opentelemetry-proto
## opentelemetry-cpp --> protoc (compiler, used at build time)
##
## For the OTLP HTTP exporter:
## opentelemetry-cpp --> nlohmann_json
## opentelemetry-cpp --> protobuf
## opentelemetry-cpp --> curl
##
## =======================================================================

# Mutual exclusion from telemetry_client and telemetry
ADD_CUSTOM_TARGET(opentelemetry_targets_seen)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/nlohmann_json.cmake)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/opentelemetry-proto.cmake)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/opentelemetry-cpp.cmake)

# INCLUDE(${CMAKE_SOURCE_DIR}/cmake/curl.cmake)

MYSQL_CHECK_NLOHMANN_JSON()
MYSQL_CHECK_OPENTELEMETRY_PROTO()
MYSQL_CHECK_OPENTELEMETRY_CPP()

#
# OPENTELEMETRY_CPP_AVAILABLE is an output parameter,
# used in the caller scripts:
# - see the telemetry_client plugin
# - see the telemetry component
#
SET(OPENTELEMETRY_CPP_AVAILABLE FALSE CACHE INTERNAL "")

# Assuming this is already done in the top level CMakeList.txt
# MYSQL_CHECK_PROTOBUF()
#
# opentelemetry-proto uses option --experimental_allow_proto3_optional,
# introduced in PROTOBUF 3.12.0
# Lowest tested version is 3.12.4 which comes with Ubuntu 22.04.2 LTS

IF (PROTOBUF_VERSION VERSION_LESS "3.12.4")
  MESSAGE(WARNING "No telemetry for this platform, PROTOBUF >= 3.12.4 required")
  RETURN()
ENDIF()

# Assuming this is already done in the top level CMakeList.txt
# MYSQL_CHECK_CURL()

# 7.28.0 is required for curl_multi_wait in opentelemetry-cpp
# 7.54.0 will be required for SSL TLS

IF (CURL_VERSION VERSION_LESS "7.28.0")
  MESSAGE(WARNING "No telemetry for this platform, CURL >= 7.28.0 required")
  RETURN()
ENDIF()

SET(OPENTELEMETRY_CPP_AVAILABLE TRUE CACHE INTERNAL "")

MESSAGE(STATUS "Adding telemetry dependencies")

# This will add_library(nlohmann_json INTERFACE)
#       and add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
ADD_SUBDIRECTORY(
  ${CMAKE_SOURCE_DIR}/extra/json
  ${CMAKE_BINARY_DIR}/extra/json)

ADD_SUBDIRECTORY(
  ${CMAKE_SOURCE_DIR}/extra/opentelemetry-proto
  ${CMAKE_BINARY_DIR}/extra/opentelemetry-proto)

ADD_SUBDIRECTORY(
  ${CMAKE_SOURCE_DIR}/extra/opentelemetry-cpp
  ${CMAKE_BINARY_DIR}/extra/opentelemetry-cpp)

