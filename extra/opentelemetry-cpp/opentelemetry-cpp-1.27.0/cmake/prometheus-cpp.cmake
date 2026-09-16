# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import prometheus-cpp targets (prometheus-cpp::core and prometheus-cpp::pull)
# 1. Find an installed prometheus-cpp package
# 2. Use FetchContent to build prometheus-cpp from a git submodule
# 3. Use FetchContent to fetch and build prometheus-cpp from GitHub

find_package(prometheus-cpp CONFIG QUIET)
set(prometheus-cpp_PROVIDER "find_package")

if(NOT prometheus-cpp_FOUND)
  set(_PROMETHEUS_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/prometheus-cpp")
  if(EXISTS "${_PROMETHEUS_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
       "prometheus-cpp"
       SOURCE_DIR "${_PROMETHEUS_SUBMODULE_DIR}"
       )
    set(prometheus-cpp_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "prometheus-cpp"
      GIT_REPOSITORY  "https://github.com/jupp0r/prometheus-cpp.git"
      GIT_TAG "${prometheus-cpp_GIT_TAG}"
      GIT_SUBMODULES "3rdparty/civetweb"
      )
    set(prometheus-cpp_PROVIDER "fetch_repository")
  endif()

  if(DEFINED ENABLE_TESTING)
    set(_SAVED_ENABLE_TESTING ${ENABLE_TESTING})
  endif()

  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
  set(USE_THIRDPARTY_LIBRARIES ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(prometheus-cpp)

  if(DEFINED _SAVED_ENABLE_TESTING)
    set(ENABLE_TESTING ${_SAVED_ENABLE_TESTING} CACHE BOOL "" FORCE)
  else()
    unset(ENABLE_TESTING CACHE)
  endif()

  # Set the prometheus-cpp_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" prometheus-cpp_VERSION "${prometheus-cpp_GIT_TAG}")

  # Disable iwyu and clang-tidy
  foreach(_prometheus_target core pull civetweb)
    set_target_properties(${_prometheus_target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE ""
                                                     CXX_CLANG_TIDY "")
  endforeach()
endif()

if(NOT TARGET prometheus-cpp::core OR
   NOT TARGET prometheus-cpp::pull)
  message(FATAL_ERROR "A required prometheus-cpp target (prometheus-cpp::core or prometheus-cpp::pull) was not imported")
endif()
