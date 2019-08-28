# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
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
