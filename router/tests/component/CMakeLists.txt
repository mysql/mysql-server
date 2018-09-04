# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

ADD_EXECUTABLE(master_key_test_writer ../helpers/master_key_test_writer.cc)
SET_TARGET_PROPERTIES(master_key_test_writer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE "component"
  INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/http/include/
  SYSTEM_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS}
  LIB_DEPENDS http_client;http_common;${LIBEVENT_LIBRARY}
  DEPENDS master_key_test_writer;mysqlrouter;mysql_server_mock
  )

# When system deployment bootstrap is executed, files are
# created in locations specified by deployment layout.
# Since component tests may not have access to directories defined
# in install_layout.cmake, system bootstrap deployment tests are
# executed only for STANDALONE layout, and are not executed on Windows.
IF(WIN32 OR NOT INSTALL_LAYOUT STREQUAL "STANDALONE")
  ADD_DEFINITIONS(-DSKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS)
ENDIF()
