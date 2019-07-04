# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

MYSQL_ADD_EXECUTABLE(mysqlrouter_plugin_info
  library_file.cc
  main.cc
  plugin_info_app.cc
  plugin.cc
  COMPONENT Router)

TARGET_INCLUDE_DIRECTORIES(
  mysqlrouter_plugin_info
  PRIVATE
  ${CMAKE_BINARY_DIR}/include
  )

ADD_DEPENDENCIES(${MYSQL_ROUTER_BUILD_ALL_TARGET} mysqlrouter_plugin_info)

TARGET_LINK_LIBRARIES(mysqlrouter_plugin_info
  harness-library
  mysys
  ${CMAKE_DL_LIBS})

SET_TARGET_PROPERTIES(mysqlrouter_plugin_info PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
)
