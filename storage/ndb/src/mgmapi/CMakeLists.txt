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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_util.cmake)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi,
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
                    ${CMAKE_SOURCE_DIR}/storage/ndb/mgmsrv
)
ADD_DEFINITIONS(-DNDB_MGMAPI)
ADD_LIBRARY(ndbmgmapi STATIC
            mgmapi.cpp
            ndb_logevent.cpp
            mgmapi_configuration.cpp
            LocalConfig.cpp
            ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/error/ndbd_exit_codes.c)
TARGET_LINK_LIBRARIES(ndbmgmapi ndbconfig)
