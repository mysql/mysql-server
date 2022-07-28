# Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

CMAKE_POLICY(SET CMP0007 NEW)

SET(MIN_DOXYGEN_VERSION_REQUIRED "1.9.2")

IF(DOXYGEN_DOT_EXECUTABLE)
  EXECUTE_PROCESS(
    COMMAND ${DOXYGEN_DOT_EXECUTABLE} -V)
  SET(ENV{GRAPHVIZ_DOT} ${DOXYGEN_DOT_EXECUTABLE})
ENDIF()

EXECUTE_PROCESS(
  COMMAND ${DOXYGEN_EXECUTABLE} --version
  OUTPUT_VARIABLE DOXYGEN_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
MESSAGE(STATUS "DOXYGEN_VERSION ${DOXYGEN_VERSION}")
IF(DOXYGEN_VERSION VERSION_LESS "${MIN_DOXYGEN_VERSION_REQUIRED}")
  MESSAGE(FATAL_ERROR
    "We require at least version ${MIN_DOXYGEN_VERSION_REQUIRED}")
ENDIF()

FUNCTION(FIND_PLANTUML_JAR_PATH)
  IF(NOT DEFINED ENV{PLANTUML_JAR_PATH})
    IF(WIN32)
      SET(JAR_PATHS
        "C:/java/plantuml"
        "S:/java/plantuml"
        )
    ELSE()
      SET(JAR_PATHS
        /usr/global/share/java/plantuml/
        /usr/local/share/java/plantuml/
        /usr/share/java/
        /usr/local/share/java/
        /usr/share/plantuml/
        )
    ENDIF()

    FIND_FILE(PLANTUML_JAR
      NAMES plantuml.8053.jar plantuml.jar
      PATHS ${JAR_PATHS}
      NO_DEFAULT_PATH
      )
    IF(PLANTUML_JAR)
      SET(ENV{PLANTUML_JAR_PATH} ${PLANTUML_JAR})
    ENDIF()
  ENDIF()
  IF(DEFINED ENV{PLANTUML_JAR_PATH})
    EXECUTE_PROCESS(
      COMMAND java -jar $ENV{PLANTUML_JAR_PATH} -version)
  ELSE()
    MESSAGE(FATAL_ERROR
      "plantuml.jar not found, "
      "Please set PLANTUML_JAR_PATH in the environment.")
  ENDIF()
ENDFUNCTION()

FIND_PLANTUML_JAR_PATH()

IF(REDIRECT_DOXYGEN_STDOUT)
  MESSAGE(STATUS "Writing stdout to ${OUTPUT_FILE}")
  SET(OUTPUT_FILE_ARGS OUTPUT_FILE ${OUTPUT_FILE})
ENDIF()
MESSAGE(STATUS "Writing stderr to ${ERROR_FILE}")

EXECUTE_PROCESS(
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
  ERROR_FILE ${ERROR_FILE}
  ${OUTPUT_FILE_ARGS}
  )

MESSAGE(STATUS "Filtering out ignored warnings/errors")
MESSAGE(STATUS "Writing warnings/errors to ${TOFIX_FILE}")

# Read IGNORE_FILE and create a list of patterns that we should ignore in
# ERROR_FILE.
FILE(READ ${IGNORE_FILE} IGNORE_FILE_CONTENTS)
STRING(REPLACE ";" "\\\;" IGNORE_FILE_CONTENTS "${IGNORE_FILE_CONTENTS}")
STRING(REPLACE "\n" ";" IGNORE_FILE_LINES "${IGNORE_FILE_CONTENTS}")
FOREACH(LINE ${IGNORE_FILE_LINES})
  STRING(REGEX MATCH "^[\r\n\t ]*#" MATCH_COMMENT "${LINE}")
  STRING(REGEX MATCH "^[\r\n\t ]*$" MATCH_EMPTY "${LINE}")
  IF(NOT (MATCH_COMMENT OR MATCH_EMPTY))
    MESSAGE(STATUS "Ignoring pattern ${LINE}")
    SET(IGNORE_LIST "${IGNORE_LIST};${LINE}")
  ENDIF()
ENDFOREACH()

# Convert ERROR_FILE contents to a list of lines
FILE(READ ${ERROR_FILE} ERROR_FILE_CONTENTS)
IF(ERROR_FILE_CONTENTS)
  STRING(REPLACE ";" "\\\;" ERROR_FILE_CONTENTS "${ERROR_FILE_CONTENTS}")
  STRING(REPLACE "[" "(" ERROR_FILE_CONTENTS "${ERROR_FILE_CONTENTS}")
  STRING(REPLACE "]" ")" ERROR_FILE_CONTENTS "${ERROR_FILE_CONTENTS}")
  STRING(REPLACE "\n" ";" ERROR_FILE_LINES "${ERROR_FILE_CONTENTS}")
ENDIF()

FILE(REMOVE ${TOFIX_FILE})
FILE(REMOVE ${REGRESSION_FILE})
UNSET(FOUND_WARNINGS)
# See if we have any warnings/errors.
FOREACH(LINE ${ERROR_FILE_LINES})

  # Filter out information messages from dia.
  STRING(REGEX MATCH "^.*\\.dia --> dia_.*\\.png\$" DIA_STATUS "${LINE}")
  STRING(LENGTH "${DIA_STATUS}" LEN_DIA_STATUS)
  IF (${LEN_DIA_STATUS} GREATER 0)
    CONTINUE()
  ENDIF()

  # Filter out git errors that occur if running on a tarball insted of a git
  # repo (doxygen_resources/doxygen-filter-mysqld calls git).
  STRING(REGEX MATCH
    "^Stopping at filesystem boundary \\(GIT_DISCOVERY_ACROSS_FILESYSTEM not set\\).\$"
    GIT_ERROR "${LINE}")
  STRING(LENGTH "${GIT_ERROR}" LEN_GIT_ERROR)
  IF (${LEN_GIT_ERROR} GREATER 0)
    CONTINUE()
  ENDIF()
  STRING(REGEX MATCH
    "^fatal: Not a git repository \\(or any parent up to mount point "
    GIT_ERROR "${LINE}")
  STRING(LENGTH "${GIT_ERROR}" LEN_GIT_ERROR)
  IF (${LEN_GIT_ERROR} GREATER 0)
    CONTINUE()
  ENDIF()

  STRING(REGEX MATCH "^(${SOURCE_DIR}/)(.*)" XXX "${LINE}")
  IF(CMAKE_MATCH_1)
    SET(LINE ${CMAKE_MATCH_2})
  ELSE()
    GET_FILENAME_COMPONENT(SOURCE_DIR_REALPATH ${SOURCE_DIR} REALPATH)
    STRING(REGEX MATCH "^(${SOURCE_DIR_REALPATH}/)(.*)" XXX "${LINE}")
    IF(CMAKE_MATCH_1)
      SET(LINE ${CMAKE_MATCH_2})
    ENDIF()
  ENDIF()

  # Check for known patterns. Known patterns are not reported as regressions.
  SET(IS_REGRESSION 1)
  FOREACH(IGNORE_PATTERN ${IGNORE_LIST})
    STRING(REGEX MATCH "${IGNORE_PATTERN}" IGNORED "${LINE}")
    STRING(LENGTH "${IGNORED}" LEN_IGNORED)
    IF (${LEN_IGNORED} GREATER 0)
      # The line matches a pattern in IGNORE_FILE, so this is a known error.
      UNSET(IS_REGRESSION)
      BREAK()
    ENDIF()
  ENDFOREACH()

  # All errors go to TOFIX_FILE.
  FILE(APPEND ${TOFIX_FILE} "${LINE}\n")

  # Only regressions go to REGRESSION_FILE.
  IF (${IS_REGRESSION})
    MESSAGE(${LINE})
    FILE(APPEND ${REGRESSION_FILE} "${LINE}\n")
    SET(FOUND_WARNINGS 1)
  ENDIF()

ENDFOREACH()

# Only report regressions.
IF(FOUND_WARNINGS)
  MESSAGE(WARNING "Found warnings/errors, see ${REGRESSION_FILE}")
ELSE()
  MESSAGE(STATUS "No warnings/errors found")
ENDIF()
