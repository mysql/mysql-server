# Copyright (c) 2019, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

FUNCTION(GET_FILE_SIZE FILE_NAME OUTPUT_SIZE)
  IF(WIN32)
    FILE(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/cmake/filesize.bat" FILESIZE_BAT)
    FILE(TO_NATIVE_PATH "${FILE_NAME}" NATIVE_FILE_NAME)

    EXECUTE_PROCESS(
      COMMAND "${FILESIZE_BAT}" "${NATIVE_FILE_NAME}"
      RESULT_VARIABLE COMMAND_RESULT
      OUTPUT_VARIABLE RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  ELSEIF(APPLE OR FREEBSD)
    EXECUTE_PROCESS(
      COMMAND stat -f '%z' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ELSE()
    EXECUTE_PROCESS(
      COMMAND stat -c '%s' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ENDIF()
  SET(${OUTPUT_SIZE} ${RESULT} PARENT_SCOPE)
ENDFUNCTION()


IF(WIN32)
  IF(NOT WIN32_CLANG AND NOT EXISTS "${CMAKE_LINKER}")
    MESSAGE(WARNING "CMAKE_LINKER not found:\n ${CMAKE_LINKER}")
    MESSAGE(WARNING "It seems you have upgraded Visual Studio")
    MESSAGE(WARNING "You should do a clean build")
    MESSAGE(WARNING
      "\n or remove these files:"
      "\n CMakeFiles/${CMAKE_VERSION}/CMakeCCompiler.cmake"
      "\n CMakeFiles/${CMAKE_VERSION}/CMakeCXXCompiler.cmake"
      "\n and re-run cmake"
      "\n"
      )
    UNSET(DUMPBIN_EXECUTABLE)
    UNSET(DUMPBIN_EXECUTABLE CACHE)
    UNSET(CMAKE_LINKER)
    UNSET(CMAKE_LINKER CACHE)
  ENDIF()
  GET_FILENAME_COMPONENT(CMAKE_LINKER_PATH "${CMAKE_LINKER}" DIRECTORY)
  FIND_PROGRAM(DUMPBIN_EXECUTABLE dumpbin PATHS "${CMAKE_LINKER_PATH}")

  FUNCTION(FIND_OBJECT_DEPENDENCIES FILE_NAME RETURN_VALUE)
    SET(${RETURN_VALUE} PARENT_SCOPE)
    IF(WIN32 AND DUMPBIN_EXECUTABLE)
      EXECUTE_PROCESS(COMMAND
        "${DUMPBIN_EXECUTABLE}" "/dependents" "${FILE_NAME}"
        OUTPUT_VARIABLE DUMPBIN_OUTPUT
        RESULT_VARIABLE DUMPBIN_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      STRING(REPLACE "\n" ";" DUMPBIN_OUTPUT_LIST "${DUMPBIN_OUTPUT}")
      SET(DEPENDENCIES)
      FOREACH(LINE ${DUMPBIN_OUTPUT_LIST})
        STRING(REGEX MATCH "^[\r\n\t ]*([A-Za-z0-9_]*\.dll)" UNUSED ${LINE})
        IF(CMAKE_MATCH_1)
          LIST(APPEND DEPENDENCIES ${CMAKE_MATCH_1})
        ENDIF()
      ENDFOREACH()
      SET(${RETURN_VALUE} ${DEPENDENCIES} PARENT_SCOPE)
    ENDIF()
  ENDFUNCTION()
ENDIF()

IF(APPLE)
  FUNCTION(FIND_OBJECT_DEPENDENCIES FILE_NAME RETURN_VALUE)
    SET(${RETURN_VALUE} PARENT_SCOPE)
    EXECUTE_PROCESS(COMMAND otool -L ${FILE_NAME}
      OUTPUT_VARIABLE OTOOL_OUTPUT
      RESULT_VARIABLE OTOOL_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    IF(NOT OTOOL_RESULT EQUAL 0)
      MESSAGE(FATAL_ERROR "obool -L ${FILE_NAME} result: ${OTOOL_RESULT}")
    ENDIF()
    STRING(REPLACE "\n" ";" OTOOL_OUTPUT_LIST "${OTOOL_OUTPUT}")
    # Skip header, and the library itself
    LIST(REMOVE_AT OTOOL_OUTPUT_LIST 0 1)
    SET(DEPENDENCIES)
    FOREACH(LINE ${OTOOL_OUTPUT_LIST})
      IF(${LINE} MATCHES "@rpath")
        CONTINUE()
      ENDIF()
      # Strip off leading spaces/tabs, the compatibility comment,
      # and the library version (so that we return the symlink).
      STRING(REGEX REPLACE "^[\t ]+" "" LINE "${LINE}")
      STRING(REGEX REPLACE " \\(compatibility version.*" "" LINE "${LINE}")
      STRING(REGEX REPLACE "\\.[0-9]+\\.[0-9]\\.[0-9]" "" LINE "${LINE}")
      # MESSAGE(STATUS "xxx ${LINE}")
      LIST(APPEND DEPENDENCIES ${LINE})
    ENDFOREACH()
    # Sort it, for readability when debugging cmake code.
    LIST(SORT DEPENDENCIES)
    SET(${RETURN_VALUE} ${DEPENDENCIES} PARENT_SCOPE)
  ENDFUNCTION(FIND_OBJECT_DEPENDENCIES)
ENDIF(APPLE)

IF(LINUX)
  # Parse output of 'ldd ${FILE_NAME}' and return anything starting with lib.
  FUNCTION(FIND_LIBRARY_DEPENDENCIES FILE_NAME RETURN_VALUE)
    SET(${RETURN_VALUE} PARENT_SCOPE)
    EXECUTE_PROCESS(COMMAND
      ldd "${FILE_NAME}"
      OUTPUT_VARIABLE LDD_OUTPUT
      RESULT_VARIABLE LDD_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    STRING(REPLACE "\n" ";" LDD_OUTPUT_LIST "${LDD_OUTPUT}")
    SET(DEPENDENCIES)
    FOREACH(LINE ${LDD_OUTPUT_LIST})
      STRING(REGEX MATCH "^[\t ]+(lib[-+_A-Za-z0-9\\.]+).*" UNUSED ${LINE})
      IF(CMAKE_MATCH_1)
        # MESSAGE(STATUS "xxx ${FILE_NAME} ${CMAKE_MATCH_1}")
        LIST(APPEND DEPENDENCIES ${CMAKE_MATCH_1})
      ENDIF()
    ENDFOREACH()
    SET(${RETURN_VALUE} ${DEPENDENCIES} PARENT_SCOPE)
  ENDFUNCTION(FIND_LIBRARY_DEPENDENCIES)

  FUNCTION(FIND_OBJECT_DEPENDENCIES FILE_NAME RETURN_VALUE)
    SET(${RETURN_VALUE} PARENT_SCOPE)
    EXECUTE_PROCESS(COMMAND
      objdump -p "${FILE_NAME}"
      OUTPUT_VARIABLE OBJDUMP_OUTPUT
      RESULT_VARIABLE OBJDUMP_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    STRING(REPLACE "\n" ";" OBJDUMP_OUTPUT_LIST "${OBJDUMP_OUTPUT}")
    SET(DEPENDENCIES)
    FOREACH(LINE ${OBJDUMP_OUTPUT_LIST})
      STRING(REGEX MATCH
        "^[ ]+NEEDED[ ]+([-_A-Za-z0-9\\.]+)" UNUSED ${LINE})
      IF(CMAKE_MATCH_1)
        LIST(APPEND DEPENDENCIES ${CMAKE_MATCH_1})
      ENDIF()
    ENDFOREACH()
    SET(${RETURN_VALUE} ${DEPENDENCIES} PARENT_SCOPE)
  ENDFUNCTION()

  FUNCTION(FIND_SONAME FILE_NAME RETURN_VALUE)
    SET(${RETURN_VALUE} PARENT_SCOPE)
    EXECUTE_PROCESS(COMMAND
      objdump -p "${FILE_NAME}"
      OUTPUT_VARIABLE OBJDUMP_OUTPUT
      RESULT_VARIABLE OBJDUMP_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    STRING(REPLACE "\n" ";" OBJDUMP_OUTPUT_LIST "${OBJDUMP_OUTPUT}")
    FOREACH(LINE ${OBJDUMP_OUTPUT_LIST})
      STRING(REGEX MATCH
        "^[ ]+SONAME[ ]+([-_A-Za-z0-9\\.]+)" UNUSED ${LINE})
      IF(CMAKE_MATCH_1)
        SET(${RETURN_VALUE} ${CMAKE_MATCH_1} PARENT_SCOPE)
      ENDIF()
    ENDFOREACH()
  ENDFUNCTION()

  FUNCTION(VERIFY_CUSTOM_LIBRARY_DEPENDENCIES)
    FOREACH(lib ${KNOWN_CUSTOM_LIBRARIES})
      FOREACH(lib_needs ${NEEDED_${lib}})
        GET_FILENAME_COMPONENT(library_name_we "${lib_needs}" NAME_WE)
        SET(SONAME ${SONAME_${library_name_we}})
        IF(SONAME)
          MESSAGE(STATUS
            "${lib} needs ${lib_needs} from ${library_name_we}")
          IF(NOT "${lib_needs}" STREQUAL "${SONAME}")
            MESSAGE(WARNING "${library_name_we} provides ${SONAME}")
          ENDIF()
        ENDIF()
      ENDFOREACH()
    ENDFOREACH()
  ENDFUNCTION()

ENDIF()


# Adds a convenience target TARGET_NAME to show soname and dependent libs
# (and misc other info depending on platform) for FILE_NAME.
FUNCTION(ADD_OBJDUMP_TARGET TARGET_NAME FILE_NAME)
  CMAKE_PARSE_ARGUMENTS(ARG
    ""
    ""
    "DEPENDENCIES"
    ${ARGN}
    )

  IF(WIN32)
    SET(OBJDUMP_COMMAND "${DUMPBIN_EXECUTABLE}" /dependents /headers /exports)
  ELSEIF(APPLE)
    SET(OBJDUMP_COMMAND otool -L)
  ELSEIF(SOLARIS)
    SET(OBJDUMP_COMMAND elfdump -d)
  ELSE()
    SET(OBJDUMP_COMMAND objdump -p)
  ENDIF()

  ADD_CUSTOM_TARGET(${TARGET_NAME} COMMAND ${OBJDUMP_COMMAND} "${FILE_NAME}")

  IF(ARG_DEPENDENCIES)
    ADD_DEPENDENCIES(${TARGET_NAME} ${ARG_DEPENDENCIES})
  ENDIF()

ENDFUNCTION()
