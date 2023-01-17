# Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/components/example
)

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

# Add tests
SET(TESTS registry dynamic_loader)

ADD_LIBRARY(unit_test_common STATIC
  unit_test_common.cc )

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t
    ${test}-t.cc
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
    ADD_TEST ${test}
    ENABLE_EXPORTS
    LINK_LIBRARIES ${GTEST_LIBRARIES} mysys minchassis unit_test_common
  )
ENDFOREACH()

MYSQL_ADD_EXECUTABLE(minimal_chassis-t
  minimal_chassis-t.cc
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
  ADD_TEST minimal_chassis-t
  ENABLE_EXPORTS
  LINK_LIBRARIES ${GTEST_LIBRARIES} mysys minchassis unit_test_common
  DEPENDENCIES component_example_component1
)

MYSQL_ADD_EXECUTABLE(minimal_chassis_test_driver-t
  minimal_chassis_test_driver-t.cc
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
  ADD_TEST minimal_chassis_test_driver-t
  ENABLE_EXPORTS
  LINK_LIBRARIES ${GTEST_LIBRARIES} server_unittest_library unit_test_common
  DEPENDENCIES component_example_component1
)

IF(WIN32)
  TARGET_LINK_LIBRARIES(minimal_chassis_test_driver-t my_nt_servc)
ENDIF()

IF(NOT WITH_SHARED_UNITTEST_LIBRARY)
  TARGET_LINK_LIBRARIES(minimal_chassis_test_driver-t
    mysql_server_component_services)
ENDIF()

IF(WIN32)
  ADD_DEPENDENCIES(minimal_chassis_test_driver-t "${PROTOBUF_LITE_LIBRARY}")
  # Copy dlls for libprotobuf-lite and libcrypto/libssl to plugin dir.
  FUNCTION(COPY_DLL_FOR_UNITTEST DLL_FILE_NAME)
    SET(run_dir  "${CMAKE_BINARY_DIR}/runtime_output_directory/")
    SET(plg_dir  "${CMAKE_BINARY_DIR}/plugin_output_directory/")
    ADD_CUSTOM_COMMAND(TARGET minimal_chassis_test_driver-t POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${run_dir}/${CMAKE_CFG_INTDIR}/${DLL_FILE_NAME}"
      "${plg_dir}/${CMAKE_CFG_INTDIR}/${DLL_FILE_NAME}"
      )
  ENDFUNCTION()
  GET_FILENAME_COMPONENT(CRYPTO_NAME "${HAVE_CRYPTO_DLL}" NAME)
  GET_FILENAME_COMPONENT(OPENSSL_NAME "${HAVE_OPENSSL_DLL}" NAME)

  COPY_DLL_FOR_UNITTEST("$<TARGET_FILE_NAME:libprotobuf-lite>")
  COPY_DLL_FOR_UNITTEST("${CRYPTO_NAME}")
  COPY_DLL_FOR_UNITTEST("${OPENSSL_NAME}")
ENDIF()

MYSQL_ADD_EXECUTABLE(reference_cache-t
  reference_cache-t.cc
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
  ADD_TEST reference_cache-t
  ENABLE_EXPORTS
  LINK_LIBRARIES ${GTEST_LIBRARIES} server_unittest_library unit_test_common
  DEPENDENCIES component_reference_cache component_test_reference_cache
)

IF(WIN32)
  TARGET_LINK_LIBRARIES(reference_cache-t my_nt_servc)
ENDIF()
IF(NOT WITH_SHARED_UNITTEST_LIBRARY)
  TARGET_LINK_LIBRARIES(reference_cache-t mysql_server_component_services)
ENDIF()

MYSQL_ADD_COMPONENT(self_required_test_component
  self_required_test_component.cc
  MODULE_ONLY
  SKIP_INSTALL
  )
MYSQL_ADD_COMPONENT(cyclic_dependency_test_component_1
  cyclic_dependency_test_component_1.cc
  MODULE_ONLY
  SKIP_INSTALL
  )
MYSQL_ADD_COMPONENT(cyclic_dependency_test_component_2
  cyclic_dependency_test_component_2.cc
  MODULE_ONLY
  SKIP_INSTALL
  )
MYSQL_ADD_COMPONENT(test_reference_cache
  test_reference_cache.cc
  MODULE_ONLY
  SKIP_INSTALL
  )

SET(EXAMPLE_COMPONENTS
  component_example_component1
  component_example_component2
  component_example_component3
  component_self_required_test_component
  component_cyclic_dependency_test_component_1
  component_cyclic_dependency_test_component_2
  )

FOREACH(EXAMPLE_COMPONENT ${EXAMPLE_COMPONENTS})
  ADD_DEPENDENCIES(dynamic_loader-t ${EXAMPLE_COMPONENT})
ENDFOREACH()
