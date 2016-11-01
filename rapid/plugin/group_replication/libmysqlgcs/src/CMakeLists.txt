# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#
# This is the root CMake to build the "src" directory of MySQL-GCS
#
# Is sets a property that states, in a list, the dynamic libraries
# that one needs to link externally if linking against the MySQL-GCS
# static library: LIBRARIES_TO_LINK_MYSQL_GCS
#
# Usage example:
# add_subdirectory(mysql_gcs)
# GET_PROPERTY(MY_VAR GLOBAL PROPERTY LIBRARIES_TO_LINK_MYSQL_GCS)
#

SET(XCOM_SUBTREE_LOCATION ${PROJECT_SOURCE_DIR}/src/bindings/xcom/xcom)
SET(XCOM_SUBTREE_BINARY_LOCATION ${PROJECT_BINARY_DIR}/src/bindings/xcom/xcom)

# Configure the interface
ADD_SUBDIRECTORY(interface)

ADD_SUBDIRECTORY(bindings/xcom)

ADD_DEFINITIONS(-DYASSL_MYSQL_COMPATIBLE)
INCLUDE_DIRECTORIES(${XCOM_SUBTREE_LOCATION}
                    ${XCOM_SUBTREE_BINARY_LOCATION}/xdr_gen
                    ${SSL_INCLUDE_DIRS})

IF(WIN32)
  INCLUDE_DIRECTORIES(${XCOM_SUBTREE_LOCATION}/windeps/include
                      ${XCOM_SUBTREE_LOCATION}/windeps/sunrpc
                      ${XCOM_SUBTREE_LOCATION}/windeps/sunrpc/rpc)
ENDIF()

SET(LIBRARIES_TO_MERGE
    "mysql_gcs_factory_static;mysql_gcs_interface;mysql_gcs_xcom")
SET(BINDINGS_BUILT mysql_gcs_xcom)

###
### configure the static lib
###
ADD_LIBRARY(mysql_gcs_factory_static STATIC gcs_interface_factory.cc)
ADD_DEPENDENCIES(mysql_gcs_factory_static mysql_gcs_xcom)
MERGE_STATIC_LIBS(mysql_gcs mysql_gcs "${LIBRARIES_TO_MERGE}")

IF(OSLIBS)
  SET_PROPERTY(GLOBAL PROPERTY LIBRARIES_TO_LINK_MYSQL_GCS "${OSLIBS}")
ENDIF()
ADD_DEPENDENCIES(mysql_gcs mysql_gcs_xcom)

IF(NOT SKIP_INSTALL_MYSQL_GCS)
  INSTALL(TARGETS mysql_gcs LIBRARY DESTINATION ${INSTALL_LIB}
          ARCHIVE DESTINATION ${INSTALL_LIB} COMPONENT ${COMP_BIN})
ENDIF()

###
### configure the shared lib (if needed)
###
IF (NOT SKIP_GCS_SHARED_LIB)
  ADD_LIBRARY(mysql_gcs_shared SHARED gcs_interface_factory.cc)
  ADD_DEPENDENCIES(mysql_gcs_shared mysql_gcs_xcom)
  TARGET_LINK_LIBRARIES(mysql_gcs_shared mysql_gcs ${BINDINGS_BUILT})
  SET_TARGET_PROPERTIES(mysql_gcs_shared PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
  IF (NOT SKIP_INSTALL_MYSQL_GCS)
    INSTALL(TARGETS mysql_gcs_shared DESTINATION ${INSTALL_LIB}
            COMPONENT ${COMP_BIN})
  ENDIF()
ENDIF()
