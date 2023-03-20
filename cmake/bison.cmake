# Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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

# This should be REQUIRED, but we have to support source tarball build.
# https://dev.mysql.com/doc/refman/8.0/en/source-installation.html

SET(MIN_BISON_VERSION_REQUIRED "3.0.4")

# Bison seems to be stuck at version 2.3 in macOS.
# Look for alternative custom installations, e.g.
# /opt/bison-3.8.2/bin
IF(APPLE AND NOT DEFINED BISON_EXECUTABLE)
  SET(OPT_BISON_DIR "/opt")
  IF(IS_DIRECTORY "${OPT_BISON_DIR}")
    SET(PREFERRED_BISON_VERSION "3.8.2")
    FILE(GLOB FOUND_BISON_BIN_DIRS
      LIST_DIRECTORIES true
      "${OPT_BISON_DIR}/bison-*/bin"
      )
    IF(FOUND_BISON_BIN_DIRS)
      # FILE GLOB seems to sort entries, but we need to REVERSE the list:
      # NATURAL uses strverscmp(3)
      LIST(SORT FOUND_BISON_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)
      IF(IS_DIRECTORY "${OPT_BISON_DIR}/bison-${PREFERRED_BISON_VERSION}/bin")
        SET(BISON_PATHS "${OPT_BISON_DIR}/bison-${PREFERRED_BISON_VERSION}/bin")
        LIST(REMOVE_ITEM FOUND_BISON_BIN_DIRS "${BISON_PATHS}")
      ENDIF()
      FOREACH(path ${FOUND_BISON_BIN_DIRS})
        LIST(APPEND BISON_PATHS ${path})
      ENDFOREACH()
      MESSAGE(STATUS "Looking for bison in ${BISON_PATHS}")
      FIND_PROGRAM(BISON_EXECUTABLE bison
        NO_DEFAULT_PATH
        PATHS ${BISON_PATHS})
    ENDIF()
  ENDIF()
ENDIF()

# Look for HOMEBREW bison before the standard OS version.
# Note that it is *not* symlinked like most other executables.
# /usr/local/opt/bison/bin/bison
# /usr/local/opt/bison -> ../Cellar/bison/3.8.2
# /opt/homebrew/opt/bison
IF(APPLE AND NOT DEFINED BISON_EXECUTABLE)
  FIND_PROGRAM(BREW_EXECUTABLE brew)
  IF(BREW_EXECUTABLE)
    EXECUTE_PROCESS(COMMAND ${BREW_EXECUTABLE} --prefix bison
      OUTPUT_VARIABLE BREW_BISON_PREFIX_OUTPUT
      RESULT_VARIABLE BREW_BISON_PREFIX_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    IF(BREW_BISON_PREFIX_RESULT EQUAL 0)
      SET(BISON_HOMEBREW_PATH "${BREW_BISON_PREFIX_OUTPUT}/bin")
    ENDIF()
  ENDIF()
  FIND_PROGRAM(BISON_EXECUTABLE bison
    NO_DEFAULT_PATH
    PATHS "${BISON_HOMEBREW_PATH}" "${HOMEBREW_HOME}/bison/bin")
ENDIF()

# Look for winflexbison3, see e.g.
# https://github.com/lexxmark/winflexbison/releases
# or
# https://chocolatey.org/install
# choco install winflexbison3
IF(WIN32 AND NOT DEFINED BISON_EXECUTABLE)
  SET(MY_BISON_PATHS
    c:/bin/bin
    c:/bin/lib/winflexbison3/tools
    c:/ProgramData/chocolatey/bin
    )
  FOREACH(_path ${MY_BISON_PATHS})
    FILE(TO_NATIVE_PATH ${_path} NATIVE_PATH)
    LIST(APPEND NATIVE_BISON_PATHS "${NATIVE_PATH}")
  ENDFOREACH()
  MESSAGE(STATUS "Looking for win_bison in ${NATIVE_BISON_PATHS}")
  FIND_PROGRAM(BISON_EXECUTABLE
    NAMES win_bison win-bison
    NO_DEFAULT_PATH
    PATHS ${NATIVE_BISON_PATHS}
    )
ENDIF()

FIND_PACKAGE(BISON)

IF(NOT BISON_FOUND)
  MESSAGE(WARNING "No bison found!!")
  MESSAGE(WARNING "If you have bison in a non-standard location, "
    "you can do 'cmake -DBISON_EXECUTABLE=</path/to/bison-executable>"
    )
  IF(APPLE)
    MESSAGE(WARNING "We recommend Homebrew bison.")
  ENDIF()
  RETURN()
ENDIF()

IF(BISON_VERSION VERSION_LESS "${MIN_BISON_VERSION_REQUIRED}")
  MESSAGE(WARNING "Bison version ${BISON_VERSION} is old.")
  MESSAGE(WARNING "If you have a newer bison in a non-standard location, "
    "you can do 'cmake -DBISON_EXECUTABLE=</path/to/bison-executable>"
    )
  IF(APPLE)
    MESSAGE(WARNING "We recommend Homebrew bison.")
  ENDIF()
  MESSAGE(FATAL_ERROR
    "Please update to version ${MIN_BISON_VERSION_REQUIRED} or higher"
    )
ENDIF()

# TODO(tdidriks): replace with "--warnings=all"
# Legacy backward compatibility suppressions:
# * no-yacc: for --yacc
# * no-precedence: for useless precedence or/and associativity rules
SET(BISON_FLAGS_WARNINGS
  "--warnings=all,no-yacc,no-precedence"
  CACHE INTERNAL "BISON 3.x flags")
