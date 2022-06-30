# Copyright (c) 2014, 2022, Oracle and/or its affiliates.
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

IF (DEFINED WITHOUT_GROUP_REPLICATION AND WITHOUT_GROUP_REPLICATION)
  RETURN()
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wno-unused-local-typedefs" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# Add tests
SET(TESTS
  group_replication_member_info
  group_replication_compatibility_module
  group_replication_member_version
  group_replication_mysql_version_gcs_protocol_map
  group_replication_gcs_mysql_network_provider
  )

FOREACH(test ${TESTS})
  SET(SRC_FILES ${test}-t.cc)
  IF(WIN32)
    LIST(APPEND SRC_FILES ../../../sql/nt_servc.cc)
  ENDIF()

  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES}
    ADD_TEST ${test}
    ENABLE_EXPORTS
    LINK_LIBRARIES
    gr_unit_test_resource
    gunit_large
    server_unittest_library
    mysqlclient
    mysqlgcs
    )
ENDFOREACH()

IF(WIN32)
  # LNK4286: symbol 'unknown_sqlstate' is imported from mysqlclient.lib
  MY_TARGET_LINK_OPTIONS(
    group_replication_gcs_mysql_network_provider-t "/ignore:4286"
    )
  # With clang we get 4217 rather than 4286
  IF(WIN32_CLANG)
    MY_TARGET_LINK_OPTIONS(
      group_replication_gcs_mysql_network_provider-t "/ignore:4217"
      )
  ENDIF()
ENDIF()
