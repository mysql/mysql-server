# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import GTest targets (GTest::gtest, GTest::gtest_main, and GTest::gmock) and set required variables GTEST_BOTH_LIBRARIES and GMOCK_LIB.
# 1. Find an installed GTest package
# 2. Use FetchContent to build googletest from a git submodule
# 3. Use FetchContent to fetch and build googletest from GitHub

find_package(GTest CONFIG QUIET)
set(GTest_PROVIDER "find_package")

if(NOT GTest_FOUND)
  set(_GOOGLETEST_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/googletest")
  if(EXISTS "${_GOOGLETEST_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
       "googletest"
       SOURCE_DIR "${_GOOGLETEST_SUBMODULE_DIR}"
       )
    set(GTest_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "googletest"
      GIT_REPOSITORY  "https://github.com/google/googletest.git"
      GIT_TAG "${googletest_GIT_TAG}"
      )
    set(GTest_PROVIDER "fetch_repository")
  endif()

  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  # Set the GTest_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" GTest_VERSION "${googletest_GIT_TAG}")
endif()

if(NOT TARGET GTest::gtest OR
   NOT TARGET GTest::gtest_main OR
   NOT TARGET GTest::gmock)
  message(FATAL_ERROR "A required GTest target (GTest::gtest, GTest::gtest_main, or GTest::gmock) was not imported")
endif()

if(NOT GTEST_BOTH_LIBRARIES)
  set(GTEST_BOTH_LIBRARIES GTest::gtest GTest::gtest_main)
endif()

if(NOT GMOCK_LIB)
  set(GMOCK_LIB GTest::gmock)
endif()
