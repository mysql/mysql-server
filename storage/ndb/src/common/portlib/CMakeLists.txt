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

ADD_LIBRARY(ndbportlib STATIC
            NdbCondition.c NdbMutex.c NdbTick.c
            NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp
            NdbDaemon.c
            NdbMem.c NdbConfig.c
            ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc
            )

#            win32/NdbCondition.c win32/NdbDaemon.c win32/NdbEnv.c
#            win32/NdbHost.c win32/NdbMem.c  win32/NdbMutex.c
#            win32/NdbSleep.c win32/NdbTCP.c win32/NdbThread.c
#            )

#ADD_EXECUTABLE(memtest memtest.c)
#TARGET_LINK_LIBRARIES(memtest ndbportlib wsock32)

#ADD_EXECUTABLE(PortLibTest NdbPortLibTest.cpp)
#TARGET_LINK_LIBRARIES(PortLibTest ndbportlib wsock32)

#ADD_EXECUTABLE(munmaptest munmaptest.cpp)
#TARGET_LINK_LIBRARIES(munmaptest ndbportlib wsock32)

#ADD_DEFINITIONS(-DNDB_DAEMON_TEST)
#ADD_EXECUTABLE(ndbdaemon_test NdbDaemon.c)

