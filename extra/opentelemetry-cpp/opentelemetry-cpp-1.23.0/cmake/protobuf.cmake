# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0


# Import Protobuf targets (protobuf::libprotobuf and protobuf::protoc) and set PROTOBUF_PROTOC_EXECUTABLE.
# 1. If gRPC was fetched from github then use the Protobuf submodule built with gRPC
# 2. Find an installed Protobuf package
# 3. Use FetchContent to fetch and build Protobuf from GitHub

if(DEFINED gRPC_PROVIDER AND NOT gRPC_PROVIDER STREQUAL "find_package" AND TARGET libprotobuf)
  # gRPC was fetched and built Protobuf as a submodule

  set(_Protobuf_VERSION_REGEX "\"cpp\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+(\\.[0-9]+)?)\"")
  set(_Protobuf_VERSION_FILE "${grpc_SOURCE_DIR}/third_party/protobuf/version.json")

  file(READ "${_Protobuf_VERSION_FILE}" _Protobuf_VERSION_FILE_CONTENTS)
  if(_Protobuf_VERSION_FILE_CONTENTS MATCHES ${_Protobuf_VERSION_REGEX})
    set(Protobuf_VERSION "${CMAKE_MATCH_1}")
  else()
    message(WARNING "Failed to parse Protobuf version from ${_Protobuf_VERSION_FILE} using regex ${_Protobuf_VERSION_REGEX}")
  endif()
  set(Protobuf_PROVIDER "grpc_submodule")
else()

  # Search for an installed Protobuf package explicitly using the CONFIG search mode first followed by the MODULE search mode.
  # Protobuf versions < 3.22.0 may be found using the module mode and some protobuf apt packages do not support the CONFIG search.

  find_package(Protobuf CONFIG QUIET)
  set(Protobuf_PROVIDER "find_package")

  if(NOT Protobuf_FOUND)
    find_package(Protobuf MODULE QUIET)
  endif()

  if(NOT Protobuf_FOUND)
    FetchContent_Declare(
      "protobuf"
      GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
      GIT_TAG "${protobuf_GIT_TAG}"
    )

    set(protobuf_INSTALL ${OPENTELEMETRY_INSTALL} CACHE BOOL "" FORCE)
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(protobuf)

    set(Protobuf_PROVIDER "fetch_repository")

    # Set the Protobuf_VERSION variable from the git tag.
    string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" Protobuf_VERSION "${protobuf_GIT_TAG}")

    if(TARGET libprotobuf)
      set_target_properties(libprotobuf PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_CLANG_TIDY "" CXX_INCLUDE_WHAT_YOU_USE "")
    endif()

  endif()
endif()

if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "A required protobuf target (protobuf::libprotobuf) was not imported")
endif()

if(PROTOBUF_PROTOC_EXECUTABLE AND NOT Protobuf_PROTOC_EXECUTABLE)
  message(WARNING "Use of PROTOBUF_PROTOC_EXECUTABLE is deprecated. Please use Protobuf_PROTOC_EXECUTABLE instead.")
  set(Protobuf_PROTOC_EXECUTABLE "${PROTOBUF_PROTOC_EXECUTABLE}")
endif()

if(CMAKE_CROSSCOMPILING)
  find_program(Protobuf_PROTOC_EXECUTABLE protoc)
else()
  if(NOT TARGET protobuf::protoc)
    message(FATAL_ERROR "A required protobuf target (protobuf::protoc) was not imported")
  endif()
  set(Protobuf_PROTOC_EXECUTABLE "$<TARGET_FILE:protobuf::protoc>")
endif()

set(PROTOBUF_PROTOC_EXECUTABLE "${Protobuf_PROTOC_EXECUTABLE}")

message(STATUS "PROTOBUF_PROTOC_EXECUTABLE=${PROTOBUF_PROTOC_EXECUTABLE}")
