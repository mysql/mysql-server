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

IF(WIN32 AND DEFAULT_MYSQL_HOME)
  # Only include if running cmake for all of cluster
  INCLUDE("${NDB_SOURCE_DIR}/config/type_mcc_utils.cmake")
  MESSAGE(STATUS "Will build/install ${DOJO_ARCHIVE}")
  SET(DOJO_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/dojo.zip") 
  ADD_ZIP_COMMAND(${DOJO_ARCHIVE} dojo)
  ADD_CUSTOM_TARGET(dojozip ALL DEPENDS ${DOJO_ARCHIVE})    
ENDIF()

IF(DEFAULT_MYSQL_HOME AND MCC_BUILDING_MSI)
  # Install the zip when building an msi
  INSTALL(DIRECTORY "." 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools
    PATTERN "dojo" EXCLUDE)
  INSTALL(FILES ${DOJO_ARCHIVE} 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools)
ELSE()
  #MESSAGE(STATUS "Will install the dojo directory")
  INSTALL(DIRECTORY "."
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools
    PATTERN "dojo.zip" EXCLUDE)
ENDIF()
