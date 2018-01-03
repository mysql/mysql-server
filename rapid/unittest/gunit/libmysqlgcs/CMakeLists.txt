# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

IF(NOT GMOCK_FOUND OR DISABLE_SHARED)
  RETURN()
ENDIF()

###
### Includes
###

INCLUDE_DIRECTORIES(
  SYSTEM
  ${GMOCK_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/rapid/plugin/group_replication/libmysqlgcs/include
  ${CMAKE_BINARY_DIR}/rapid/plugin/group_replication/libmysqlgcs/include
  ${CMAKE_BINARY_DIR}/rapid/plugin/group_replication/libmysqlgcs/xdr_gen
  ${CMAKE_SOURCE_DIR}/rapid/plugin/group_replication/libmysqlgcs/src/bindings/xcom
  ${CMAKE_SOURCE_DIR}/rapid/plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom
  ${CMAKE_SOURCE_DIR}/rapid/unittest/gunit/libmysqlgcs/include)

IF (WIN32)
  SET (XCOM_BASEDIR
       ${CMAKE_SOURCE_DIR}/rapid/plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom)
  INCLUDE_DIRECTORIES(${XCOM_BASEDIR}/windeps/include
                      ${XCOM_BASEDIR}/windeps/sunrpc
                      ${XCOM_BASEDIR}/windeps/sunrpc/rpc)
ENDIF()

###
### Tests
###

SET(GCS_INTERFACE_TESTS
    interface/gcs_message)

SET(GCS_XCOM_TESTS
    xcom/gcs_logging
    xcom/gcs_logging_system
    xcom/gcs_parameters
    xcom/gcs_xcom_notification
    xcom/gcs_xcom_utils
    xcom/gcs_msg_stages
    xcom/gcs_whitelist
    xcom/gcs_xcom_group_management
    xcom/gcs_xcom_state_exchange
    xcom/gcs_packet
    xcom/gcs_xcom_communication_interface
    xcom/gcs_xcom_interface_unit
    xcom/gcs_xcom_statistics_interface
    xcom/gcs_xcom_group_member_information
   )

###
### Configure the tests and add target
###

FOREACH(tpath ${GCS_INTERFACE_TESTS} ${GCS_XCOM_TESTS})
  GET_FILENAME_COMPONENT(test ${tpath} NAME)
  MYSQL_ADD_EXECUTABLE(${test}-t ${tpath}-t.cc ADD_TEST ${test})
  TARGET_LINK_LIBRARIES(${test}-t gunit_small ${GCS_LIBRARY} ${LZ4_LIBRARY})
  SET(GCS_UNIT_TESTS "${GCS_UNIT_TESTS}" "${test}-t")
ENDFOREACH()

ADD_CUSTOM_TARGET(libmysqlgcs-tests
                  DEPENDS
                  ${GCS_LIBRARY}
                  ${LZ4_LIBRARY}
                  ${GCS_UNIT_TESTS})
