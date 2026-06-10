# Copyright (c) 2021, 2026, Oracle and/or its affiliates.
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

INCLUDE(GenerateExportHeader)

SET(jit_executor_plugin_SOURCE
  database/column.cc
  database/query_attributes.cc
  database/result.cc
  database/row_copy.cc
  database/row.cc
  database/session.cc

  languages/polyglot_common_context.cc
  languages/polyglot_garbage_collector.cc
  languages/polyglot_javascript.cc
  languages/polyglot_language.cc

  native_wrappers/polyglot_array_wrapper.cc
  native_wrappers/polyglot_collectable.cc
  native_wrappers/polyglot_file_system_wrapper.cc
  native_wrappers/polyglot_iterator_wrapper.cc
  native_wrappers/polyglot_map_wrapper.cc
  native_wrappers/polyglot_object_bridge.cc
  native_wrappers/polyglot_object_wrapper.cc
  native_wrappers/polyglot_seekable_channel_wrapper.cc

  objects/polyglot_column.cc
  objects/polyglot_date.cc
  objects/polyglot_result.cc
  objects/polyglot_row.cc
  objects/polyglot_session.cc

  polyglot_wrappers/types_polyglot.cc

  utils/polyglot_error.cc
  utils/polyglot_scope.cc
  utils/polyglot_store.cc
  utils/polyglot_utils.cc
  utils/profiling.cc
  utils/utils_encoding.cc
  utils/utils_json.cc
  utils/utils_path.cc
  utils/utils_string.cc
  utils/utils_system.cc

  jit_executor_common_context.cc
  jit_executor_plugin.cc
  jit_executor_javascript_context.cc
  jit_executor_debug_context_handle.cc
  jit_executor_service_handlers.cc
  jit_executor_javascript.cc
  jit_executor_component.cc
  jit_executor_type_conversion.cc
  jit_executor_context_pool.cc
  jit_executor_value.cc
  jit_executor_db_interface.cc
  )

IF(UNIX)
    LIST(APPEND jit_executor_plugin_SOURCE utils/utils_path_unix.cc)
ELSEIF(WIN32)
    LIST(APPEND jit_executor_plugin_SOURCE utils/utils_path_win32.cc)
ENDIF()

FIND_LIBRARY(JIT_EXECUTOR_LIBRARY
  NAMES jitexecutor
  HINTS "${MRS_JIT_EXECUTOR_LIB}"
  NO_DEFAULT_PATH
  )

MESSAGE(STATUS "Jit Executor Library: ${JIT_EXECUTOR_LIBRARY}")

ROUTER_COPY_CUSTOM_SHARED_LIBRARY("${JIT_EXECUTOR_LIBRARY}"
  EXT_JIT_LIBRARY
  ext_jit_target
  )

ROUTER_COPY_CUSTOM_DLL("${JIT_EXECUTOR_LIBRARY}"
  EXT_JIT_LIBRARY
  ext_jit_target
  )

ROUTER_COPY_CUSTOM_DYLIB("${JIT_EXECUTOR_LIBRARY}"
  EXT_JIT_LIBRARY
  ext_jit_target
  )

ADD_HARNESS_PLUGIN(jit_executor_plugin
  OUTPUT_NAME "jit_executor"
  SOURCES ${jit_executor_plugin_SOURCE}
  REQUIRES harness_stdx router_mysql "${EXT_JIT_LIBRARY}"
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(jit_executor_plugin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${PROJECT_SOURCE_DIR}/src/router/include/
  ${PROJECT_SOURCE_DIR}/src/mysql_rest_service/include/
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  ${MRS_JIT_EXECUTOR_LIB}
  )

ADD_DEPENDENCIES(jit_executor_plugin ${ext_jit_target})

GENERATE_EXPORT_HEADER(jit_executor_plugin
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/jit_executor_plugin_export.h)
