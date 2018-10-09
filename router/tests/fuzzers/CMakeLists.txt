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

IF(LIBFUZZER_COMPILE_FLAGS)
  SET(TARGET routerfuzz_router_uri)

  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  ADD_EXECUTABLE(${TARGET}
    fuzz_router_uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc
  )

  TARGET_LINK_LIBRARIES(${TARGET} harness-library ${MySQL_CLIENT_LIB}
                        ${SSL_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(${TARGET}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/router/include/)

  LIBFUZZER_ADD_TEST(${TARGET}
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fuzz_router_uri)
  ADD_DEPENDENCIES(mysqlrouter_all ${TARGET})


  SET(TARGET routerfuzz_router_uri_tostring)

  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  ADD_EXECUTABLE(${TARGET}
    fuzz_router_uri_tostring.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc)

  TARGET_LINK_LIBRARIES(${TARGET} harness-library ${MySQL_CLIENT_LIB}
                        ${SSL_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(${TARGET}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/router/include/)

  LIBFUZZER_ADD_TEST(${TARGET}
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fuzz_router_uri_tostring)
  ADD_DEPENDENCIES(mysqlrouter_all ${TARGET})
ENDIF()
