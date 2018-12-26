# Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
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

MACRO (MYSQL_USE_BUNDLED_LIBEVENT)
  SET(LIBEVENT_LIBRARY  event)
  SET(LIBEVENT_INCLUDE_DIR  ${CMAKE_SOURCE_DIR}/libevent)
  SET(LIBEVENT_FOUND  TRUE)
  ADD_DEFINITIONS("-DHAVE_LIBEVENT1")
  SET(WITH_LIBEVENT "bundled" CACHE STRING "Use bundled libevent")
  ADD_SUBDIRECTORY(libevent)
  GET_TARGET_PROPERTY(src libevent SOURCES)
  FOREACH(file ${src})
    SET(LIBEVENT_SOURCES ${LIBEVENT_SOURCES} ${CMAKE_SOURCE_DIR}/libevent/${file})
  ENDFOREACH()
ENDMACRO()

# MYSQL_CHECK_LIBEVENT
#
# Provides the following configure options:
# WITH_LIBEVENT_BUNDLED
# If this is set,we use bindled libevent
# If this is not set,search for system libevent. 
# if system libevent is not found, use bundled copy
# LIBEVENT_LIBRARIES, LIBEVENT_INCLUDE_DIR and LIBEVENT_SOURCES
# are set after this macro has run

MACRO (MYSQL_CHECK_LIBEVENT)

    IF (NOT WITH_LIBEVENT)
      SET(WITH_LIBEVENT "bundled"  CACHE STRING "By default use bundled libevent on this platform")
    ENDIF()
  
  IF(WITH_LIBEVENT STREQUAL "bundled")
    MYSQL_USE_BUNDLED_LIBEVENT()
  ELSEIF(WITH_LIBEVENT STREQUAL "system" OR WITH_LIBEVENT STREQUAL "yes")
    SET(LIBEVENT_FIND_QUIETLY TRUE)

    IF (NOT LIBEVENT_INCLUDE_PATH)
      set(LIBEVENT_INCLUDE_PATH /usr/local/include /opt/local/include)
    ENDIF()

    find_path(LIBEVENT_INCLUDE_DIR event.h PATHS ${LIBEVENT_INCLUDE_PATH})

    if (NOT LIBEVENT_INCLUDE_DIR)
        MESSAGE(SEND_ERROR "Cannot find appropriate event.h in /usr/local/include or /opt/local/include. Use bundled libevent")
    endif() 

    IF (NOT LIBEVENT_LIB_PATHS) 
      set(LIBEVENT_LIB_PATHS /usr/local/lib /opt/local/lib)
    ENDIF()

    ## libevent.so is historical, use libevent_core.so if found.
    find_library(LIBEVENT_CORE event_core PATHS ${LIBEVENT_LIB_PATHS})
    find_library(LIBEVENT_LIB event PATHS ${LIBEVENT_LIB_PATHS})

    if (NOT LIBEVENT_LIB AND NOT LIBEVENT_CORE)
        MESSAGE(SEND_ERROR "Cannot find appropriate event lib in /usr/local/lib or /opt/local/lib. Use bundled libevent")
    endif() 

    IF ((LIBEVENT_LIB OR LIBEVENT_CORE) AND LIBEVENT_INCLUDE_DIR)
      set(LIBEVENT_FOUND TRUE)
      IF (LIBEVENT_CORE)
        set(LIBEVENT_LIBS ${LIBEVENT_CORE})
      ELSE()
        set(LIBEVENT_LIBS ${LIBEVENT_LIB})
      ENDIF()
    ELSE()
      set(LIBEVENT_FOUND FALSE)
    ENDIF()

    IF(LIBEVENT_FOUND)
      SET(LIBEVENT_SOURCES "")
      SET(LIBEVENT_LIBRARIES ${LIBEVENT_LIBS})
      SET(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
      find_path(LIBEVENT2_INCLUDE_DIR event2 HINTS ${LIBEVENT_INCLUDE_PATH}/event)
      IF (LIBEVENT2_INCLUDE_DIR)
        ADD_DEFINITIONS("-DHAVE_LIBEVENT2")
      ELSE()
        ADD_DEFINITIONS("-DHAVE_LIBEVENT1")
      ENDIF()
    ELSE()
      IF(WITH_LIBEVENT STREQUAL "system")
        MESSAGE(SEND_ERROR "Cannot find appropriate system libraries for libevent. Use bundled libevent")
      ENDIF()
      MYSQL_USE_BUNDLED_LIBEVENT()
    ENDIF()

  ENDIF()
ENDMACRO()
