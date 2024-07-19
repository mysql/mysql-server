# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

SET(GOOGLETEST_RELEASE googletest-1.14.0)
SET(GMOCK_SOURCE_DIR
  ${CMAKE_SOURCE_DIR}/extra/googletest/${GOOGLETEST_RELEASE}/googlemock)
SET(GTEST_SOURCE_DIR
  ${CMAKE_SOURCE_DIR}/extra/googletest/${GOOGLETEST_RELEASE}/googletest)

SET(GMOCK_FOUND 1)
SET(GMOCK_FOUND 1 CACHE INTERNAL "" FORCE)

SET(GMOCK_INCLUDE_DIRS
  ${GMOCK_SOURCE_DIR}
  ${GMOCK_SOURCE_DIR}/include
  ${GTEST_SOURCE_DIR}
  ${GTEST_SOURCE_DIR}/include
  CACHE INTERNAL "")

ADD_LIBRARY(gmock STATIC ${GMOCK_SOURCE_DIR}/src/gmock-all.cc)
ADD_LIBRARY(gtest STATIC ${GTEST_SOURCE_DIR}/src/gtest-all.cc)
SET(GTEST_LIBRARIES gmock gtest)

ADD_LIBRARY(gmock_main STATIC ${GMOCK_SOURCE_DIR}/src/gmock_main.cc)
ADD_LIBRARY(gtest_main STATIC ${GTEST_SOURCE_DIR}/src/gtest_main.cc)
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  SET_TARGET_PROPERTIES(gtest_main gmock_main
    PROPERTIES
    COMPILE_FLAGS "-Wno-undef -Wno-conversion")
ENDIF()

IF(MSVC AND MSVC_CPPCHECK)
  # Function ... should be marked with 'override'
  TARGET_COMPILE_OPTIONS(gmock PRIVATE "/wd26433")
  TARGET_COMPILE_OPTIONS(gtest PRIVATE "/wd26433")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
MY_CHECK_CXX_COMPILER_WARNING("-Wsuggest-override" HAS_SUGGEST_OVERRIDE)

IF(HAS_SUGGEST_OVERRIDE)
  # Google-test TYPED_TEST does not override the virtual function in derived class.
  SET_TARGET_PROPERTIES(gmock PROPERTIES INTERFACE_COMPILE_OPTIONS
    "-Wno-suggest-override")
ENDIF()

FOREACH(googletest_library
    gmock
    gtest
    gmock_main
    gtest_main
    )
  TARGET_INCLUDE_DIRECTORIES(${googletest_library} SYSTEM PUBLIC
    ${GMOCK_INCLUDE_DIRS}
    )
  IF(HAS_MISSING_PROFILE)
    TARGET_COMPILE_OPTIONS(${googletest_library} PRIVATE ${HAS_MISSING_PROFILE})
  ENDIF()
ENDFOREACH()
