# Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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

# destination status library

ADD_LIBRARY(destination_status SHARED
  unreachable_destinations_quarantine.cc
  destination_status_component.cc
  )
TARGET_LINK_LIBRARIES(destination_status
  PUBLIC
  harness_stdx
  router_lib
  )

TARGET_INCLUDE_DIRECTORIES(destination_status PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
GENERATE_EXPORT_HEADER(destination_status
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/destination_status_export.h
  )

SET_TARGET_PROPERTIES(destination_status PROPERTIES
  OUTPUT_NAME "mysqlrouter_destination_status"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

#ADD_INSTALL_RPATH_FOR_OPENSSL(destination_status)
#SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(destination_status)

INSTALL(TARGETS destination_status
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

# destination status plugin

ADD_HARNESS_PLUGIN(destination_status_plugin
  OUTPUT_NAME "destination_status"
  SOURCES
  destination_status_plugin.cc
  REQUIRES
  router_lib
  harness_stdx
  destination_status
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_INCLUDE_DIRECTORIES(destination_status_plugin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )

GENERATE_EXPORT_HEADER(destination_status_plugin
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/destination_status_plugin_export.h
  )
