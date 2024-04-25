# Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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


GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_LIB
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_LIB
)

INCLUDE(source_files.cmake)

IF(MYSQLX_ADDITIONAL_TESTS_ENABLE)
  INCLUDE(mysql_function_names.cmake)
  LIST(APPEND XPL_TEST_SRC "${MYSQLX_GENERATE_DIR}/mysql_function_names_t.cc")
ENDIF()

# Fixes "C1128: number of sections exceeded object file format limit" in MSVC
IF(MSVC)
  ADD_COMPILE_FLAGS(
    mock/mock.cc
    COMPILE_FLAGS "/bigobj"
    )
ENDIF()

MYSQL_ADD_EXECUTABLE(xplugin_unit_tests
  ../test_main.cc
  ${XPL_TEST_SRC}
  LINK_LIBRARIES harness_net_ts ext::zlib ext::lz4 ext::zstd extra::rapidjson ext::icu
  DEPENDENCIES mysqlx
  ADD_TEST xplugin
)
SET_TESTS_PROPERTIES(xplugin PROPERTIES TIMEOUT 300)
DOWNGRADE_STRINGOP_WARNINGS(xplugin_unit_tests)

ADD_COMPILE_FLAGS(${XPL_TEST_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)
IF(WITH_PROTOBUF STREQUAL "bundled")
  TARGET_INCLUDE_DIRECTORIES(xplugin_unit_tests SYSTEM BEFORE PUBLIC
    "${BUNDLED_ABSEIL_SRCDIR}")
ENDIF()

# New warning in Visual Studio 2008
# '%$S': virtual function overrides '%$pS', previous versions of the compiler
# did not override when parameters only differed by const/volatile qualifiers
IF(WIN32)
  SET_TARGET_PROPERTIES(xplugin_unit_tests PROPERTIES COMPILE_FLAGS "/wd4373")
ENDIF(WIN32)

TARGET_LINK_LIBRARIES(xplugin_unit_tests
  ${GCOV_LDFLAGS}
  ${MYSQLX_CLIENT_LIB}
  mysqlx
  mysqlclient
  gtest
  gmock
)

