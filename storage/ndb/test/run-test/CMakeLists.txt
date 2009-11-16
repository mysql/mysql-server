# Copyright (C) 2009 Sun Microsystems
#  All rights reserved. Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapi.cmake)
INCLUDE_DIRECTORIES(
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/storage/ndb/include
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/logger
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/util
 ${CMAKE_SOURCE_DIR}/storage/ndb/test/include
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
 ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
 ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmcommon
 ${CMAKE_SOURCE_DIR}/sql
)
LINK_LIBRARIES(ndbNDBT ndbclient dbug mysys strings mysqlclient
                       ndbdbutil ${NDB_SCI_LIBS} )
ADD_EXECUTABLE(atrt main.cpp setup.cpp files.cpp db.cpp command.cpp)

INSTALL(TARGETS atrt DESTINATION bin)
INSTALL(FILES
               atrt-testBackup
               atrt-analyze-result.sh
               atrt-backtrace.sh
               atrt-gather-result.sh
               atrt-setup.sh
               autotest-boot.sh
               autotest-run.sh
               check-tests.sh
               make-config.sh
               make-html-reports.sh
               make-index.sh
               ndb-autotest.sh
               upgrade-boot.sh
         DESTINATION bin)
