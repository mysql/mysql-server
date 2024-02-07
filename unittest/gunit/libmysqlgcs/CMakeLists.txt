# Copyright (c) 2016, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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

IF(NOT WIN32)
  MYSQL_CHECK_RPC()
ENDIF()

INCLUDE_DIRECTORIES(
  SYSTEM
  ${CMAKE_SOURCE_DIR}/plugin/group_replication/libmysqlgcs/include
  ${CMAKE_BINARY_DIR}/plugin/group_replication/libmysqlgcs/include
  ${CMAKE_BINARY_DIR}/plugin/group_replication/libmysqlgcs
  ${CMAKE_BINARY_DIR}/plugin/group_replication/libmysqlgcs/xdr_gen
  ${CMAKE_SOURCE_DIR}/plugin/group_replication/libmysqlgcs/src/interface
  ${CMAKE_SOURCE_DIR}/plugin/group_replication/libmysqlgcs/src/bindings/xcom
  ${CMAKE_SOURCE_DIR}/plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom
  ${CMAKE_SOURCE_DIR}/unittest/gunit/libmysqlgcs/include)

IF(WIN32)
  SET(XCOM_BASEDIR
    ${CMAKE_SOURCE_DIR}/plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom)
  INCLUDE_DIRECTORIES(
    ${XCOM_BASEDIR}/windeps/include
    ${XCOM_BASEDIR}/windeps/sunrpc
    ${XCOM_BASEDIR}/windeps/sunrpc/rpc)
ENDIF()

###
### Tests
###

SET(GCS_INTERFACE_TESTS
  interface/gcs_message
  interface/gcs_tagged_lock)

SET(GCS_XCOM_TESTS
  xcom/gcs_logging
  xcom/gcs_logging_system
  xcom/gcs_parameters
  xcom/gcs_xcom_notification
  xcom/gcs_xcom_utils
  xcom/gcs_msg_stages
  xcom/gcs_allowlist
  xcom/gcs_xcom_group_management
  xcom/gcs_xcom_state_exchange
  xcom/gcs_packet
  xcom/gcs_xcom_communication_interface
  xcom/gcs_xcom_interface_unit
  xcom/gcs_xcom_statistics_interface
  xcom/gcs_xcom_group_member_information
  xcom/gcs_mpsc_queue
  xcom/gcs_xcom_input_queue
  xcom/gcs_xcom_networking
  xcom/gcs_xcom_xcom_base
  xcom/gcs_xcom_xcom_transport
  xcom/gcs_xcom_communication_protocol_changer
  xcom/gcs_xcom_xcom_cache
  xcom/gcs_xcom_control_interface
  xcom/gcs_xcom_view_identifier
  xcom/gcs_message_stage_fragmentation
  xcom/gcs_xcom_site_def
  xcom/gcs_xcom_expels_in_progress
  xcom/gcs_xcom_network_provider
  xcom/gcs_xcom_network_provider_manager
  xcom/gcs_xcom_xcom_single_writer
  xcom/gcs_xcom_statistics_manager
  xcom/gcs_xcom_statistics_storage_impl
)

# When binding using IPv6 the way XCom does (dual stack IPv4/IPv6), it
# does not detect that a port is busy when binding and overruns the
# previous port. As such, this test cannot run on Solaris
IF(SOLARIS)
  LIST(REMOVE_ITEM GCS_XCOM_TESTS xcom/gcs_xcom_network_provider)
ENDIF()

IF(WIN32)
  # UINT8_MAX: macro redefinition
  ADD_COMPILE_FLAGS(xcom/gcs_xcom_site_def-t.cc COMPILE_FLAGS "/wd4005")
ENDIF()

###
### Configure the tests and add target
###

FOREACH(tpath ${GCS_INTERFACE_TESTS} ${GCS_XCOM_TESTS})
  GET_FILENAME_COMPONENT(test ${tpath} NAME)
  MYSQL_ADD_EXECUTABLE(${test}-t ${tpath}-t.cc
    ADD_TEST ${test}
    ENABLE_EXPORTS
    LINK_LIBRARIES gunit_small mysqlgcs ext::lz4
    )
  SET(GCS_UNIT_TESTS "${GCS_UNIT_TESTS}" "${test}-t")
  IF(WIN32)
    # Suppress linker warnings arising from linking to mysqlgcs and mysys
    # LNK4217: symbol 'symbol' defined in 'f1.obj' is imported by 'f2.obj'...
    # LNK4286: symbol 'symbol' defined in 'f1.obj' is imported by 'f2.obj'
    MY_TARGET_LINK_OPTIONS(${test}-t "/ignore:4217 /ignore:4286")
  ENDIF()
ENDFOREACH()

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated")
ENDIF()

ADD_CUSTOM_TARGET(
  libmysqlgcs-tests
  DEPENDS
  mysqlgcs
  ext::lz4
  ${GCS_UNIT_TESTS})
