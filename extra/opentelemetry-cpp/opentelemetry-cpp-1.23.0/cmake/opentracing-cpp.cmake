# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

find_package(OpenTracing CONFIG QUIET)
set(OpenTracing_PROVIDER "find_package")

if(NOT OpenTracing_FOUND)
  set(_OPENTRACING_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/opentracing-cpp")
  if(EXISTS "${_OPENTRACING_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
       "opentracing"
       SOURCE_DIR "${_OPENTRACING_SUBMODULE_DIR}"
       )
    set(OpenTracing_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "opentracing"
      GIT_REPOSITORY  "https://github.com/opentracing/opentracing-cpp.git"
      GIT_TAG "${opentracing-cpp_GIT_TAG}"
      )
    set(OpenTracing_PROVIDER "fetch_repository")
  endif()

  # OpenTracing uses the BUILD_TESTING variable directly and we must force the cached value to OFF.
  # save the current state of BUILD_TESTING
  if(DEFINED BUILD_TESTING)
    set(_SAVED_BUILD_TESTING ${BUILD_TESTING})
  endif()

  if(DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(_SAVED_CMAKE_POLICY_VERSION_MINIMUM ${CMAKE_POLICY_VERSION_MINIMUM})
  endif()

  # Set the cache variables for the opentracing build
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_MOCKTRACER OFF CACHE BOOL "" FORCE)
  set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "" FORCE)

  FetchContent_MakeAvailable(opentracing)

  # Restore the saved state of BUILD_TESTING
  if(DEFINED _SAVED_BUILD_TESTING)
    set(BUILD_TESTING ${_SAVED_BUILD_TESTING} CACHE BOOL "" FORCE)
  else()
    unset(BUILD_TESTING CACHE)
  endif()

  # Restore the saved state of CMAKE_POLICY_VERSION_MINIMUM
  if(DEFINED _SAVED_CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM ${_SAVED_CMAKE_POLICY_VERSION_MINIMUM} CACHE STRING "" FORCE)
  else()
    unset(CMAKE_POLICY_VERSION_MINIMUM CACHE)
  endif()

  # Patch the opentracing targets to set missing includes, add namespaced alias targets, disable iwyu and clang-tidy.
  foreach(_target opentracing opentracing-static)
    if(TARGET ${_target})
      # Add missing include directories
      target_include_directories(${_target} PUBLIC
        "$<BUILD_INTERFACE:${opentracing_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${opentracing_SOURCE_DIR}/3rd_party/include>"
        "$<BUILD_INTERFACE:${opentracing_BINARY_DIR}/include>"
        )
      # Disable CXX_INCLUDE_WHAT_YOU_USE and CXX_CLANG_TIDY
      set_target_properties(${_target}
                          PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "" CXX_CLANG_TIDY "")
      # Create alias targets
      if(NOT TARGET OpenTracing::${_target})
        add_library(OpenTracing::${_target} ALIAS ${_target})
      endif()
    endif()
  endforeach()

  # Set the OpenTracing_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" OpenTracing_VERSION "${opentracing-cpp_GIT_TAG}")
endif(NOT OpenTracing_FOUND)

if(NOT TARGET OpenTracing::opentracing AND NOT TARGET OpenTracing::opentracing-static)
  message(FATAL_ERROR "No OpenTracing targets (OpenTracing::opentracing or OpenTracing::opentracing-static) were imported")
endif()
