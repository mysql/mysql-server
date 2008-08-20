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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapi.cmake)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/mgmcommon
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/portlib
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmclient
                    ${CMAKE_SOURCE_DIR}/sql)

LINK_LIBRARIES(ndbmgmclient
               ndbclient
               dbug
               mysys
               strings
               ndbgeneral
               ndbportlib
               ${NDB_SCI_LIBS})

ADD_LIBRARY(ndbconf
               Config.cpp
               ConfigInfo.cpp
               InitConfigFileParser.cpp
)

ADD_EXECUTABLE(ndb_mgmd
               MgmtSrvr.cpp
               main.cpp
               Services.cpp
               MgmtSrvrConfig.cpp
               ${CMAKE_SOURCE_DIR}/sql/nt_servc.cc)
TARGET_LINK_LIBRARIES(ndb_mgmd ndbconf)
