# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi)

IF(WIN32)
  SET(EXTRA_SRC ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
ENDIF(WIN32)
ADD_CONVENIENCE_LIBRARY(ndbportlib
            NdbCondition.cpp NdbMutex.cpp ndb_socket.cpp
            NdbEnv.cpp NdbThread.cpp NdbHost.cpp NdbTCP.cpp
            NdbMem.cpp NdbConfig.cpp NdbTick.cpp NdbDir.cpp
            ndb_daemon.cpp ${EXTRA_SRC}
            NdbNuma.cpp NdbMutex_DeadlockDetector.cpp
            NdbGetRUsage.cpp NdbLockCpuUtil.cpp)
TARGET_LINK_LIBRARIES(ndbportlib mysys ${LIBSOCKET})

NDB_ADD_TEST(NdbDir-t NdbDir.cpp ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/util/basestring_vsnprintf.cpp
             LIBS ndbportlib)

NDB_ADD_TEST(NdbGetInAddr-t NdbTCP.cpp LIBS ${LIBSOCKET} ${LIBNSL})
