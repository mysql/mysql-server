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

# Bug#27303447 - WL#9809 declared function "signal_event()" in
# "nt_servc.cc" and defined the same in server library. But "nt_servc.cc"
# is a general file also used in cluster libraries. So, when building cluster,
# compiler gives linking error for cluster libraries as the definition for the
# symbol "signal_event()" is not found in cluster libraries. Untill the issue
# with "nt_servc.cc" gets fixed as a temporary solution have forked the old code
# of "nt_servc.cc" into "ndb_nt_servc.cc"

# SET(EXTRA_SRC ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
  SET(EXTRA_SRC ndb_nt_servc.cc)
ENDIF(WIN32)
ADD_CONVENIENCE_LIBRARY(ndbportlib
            NdbCondition.c NdbMutex.c ndb_socket.cpp
            NdbEnv.c NdbThread.cpp NdbHost.c NdbTCP.cpp
            NdbMem.c NdbConfig.c NdbTick.cpp NdbDir.cpp
            ndb_daemon.cc ${EXTRA_SRC}
            NdbNuma.cpp NdbMutex_DeadlockDetector.cpp
            NdbGetRUsage.cpp NdbLockCpuUtil.c)
TARGET_LINK_LIBRARIES(ndbportlib mysys ${LIBSOCKET})

NDB_ADD_TEST(NdbDir-t NdbDir.cpp ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/util/basestring_vsnprintf.c
             LIBS ndbportlib)

NDB_ADD_TEST(NdbGetInAddr-t NdbTCP.cpp LIBS ${LIBSOCKET} ${LIBNSL})
