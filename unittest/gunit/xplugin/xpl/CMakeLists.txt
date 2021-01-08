# Copyright (c) 2020, 2021, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
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

SET(XPL_UNIT_TESTS xplugin_unit_tests)

INCLUDE(source_files.cmake)

# This compiler fails to handle some of the mock functions involved.
IF(MY_COMPILER_IS_SUNPRO)
  LIST(REMOVE_ITEM XPL_TEST_SRC
    account_verification_handler_t.cc
    admin_cmd_index_t.cc
    index_field_t.cc
    listener_tcp_t.cc
    listener_unix_socket_t.cc
    protocol_decoder_t.cc
    sql_data_context_t.cc
    socket_acceptor_task_t.cc
    timeouts_t.cc
  )
ENDIF()

# googlemock uses GTEST_DISALLOW_ASSIGN_ which will generate:
# definition of implicit copy constructor for  ....
# is deprecated because it has a user-declared copy assignment operator
IF(MY_COMPILER_IS_CLANG)
  ADD_COMPILE_FLAGS(
    account_verification_handler_t.cc
    admin_cmd_index_t.cc
    index_field_t.cc
    listener_unix_socket_t.cc
    protocol_decoder_t.cc
    timeouts_t.cc
    COMPILE_FLAGS "-Wno-deprecated"
    )
ENDIF()

IF(MYSQLX_ADDITIONAL_TESTS_ENABLE)
  INCLUDE(mysql_function_names.cmake)
  LIST(APPEND XPL_TEST_SRC "${MYSQLX_GENERATE_DIR}/mysql_function_names_t.cc")
ENDIF()


MYSQL_ADD_EXECUTABLE(${XPL_UNIT_TESTS}
  ../test_main.cc
  ${XPL_TEST_SRC}
  ADD_TEST xplugin
)
SET_TESTS_PROPERTIES(xplugin PROPERTIES TIMEOUT 180)

ADD_COMPILE_FLAGS(${XPL_TEST_SRC}
  COMPILE_FLAGS "${MYSQLX_PROTOCOL_FLAGS}"
)

IF(DISABLE_MYSQLX_PROTOBUF_LITE)
  ADD_COMPILE_DEFINITIONS(${XPL_TEST_SRC}
    COMPILE_DEFINITIONS USE_MYSQLX_FULL_PROTO)
ENDIF()

ADD_DEPENDENCIES(${XPL_UNIT_TESTS}
  mysqlx
)

# New warning in Visual Studio 2008
# '%$S': virtual function overrides '%$pS', previous versions of the compiler
# did not override when parameters only differed by const/volatile qualifiers
IF(WIN32)
  SET_TARGET_PROPERTIES(${XPL_UNIT_TESTS} PROPERTIES COMPILE_FLAGS "/wd4373")
ENDIF(WIN32)

# assertion failed in function assert_with_dumps() @ ssa.c:621 when using -xO3
# Segfault in the unit test, so lower optimization level there as well
IF(MY_COMPILER_IS_SUNPRO)
  ADD_COMPILE_FLAGS(
    callback_command_delegate_t.cc
    COMPILE_FLAGS "-xO2")
ENDIF()

TARGET_LINK_LIBRARIES(${XPL_UNIT_TESTS}
  ${GCOV_LDFLAGS}
  ${MYSQLX_CLIENT_LIB}
  mysqlx
  mysqlclient
  gtest
  gmock
)

