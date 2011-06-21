# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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


ADD_SUBDIRECTORY(signaldata)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

ADD_CONVENIENCE_LIBRARY(ndbtrace
            SignalLoggerManager.cpp
            DebuggerNames.cpp
            BlockNames.cpp
            EventLogger.cpp
            ${NDB_SOURCE_DIR}/src/kernel/error/ndbd_exit_codes.c
)

TARGET_LINK_LIBRARIES(ndbtrace ndbsignaldata ndblogger ndbgeneral)
