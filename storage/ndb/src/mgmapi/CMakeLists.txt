# Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi
                    ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
)
#ADD_DEFINITIONS(-DNDB_MGMAPI)
ADD_CONVENIENCE_LIBRARY(ndbmgmapi
            mgmapi.cpp
            mgmapi_error.c
            ndb_logevent.cpp
            mgmapi_configuration.cpp
            LocalConfig.cpp
            ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/error/ndbd_exit_codes.c)
TARGET_LINK_LIBRARIES(ndbmgmapi ndbconf ndbportlib)


ADD_EXECUTABLE(mgmapi-t mgmapi-t.cpp)
TARGET_LINK_LIBRARIES(mgmapi-t ndbmgmapi ndbgeneral)
