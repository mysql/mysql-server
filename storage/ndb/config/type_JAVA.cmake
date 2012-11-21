# Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE(cmake_parse_arguments)

SET(JAVAC_TARGET "1.5")

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
  FILE(WRITE ${filename} "Manifest-Version: 1.0
Export-Package: ${EXPORTS}
Bundle-Name: ${NAME}
Bundle-Description: ClusterJ")
ENDMACRO(CREATE_MANIFEST)

MACRO(ADD_FILES_TO_JAR TARGET)

  MYSQL_PARSE_ARGUMENTS(ARG
    ""
    ""
    ${ARGN}
  )

  SET(JAVA_CLASSES ${ARG_DEFAULT_ARGS})

  LIST(LENGTH JAVA_CLASSES CLASS_LIST_LENGTH)
  MATH(EXPR EVEN "${CLASS_LIST_LENGTH}%2")
  IF(EVEN GREATER 0)
    MESSAGE(SEND_ERROR "CREATE_JAR_FROM_CLASSES has ${CLASS_LIST_LENGTH} but needs equal number of class parameters")
  ENDIF()

  MATH(EXPR CLASS_LIST_LENGTH "${CLASS_LIST_LENGTH} - 2")

  SET_JAVA_NDB_VERSION()

  FOREACH(I RANGE 0 ${CLASS_LIST_LENGTH} 2)

    MATH(EXPR J "${I} + 1")
    LIST(GET JAVA_CLASSES ${I} DIR)
    LIST(GET JAVA_CLASSES ${J} IT)
    SET(CLASS_DIRS -C ${DIR} ${IT})

    ADD_CUSTOM_COMMAND( TARGET  ${TARGET}.jar POST_BUILD
      COMMAND echo \"${JAVA_ARCHIVE} ufv ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar ${CLASS_DIRS}\"
      COMMAND ${JAVA_ARCHIVE} ufv ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar ${CLASS_DIRS}
      COMMENT "adding ${CLASS_DIRS} to target ${TARGET}-${JAVA_NDB_VERSION}.jar")

  ENDFOREACH(I RANGE 0 ${CLASS_LIST_LENGTH} 2)

ENDMACRO(ADD_FILES_TO_JAR)


MACRO(CREATE_JAR_FROM_CLASSES TARGET)

  MYSQL_PARSE_ARGUMENTS(ARG
    "DEPENDENCIES;MANIFEST"
    ""
    ${ARGN}
  )

  SET_JAVA_NDB_VERSION()

  ADD_CUSTOM_TARGET( ${TARGET}.jar ALL
      COMMAND echo \"${JAVA_ARCHIVE} cfvm ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar ${ARG_MANIFEST}\"
      COMMAND ${JAVA_ARCHIVE} cfvm ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar ${ARG_MANIFEST} )
  FOREACH(DEP ${ARG_DEPENDENCIES})
    ADD_DEPENDENCIES(${TARGET}.jar ${DEP})
  ENDFOREACH(DEP ${ARG_DEPENDENCIES})

  ADD_FILES_TO_JAR(${TARGET} "${ARG_DEFAULT_ARGS}")

ENDMACRO(CREATE_JAR_FROM_CLASSES)


# the target
MACRO(CREATE_JAR)

  MYSQL_PARSE_ARGUMENTS(ARG
    "CLASSPATH;DEPENDENCIES;MANIFEST;ENHANCE;EXTRA_FILES"
    ""
    ${ARGN}
  )

  LIST(GET ARG_DEFAULT_ARGS 0 TARGET)
  SET(JAVA_FILES ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT JAVA_FILES 0)

  SET (CLASS_DIR "target/classes")
  SET (JAR_DIR ".")

  FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CLASS_DIR})
  FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${JAR_DIR})
  SET(TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CLASS_DIR})

  SET_JAVA_NDB_VERSION()

  ADD_CUSTOM_TARGET( ${TARGET}.jar ALL
    COMMAND echo \"${JAVA_ARCHIVE} cfv ${JAR_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar -C ${CLASS_DIR} .\"
    COMMAND ${JAVA_ARCHIVE} cfv ${JAR_DIR}/${TARGET}-${JAVA_NDB_VERSION}.jar -C ${CLASS_DIR} .)

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


  IF(EXISTS ${ARG_ENHANCE})
    MESSAGE(STATUS "enhancing ${TARGET}.jar")
    SET(ENHANCER org.apache.openjpa.enhance.PCEnhancer)
    ADD_CUSTOM_COMMAND( TARGET ${TARGET}.jar PRE_BUILD
      COMMAND echo \"${JAVA_COMPILE} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath ${classpath_str} ${JAVA_FILES}\"
      COMMAND ${JAVA_COMPILE} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath ${classpath_str} ${JAVA_FILES}
      COMMAND echo \"${JAVA_RUNTIME} -classpath ${classpath_str}${separator}${WITH_CLASSPATH} ${ENHANCER} -p ${ARG_ENHANCE} -d ${TARGET_DIR}\"
      COMMAND ${JAVA_RUNTIME} -classpath "${classpath_str}${separator}${WITH_CLASSPATH}" ${ENHANCER} -p ${ARG_ENHANCE} -d ${TARGET_DIR}
    )
  ELSE()
    ADD_CUSTOM_COMMAND( TARGET ${TARGET}.jar PRE_BUILD
      COMMAND echo \"${JAVA_COMPILE} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath ${classpath_str} ${JAVA_FILES}\"
      COMMAND ${JAVA_COMPILE} -target ${JAVAC_TARGET} -source ${JAVAC_TARGET} -d ${TARGET_DIR} -classpath "${classpath_str}" ${JAVA_FILES}
    )
  ENDIF()

  LIST(LENGTH ARG_EXTRA_FILES LIST_LENGTH)
  IF(LIST_LENGTH GREATER 0)
    ADD_FILES_TO_JAR(${TARGET} "${ARG_EXTRA_FILES}")
  ENDIF()

  FOREACH(DEP ${ARG_DEPENDENCIES})
    ADD_DEPENDENCIES(${TARGET}.jar ${DEP})
  ENDFOREACH(DEP ${ARG_DEPENDENCIES})

ENDMACRO()
