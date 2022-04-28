# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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


INCLUDE(source_files.cmake)
IF(HAVE_SETNS)
  LIST(APPEND XTEST_SRC "${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc")
ENDIF()

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_PROJECT_DIR}
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)

MYSQL_ADD_EXECUTABLE(mysqlxtest
  ${XTEST_SRC}
  COMPONENT Test
  DEPENDENCIES GenError GenClientError
  LINK_LIBRARIES
  mysys
  strings
  mysqlclient
  ${MYSQLX_CLIENT_FULL_LIB}
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${PROTOBUF_LIBRARY}
  ${LZ4_LIBRARY}
)

IF(UNIX_INSTALL_RPATH_ORIGIN_PRIV_LIBDIR)
  ADD_INSTALL_RPATH_FOR_PROTOBUF(mysqlxtest)
ENDIF()

MYSQLX_APPEND_COMPILE_FLAGS(mysqlxtest
  "${MYSQLX_PROTOCOL_FLAGS}"
)

MYSQLX_APPEND_COMPILE_DEFINITIONS(mysqlxtest
  "NGS_DISABLE_LOGGING"
)

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-parameter" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${XTEST_SRC} COMPILE_FLAGS ${HAS_WARN_FLAG})
ENDIF()
