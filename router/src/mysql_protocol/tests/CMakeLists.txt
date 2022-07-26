# Copyright (c) 2016, 2022, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(TEST_MODULE mysql_protocol)

FOREACH(test_file
    test_classic_protocol_frame.cc
    test_classic_protocol_message.cc
    test_classic_protocol_session_track.cc
    test_classic_protocol_wire.cc
    test_classic_protocol.cc
  )
  ADD_GOOGLETEST_FILE(${test_file}
    MODULE ${TEST_MODULE}
    SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
    INCLUDE_DIRS ../include/
    LIB_DEPENDS harness_stdx)
ENDFOREACH()

IF(WIN32)
  ADD_COMPILE_FLAGS(
    test_classic_protocol_message.cc
    COMPILE_FLAGS "/bigobj")
ENDIF()
