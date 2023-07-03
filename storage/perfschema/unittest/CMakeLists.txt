# Copyright (c) 2009, 2022, Oracle and/or its affiliates.
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

DISABLE_MISSING_PROFILE_WARNING()
ADD_DEFINITIONS(-DMYSQL_SERVER)

ADD_LIBRARY(pfs_server_stubs STATIC pfs_server_stubs.cc)

ADD_DEPENDENCIES(pfs_server_stubs GenError)

MACRO (PFS_ADD_TEST name)
  MYSQL_ADD_EXECUTABLE(${name}-t ${name}-t.cc ADD_TEST ${name})
  TARGET_LINK_LIBRARIES(${name}-t
    mytap perfschema mysys pfs_server_stubs strings ${ICU_LIBRARIES})
ENDMACRO()

SET(tests
 pfs_instr_class
 pfs_instr_class-oom
 pfs_instr
 pfs_instr-oom
 pfs_account-oom
 pfs_host-oom
 pfs_user-oom
 pfs_noop
 pfs
 pfs_misc
 pfs_mem
)
FOREACH(testname ${tests})
  PFS_ADD_TEST(${testname})
ENDFOREACH()

SET(MYSQLD_EXTRA_SOURCES)
IF(WIN32)
  LIST(APPEND MYSQLD_EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
ENDIF()
# Add sql_builtin.cc, to force linkage of plugins below.
IF(NOT WITH_SHARED_UNITTEST_LIBRARY)
  LIST(APPEND MYSQLD_EXTRA_SOURCES ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc)
ENDIF()

# We need the server libs to test the blob parser.
MYSQL_ADD_EXECUTABLE(pfs_connect_attr-t
  pfs_connect_attr-t.cc
  ${MYSQLD_EXTRA_SOURCES}
  ADD_TEST pfs_connect_attr
)
IF(WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO)
  # We get ODR violations because of multiple bison parsers.
  MY_CHECK_CXX_COMPILER_WARNING("error=odr" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    MY_TARGET_LINK_OPTIONS(pfs_connect_attr-t "${HAS_WARN_FLAG}")
  ENDIF()
ENDIF()

ADD_DEPENDENCIES(pfs_connect_attr-t GenServerSource)
# For static server_unittest_library we need to explicitly link in everything
# referenced in sql/sql_builtin.cc
IF(WITH_SHARED_UNITTEST_LIBRARY)
  UNSET(MYSQLD_EXTRA_LIBRARIES)
ELSE()
  SET(MYSQLD_EXTRA_LIBRARIES ${MYSQLD_STATIC_PLUGIN_LIBS})
ENDIF()

TARGET_LINK_LIBRARIES(pfs_connect_attr-t
  mytap
  server_unittest_library
  ${MYSQLD_EXTRA_LIBRARIES}
  )
