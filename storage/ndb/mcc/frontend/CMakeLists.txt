# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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

if(NOT DEFAULT_MYSQL_HOME)
  # If not building as part of Cluster we don't have the CREATE_ZIP
  # macro available, so we just install dojo normally
  install(DIRECTORY "."
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools)
  return()
endif()

INCLUDE("${NDB_SOURCE_DIR}/config/type_mcc_utils.cmake")

SET(DOJO_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/dojo.zip") 
CREATE_ZIP(${DOJO_ARCHIVE} dojo)

IF(ZIP_EXECUTABLE)
  ADD_CUSTOM_TARGET(dojozip ALL DEPENDS "${DOJO_ARCHIVE}")
  install(DIRECTORY "." 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
    COMPONENT ClusterTools
    PATTERN ".hg" EXCLUDE
    PATTERN "dojo" EXCLUDE)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dojo.zip" 
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
	COMPONENT ClusterTools)
ELSE()
  MESSAGE(STATUS ">>>>>>>>>>>>> NO ZIP FOUND")
  install(DIRECTORY "."
    DESTINATION "${MCC_INSTALL_SUBDIR}/frontend" 
	COMPONENT ClusterTools)
ENDIF(ZIP_EXECUTABLE)


