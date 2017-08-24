# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WIX_ARCH "x64")
ELSE()
  SET(WIX_ARCH "x86")
ENDIF()

IF(MSVC_VERSION EQUAL 1900)
  SET(WIX_MSVC_DIR "VS2015")
ELSEIF(MSVC_VERSION EQUAL 1800)
  SET(WIX_MSVC_DIR "VS2013")
ELSEIF(MSVC_VERSION EQUAL 1600)
  SET(WIX_MSVC_DIR "VS2010") 
ELSE()
  # When next VS is out, add the correct version here
  MESSAGE(FATAL_ERROR "Unknown VS version")
ENDIF()

# Installed directory layout
IF(IS_DIRECTORY ${WIX_DIR}/../SDK/${WIX_MSVC_DIR})
  SET(WIX_MSVC_DIR "${WIX_DIR}/../SDK/${WIX_MSVC_DIR}")
  INCLUDE_DIRECTORIES(${WIX_MSVC_DIR}/inc)
# Unzipped directory layout
ELSEIF(IS_DIRECTORY ${WIX_DIR}/SDK/${WIX_MSVC_DIR})
  SET(WIX_MSVC_DIR "${WIX_DIR}/SDK/${WIX_MSVC_DIR}")
  INCLUDE_DIRECTORIES(${WIX_DIR}/SDK/inc)
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find ${WIX_MSVC_DIR} directory")
ENDIF()

# INCLUDE_DIRECTORIES(${WIX_DIR}/../SDK/${WIX_MSVC_DIR}/inc)
LINK_DIRECTORIES(${WIX_MSVC_DIR}/lib/${WIX_ARCH})

SET(WIXCA_SOURCES CustomAction.cpp CustomAction.def)

MESSAGE(STATUS "Searching for wcautil in ${WIX_MSVC_DIR}/lib/${WIX_ARCH}")
MESSAGE(STATUS "Searching for dutil in ${WIX_MSVC_DIR}/lib/${WIX_ARCH}")

FIND_LIBRARY(WIX_WCAUTIL_LIBRARY 
  NAMES wcautil
  PATHS ${WIX_MSVC_DIR}/lib/${WIX_ARCH})

FIND_LIBRARY(WIX_DUTIL_LIBRARY 
  NAMES dutil
  PATHS ${WIX_MSVC_DIR}/lib/${WIX_ARCH})

MESSAGE(STATUS "Found: ${WIX_WCAUTIL_LIBRARY}")
MESSAGE(STATUS "Found: ${WIX_DUTIL_LIBRARY}")

IF("${VERSION}" MATCHES "-ndb-")
  MESSAGE(STATUS "This is Cluster build, turning on extra custom actions")
  ADD_DEFINITIONS(-DCLUSTER_EXTRA_CUSTOM_ACTIONS)
ENDIF()

ADD_VERSION_INFO(wixca SHARED WIXCA_SOURCES)
ADD_LIBRARY(wixca SHARED EXCLUDE_FROM_ALL ${WIXCA_SOURCES})
TARGET_LINK_LIBRARIES(wixca ${WIX_WCAUTIL_LIBRARY} ${WIX_DUTIL_LIBRARY} 
  msi version )
