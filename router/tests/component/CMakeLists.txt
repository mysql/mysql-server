# Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

IF(NOT TARGET mysql_server_mock)
  RETURN()
ENDIF()

ADD_HARNESS_PLUGIN(routertestplugin_logger NO_INSTALL
  DESTINATION harness
  SOURCES logger_testplugin.cc)

MYSQL_ADD_EXECUTABLE(master_key_test_writer
  ../helpers/master_key_test_writer.cc
  COMPONENT Router
  SKIP_INSTALL
  )

MYSQL_ADD_EXECUTABLE(signal_me
  signal_me.cc
  LINK_LIBRARIES mysys
  COMPONENT Router
  SKIP_INSTALL
  )

ADD_SUBDIRECTORY(rest_signal)

ADD_DEFINITIONS(-DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")
ADD_DEFINITIONS(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
FOREACH(test_file
    test_async_replicaset.cc
    test_bootstrap.cc
    test_bootstrap_account.cc
    test_bootstrap_clusterset.cc
    test_bootstrap_system_deployment.cc
    test_bootstrap_tls_endpoint.cc
    test_clusterset.cc
    test_component_test_framework.cc
    test_config.cc
    test_config_overwrites.cc
    test_connection_pool.cc
    test_gr_notifications.cc
    test_http_server.cc
    test_logging.cc
    test_logging_eventlog.cc
    test_master_key_reader_writer.cc
    test_metadata_ttl.cc
    test_cluster_metadata.cc
    test_node_attributes.cc
    test_gr_state.cc
    test_pidfile.cc
    test_read_replica.cc
    test_rest_api.cc
    test_rest_api_enable.cc
    test_rest_metadata_cache.cc
    test_rest_mock_server.cc
    test_rest_router.cc
    test_rest_routing.cc
    test_rest_connection_pool.cc
    test_router_configuration_errors.cc
    test_router_stacktrace.cc
    test_routing.cc
    test_routing_connection.cc
    test_routing_connection_errors.cc
    test_routing_retry.cc
    test_routing_router_require.cc
    test_routing_strategy.cc
    test_routing_sharing.cc
    test_routing_splitting.cc
    test_sd_notify.cc
    test_shutdown.cc
    test_state_file.cc
    test_user_option.cc
    test_metadata_http_auth_backend.cc
    test_socket_close.cc
    test_stacktrace.cc
    test_destination_status.cc
    )
add_test_file(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
  MODULE "component"
  INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/src/mock_server/include/mysqlrouter/
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
                      ${PROTOBUF_INCLUDE_DIR}
  LIB_DEPENDS
  http_client
  http_common
  mock_server_rest_client
  mysql_protocol
  router_mysqlxclient
  router_mysqlxmessages
  router_cluster

  DEPENDS
  http_auth_backend
  http_auth_realm
  http_server
  master_key_test_writer
  metadata_cache
  mock_server
  mysql_server_mock
  mysqlrouter_passwd
  mysqlrouter
  rest_api
  rest_metadata_cache
  rest_router
  rest_routing
  routing
  routertestplugin_logger
  signal_me
  )
  ROUTERTEST_GET_TARGET(ttt ${test_file} "component")
  DOWNGRADE_STRINGOP_WARNINGS(${ttt})
ENDFOREACH()

FOREACH(test_file
    test_routing_splicer.cc
    test_mock_server.cc
    )
add_router_test_file(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
  MODULE "component"
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
                      ${PROTOBUF_INCLUDE_DIR}
  INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/plugin/x/client/
  $<TARGET_PROPERTY:routing_connections,INCLUDE_DIRECTORIES>

  LIB_DEPENDS
  router_mysqlxclient
  router_mysqlxmessages

  DEPENDS
  http_auth_backend
  http_auth_realm
  http_server
  master_key_test_writer
  metadata_cache
  mock_server
  mysql_server_mock
  mysqlrouter
  rest_api
  rest_metadata_cache
  rest_router
  rest_routing
  router_openssl
  router_protobuf
  routing
  )
ENDFOREACH()

# the mock-server test needs the mysql_native_password client plugin,
# if it built (depends on WITH_AUTHENTICATION_CLIENT_PLUGINS)
IF(TARGET mysql_native_password)
  ADD_DEPENDENCIES(routertest_component_mock_server
    mysql_native_password)
ELSE()
  ADD_DEFINITIONS(-DSKIP_MYSQL_NATIVE_PASSWORD_TESTS)
ENDIF()


# When system deployment bootstrap is executed, files are
# created in locations specified by deployment layout.
# Since component tests may not have access to directories defined
# in install_layout.cmake, system bootstrap deployment tests are
# executed only for STANDALONE layout, and are not executed on Windows.
IF(WIN32 OR NOT INSTALL_LAYOUT STREQUAL "STANDALONE")
  ADD_DEFINITIONS(-DSKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS)
ENDIF()

ADD_DEFINITIONS(
  -DROUTER_SRC_DIR="${CMAKE_SOURCE_DIR}/router")
