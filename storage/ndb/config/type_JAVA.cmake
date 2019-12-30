# Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(libutils)
INCLUDE(cmake_parse_arguments)

SET(JAVAC_TARGET "1.7")

# Build (if not already done) NDB version string used for generating jars etc.
MACRO(SET_JAVA_NDB_VERSION)

  # Check that the NDB_VERSION_XX variables seems to have been set
  IF(NOT NDB_VERSION_MAJOR)
    MESSAGE(FATAL_ERROR "NDB_VERSION_MAJOR variable not set!")
  ENDIF()

  SET(JAVA_NDB_VERSION "${NDB_VERSION_MAJOR}.${NDB_VERSION_MINOR}.${NDB_VERSION_BUILD}")
  IF(NDB_VERSION_STATUS)
    SET(JAVA_NDB_VERSION "${JAVA_NDB_VERSION}.${NDB_VERSION_STATUS}")
  ENDIF()

  # MESSAGE(STATUS "JAVA_NDB_VERSION: ${JAVA_NDB_VERSION}")

ENDMACRO(SET_JAVA_NDB_VERSION)

MACRO(CREATE_MANIFEST filename EXPORTS NAME)
  FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${filename}" "Manifest-Version: 1.0
Export-Package: ${EXPORTS}
Bundle-Name: ${NAME}
Bundle-Description: ClusterJ")
ENDMACRO(CREATE_MANIFEST)

MACRO(CREATE_JAR)

  MYSQL_PARSE_ARGUMENTS(ARG
    "CLASSPATH;MERGE_JARS;DEPENDENCIES;MANIFEST;ENHANCE;EXTRA_FILES;BROKEN_JAVAC"
    ""
    ${ARGN}
  )

  LIST(GET ARG_DEFAULT_ARGS 0 TARGET)
  SET(JAVA_FILES ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT JAVA_FILES 0)

  SET (BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")
  SET (CLASS_DIR "${BUILD_DIR}/classes")
  SET (TARGET_DIR "${CLASS_DIR}")

  SET_JAVA_NDB_VERSION()

  # Concatenate the ARG_CLASSSPATH(a list of strings) into a string
  # with platform specific separator
  SET(separator) # Empty separator to start with
  SET(classpath_str)
  FOREACH(item ${ARG_CLASSPATH})
    SET(classpath_str "${classpath_str}${separator}${item}")
    IF (WIN32)
      # Quote the semicolon since it's cmakes list separator
      SET(separator "\;")
    ELSE()
      SET(separator ":")
    ENDIF()
  ENDFOREACH()
  # MESSAGE(STATUS "classpath_str: ${classpath_str}")

  # Target jar-file
  SET(JAR ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar)

  # Marker for dependency handling
  SET(MARKER_BASE ${BUILD_DIR}/create_jar)
  SET(COUNTER 0)
  SET(MARKER "${MARKER_BASE}.${COUNTER}")

  # Add target
  ADD_CUSTOM_TARGET(${TARGET}.jar ALL DEPENDS ${JAR})

  # Limit memory of javac, otherwise build might fail for parallel builds.
  # (out-of-memory/timeout if garbage collector kicks in too late)
  IF(SIZEOF_VOIDP EQUAL 8)
    SET(JAVA_ARGS "-J-Xmx3G")
  ELSE()
    SET(JAVA_ARGS "-J-Xmx1G")
  ENDIF()

  # Compile
  IF (JAVA_FILES)
    IF (ARG_BROKEN_JAVAC)
      ADD_CUSTOM_COMMAND(
        OUTPUT ${MARKER}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CLASS_DIR}
        COMMAND echo \"${JAVA_COMPILE} ${JAVA_ARGS} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath ${classpath_str} ${ARG_BROKEN_JAVAC}\"
        COMMAND ${JAVA_COMPILE} ${JAVA_ARGS} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath "${classpath_str}" ${ARG_BROKEN_JAVAC}
        COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
        DEPENDS ${JAVA_FILES}
        COMMENT "Building objects for ${TARGET}.jar"
      )
    ELSE()
      ADD_CUSTOM_COMMAND(
        OUTPUT ${MARKER}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CLASS_DIR}
        COMMAND echo \"${JAVA_COMPILE} ${JAVA_ARGS} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath ${classpath_str} ${JAVA_FILES}\"
        COMMAND ${JAVA_COMPILE} ${JAVA_ARGS} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath "${classpath_str}" ${JAVA_FILES}
        COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
        DEPENDS ${JAVA_FILES}
        COMMENT "Building objects for ${TARGET}.jar"
      )
    ENDIF()
  ELSE()
    ADD_CUSTOM_COMMAND(
      OUTPUT ${MARKER}
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CLASS_DIR}
      COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
      DEPENDS ${JAVA_FILES}
      COMMENT "No files to compile for ${TARGET}.jar"
    )
  ENDIF()

  # Copy extra files/directories
  FOREACH(F ${ARG_EXTRA_FILES})

    SET(OLD_MARKER ${MARKER})
    MATH(EXPR COUNTER "${COUNTER} + 1")
    SET(MARKER "${MARKER_BASE}.${COUNTER}")

    GET_FILENAME_COMPONENT(N ${F} NAME)
    IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${F})
      ADD_CUSTOM_COMMAND(
        OUTPUT ${MARKER}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${F} ${CLASS_DIR}/${N}
        COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
        DEPENDS ${F} ${OLD_MARKER}
        COMMENT "Adding directory ${N} to ${TARGET}.jar"
      )
    ELSE()
      ADD_CUSTOM_COMMAND(
        OUTPUT ${MARKER}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${F} ${CLASS_DIR}/${N}
        COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
        DEPENDS ${F} ${OLD_MARKER}
        COMMENT "Adding file ${N} to ${TARGET}.jar"
      )
    ENDIF()
  ENDFOREACH()

  # Unpack extra jars
  FOREACH(_J ${ARG_MERGE_JARS})

    SET(OLD_MARKER ${MARKER})
    MATH(EXPR COUNTER "${COUNTER} + 1")
    SET(MARKER "${MARKER_BASE}.${COUNTER}")

    GET_FILENAME_COMPONENT(P ${_J} PATH)
    GET_FILENAME_COMPONENT(N ${_J} NAME_WE)
    SET(J ${P}/${N}-${JAVA_NDB_VERSION}.jar)

    ADD_CUSTOM_COMMAND(
      OUTPUT ${MARKER}
      COMMAND ${JAVA_ARCHIVE} xf ${J}
      COMMAND ${CMAKE_COMMAND} -E touch ${MARKER}
      DEPENDS ${J} ${OLD_MARKER}
      WORKING_DIRECTORY ${CLASS_DIR}
      COMMENT "Unpacking ${N}.jar"
    )
  ENDFOREACH()

  # Create JAR
  SET(_ARG cf)
  IF(ARG_MANIFEST)
    SET(_ARG cfm)
  ENDIF()

  ADD_CUSTOM_COMMAND(
    OUTPUT ${JAR}
    COMMAND echo \"${JAVA_ARCHIVE} ${_ARG} ${JAR}.tmp ${ARG_MANIFEST} -C ${CLASS_DIR} .\"
    COMMAND ${JAVA_ARCHIVE} ${_ARG} ${JAR}.tmp ${ARG_MANIFEST} -C ${CLASS_DIR} .
    COMMAND ${CMAKE_COMMAND} -E copy ${JAR}.tmp ${JAR}
    COMMAND ${CMAKE_COMMAND} -E remove ${JAR}.tmp
    COMMENT "Creating ${TARGET}.jar"
    DEPENDS ${MARKER}
  )

  # Add CMake dependencies
  FOREACH(DEP ${ARG_DEPENDENCIES})
    ADD_DEPENDENCIES(${TARGET}.jar ${DEP})
  ENDFOREACH(DEP ${ARG_DEPENDENCIES})

ENDMACRO()
