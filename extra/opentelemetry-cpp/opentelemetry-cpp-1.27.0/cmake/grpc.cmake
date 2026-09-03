# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import gRPC targets (gRPC::grpc++ and gRPC::grpc_cpp_plugin).
# 1. Find an installed gRPC package
# 2. Use FetchContent to fetch and build gRPC (and its submodules) from GitHub

# Including the CMakeFindDependencyMacro resolves an error from
# gRPCConfig.cmake on some grpc versions. See
# https://github.com/grpc/grpc/pull/33361 for more details.
include(CMakeFindDependencyMacro)

find_package(gRPC CONFIG QUIET)
set(gRPC_PROVIDER "find_package")

if(NOT gRPC_FOUND)
  FetchContent_Declare(
      "grpc"
      GIT_REPOSITORY  "https://github.com/grpc/grpc.git"
      GIT_TAG "${grpc_GIT_TAG}"
      GIT_SUBMODULES
          "third_party/re2"
          "third_party/abseil-cpp"
          "third_party/protobuf"
          "third_party/cares/cares"
          "third_party/boringssl-with-bazel"
      )
  set(gRPC_PROVIDER "fetch_repository")

  set(gRPC_INSTALL ${OPENTELEMETRY_INSTALL} CACHE BOOL "" FORCE)
  set(protobuf_INSTALL ${OPENTELEMETRY_INSTALL} CACHE BOOL "" FORCE)
  set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_BUILD_GRPCPP_OTEL_PLUGIN OFF CACHE BOOL "" FORCE)
  set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)
  set(gRPC_RE2_PROVIDER "module"  CACHE STRING "" FORCE)
  set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
  set(gRPC_PROTOBUF_PACKAGE_TYPE "CONFIG" CACHE STRING "" FORCE)
  set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
  set(gRPC_CARES_PROVIDER "module" CACHE STRING "" FORCE)

  FetchContent_MakeAvailable(grpc)

  # Set the gRPC_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" gRPC_VERSION "${grpc_GIT_TAG}")

  #Disable iwyu and clang-tidy
  foreach(_grpc_target grpc++ grpc_cpp_plugin)
    if(TARGET ${_grpc_target})
      set_target_properties(${_grpc_target} PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_INCLUDE_WHAT_YOU_USE ""
                                                      CXX_CLANG_TIDY "")
    endif()
  endforeach()

  if(TARGET grpc++ AND NOT TARGET gRPC::grpc++)
    add_library(gRPC::grpc++ ALIAS grpc++)
  endif()

  if(TARGET grpc_cpp_plugin AND NOT TARGET gRPC::grpc_cpp_plugin)
    add_executable(gRPC::grpc_cpp_plugin ALIAS grpc_cpp_plugin)
  endif()

endif()

if(NOT TARGET gRPC::grpc++)
  message(FATAL_ERROR "A required gRPC target (gRPC::grpc++) was not imported")
endif()

if(CMAKE_CROSSCOMPILING)
  find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  if(NOT TARGET gRPC::grpc_cpp_plugin)
    message(FATAL_ERROR "A required gRPC target (gRPC::grpc_cpp_plugin) was not imported")
  endif()
  set(gRPC_CPP_PLUGIN_EXECUTABLE "$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
endif()

message(STATUS "gRPC_CPP_PLUGIN_EXECUTABLE=${gRPC_CPP_PLUGIN_EXECUTABLE}")
