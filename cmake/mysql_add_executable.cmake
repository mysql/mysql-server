# Copyright (C) 2009 Sun Microsystems, Inc
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

# Add executable plus some additional MySQL specific stuff
# Usage (same as for standard CMake's ADD_EXECUTABLE)
#
# MYSQL_ADD_EXECUTABLE(target source1...sourceN)
#
# MySQL specifics:
# - instruct CPack to install executable under ${CMAKE_INSTALL_PREFIX}/bin directory
# On Windows :
# - add version resource
# - instruct CPack to do autenticode signing if SIGNCODE is set

INCLUDE(cmake_parse_arguments)

FUNCTION (MYSQL_ADD_EXECUTABLE)
  # Pass-through arguments for ADD_EXECUTABLE
  CMAKE_PARSE_ARGUMENTS(ARG
   "WIN32;MACOSX_BUNDLE;EXCLUDE_FROM_ALL"
   ""
   ${ARGN}
  )
  LIST(GET ARG_DEFAULT_ARGS 0 target)
  LIST(REMOVE_AT  ARG_DEFAULT_ARGS 0)
  
  SET(sources ${ARG_DEFAULT_ARGS})
  IF(WIN32)
    ADD_VERSION_INFO(${target}  VFT_APP sources)
  ENDIF()
  
  ADD_EXECUTABLE(${target} ${ARG_WIN32} ${ARG_MACOSX_BUNDLE} ${ARG_EXCLUDE_FROM_ALL} ${sources})
  
  # tell CPack where to install
  IF(NOT ARG_EXCLUDE_FROM_ALL)
    MYSQL_INSTALL_TARGETS(${target} DESTINATION bin)
  ENDIF()
ENDFUNCTION()

# Function to embed version info into executables/dlls on Windows
# Refer http://msdn.microsoft.com/en-us/library/aa381058(VS.85).aspx
# for more information
MACRO(ADD_VERSION_INFO target filetype sources) 
  SET(HAVE_RC_FILE)
  FOREACH(src ${sources})
    IF("${src}" MATCHES "\\.rc$")
      SET(HAVE_RC_FILE 1)
      BREAK()
    ENDIF()
  ENDFOREACH()
  IF(NOT HAVE_RC_FILE)
    IF(NOT MAJOR_VERSION)
      MESSAGE(FATAL_ERROR "MAJOR_VERSION is not defined")
    ENDIF()
    IF(NOT MINOR_VERSION)
      MESSAGE(FATAL_ERROR "MINOR_VERSION is not defined")
    ENDIF()
    IF(NOT PATCH)
      SET(PATCH 0)
    ENDIF()
    SET(FILETYPE ${filetype})
    SET(INTERNALNAME ${target})
    IF(FILETYPE STREQUAL VFT_APP)
     SET(ORIGINALFILENAME ${target}.exe)
    ELSE()
     SET(ORIGINALFILENAME ${target}.dll)
    ENDIF()
    SET(FILEDESCRIPTION ${target})
    FIND_FILE(VERINFO_IN versioninfo.rc.in ${CMAKE_MODULE_PATH})
    IF(NOT VERINFO_IN)
      MESSAGE(FATAL_ERROR "cannot find version info template")
    ENDIF()
    CONFIGURE_FILE(${VERINFO_IN}
     ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
    LIST(APPEND ${sources} ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
  ENDIF()
 ENDMACRO()