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


ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(mgmapi)
ADD_SUBDIRECTORY(ndbapi)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(mgmclient)
ADD_SUBDIRECTORY(mgmsrv)
ADD_SUBDIRECTORY(cw)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/include
                     {CMAKE_SOURCE_DIR}/include)

FILE(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/dummy.cpp "")
ADD_LIBRARY(ndbclient STATIC dummy.cpp)
TARGET_LINK_LIBRARIES(ndbclient
                      ndbapi
                      ndbtransport
                      ndbtrace
                      ndbsignaldata
                      ndbmgmapi
                      ndbmgmsrv
                      ndblogger
                      ndbportlib
                      ndbgeneral)
