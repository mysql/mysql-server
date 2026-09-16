# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import nlohmann_json target (nlohmann_json::nlohmann_json).
# 1. Find an installed nlohmann-json package
# 2. Use FetchContent to build nlohmann-json from a git submodule
# 3. Use FetchContent to fetch and build nlohmann-json from GitHub

find_package(nlohmann_json CONFIG QUIET)
set(nlohmann_json_PROVIDER "find_package")

if(NOT nlohmann_json_FOUND)
  set(_NLOHMANN_JSON_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/nlohmann-json")
  if(EXISTS "${_NLOHMANN_JSON_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
       "nlohmann_json"
       SOURCE_DIR "${_NLOHMANN_JSON_SUBMODULE_DIR}"
       )
    set(nlohmann_json_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "nlohmann_json"
      GIT_REPOSITORY  "https://github.com/nlohmann/json.git"
      GIT_TAG "${nlohmann-json_GIT_TAG}"
      )
    set(nlohmann_json_PROVIDER "fetch_repository")
  endif()

  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  set(JSON_Install ${OPENTELEMETRY_INSTALL} CACHE BOOL "" FORCE)
  set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(nlohmann_json)

  # Set the nlohmann_json_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" nlohmann_json_VERSION "${nlohmann-json_GIT_TAG}")

  # Disable iwyu and clang-tidy only if the CMake version is greater or equal to 3.19.
  # CMake 3.19+ is needed to set the iwyu and clang-tidy properties on the INTERFACE target
  if(TARGET nlohmann_json AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
    set_target_properties(nlohmann_json PROPERTIES CXX_INCLUDE_WHAT_YOU_USE ""
                                                     CXX_CLANG_TIDY "")
  endif()
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR "A required nlohmann_json target (nlohmann_json::nlohmann_json) was not imported")
endif()

