# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA


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

IF(WIN32)
  SET(EXTRA_SRC ${EXTRA_SRC} ndb_file_win32.cpp)
ELSE()
  SET(EXTRA_SRC ${EXTRA_SRC} ndb_file_posix.cpp)
ENDIF()

IF(CMAKE_CXX_FLAGS MATCHES " -DNDB_MUTEX_DEADLOCK_DETECTOR")
  SET(EXTRA_SRC ${EXTRA_SRC} NdbMutex_DeadlockDetector.cpp)
ENDIF()

ADD_CONVENIENCE_LIBRARY(ndbportlib
  NdbCondition.cpp
  NdbMutex.cpp
  NdbEnv.cpp
  NdbThread.cpp
  NdbHost.cpp
  NdbHW.cpp
  NdbTCP.cpp
  NdbMem.cpp
  NdbConfig.cpp
  NdbTick.cpp
  NdbDir.cpp
  ndb_daemon.cpp
  ndb_file_common.cpp
  ndb_password.cpp
  ndb_sockaddr.cpp
  ndb_socket_poller.cpp
  ndb_socketpair.cpp
  ndb_stacktrace.cpp
  ${EXTRA_SRC}
  NdbNuma.cpp
  NdbGetRUsage.cpp
  NdbLockCpuUtil.cpp
  NdbSpin.cpp
  LINK_LIBRARIES mysys ${LIBSOCKET}
  )

IF(WIN32_CLANG)
  ADD_COMPILE_FLAGS(ndb_daemon.cpp COMPILE_FLAGS " -Wno-microsoft-cast")
ENDIF()

NDB_ADD_TEST(NdbHW-t NdbHW.cpp LIBS ndbmgmapi ndbgeneral ndbportlib)

NDB_ADD_TEST(NdbDir-t NdbDir.cpp LIBS ndbmgmapi ndbgeneral ndbportlib)

NDB_ADD_TEST(NdbGetInAddr-t NdbTCP.cpp LIBS ndbgeneral ${LIBSOCKET} ${LIBNSL})
NDB_ADD_TEST(NdbProcess-t NdbProcess.cpp LIBS ndbmgmapi ndbgeneral ndbportlib)
