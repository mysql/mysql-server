# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WIX_ARCH "x64")
ELSE()
  SET(WIX_ARCH "x86")
ENDIF()

IF(MSVC_VERSION LESS 1910)
  SET(WIX_MSVC_DIR "VS2015")
ELSEIF(MSVC_VERSION LESS 1920)
  SET(WIX_MSVC_DIR "VS2017")
ELSE()
  # When next VS is out, add the correct version here
  MESSAGE(FATAL_ERROR "Unknown VS version")
ENDIF()

IF(BUNDLE_RUNTIME_LIBRARIES)
  FOREACH(flag
      CMAKE_C_FLAGS_RELEASE    CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS_DEBUG      CMAKE_C_FLAGS_DEBUG_INIT
      CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_DEBUG    CMAKE_CXX_FLAGS_DEBUG_INIT)
    # When bundling CRT libraries we accept that the user
    # doesn't have the redist installed. We can't control
    # the current working directory for all our actions, so
    # we want to embedd CRT with /MT.
    STRING(REPLACE "/MD"  "/MT" "${flag}" "${${flag}}")
  ENDFOREACH()
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
