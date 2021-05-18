# Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.
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
include(ExternalProject)
ExternalProject_Add(
  fastbit-2.0.3
  URL https://codeforge.lbl.gov/frs/download.php/file/426/fastbit-2.0.3.tar.gz
  CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/fastbit-2.0.3/configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/
  PATCH_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/patch_fastbit.sh ${CMAKE_CURRENT_SOURCE_DIR}
  BUILD_COMMAND make -j4 && make install
  BUILD_IN_SOURCE 1
)

SET(WARP_PLUGIN_STATIC  "warp")
SET(WARP_PLUGIN_MANDATORY TRUE)

SET(WARP_SOURCES  ha_warp.cc ha_warp.h)
SET(FASTBIT_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfastbit.so  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfastbit.so.1  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfastbit.so.1.0.0)
SET(COMPILE_FLAGS "-O3")

MYSQL_ADD_PLUGIN(warp ${WARP_SOURCES} STORAGE_ENGINE MANDATORY)
add_library(fastbit SHARED IMPORTED)
set_property(TARGET fastbit PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfastbit.so)
add_dependencies(warp fastbit fastbit-2.0.3)
target_link_libraries(warp fastbit)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
install(FILES ${FASTBIT_LIBS} DESTINATION ${PLUGINDIR})
