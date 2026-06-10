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

# See extra/opentelemetry-cpp/

SET(OPENTELEMETRY_CPP_TAG "opentelemetry-cpp-1.23.0" CACHE INTERNAL "")

MACRO(MYSQL_USE_BUNDLED_OPENTELEMETRY_CPP)

  SET(OPENTELEMETRY_CPP_BUNDLE_SRC_PATH
    "${CMAKE_SOURCE_DIR}/extra/opentelemetry-cpp/")
  STRING_APPEND(OPENTELEMETRY_CPP_BUNDLE_SRC_PATH
    "${OPENTELEMETRY_CPP_TAG}")

  SET(OPENTELEMETRY_CPP_INCLUDE_API_DIR
    "${OPENTELEMETRY_CPP_BUNDLE_SRC_PATH}/api/include")

  SET(OPENTELEMETRY_CPP_INCLUDE_SDK_DIR
    "${OPENTELEMETRY_CPP_BUNDLE_SRC_PATH}/sdk/include")

  SET(OPENTELEMETRY_CPP_INCLUDE_EXPORTERS_OTLP_DIR
    "${OPENTELEMETRY_CPP_BUNDLE_SRC_PATH}/exporters/otlp/include")

  SET(OPENTELEMETRY_CPP_INCLUDE_EXT_DIR
    "${OPENTELEMETRY_CPP_BUNDLE_SRC_PATH}/ext/include")

  SET(OPENTELEMETRY_CPP_INCLUDE_DIRS
    ${OPENTELEMETRY_CPP_INCLUDE_API_DIR}
    ${OPENTELEMETRY_CPP_INCLUDE_SDK_DIR}
    ${OPENTELEMETRY_CPP_INCLUDE_EXPORTERS_OTLP_DIR}
    ${OPENTELEMETRY_CPP_INCLUDE_EXT_DIR} CACHE INTERNAL "")

  # The opentelemetry-cpp code is not warning free.
  # Set the minimum flags needed to build cleanly user code (in MySQL).
  # We do not want too many flags, to avoid hiding errors in the MySQL code.
  #
  # Do not confuse with related OTELCPP_CXX_FLAGS,
  # which is to build opentelemetry-cpp itself,
  # and may contain more flags.

  SET(OTELCPP_USER_COMPILE_OPTIONS "")

  # For opentelemetry-cpp warnings (WITH_STL=OFF)
  # We include opentelemetry as SYSTEM headers, which means
  # warnings will not be propagated to user code.
  # So there should be no need to add -Wno-error here.
  if(WIN32)
#    STRING(APPEND OTELCPP_USER_COMPILE_OPTIONS " /WX-")
  else()
#    STRING(APPEND OTELCPP_USER_COMPILE_OPTIONS " -Wno-error")
  endif()

  # Feature flags

  # When opentelemetry-cpp is built with WITH_STL,
  # user code needs to be compiled with #define HAVE_CPP_STDLIB.

  # Fails on many platform.
  # STRING(APPEND OTELCPP_USER_COMPILE_OPTIONS " -DHAVE_CPP_STDLIB")

  SET(OTELCPP_USER_COMPILE_OPTIONS
    "${OTELCPP_USER_COMPILE_OPTIONS}" CACHE INTERNAL "")

  # We use the bundled version, so:
  SET(OPENTELEMETRY_CPP_FOUND TRUE)
ENDMACRO()

MACRO(MYSQL_CHECK_OPENTELEMETRY_CPP)
  MYSQL_USE_BUNDLED_OPENTELEMETRY_CPP()

  MESSAGE(STATUS "OPENTELEMETRY_CPP_TAG is ${OPENTELEMETRY_CPP_TAG}")
  MESSAGE(STATUS
    "OPENTELEMETRY_CPP_INCLUDE_DIRS ${OPENTELEMETRY_CPP_INCLUDE_DIRS}")

ENDMACRO()
