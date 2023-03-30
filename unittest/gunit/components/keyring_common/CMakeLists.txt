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

INCLUDE_DIRECTORIES (${GMOCK_INCLUDE_DIRS})

# Add tests here
SET(KEYRING_COMPONENT_COMMON_TESTS
  cache
  config_reader
  data
  data_file
  iterator
  json_data_file_operations
  json_data_reader
  json_data_writer
  operations
  parser
  aes_encryption
  )

SET(ALL_KEYRING_COMPONENT_COMMON_TESTS)
FOREACH(test ${KEYRING_COMPONENT_COMMON_TESTS})
  LIST(APPEND ALL_KEYRING_COMPONENT_COMMON_TESTS ${test}-t.cc)
ENDFOREACH()
LIST(APPEND ALL_KEYRING_COMPONENT_COMMON_TESTS
  ${CMAKE_SOURCE_DIR}/components/keyrings/common/encryption/aes.cc
  )

SET(KEYRING_COMPONENT_LIBS
  keyring_common
  ${SSL_LIBRARIES}
  ${LIBDL}
  )

IF(COMPONENT_COMPILE_VISIBILITY)
  SET(COMPILE_OPTIONS_ARG COMPILE_OPTIONS "${COMPONENT_COMPILE_VISIBILITY}")
ENDIF()

MYSQL_ADD_EXECUTABLE(merge_component_keyring_common_tests-t
  ${ALL_KEYRING_COMPONENT_COMMON_TESTS}
  ${COMPILE_OPTIONS_ARG}
  ENABLE_EXPORTS
  ADD_TEST merge_component_keyring_common_tests
  LINK_LIBRARIES
  ${KEYRING_COMPONENT_LIBS} ${GTEST_LIBRARIES} gtest_main
 )

FOREACH(test ${KEYRING_COMPONENT_COMMON_TESTS})
  MYSQL_ADD_EXECUTABLE(keyring_common_${test}-t ${test}-t.cc
    ${COMPILE_OPTIONS_ARG}
    ENABLE_EXPORTS
    SKIP_INSTALL
    EXCLUDE_FROM_ALL
    LINK_LIBRARIES
    ${KEYRING_COMPONENT_LIBS} ${GTEST_LIBRARIES} gtest_main
    )
ENDFOREACH()

