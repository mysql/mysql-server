# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

INCLUDE(GenerateExportHeader)

ADD_LIBRARY(io_component
  SHARED
  io_backend.cc
  io_component.cc
  io_thread.cc
  thread_affinity.cc
  )
TARGET_INCLUDE_DIRECTORIES(io_component
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../include/
)
TARGET_LINK_LIBRARIES(io_component PUBLIC harness_net_ts;harness-library)


SET_TARGET_PROPERTIES(io_component PROPERTIES
  OUTPUT_NAME "mysqlrouter_io_component"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  )

INSTALL(TARGETS io_component
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

GENERATE_EXPORT_HEADER(io_component
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/io_component_export.h
  )

ADD_HARNESS_PLUGIN(io
  SOURCES
  io_plugin.cc
  REQUIRES io_component;harness-library;router_lib
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_INCLUDE_DIRECTORIES(io PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
)
GENERATE_EXPORT_HEADER(io
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/io_export.h)



ADD_INSTALL_RPATH_FOR_OPENSSL(io_component)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(io_component)
