# Copyright (c) 2026, Oracle and/or its affiliates.
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

SET(RAPIDYAML_TAG rapidyaml-0.16.0)
SET(RAPIDYAML_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${RAPIDYAML_TAG})

# Build only the RapidYaml and c4core production sources. This avoids the
# upstream development CMake framework and its optional download paths.
ADD_LIBRARY(ryml STATIC
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/common.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/emit_buf.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/emit_file.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/node_type.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/parse.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/reference_resolver.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/scalar_style.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/tag.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/tree.cpp
  ${RAPIDYAML_SOURCE_DIR}/src/c4/yml/version.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/base64.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/error.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/format.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/language.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/memory_util.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/utf.cpp
  ${RAPIDYAML_SOURCE_DIR}/ext/c4core.src/c4/version.cpp)

TARGET_INCLUDE_DIRECTORIES(ryml SYSTEM PUBLIC
  $<BUILD_INTERFACE:${RAPIDYAML_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${RAPIDYAML_SOURCE_DIR}/ext/c4core.src>)
TARGET_COMPILE_FEATURES(ryml PUBLIC cxx_std_11)

# This matches the setting OpenTelemetry applies when it fetches RapidYaml.
TARGET_COMPILE_DEFINITIONS(ryml PUBLIC RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS)

# Keep MySQL's strict warning policy for MySQL code, while allowing warnings
# from the imported RapidYaml/c4core sources.
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  TARGET_COMPILE_OPTIONS(ryml PRIVATE -Wno-error)
  IF(MY_COMPILER_IS_GNU)
    TARGET_COMPILE_OPTIONS(ryml PRIVATE -Wno-suggest-attribute=format)
  ENDIF()
ELSEIF(WIN32)
  TARGET_COMPILE_OPTIONS(ryml PRIVATE /WX-)
ENDIF()

ADD_LIBRARY(ryml::ryml ALIAS ryml)
ADD_LIBRARY(extra::rapidyaml ALIAS ryml)
