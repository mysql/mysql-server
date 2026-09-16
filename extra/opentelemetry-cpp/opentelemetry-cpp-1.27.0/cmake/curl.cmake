# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Import the curl target (CURL::libcurl).
# 1. Find an installed curl package
# 2. Use FetchContent to fetch and build curl from GitHub

# Find the curl package with the default search mode
find_package(CURL QUIET)
set(CURL_PROVIDER "find_package")

if(NOT CURL_FOUND)
  FetchContent_Declare(
    "curl"
    GIT_REPOSITORY "https://github.com/curl/curl.git"
    GIT_TAG "${curl_GIT_TAG}"
    )
  set(CURL_PROVIDER "fetch_repository")

  if(OPENTELEMETRY_INSTALL)
    set(_CURL_DISABLE_INSTALL OFF)
  else()
    set(_CURL_DISABLE_INSTALL ON)
  endif()

  if(DEFINED BUILD_SHARED_LIBS)
    set(_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
  endif()

  set(CURL_DISABLE_INSTALL ${_CURL_DISABLE_INSTALL} CACHE BOOL "" FORCE)
  set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
  set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
  set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
  set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
  set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(curl)

  # Restore BUILD_SHARED_LIBS
  if(DEFINED _SAVED_BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${_SAVED_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
  else()
    unset(BUILD_SHARED_LIBS CACHE)
  endif()

  # Set the CURL_VERSION variable from the git tag.
  string(REGEX REPLACE "^curl-([0-9]+)_([0-9]+)_([0-9]+)$" "\\1.\\2.\\3" CURL_VERSION "${curl_GIT_TAG}")

  # disable iwyu and clang-tidy
  foreach(_curl_target libcurl_shared libcurl_static)
    if(TARGET ${_curl_target})
      set_target_properties(${_curl_target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE ""
                                                     CXX_CLANG_TIDY "")
    endif()
  endforeach()
endif()

# Set the CURL_VERSION from the legacy CURL_VERSION_STRING Required for CMake
# versions below 4.0
if(NOT CURL_VERSION AND CURL_VERSION_STRING)
  set(CURL_VERSION ${CURL_VERSION_STRING})
endif()

# Add the main CURL::libcurl alias target if missing. Prefer the shared target followed by the static target
if(NOT TARGET CURL::libcurl)
  if(TARGET libcurl_shared)
    add_library(CURL::libcurl ALIAS libcurl_shared)
  elseif(TARGET libcurl_static)
    add_library(CURL::libcurl ALIAS libcurl_static)
  endif()
endif()

if(NOT TARGET CURL::libcurl)
  message(FATAL_ERROR "The required curl target (CURL::libcurl) was not imported.")
endif()
