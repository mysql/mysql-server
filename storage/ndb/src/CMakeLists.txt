# Copyright (C) 2007 MySQL AB
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


ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(mgmapi)
ADD_SUBDIRECTORY(ndbapi)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(mgmclient)
ADD_SUBDIRECTORY(mgmsrv)
ADD_SUBDIRECTORY(ndbjtie)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include
                    ${CMAKE_SOURCE_DIR}/include)

SET(NDBCLIENT_LIBS
                mysys dbug strings
                ndbapi
                ndbtransport
                ndbtrace
                ndbsignaldata
                ndbmgmapi
                ndbmgmsrv
                ndblogger
                ndbportlib
                ndbgeneral
                ndbconf)

IF(HAVE_JDK AND HAVE_JAVA)

  SET(NDBCLIENT_LIBS ${NDBCLIENT_LIBS} mysqlutils ndbjtie)

  INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
                      ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils
                      ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/mysql
                     )
  # JNI needs a DLL
  ADD_LIBRARY(ndbclient 
    SHARED ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/ndbjtie_lib.cpp)
  SET_TARGET_PROPERTIES(ndbclient PROPERTIES OUTPUT_NAME "ndbclient")
  TARGET_LINK_LIBRARIES(ndbclient ${NDBCLIENT_LIBS})

ELSE()
  MERGE_STATIC_LIBS(ndbclient ndbclient "${NDBCLIENT_LIBS}")
ENDIF()