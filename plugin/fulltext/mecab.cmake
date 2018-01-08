# Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

# We support MECAB:
# - "system" uses headers/libraries in /usr/local/lib
# - a custom installation of mecab can be used like this
#     - cmake -DWITH_MECAB=</path/to/custom/mecab>
#
# In order to find the .dll files at runtime, you might need to set some
# environment variables.
# linux:
#   'export LD_LIBRARY_PATH=</path/to/custom/mecab>/lib:$LD_LIBRARY_PATH'
# windows:
#   'set path=</path/to/custom/mecab>\bin;%PATH%'
#
# If you turn on the option BUNDLE_MECAB, we try to link with the static library.

SET(WITH_MECAB_DOC
  "<empty> (disabled) | system (use os library)")
SET(WITH_MECAB_DOC
  "${WITH_MECAB_DOC} | </path/to/custom/installation> (use custom version)")


# Off by default, can be overridden on command line.
SET(WITH_MECAB CACHE STRING "${WITH_MECAB_DOC}")

# Bundle libmecab with our binaries, if not built from system libs
IF(WITH_MECAB STREQUAL "system")
  OPTION(BUNDLE_MECAB "Bundle mecab and ipadic with plugin" OFF)
ELSE()
  OPTION(BUNDLE_MECAB "Bundle mecab and ipadic with plugin" ON)
ENDIF()


# MYSQL_CHECK_MECAB
#
# Provides the following configure options:
# WITH_MECAB=[system|<path/to/custom/installation>]
FUNCTION (MYSQL_CHECK_MECAB)
  IF(NOT WITH_MECAB)
    RETURN()
  ENDIF()

  FILE(TO_CMAKE_PATH "${WITH_MECAB}" WITH_MECAB)

  IF(WIN32)
    FILE(GLOB WITH_MECAB_HEADER ${WITH_MECAB}/sdk/mecab.h)
  ELSE()
    FILE(GLOB WITH_MECAB_HEADER ${WITH_MECAB}/include/mecab.h)
  ENDIF()

  IF (WITH_MECAB_HEADER)
    SET(WITH_MECAB_PATH ${WITH_MECAB} CACHE
      PATH "path to custom MECAB installation")
  ENDIF()

  IF(WITH_MECAB STREQUAL "system" OR WITH_MECAB_PATH)
    # We reverse the list, in order to prefer .a over .so (or .dylib)
    IF(BUNDLE_MECAB)
      LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    ENDIF()

    IF(WIN32)
      FIND_PATH(MECAB_INCLUDE_DIR
        NAMES mecab.h
        HINTS ${WITH_MECAB}/sdk
      )

      FIND_LIBRARY(MECAB_LIBRARY
        NAMES libmecab
        HINTS ${WITH_MECAB}/sdk
      )
    ELSE()
      FIND_PATH(MECAB_INCLUDE_DIR
        NAMES mecab.h
        HINTS ${WITH_MECAB}/include
      )

      FIND_LIBRARY(MECAB_LIBRARY
        NAMES mecab
        HINTS ${WITH_MECAB}/lib
      )
    ENDIF()

    IF(BUNDLE_MECAB)
      LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    ENDIF()

    IF(MECAB_INCLUDE_DIR AND
       MECAB_LIBRARY
      )
      SET(MECAB_FOUND TRUE)
    ELSE()
      SET(MECAB_FOUND FALSE)
    ENDIF()

    MESSAGE(STATUS "MECAB_INCLUDE_DIR = ${MECAB_INCLUDE_DIR}")
    MESSAGE(STATUS "MECAB_LIBRARY = ${MECAB_LIBRARY}")

    IF (MECAB_FOUND AND BUNDLE_MECAB)
      IF(WIN32)
        FILE(GLOB MECAB_DLL "${WITH_MECAB}/bin/libmecab.dll")
        FILE(GLOB MECAB_IPADIC "${WITH_MECAB}/dic")
        IF (MECAB_DLL AND MECAB_IPADIC)
          INSTALL(
            FILES "${WITH_MECAB}/bin/libmecab.dll"
            DESTINATION ${INSTALL_BINDIR}
            COMPONENT "Server"
          )
          INSTALL(
            DIRECTORY "${WITH_MECAB}/dic"
            DESTINATION ${INSTALL_LIBDIR}/mecab
            USE_SOURCE_PERMISSIONS
            COMPONENT "Server"
          )
          MESSAGE(STATUS "INSTALL ${WITH_MECAB}/bin/libmecab.dll")
          MESSAGE(STATUS "INSTALL ${WITH_MECAB}/dic")
        ELSE()
          MESSAGE(STATUS
            "Could not find ${WITH_MECAB}/bin/libmecab.dll or ${WITH_MECAB}/dic")
        ENDIF()
      ELSE()
        GET_FILENAME_COMPONENT(MECAB_LIBRARY_LOCATION "${MECAB_LIBRARY}" PATH)
        FILE(GLOB MECAB_IPADIC "${MECAB_LIBRARY_LOCATION}/mecab/dic")
        IF (MECAB_IPADIC)
          INSTALL(
            DIRECTORY "${MECAB_LIBRARY_LOCATION}/mecab"
            DESTINATION ${INSTALL_LIBDIR}
            USE_SOURCE_PERMISSIONS
            COMPONENT "Server"
          )
          MESSAGE(STATUS "INSTALL ${MECAB_LIBRARY_LOCATION}/mecab")
        ELSE()
          MESSAGE(STATUS
            "Could not find ${MECAB_LIBRARY_LOCATION}/mecab/dic")
        ENDIF()
      ENDIF()

      # Install mecabrc file.
      INSTALL(
        FILES "${CMAKE_SOURCE_DIR}/plugin/fulltext/mecab_parser/mecabrc"
        DESTINATION "${INSTALL_LIBDIR}/mecab/etc"
        COMPONENT "Server"
      )
    ENDIF()

    IF(NOT MECAB_FOUND)

      UNSET(MECAB_INCLUDE_DIR)
      UNSET(MECAB_INCLUDE_DIR CACHE)
      UNSET(MECAB_LIBRARY)
      UNSET(MECAB_LIBRARY CACHE)

      MESSAGE(SEND_ERROR
        "Cannot find appropriate libraries for MECAB. "
        "WITH_MECAB option are : ${WITH_MECAB_DOC}")
    ENDIF()
  ELSE()
    MESSAGE(SEND_ERROR
      "Wrong option or path for WITH_MECAB. "
      "Valid WITH_MECAB options are : ${WITH_MECAB_DOC}")
  ENDIF()
ENDFUNCTION()
