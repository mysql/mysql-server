# Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

ROUTER_ADD_SHARED_LIBRARY(io_component
  io_backend.cc
  io_component.cc
  io_thread.cc
  thread_affinity.cc

  OUTPUT_NAME "mysqlrouter_io_component"

  LINK_LIBRARIES
  PUBLIC
  harness_net_ts
  harness-library
  )

ADD_HARNESS_PLUGIN(io
  SOURCES
  io_plugin.cc
  REQUIRES
  io_component
  harness-library
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(io
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    ${CMAKE_CURRENT_BINARY_DIR}/../include/
    )
GENERATE_EXPORT_HEADER(io
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/io_export.h)
