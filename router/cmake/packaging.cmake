# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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


IF(NOT WIN32)
  SET(CPACK_ROUTER_PACKAGE_NAME "mysql-router")
ELSE()
  SET(CPACK_ROUTER_PACKAGE_NAME "MySQL Router")
ENDIF()

SET(CPACK_ROUTER_PACKAGE_VERSION ${PROJECT_VERSION_TEXT})
SET(CPACK_ROUTER__PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
SET(CPACK_ROUTER__PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
SET(CPACK_ROUTER__PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

SET(EXTRA_NAME_SUFFIX "" CACHE STRING "Extra text in package name")

IF(WIN32)
  SET(CPACK_SYSTEM_NAME "winx64")
  SET(CPACK_ROUTER_PACKAGE_FILE_NAME "mysql-router${EXTRA_NAME_SUFFIX}-${CPACK_ROUTER_PACKAGE_VERSION}${PROJECT_PACKAGE_EXTRAS}-${CPACK_SYSTEM_NAME}")
ENDIF()

#
# Source Distribution
#
SET(CPACK_ROUTER_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
SET(CPACK_ROUTER_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
SET(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PROJECT_PACKAGE_EXTRAS}")

# We ignore all files in the root of the repository and then
# exclude from the list which we want to keep.
FILE(GLOB cpack_source_ignore_files "${PROJECT_SOURCE_DIR}/*")
SET(src_dir ${PROJECT_SOURCE_DIR})
SET(source_include
  "${src_dir}/cmake"
  "${src_dir}/include"
  "${src_dir}/doc"
  "${src_dir}/ext"
  "${src_dir}/src"
  "${src_dir}/tests"
  "${src_dir}/tools"
  "${src_dir}/packaging"
  "${src_dir}/CMakeLists.txt"
  "${src_dir}/config.h.in"
  "${src_dir}/README.txt"
  "${src_dir}/License.txt")
LIST(REMOVE_ITEM cpack_source_ignore_files ${source_include})
LIST(APPEND cpack_source_ignore_files "${src_dir}/harness/.gitignore")

# We need to escape the dots
STRING(REPLACE "." "\\\\." cpack_source_ignore_files "${cpack_source_ignore_files}")

SET(CPACK_SOURCE_IGNORE_FILES "${cpack_source_ignore_files}")

INCLUDE(CPack)

#
# RPM-based
#
# FIXME: wrong folder structure (is it needed?)
#IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#  ADD_SUBDIRECTORY("${PROJECT_SOURCE_DIR}/packaging/rpm-oel")
#ENDIF()

#
# MSI for Windows
#
IF(WIN32)
  ADD_SUBDIRECTORY("${CMAKE_SOURCE_DIR}/packaging/WiX/router" packaging)
ENDIF()
