# Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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

ADD_WSHADOW_WARNING()
DISABLE_MISSING_PROFILE_WARNING()

# This is not a unit test per se, but uses the Google Test framework
# We are thus treating it as a unit test.
IF(NOT WITH_UNIT_TESTS)
  RETURN()
ENDIF()

ADD_DEFINITIONS(-DSTANDALONE_BINLOG)

SET(TESTS_DIR ${CS_BASE_SRC_DIR}/tests)

SET(TESTS "")

SET(TEST_LINK_LIBRARIES
  changestreams_standalone_static
  binlogstandalone_static
  mysqlclient
  ${GTEST_LIBRARIES})

# If we are using the protobuf extensions
IF (WITH_CS_PROTOBUF)
  SET (TESTS
    ${TESTS}
    cs-test-mysql-proto-reader-mtr)

  LIST(APPEND TEST_LINK_LIBRARIES
    changestreams_pb_static ${PROTOBUF_LITE_LIBRARY})
ENDIF()

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test} ${TESTS_DIR}/${test}.cpp
    ADD_TEST ${test}
    LINK_LIBRARIES ${TEST_LINK_LIBRARIES})
ENDFOREACH()
