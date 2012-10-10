# Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

# This file includes OSX specific options and quirks, related to system checks

# Workaround for CMake bug#9051
# (CMake does not pass CMAKE_OSX_SYSROOT and CMAKE_OSX_DEPLOYMENT_TARGET when 
# running TRY_COMPILE)

IF(CMAKE_OSX_SYSROOT)
 SET(ENV{CMAKE_OSX_SYSROOT} ${CMAKE_OSX_SYSROOT})
ENDIF()
IF(CMAKE_OSX_SYSROOT)
 SET(ENV{MACOSX_DEPLOYMENT_TARGET} ${OSX_DEPLOYMENT_TARGET})
ENDIF()

IF(CMAKE_OSX_DEPLOYMENT_TARGET)
  # Workaround linker problems  on OSX 10.4
  IF(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.5")
    ADD_DEFINITIONS(-fno-common)
  ENDIF()
ENDIF()

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()
