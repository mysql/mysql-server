# Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.
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
IF(NOT WIN32)
  ADD_DEFINITIONS(-DDEFAULT_PREFIX=\"${CMAKE_INSTALL_PREFIX}\")
ENDIF()
ADD_EXECUTABLE(atrt main.cpp setup.cpp files.cpp db.cpp command.cpp)
TARGET_LINK_LIBRARIES(atrt ndbNDBT ndbclient_static mysqlclient)

FILE(GLOB testcase_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*-tests.txt")
ADD_TEST(NAME check_testcase_files
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         COMMAND atrt --check-testcase-files ${testcase_files})

ADD_CUSTOM_COMMAND(OUTPUT check_testcase_files.passed
                   COMMAND atrt
                           --check-testcase-files ${testcase_files}
                           > ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.output
                   COMMAND echo See output in ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.output
                           > ${CMAKE_CURRENT_BINARY_DIR}/check_testcase_files.passed
                   MAIN_DEPENDENCY atrt
                   DEPENDS ${testcase_files}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

ADD_CUSTOM_TARGET(check_testcase_files ALL
                  SOURCES check_testcase_files.passed)

INSTALL(TARGETS atrt DESTINATION mysql-test/ndb)
INSTALL(PROGRAMS   atrt-testBackup atrt-mysql-test-run
        DESTINATION bin)
INSTALL(PROGRAMS atrt-analyze-result.sh atrt-backtrace.sh atrt-gather-result.sh
                 atrt-setup.sh atrt-mysqltest autotest-boot.sh autotest-run.sh
                 check-tests.sh make-config.sh make-html-reports.sh
                 make-index.sh ndb-autotest.sh upgrade-boot.sh
        DESTINATION mysql-test/ndb)
INSTALL(FILES   daily-basic-tests.txt daily-devel-tests.txt 16node-tests.txt
                conf-ndbmaster.cnf conf-fimafeng08.cnf conf-fimafeng09.cnf 
                conf-dl145a.cnf conf-loki27.cnf conf-ndb07.cnf conf-repl.cnf
                conf-techra29.cnf conf-test.cnf conf-tyr64.cnf conf-upgrade.cnf
                test-tests.txt upgrade-tests.txt release-bigmem-tests.txt
                conf-tyr13.cnf
		conf-blade08.cnf conf-mikael1.cnf
        DESTINATION mysql-test/ndb)
