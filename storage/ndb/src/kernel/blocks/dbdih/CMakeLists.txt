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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

LINK_LIBRARIES(ndbgeneral
               ndbportlib)
IF(WIN32)
LINK_LIBRARIES(wsock32)
ENDIF(WIN32)

ADD_LIBRARY(ndbdbdih STATIC
            DbdihInit.cpp
            DbdihMain.cpp)

ADD_EXECUTABLE(ndbd_sysfile_reader
               printSysfile/printSysfile.cpp)
TARGET_LINK_LIBRARIES(ndbd_sysfile_reader dbug mysys strings)
