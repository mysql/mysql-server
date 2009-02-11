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


INCLUDE(${CMAKE_BINARY_DIR}/storage/ndb/config/type_util.cmake)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
                   )

IF(WIN32)
 SET(EXTRA_SRC win32/NdbTick.c
     win32/NdbDaemon.c
 )
 SET(EXTRA_LIB wsock32 ws2_32)
ENDIF(WIN32)
ADD_LIBRARY(ndbportlib STATIC
            NdbCondition.c NdbMutex.c
            NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp
            NdbMem.c NdbConfig.c NdbTick.c
            ${EXTRA_SRC}
)
TARGET_LINK_LIBRARIES(ndbportlib mysys ${EXTRA_LIB})

#ADD_EXECUTABLE(memtest memtest.c)
#TARGET_LINK_LIBRARIES(memtest ndbportlib wsock32)

#ADD_EXECUTABLE(PortLibTest NdbPortLibTest.cpp)
#TARGET_LINK_LIBRARIES(PortLibTest ndbportlib wsock32)

#ADD_EXECUTABLE(munmaptest munmaptest.cpp)
#TARGET_LINK_LIBRARIES(munmaptest ndbportlib wsock32)

#ADD_DEFINITIONS(-DNDB_DAEMON_TEST)
#ADD_EXECUTABLE(ndbdaemon_test NdbDaemon.c)

