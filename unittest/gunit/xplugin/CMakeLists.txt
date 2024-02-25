# Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

IF(NOT WITH_MYSQLX)
  RETURN()
ENDIF()

INCLUDE(CTest)

OPTION(MYSQLX_ADDITIONAL_TESTS_ENABLE "MYSQLX - enable additional tests" OFF)

SET(MYSQLX_PROJECT_DIR "${PROJECT_SOURCE_DIR}/plugin/x")

GET_DIRECTORY_PROPERTY(MYSQLX_CLIENT_INCLUDE_DIR
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_CLIENT_INCLUDE_DIR
)

GET_DIRECTORY_PROPERTY(MYSQLX_GENERATE_DIR
  DIRECTORY ${MYSQLX_PROJECT_DIR}
  DEFINITION MYSQLX_GENERATE_DIR
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
)

ADD_DEFINITIONS(-DXPLUGIN_DISABLE_LOG)
# Protobuf 3.7.0 will generate #if 3007000 < PROTOBUF_MIN_PROTOC_VERSION
IF(PROTOBUF_VERSION_NUMBER MATCHES "3007000")
  ADD_DEFINITIONS(-DPROTOBUF_MIN_PROTOC_VERSION=2006000)
ENDIF()

ADD_SUBDIRECTORY(xpl)
ADD_SUBDIRECTORY(xcl)

