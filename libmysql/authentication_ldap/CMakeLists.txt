# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#
# Configuration for building LDAP SASL Authentication Plugin (client-side)
#

INCLUDE(CheckIncludeFiles)

# If cmake was invoked with -DWITH_AUTHENTICATION_LDAP=1
# then fail if we are unable to build the LDAP plugin.
MACRO(CROAK_AND_RETURN)
  IF (WITH_AUTHENTICATION_LDAP)
    MESSAGE(SEND_ERROR ${ARGV})
  ELSE()
    MESSAGE(STATUS ${ARGV} " Skipping the LDAP SASL client authentication plugin.")
    RETURN()
  ENDIF()
ENDMACRO()

IF(NOT WIN32)
  IF(DEFINED WITH_SASL)
    MESSAGE(STATUS ${ARGV} "Currently LDAP SASL client authentication plug-in is build with only system installed cyrus SASL library.")
  ENDIF()

  IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    INCLUDE_DIRECTORIES(SYSTEM /usr/local/include)
    LIST(APPEND CMAKE_REQUIRED_INCLUDES "/usr/local/include")
  ENDIF()

  CHECK_INCLUDE_FILES(sasl/sasl.h HAVE_SASL_H)
  CHECK_INCLUDE_FILES(lber.h HAVE_LBER_H)

  IF(HAVE_SASL_H)
    SET(CMAKE_EXTRA_INCLUDE_FILES sasl/sasl.h)
  ELSE()
    CROAK_AND_RETURN("Required SASL library is missing.")
  ENDIF()
  IF(HAVE_LBER_H)
    SET(CMAKE_EXTRA_INCLUDE_FILES lber.h)
  ELSE()
    CROAK_AND_RETURN("Required LBER library is missing.")
  ENDIF()
  IF(SASL_LIBRARY_PATH STREQUAL "")
    SET(SASL_LIBRARY_PATH /usr/lib64/libsasl2.so)
  ENDIF ()
ELSE()
  # There is no system installed SASL library on Windows
  SET(WITH_SASL "" CACHE PATH "Location of the SASL install")

  IF(NOT WITH_SASL OR WITH_SASL STREQUAL "system")
    # This plug-in is only compulsory for the enterprise MySQL.
    # For MySQL community/Dev's user can build without this plug-in.
    MESSAGE(STATUS ${ARGV} " You need to set WITH_SASL path to build LDAP SASL client authentication plugin.")
    RETURN()
  ENDIF()

  # Use forward slashes from CMake code
  FILE(TO_CMAKE_PATH "${WITH_SASL}" SASL_ROOT_DIR)

  FIND_PATH(SASL_INCLUDE_DIR
    NAMES sasl/sasl.h
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    PATHS ${SASL_ROOT_DIR}/include
  )

  FIND_FILE(SASL_LIBRARY_DLL
    NAMES libsasl.dll
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    PATHS ${SASL_ROOT_DIR}/lib
  )

  FIND_FILE(SASL_SCRAM_PLUGIN
    NAMES saslSCRAM.dll
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    PATHS ${SASL_ROOT_DIR}/lib
  )

  IF(SASL_INCLUDE_DIR AND
     SASL_LIBRARY AND
     SASL_LIBRARY_DLL AND
     SASL_SCRAM_PLUGIN)
    SET(SASL_FOUND TRUE)
  ELSE()
    SET(SASL_FOUND FALSE)
  ENDIF()

  INCLUDE_DIRECTORIES(${SASL_INCLUDE_DIR})

  MESSAGE(STATUS "SASL_INCLUDE_DIR  = ${SASL_INCLUDE_DIR}")
  MESSAGE(STATUS "SASL_LIBRARY_DLL  = ${SASL_LIBRARY_DLL}")
  MESSAGE(STATUS "SASL_SCRAM_PLUGIN = ${SASL_SCRAM_PLUGIN}")

ENDIF()

MESSAGE(STATUS "SASL_LIBRARY      = ${SASL_LIBRARY}")
MYSQL_ADD_PLUGIN(authentication_ldap_sasl_client
                 auth_ldap_sasl_client.cc log_client.cc
                 LINK_LIBRARIES ${SASL_LIBRARY}
                 CLIENT_ONLY MODULE_ONLY
                 MODULE_OUTPUT_NAME "authentication_ldap_sasl_client")

IF(WIN32)
  GET_FILENAME_COMPONENT(SASL_DLL_NAME ${SASL_LIBRARY_DLL} NAME)
  GET_FILENAME_COMPONENT(SASL_SCRAM_PLUGIN_NAME "${SASL_SCRAM_PLUGIN}" NAME)

  # Note that "libsasl.dll" and "saslSCRAM.dll" go into the "bin" directory where
  # "mysql.exe" and other client executables are located.
  INSTALL(FILES "${SASL_LIBRARY_DLL}"
          DESTINATION ${INSTALL_BINDIR}
          COMPONENT SharedLibraries)
  INSTALL(FILES "${SASL_SCRAM_PLUGIN}"
          DESTINATION ${INSTALL_BINDIR}
          COMPONENT SharedLibraries)

  # To run client executables that load the plug-in from the build tree we need
  # to copy the SASL library DLL and SASL SCRAM library DLL to the same directory as the client executables.
  ADD_CUSTOM_COMMAND(TARGET authentication_ldap_sasl_client POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             "${SASL_LIBRARY_DLL}"
                             "${CMAKE_BINARY_DIR}/client/${CMAKE_CFG_INTDIR}/${SASL_DLL_NAME}"
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             "${SASL_SCRAM_PLUGIN}"
                             "${CMAKE_BINARY_DIR}/client/${CMAKE_CFG_INTDIR}/${SASL_SCRAM_PLUGIN_NAME}"
  )
  ADD_DEPENDENCIES(authentication_ldap_sasl_client mysqltest)

ENDIF()
