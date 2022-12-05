# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})

# TODO(sgunders): Remove when all GMock tests use MOCK_METHOD().
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

IF(BUILD_IS_SINGLE_CONFIG)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E
    make_directory ${CMAKE_CURRENT_BINARY_DIR}/var
    COMMAND ${CMAKE_COMMAND} -E
    make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log
    COMMAND ${CMAKE_COMMAND} -E
    make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/harness
    )
ELSE()
  FOREACH(config ${CMAKE_CONFIGURATION_TYPES})
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E
      make_directory ${CMAKE_CURRENT_BINARY_DIR}/${config}/var
      COMMAND ${CMAKE_COMMAND} -E
      make_directory ${CMAKE_CURRENT_BINARY_DIR}/${config}/var/log
      COMMAND ${CMAKE_COMMAND} -E
      make_directory ${CMAKE_CURRENT_BINARY_DIR}/${config}/var/log/harness
    )
  ENDFOREACH()
ENDIF()

ADD_SUBDIRECTORY(plugins)

SET(TEST_MODULE harness)

SET(TESTS
  test_access_rights.cc
  test_utility_string.cc
  test_loader.cc
  test_loader_lifecycle.cc
  test_logging.cc
  test_designator.cc
  test_iterator.cc
  test_utilities.cc
  test_common.cc
  test_config.cc
  test_filesystem.cc
  test_hostname_validator.cc
  test_bug22104451.cc
  test_random_generator.cc
  test_mysql_router_thread.cc
  test_mpmc_queue.cc
  test_dynamic_state.cc
  test_arg_handler.cc
  test_vt100.cc
  test_string_utils.cc
  test_process_launcher.cc
  test_tcp_address.cc
  test_config_builder.cc
  test_event_state_tracker.cc
  test_config_option.cc
  test_hexify.cc
)

FOREACH(TEST ${TESTS})
  ADD_HARNESS_TEST_FILE(${TEST} MODULE ${TEST_MODULE}
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/harness/shared/include/
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/harness/src/
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/harness/tests/include/
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/tests/
    LIB_DEPENDS test-helpers;harness_stdx)
ENDFOREACH()

SET(TESTS
  test_keyring.cc
  test_keyring_manager.cc
)

FOREACH(TEST ${TESTS})
  ADD_HARNESS_TEST_FILE(${TEST} MODULE ${TEST_MODULE}
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/harness/shared/include/
    LIB_DEPENDS test-helpers;${SSL_LIBRARIES})
ENDFOREACH()

SET(TESTS
  test_net_ts_buffer.cc
  test_net_ts_executor.cc
  test_net_ts_impl_callstack.cc
  test_net_ts_impl_file.cc
  test_net_ts_impl_io_service_base.cc
  test_net_ts_impl_kqueue_io_service.cc
  test_net_ts_impl_linux_epoll_io_service.cc
  test_net_ts_impl_netif.cc
  test_net_ts_impl_poll_io_service.cc
  test_net_ts_impl_resolver.cc
  test_net_ts_impl_socket.cc
  test_net_ts_internet.cc
  test_net_ts_io_context.cc
  test_net_ts_io_context_socket.cc
  test_net_ts_socket.cc
  test_net_ts_timer.cc
  test_net_ts_win32_named_pipe.cc
)

FOREACH(TEST ${TESTS})
  ADD_GOOGLETEST_FILE(${TEST} MODULE ${TEST_MODULE}
    LIB_DEPENDS harness_net_ts)
ENDFOREACH()

ADD_GOOGLETEST_FILE(test_net_ts_local.cc
  MODULE ${TEST_MODULE}
  LIB_DEPENDS harness_net_ts
  # TempDirectory
  LIB_DEPENDS test-helpers
  )

SET(TESTS
  test_stdx_bit.cc
  test_stdx_expected.cc
  test_stdx_flags.cc
  test_stdx_io_file_handle.cc
  test_stdx_monitor.cc
  test_stdx_ranges.cc
)

FOREACH(TEST ${TESTS})
  ADD_GOOGLETEST_FILE(${TEST} MODULE ${TEST_MODULE}
    LIB_DEPENDS harness_stdx)
ENDFOREACH()

# Use configuration file templates to generate configuration files
CONFIGURE_TEST_FILE_TEMPLATES(${CMAKE_CURRENT_SOURCE_DIR}/data
  "tests-good-1.cfg.in;tests-good-2.cfg.in;tests-start-1.cfg.in")
SET(TEST_FILES
  logger.cfg
  logger.d/default.cfg
  logger.d/magic.cfg
  logger.d/one.cfg
  magic-alt.cfg
  tests-bad-1.cfg
  tests-bad-2.cfg
  tests-bad-3.cfg
  )
COPY_TEST_FILES(${CMAKE_CURRENT_SOURCE_DIR}/data "${TEST_FILES}")

MYSQL_ADD_EXECUTABLE(net_ts_netifs net_ts_netifs.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(net_ts_netifs harness_net_ts)
ADD_DEPENDENCIES(mysqlrouter_all net_ts_netifs)

MYSQL_ADD_EXECUTABLE(net_ts_connect net_ts_connect.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(net_ts_connect harness_net_ts)
ADD_DEPENDENCIES(mysqlrouter_all net_ts_connect)

MYSQL_ADD_EXECUTABLE(net_ts_accept net_ts_accept.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(net_ts_accept harness_net_ts)
ADD_DEPENDENCIES(mysqlrouter_all net_ts_accept)

MYSQL_ADD_EXECUTABLE(net_ts_forward net_ts_forward.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(net_ts_forward harness_net_ts)
ADD_DEPENDENCIES(mysqlrouter_all net_ts_forward)

MYSQL_ADD_EXECUTABLE(acl_cli acl_cli.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(acl_cli harness-library)
ADD_DEPENDENCIES(mysqlrouter_all acl_cli)

MYSQL_ADD_EXECUTABLE(linux_timestamping linux_timestamping.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(linux_timestamping harness_net_ts harness_stdx)
ADD_DEPENDENCIES(mysqlrouter_all linux_timestamping)
