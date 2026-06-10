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

include(GNUInstallDirs)

# Build as a string, not a list,
# because CMAKE_CXX_FLAGS can not be set with a list.

SET(OTELCPP_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING(APPEND OTELCPP_CXX_FLAGS " -Wno-error")
ELSEIF(WIN32)
  REMOVE_CMAKE_COMPILER_FLAGS(OTELCPP_CXX_FLAGS "/WX")
  STRING(APPEND OTELCPP_CXX_FLAGS " /WX-")
ENDIF()

MESSAGE(STATUS
  "Compiling opentelemetry-cpp with CMAKE_CXX_FLAGS ${OTELCPP_CXX_FLAGS}")

SET(OTELCPP_PROTO_PATH "${OPENTELEMETRY_PROTO_BUNDLE_SRC_PATH}")

MESSAGE(STATUS
  "Compiling opentelemetry-cpp with OTELCPP_PROTO_PATH ${OTELCPP_PROTO_PATH}")

SET(OTELCPP_NLOHMANN_JSON "${NLOHMANN_JSON_BUNDLE_SRC_PATH}")

MESSAGE(STATUS
  "Compiling opentelemetry-cpp with OTELCPP_NLOHMANN_JSON ${OTELCPP_NLOHMANN_JSON}")

SET(Protobuf_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR} CACHE INTERNAL "")

# Verify that we have the ALIAS or IMPORTED target ext::libprotobuf
# available, for bundled and system configurations.
IF(NOT TARGET ext::libprotobuf)
  MESSAGE(FATAL_ERROR "Missing TARGET ext::libprotobuf")
ENDIF()

SET(CMAKE_CXX_FLAGS ${OTELCPP_CXX_FLAGS})
DISABLE_MISSING_PROFILE_WARNING()

MSVC_CPPCHECK_DISABLE()
# TODO: remove when all -Wdocumentation warnings are fixed.
DISABLE_DOCUMENTATION_WARNINGS()

# parse_min_ssl_version() may be unused
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-function")
ENDIF()

IF(WIN32_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-undef")
ENDIF()

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING(REPLACE "-Wzero-as-null-pointer-constant" ""
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ENDIF()

# Clang 16: builtin __has_trivial_destructor is deprecated;
#           use __is_trivially_destructible instead
MY_CHECK_CXX_COMPILER_WARNING("-Wdeprecated-builtins" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# Clang 12: definition of implicit copy constructor for 'shared_ptr_wrapper'
# is deprecated because it has a user-declared destructor
# [-Wdeprecated-copy-dtor]
MY_CHECK_CXX_COMPILER_WARNING("-Wdeprecated-copy-dtor" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING(
  "-Wdeprecated-copy-with-user-provided-dtor" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wextra-semi" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wno-unused-parameter" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# Clang 19: error: a template argument list is expected after a name
# prefixed by the template keyword
MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-template-arg-list-after-template-kw"
  HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# Fails on many platforms in PB2:
# SET(WITH_STL ON CACHE INTERNAL "")

# Build config
SET(WITH_NO_GETENV ON CACHE INTERNAL "")
SET(WITH_STL OFF CACHE INTERNAL "")
SET(WITH_GSL OFF CACHE INTERNAL "")
SET(OPENTELEMETRY_INSTALL OFF CACHE INTERNAL "")
SET(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Feature used
SET(WITH_OTLP_HTTP ON CACHE INTERNAL "")
# Not in 1.15.0: SET(WITH_OTLP_HTTP_SSL_PREVIEW ON CACHE INTERNAL "")
# Not in 1.15.0: SET(WITH_OTLP_HTTP_SSL_TLS_PREVIEW ON CACHE INTERNAL "")
# Not in 1.15.0: SET(WITH_REMOVE_METER_PREVIEW ON CACHE INTERNAL "")
SET(WITH_ABI_VERSION_1 OFF CACHE INTERNAL "")
SET(WITH_ABI_VERSION_2 ON CACHE INTERNAL "")
SET(WITH_OTLP_HTTP_COMPRESSION ON CACHE INTERNAL "")
SET(WITH_CURL_LOGGING ON CACHE INTERNAL "")

# Features not used
SET(WITH_ZIPKIN OFF CACHE INTERNAL "")
SET(WITH_PROMETHEUS OFF CACHE INTERNAL "")
SET(WITH_ELASTICSEARCH OFF CACHE INTERNAL "")
SET(BUILD_W3CTRACECONTEXT_TEST OFF CACHE INTERNAL "")
SET(WITH_EXAMPLES OFF CACHE INTERNAL "")
SET(WITH_FUNC_TESTS OFF CACHE INTERNAL "")
# Not in 1.15.0: SET(WITH_ZPAGES OFF CACHE INTERNAL "")
SET(WITH_ETW OFF CACHE INTERNAL "")
SET(WITH_OTLP_GRPC OFF CACHE INTERNAL "")

# Experimental features
SET(WITH_ASYNC_EXPORT_PREVIEW OFF CACHE INTERNAL "")
SET(WITH_METRICS_EXEMPLAR_PREVIEW OFF CACHE INTERNAL "")
SET(WITH_THREAD_INSTRUMENTATION_PREVIEW ON CACHE INTERNAL "")

# Dummy macros, to minimize the size of our diff against original code.
# We do not want to install any of the libraries generated as part of
# opentelemetry code.
# We do not want to use find_package for code provided as a bundle.
# We do not want to use cmake_minimum_required in sub directories

# This assume we change
# - install -> do_not_install
# - export -> do_not_export
# - find_package -> do_not_find_package
# - cmake_minimum_required -> do_not_cmake_minimum_required
# - message -> do_not_message
# - include -> do_not_include
# in opentelemetry-cpp CMakeLists.txt

MACRO(do_not_install)
ENDMACRO()

MACRO(do_not_export)
ENDMACRO()

MACRO(do_not_find_package)
ENDMACRO()

MACRO(do_not_cmake_minimum_required)
ENDMACRO()

MACRO(do_not_message)
ENDMACRO()

MACRO(do_not_include)
ENDMACRO()

ADD_SUBDIRECTORY(${OPENTELEMETRY_CPP_TAG})

ADD_DEPENDENCIES(protobuf_generated_all opentelemetry_proto)
