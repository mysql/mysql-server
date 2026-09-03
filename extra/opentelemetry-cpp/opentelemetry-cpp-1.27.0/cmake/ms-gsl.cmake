# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

find_package(Microsoft.GSL CONFIG QUIET)
set(Microsoft.GSL_PROVIDER "find_package")

if(NOT Microsoft.GSL_FOUND)
  set(_Microsoft.GSL_SUBMODULE_DIR "${opentelemetry-cpp_SOURCE_DIR}/third_party/ms-gsl")
  if(EXISTS "${_Microsoft.GSL_SUBMODULE_DIR}/.git")
    FetchContent_Declare(
       "gsl"
       SOURCE_DIR "${_Microsoft.GSL_SUBMODULE_DIR}"
       )
    set(Microsoft.GSL_PROVIDER "fetch_source")
  else()
    FetchContent_Declare(
      "gsl"
      GIT_REPOSITORY  "https://github.com/microsoft/GSL.git"
      GIT_TAG "${ms-gsl_GIT_TAG}"
      )
    set(Microsoft.GSL_PROVIDER "fetch_repository")
  endif()

  set(GSL_TEST OFF CACHE BOOL "" FORCE)
  set(GSL_INSTALL ${OPENTELEMETRY_INSTALL} CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(gsl)

  # Set the Microsoft.GSL_VERSION variable from the git tag.
  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1" Microsoft.GSL_VERSION "${ms-gsl_GIT_TAG}")
endif()

if(NOT TARGET Microsoft.GSL::GSL)
  message(FATAL_ERROR "A required Microsoft.GSL target Microsoft.GSL::GSL was not imported")
endif()
