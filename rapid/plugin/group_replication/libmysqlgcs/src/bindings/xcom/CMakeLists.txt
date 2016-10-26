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

IF(SKIP_GCS_SHARED_LIB)
  SET(NO_SHARED_XCOM_LIB ON CACHE INTERNAL "Disables shared libraries")
ENDIF()
PROJECT("mysql-gcs-xcom")

INCLUDE(xcom/cmake/platform.cmake)
ADD_SUBDIRECTORY(xcom)

#
# Set include directories
#
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/xcom)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/xcom/xdr_gen)
INCLUDE_DIRECTORIES(${GCS_LZ4_INCLUDE_DIR})
IF(WIN32)
  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/xcom/windeps/include
                      ${PROJECT_SOURCE_DIR}/xcom/windeps/sunrpc
                      ${PROJECT_SOURCE_DIR}/xcom/windeps/sunrpc/rpc)
ENDIF()
INCLUDE_DIRECTORIES(${SSL_INCLUDE_DIRS})

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DYASSL_MYSQL_COMPATIBLE")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DYASSL_MYSQL_COMPATIBLE")
SET(GCS_XCOM_BINDING_SRCS
    gcs_xcom_communication_interface.cc
    gcs_xcom_communication_interface.h
    gcs_xcom_control_interface.cc
    gcs_xcom_control_interface.h
    gcs_xcom_group_member_information.cc
    gcs_xcom_group_member_information.h
    gcs_xcom_interface.cc
    gcs_xcom_interface.h
    gcs_xcom_state_exchange.cc
    gcs_xcom_state_exchange.h
    gcs_xcom_statistics_interface.cc
    gcs_xcom_statistics_interface.h
    gcs_xcom_group_management.cc
    gcs_xcom_group_management.h
    gcs_xcom_utils.cc
    gcs_xcom_utils.h
    gcs_xcom_view_identifier.cc
    gcs_xcom_view_identifier.h
    gcs_message_stages.cc
    gcs_internal_message.cc
    gcs_message_stage_lz4.cc
    gcs_xcom_networking.cc
    gcs_xcom_notification.cc
    gcs_xcom_notification.h)

ADD_LIBRARY(mysql_gcs_xcom_static STATIC ${GCS_XCOM_BINDING_SRCS})

# add a dependency for the mysql_gcs_xcom_static for LZ4 lib
ADD_DEPENDENCIES(mysql_gcs_xcom_static xcom)

TARGET_LINK_LIBRARIES(mysql_gcs_xcom_static
                      mysql_gcs_interface
                      xcom
                      ${GCS_LZ4_LIBRARY})

IF (NOT SKIP_GCS_SHARED_LIB)
  ADD_LIBRARY(mysql_gcs_xcom_shared SHARED ${GCS_XCOM_BINDING_SRCS})
  ADD_DEPENDENCIES(mysql_gcs_xcom_shared mysql_gcs_xcom_static)
  TARGET_LINK_LIBRARIES(mysql_gcs_xcom_shared
                        mysql_gcs_xcom_static
                        mysql_gcs_interface
                        xcom
                        ${GCS_LZ4_LIBRARY})
  SET_TARGET_PROPERTIES(mysql_gcs_xcom_shared PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
  IF (NOT SKIP_INSTALL_MYSQL_GCS)
    INSTALL(TARGETS mysql_gcs_xcom_shared DESTINATION ${INSTALL_LIB}
            COMPONENT ${COMP_BIN})
  ENDIF()
ENDIF()

MERGE_STATIC_LIBS(mysql_gcs_xcom mysql_gcs_xcom "mysql_gcs_xcom_static;xcom;${SSL_LIBRARIES};${GCS_LZ4_LIBRARY}")
