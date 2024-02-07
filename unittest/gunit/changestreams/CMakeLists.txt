# Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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

ADD_DEFINITIONS(-DSTANDALONE_BINLOG)

MY_CHECK_CXX_COMPILER_WARNING("-Wno-unused-local-typedefs" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})

# Add tests
SET(TESTS
  cstreams_reader_state)

SET(TESTS_LINK_LIBRARIES
  changestreams_standalone_static
  mysql_binlog_event_standalone
  mysqlclient
  gunit_small)

IF (WITH_CS_PROTOBUF)
  LIST(APPEND TESTS
    cstreams_reader_state_codec_pb)

  LIST(APPEND TESTS_LINK_LIBRARIES
    changestreams_pb_static
    ext::libprotobuf-lite)
ENDIF()

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    ADD_TEST ${test}
    LINK_LIBRARIES ${TESTS_LINK_LIBRARIES}
    COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")
ENDFOREACH()

