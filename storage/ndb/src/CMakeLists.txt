# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(libutils) # MERGE_STATIC_LIBS

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(mgmapi)
ADD_SUBDIRECTORY(ndbapi)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(mgmclient)
ADD_SUBDIRECTORY(mgmsrv)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/include)

SET(NDBCLIENT_LIBS
                mysys dbug strings
                ndbapi
                ndbtransport
                ndbtrace
                ndbsignaldata
                ndbmgmapi
                ndbmgmcommon
                ndblogger
                ndbportlib
                ndbgeneral
                ndbconf)

MERGE_LIBRARIES(ndbclient STATIC ${NDBCLIENT_LIBS} COMPONENT Development)

#
# Do a shared libd
#
SET(NDBCLIENT_SO_LIBS ${NDBCLIENT_LIBS})
SET(EXTRA_SRC dummy.cpp)

IF(HAVE_JDK AND HAVE_JAVA)

  ADD_SUBDIRECTORY(ndbjtie)

  SET(NDBCLIENT_SO_LIBS ${NDBCLIENT_SO_LIBS} mysqlutils ndbjtie)

  INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
                      ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/mysql
                     )
  # JNI needs a DLL
  SET(EXTRA_SRC ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/ndbjtie_lib.cpp)
ENDIF()

ADD_LIBRARY(ndbclient_so SHARED ${EXTRA_SRC})
TARGET_LINK_LIBRARIES(ndbclient_so ${NDBCLIENT_SO_LIBS})
SET_TARGET_PROPERTIES(ndbclient_so PROPERTIES OUTPUT_NAME "ndbclient")
