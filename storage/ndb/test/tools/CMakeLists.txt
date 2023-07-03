# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/test/include)

NDB_ADD_EXECUTABLE(hugoFill hugoFill.cpp NDBTEST NDBCLIENT)

NDB_ADD_EXECUTABLE(hugoLoad hugoLoad.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoLockRecords hugoLockRecords.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoPkDelete hugoPkDelete.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoPkRead hugoPkRead.cpp NDBTEST NDBCLIENT)

NDB_ADD_EXECUTABLE(hugoPkReadRecord hugoPkReadRecord.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoPkUpdate hugoPkUpdate.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoScanRead hugoScanRead.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoScanUpdate hugoScanUpdate.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(restart restart.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(verify_index verify_index.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(copy_tab copy_tab.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(create_index create_index.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(hugoJoin hugoJoin.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(ndb_connect connect.cpp NDBTEST NDBCLIENT)

NDB_ADD_EXECUTABLE(create_fk create_fk.cpp NDBTEST NDBCLIENT)
NDB_ADD_EXECUTABLE(drop_fk drop_fk.cpp NDBTEST NDBCLIENT)

NDB_ADD_EXECUTABLE(ndb_cpcc cpcc.cpp)
NDB_ADD_EXECUTABLE(statUtils statUtils.cpp NDBTEST NDBCLIENT)

TARGET_LINK_LIBRARIES(ndb_cpcc ndbNDBT ndbclient_static)

NDB_ADD_EXECUTABLE(test_cpcd test_cpcd.cpp NDBTEST NDBCLIENT SKIP_INSTALL)

IF(NOT WIN32)
  NDB_ADD_EXECUTABLE(spj_performance_test spj_performance_test.cpp
    NDBTEST NDBCLIENT MYSQLCLIENT)
  IF(MY_COMPILER_IS_CLANG)
    ADD_COMPILE_FLAGS(spj_performance_test.cpp COMPILE_FLAGS
      " -Wno-error=undefined-reinterpret-cast")
  ENDIF()

ENDIF()
