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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_util.cmake)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
                   )

IF(WIN32)
  SET(EXTRA_SRC ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
ENDIF(WIN32)
ADD_CONVENIENCE_LIBRARY(ndbportlib
            NdbCondition.c NdbMutex.c ndb_socket.cpp
            NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp
            NdbMem.c NdbConfig.c NdbTick.c NdbDir.cpp
            ndb_daemon.cc ${EXTRA_SRC}
            NdbNuma.cpp)
TARGET_LINK_LIBRARIES(ndbportlib mysys ${LIBSOCKET})

ADD_EXECUTABLE(NdbDir-t
  NdbDir.cpp
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/util/basestring_vsnprintf.c)
SET_TARGET_PROPERTIES(NdbDir-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_NDBDIR")
TARGET_LINK_LIBRARIES(NdbDir-t ndbportlib)

ADD_EXECUTABLE(NdbGetInAddr-t NdbTCP.cpp)
SET_TARGET_PROPERTIES(NdbGetInAddr-t
                      PROPERTIES COMPILE_FLAGS "-DTEST_NDBGETINADDR")
TARGET_LINK_LIBRARIES(NdbGetInAddr-t ${LIBSOCKET} ${LIBNSL})



