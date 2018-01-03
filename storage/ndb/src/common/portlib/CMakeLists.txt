# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_util.cmake)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi)

IF(WIN32)
  SET(EXTRA_SRC ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
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
