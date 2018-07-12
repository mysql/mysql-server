# Copyright (c) 2017, 2018 Oracle and/or its affiliates. All rights reserved.
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

SET(CLIENT_FULL_LIB_NAME mysqlxclient)
SET(CLIENT_LITE_LIB_NAME mysqlxclient_lite)

SET(CLIENT_SRC
 "${CMAKE_CURRENT_SOURCE_DIR}/mysql41_hash.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xprotocol_impl.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xrow_impl.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/password_hasher.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/sha256_scramble_generator.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xquery_result_impl.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xsession_impl.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xconnection_impl.cc"
 "${CMAKE_CURRENT_SOURCE_DIR}/xrow.cc"
)

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_PROJECT_DIR}/client/
)

INCLUDE_DIRECTORIES(SYSTEM
  ${PROTOBUF_INCLUDE_DIRS}
  ${LIBEVENT_INCLUDE_DIRS}
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
)

SET(XCLIENT_FLAGS "")

IF(HAVE_VISIBILITY_HIDDEN)
  SET(XCLIENT_FLAGS "${XCLIENT_FLAGS} -fvisibility=hidden")
ENDIF(HAVE_VISIBILITY_HIDDEN)

ADD_COMPILE_FLAGS(${CLIENT_SRC}
  COMPILE_FLAGS
  "${XCLIENT_FLAGS}"
  "${MYSQLX_PROTOCOL_FLAGS}"
)

ADD_CONVENIENCE_LIBRARY(${CLIENT_FULL_LIB_NAME} ${CLIENT_SRC})
ADD_DEPENDENCIES(${CLIENT_FULL_LIB_NAME} ${MYSQLX_PROTOCOL_FULL_LIB} GenError)

MYSQLX_APPEND_COMPILE_FLAGS(${CLIENT_FULL_LIB_NAME}
  "-DUSE_MYSQLX_FULL_PROTO"
)

ADD_CONVENIENCE_LIBRARY(${CLIENT_LITE_LIB_NAME} ${CLIENT_SRC})
ADD_DEPENDENCIES(${CLIENT_LITE_LIB_NAME} ${MYSQLX_PROTOCOL_LITE_LIB} GenError)

SET(MYSQLX_CLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
SET(MYSQLX_CLIENT_FULL_LIB ${CLIENT_FULL_LIB_NAME} PARENT_SCOPE)
SET(MYSQLX_CLIENT_LITE_LIB ${CLIENT_LITE_LIB_NAME} PARENT_SCOPE)
