# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

IF(DEFAULT_MYSQL_HOME)
  # Only include if running cmake for all of cluster
  INCLUDE("${NDB_SOURCE_DIR}/config/type_mcc_utils.cmake")
ENDIF()

IF(WIN32)
  # Always create the zip on Win, since the modified msi install won't 
  # trigger a build if the zip does not exist. 
  SET(DOJO_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/dojo.zip") 
  MESSAGE(STATUS "Creating ${DOJO_ARCHIVE}")
  CREATE_ZIP(${DOJO_ARCHIVE} dojo)
  IF(NOT ZIP_EXECUTABLE)
    MESSAGE(WARNING "No zip execuatble available")
  ENDIF()
ENDIF()

IF(DEFAULT_MYSQL_HOME AND ZIP_EXECUTABLE AND MCC_BUILDING_MSI)
  # Only install the zip when building an msi
  MESSAGE(STATUS "Will install ${DOJO_ARCHIVE}")
  INSTALL(DIRECTORY "." 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools
    PATTERN "dojo" EXCLUDE)
  INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/dojo.zip" 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools)
ELSE()
  MESSAGE(STATUS "Will install the dojo directory")
  INSTALL(DIRECTORY "."
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools
    PATTERN "dojo.zip" EXCLUDE)
ENDIF()
