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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)

ADD_EXECUTABLE(hugoFill hugoFill.cpp)

ADD_EXECUTABLE(hugoLoad hugoLoad.cpp)
ADD_EXECUTABLE(hugoLockRecords hugoLockRecords.cpp)
ADD_EXECUTABLE(hugoPkDelete hugoPkDelete.cpp)
ADD_EXECUTABLE(hugoPkRead hugoPkRead.cpp)

ADD_EXECUTABLE(hugoPkReadRecord hugoPkReadRecord.cpp)
ADD_EXECUTABLE(hugoPkUpdate hugoPkUpdate.cpp)
ADD_EXECUTABLE(hugoScanRead hugoScanRead.cpp)
ADD_EXECUTABLE(hugoScanUpdate hugoScanUpdate.cpp)
ADD_EXECUTABLE(restart restart.cpp)
ADD_EXECUTABLE(verify_index verify_index.cpp)
ADD_EXECUTABLE(copy_tab copy_tab.cpp)
ADD_EXECUTABLE(create_index create_index.cpp)
ADD_EXECUTABLE(hugoJoin hugoJoin.cpp)
ADD_EXECUTABLE(ndb_connect connect.cpp)

SET(BINS
    hugoFill
    hugoLoad
    hugoLockRecords
    hugoPkDelete
    hugoPkRead
    hugoPkReadRecord
    hugoPkUpdate
    hugoScanRead
    hugoScanUpdate
    restart
    verify_index
    copy_tab
    create_index
    hugoJoin
    ndb_connect)

FOREACH(B ${BINS})
  TARGET_LINK_LIBRARIES(${B} ndbNDBT)
  TARGET_LINK_LIBRARIES(${B} ndbclient_so)
ENDFOREACH()

INSTALL(TARGETS ${BINS} DESTINATION bin)

ADD_EXECUTABLE(ndb_cpcc cpcc.cpp)
TARGET_LINK_LIBRARIES(ndb_cpcc ndbNDBT)
TARGET_LINK_LIBRARIES(ndb_cpcc ndbclient_static)
INSTALL(TARGETS ndb_cpcc DESTINATION bin)

