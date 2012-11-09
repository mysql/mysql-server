# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
                    ${CMAKE_SOURCE_DIR}/sql)


ADD_EXECUTABLE(ndb_cpcd
        APIService.cpp
        CPCD.cpp
        Monitor.cpp
        Process.cpp
        common.cpp
        main.cpp)

TARGET_LINK_LIBRARIES(ndb_cpcd
               ndbclient_static)

INSTALL(TARGETS ndb_cpcd DESTINATION bin)
