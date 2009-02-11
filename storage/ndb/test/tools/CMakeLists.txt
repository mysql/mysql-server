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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)

ADD_EXECUTABLE(hugoFill hugoFill.cpp)

ADD_EXECUTABLE(hugoLoad hugoLoad.cpp)
ADD_EXECUTABLE(hugoLockRecords hugoLockRecords.cpp)
ADD_EXECUTABLE(hugoPkDelete hugoPkDelete.cpp)
ADD_EXECUTABLE(hugoPkRead hugoPkRead.cpp)
ADD_DEPENDENCIES(hugoPkRead ndbclient)

ADD_EXECUTABLE(hugoPkReadRecord hugoPkReadRecord.cpp)
ADD_DEPENDENCIES(hugoPkReadRecord ndbclient)
ADD_DEPENDENCIES(hugoPkRead ndbclient)
ADD_EXECUTABLE(hugoPkUpdate hugoPkUpdate.cpp)
ADD_DEPENDENCIES(hugoPkUpdate ndbNDBT)
ADD_EXECUTABLE(hugoScanRead hugoScanRead.cpp)
ADD_EXECUTABLE(hugoScanUpdate hugoScanUpdate.cpp)
ADD_EXECUTABLE(restart restart.cpp)
#ADD_EXECUTABLE(transproxy transproxy.cpp)
ADD_EXECUTABLE(verify_index verify_index.cpp)
ADD_EXECUTABLE(copy_tab copy_tab.cpp)
ADD_EXECUTABLE(create_index create_index.cpp)
ADD_EXECUTABLE(ndb_cpcc cpcc.cpp)
