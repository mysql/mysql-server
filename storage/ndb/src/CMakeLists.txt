# Copyright (c) 2008, 2012, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(libutils)

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(mgmapi)
ADD_SUBDIRECTORY(ndbapi)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(mgmclient)
ADD_SUBDIRECTORY(mgmsrv)

SET(NDBJTIE_LIB)
IF(HAVE_JDK)
  ADD_SUBDIRECTORY(ndbjtie)
  SET(NDBJTIE_LIB ndbjtie)
  ADD_DEFINITIONS(-DNDB_WITH_NDBJTIE)
ENDIF()

#
# Build static ndbclient library
#
SET(NDBCLIENT_LIBS
  mysys dbug strings
  ndbapi
  ndbtransport
  ndbtrace
  ndbsignaldata
  ndbmgmapi
  ndbmgmcommon
  ndblogger
  ndbportlib
  ndbgeneral
  ndbconf)

MERGE_LIBRARIES(ndbclient_static STATIC ${NDBCLIENT_LIBS} COMPONENT Development)
# Build test program to check linking against ndclient_static
ADD_EXECUTABLE(ndbclient_static_link_test ndbclient_link_test.cpp)
TARGET_LINK_LIBRARIES(ndbclient_static_link_test ndbclient_static)

#
# Build shared ndbclient library
#
SET(NDBCLIENT_SO_LIBS ${NDBCLIENT_LIBS} ${NDBJTIE_LIB})

# Extract version from storage/ndb/VERSION
INCLUDE(ndb_get_config_value)
NDB_GET_CONFIG_VALUE(NDB_SHARED_LIB_VERSION_MAJOR major)
NDB_GET_CONFIG_VALUE(NDB_SHARED_LIB_VERSION_MINOR minor)
NDB_GET_CONFIG_VALUE(NDB_SHARED_LIB_VERSION_BUILD build)
SET(NDB_SHARED_LIB_VERSION "${major}.${minor}.${build}")

ADD_LIBRARY(ndbclient_so SHARED ndbclient_exports.cpp)
TARGET_LINK_LIBRARIES(ndbclient_so ${NDBCLIENT_SO_LIBS})

IF(MSVC)
# Not yet!
#  # Add a PREBUILD command which creates a module definition file
#  # for all the symbols which should be exported from ndbclient_so
#  # PROBLEM: Fails to get the full path to mysys, strings etc since those
#  # targets haven't been added yet -> defer this step to build time?
#  FOREACH (lib ${NDBCLIENT_SO_LIBS})
#    GET_TARGET_PROPERTY(loc ${lib} LOCATION)
#    FILE(TO_NATIVE_PATH ${loc} loc)
#    SET (lib_locations ${lib_locations} ${loc})
#  ENDFOREACH ()
#  
#  SET(_PLATFORM x86)
#  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
#    SET(_PLATFORM  x64)
#  ENDIF()
#  ADD_CUSTOM_COMMAND(TARGET ndbclient_so PRE_BUILD
#    COMMAND cscript ARGS //nologo ${PROJECT_SOURCE_DIR}/win/create_def_file.js
#                         ${_PLATFORM}  ${lib_locations} > ndbclient_exports.def
#    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	
#  # Modify link flags to use the generated module definition file
#  GET_TARGET_PROPERTY(ndbclient_so_link_flags ndbclient_so LINK_FLAGS)
#  SET_TARGET_PROPERTIES(ndbclient_so PROPERTIES LINK_FLAGS "${ndbclient_link_flags} /DEF:ndbclient_exports.def")
ELSE()
  # Turn off cmake's transitive link behaviour so that the shared
  # ndbclient library contains everything except functions from
  # system libs
  SET(NDBCLIENT_SYSTEM_LIBS)
  FOREACH(lib ${NDBCLIENT_SO_LIBS})
    SET(deps ${${lib}_LIB_DEPENDS})
    FOREACH(dep_lib ${deps})
    SET(ignore_dep_lib)

      # The list of dependent libs contains keywords used for debug vs optimized
      # builds, ignore them 
      IF(dep_lib MATCHES "general" OR
         dep_lib MATCHES "debug" OR
         dep_lib MATCHES "optimized")
        # MESSAGE(STATUS "Ignoring keyword ${dep_lib}")
        SET(ignore_dep_lib 1)
      ENDIF()

      # Ignore libs already in NDBCLIENT_SO_LIBS list, manily used
      # to ignore mysys, strings etc. since those targets have not
      # been added yet
      LIST(FIND NDBCLIENT_SO_LIBS ${dep_lib} dep_lib_found)
      IF(dep_lib_found GREATER -1)
        # MESSAGE(STATUS "Ignoring lib ${dep_lib} since it's already in NDBCLIENT_SO_LIBS")
        SET(ignore_dep_lib 1)
      ENDIF()

      # Ignore libs where location is known(this means it's built locally)
      GET_TARGET_PROPERTY(dep_lib_location ${dep_lib} LOCATION)
      IF(dep_lib_location)
        # MESSAGE(STATUS "Ignoring ${dep_lib} since location is known")
        SET(ignore_dep_lib 1)
      ENDIF()

      IF(NOT ignore_dep_lib)
        LIST(APPEND NDBCLIENT_SYSTEM_LIBS ${dep_lib})
      ENDIF()

    ENDFOREACH()
  ENDFOREACH()
  MESSAGE(STATUS "System libs used by ndbclient_so: ${NDBCLIENT_SYSTEM_LIBS}")
  TARGET_LINK_LIBRARIES(ndbclient_so LINK_INTERFACE_LIBRARIES
    ${NDBCLIENT_SYSTEM_LIBS})
ENDIF()

SET_TARGET_PROPERTIES(ndbclient_so PROPERTIES
  OUTPUT_NAME "ndbclient"
  SOVERSION ${NDB_SHARED_LIB_VERSION})
MYSQL_INSTALL_TARGETS(ndbclient_so
  DESTINATION "${INSTALL_LIBDIR}"
  COMPONENT Developement)
# Build test program to check linking against ndclient_so
ADD_EXECUTABLE(ndbclient_shared_link_test ndbclient_link_test.cpp)
TARGET_LINK_LIBRARIES(ndbclient_shared_link_test ndbclient_so)
