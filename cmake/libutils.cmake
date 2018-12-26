# Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.
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


# This file exports macros that emulate some functionality found  in GNU libtool
# on Unix systems. One such feature is convenience libraries. In this context,
# convenience library is a static library that can be linked to shared library
# On systems that force position-independent code, linking into shared library
# normally requires compilation with a special flag (often -fPIC). To enable 
# linking static libraries to shared, we compile source files that come into 
# static library with the PIC flag (${CMAKE_SHARED_LIBRARY_C_FLAGS} in CMake)
# Some systems, like Windows or OSX do not need special compilation (Windows 
# never uses PIC and OSX always uses it). 
#
# The intention behind convenience libraries is simplify the build and to reduce
# excessive recompiles.

# Except for convenience libraries, this file provides macros to merge static 
# libraries (we need it for mysqlclient) and to create shared library out of 
# convenience libraries(again, for mysqlclient)

# Important global flags 
# - WITH_PIC : If set, it is assumed that everything is compiled as position
# independent code (that is CFLAGS/CMAKE_C_FLAGS contain -fPIC or equivalent)
# If defined, ADD_CONVENIENCE_LIBRARY does not add PIC flag to compile flags
#
# - DISABLE_SHARED: If set, it is assumed that shared libraries are not produced
# during the build. ADD_CONVENIENCE_LIBRARY does not add anything to compile flags


GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
IF(WIN32 OR APPLE OR WITH_PIC OR DISABLE_SHARED OR NOT CMAKE_SHARED_LIBRARY_C_FLAGS)
 SET(_SKIP_PIC 1)
ENDIF()

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)
# CREATE_EXPORT_FILE (VAR target api_functions)
# Internal macro, used on Windows to export API functions as dllexport.
# Returns a list of extra files that should be linked into the library
# (in the variable pointed to by VAR).
MACRO(CREATE_EXPORT_FILE VAR TARGET API_FUNCTIONS)
  SET(DUMMY ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_dummy.cc)
  CONFIGURE_FILE_CONTENT("" ${DUMMY})
  IF(WIN32)
    SET(EXPORTS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_exports.def)
    SET(CONTENT "EXPORTS\n")
    FOREACH(FUNC ${API_FUNCTIONS})
      SET(CONTENT "${CONTENT} ${FUNC}\n")
    ENDFOREACH()
    CONFIGURE_FILE_CONTENT(${CONTENT} ${EXPORTS})
    SET(${VAR} ${DUMMY} ${EXPORTS})
  ELSE()
    SET(${VAR} ${DUMMY})
  ENDIF()
ENDMACRO()


# ADD_CONVENIENCE_LIBRARY(name source1...sourceN)
# Create static library that can be merged with other libraries.
# On systems that force position-independent code, adds -fPIC or 
# equivalent flag to compile flags.
MACRO(ADD_CONVENIENCE_LIBRARY)
  SET(TARGET ${ARGV0})
  SET(SOURCES ${ARGN})
  LIST(REMOVE_AT SOURCES 0)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCES})
  IF(NOT _SKIP_PIC)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  ENDIF()

  # Collect all static libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

  # Keep track of known convenience libraries, in a global scope.
  SET(KNOWN_CONVENIENCE_LIBRARIES
    ${KNOWN_CONVENIENCE_LIBRARIES} ${TARGET} CACHE INTERNAL "" FORCE)

  # Generate a cmake file which will save the name of the library.
  CONFIGURE_FILE(
    ${MYSQL_CMAKE_SCRIPT_DIR}/save_archive_location.cmake.in
    ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${TARGET}.cmake
    @ONLY)
  ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -DTARGET_NAME=${TARGET}
    -DTARGET_LOC=$<TARGET_FILE:${TARGET}>
    -DCFG_INTDIR=${CMAKE_CFG_INTDIR}
    -P ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${TARGET}.cmake
    )
ENDMACRO()


# An IMPORTED library can also be merged.
MACRO(ADD_IMPORTED_LIBRARY TARGET LOC)
  ADD_LIBRARY(${TARGET} STATIC IMPORTED)
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES IMPORTED_LOCATION ${LOC})
  SET(KNOWN_CONVENIENCE_LIBRARIES
    ${KNOWN_CONVENIENCE_LIBRARIES} ${TARGET} CACHE INTERNAL "" FORCE)
  CONFIGURE_FILE(
    ${MYSQL_CMAKE_SCRIPT_DIR}/save_archive_location.cmake.in
    ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${TARGET}.cmake
    @ONLY)
  ADD_CUSTOM_TARGET(${TARGET}_location
    COMMAND ${CMAKE_COMMAND}
    -DTARGET_NAME=${TARGET}
    -DTARGET_LOC=$<TARGET_FILE:${TARGET}>
    -DCFG_INTDIR=${CMAKE_CFG_INTDIR}
    -P ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${TARGET}.cmake
    )
ENDMACRO()

# Write content to file, using CONFIGURE_FILE
# The advantage compared to FILE(WRITE) is that timestamp
# does not change if file already has the same content
MACRO(CONFIGURE_FILE_CONTENT content file)
 SET(CMAKE_CONFIGURABLE_FILE_CONTENT 
  "${content}\n")
 CONFIGURE_FILE(
  ${MYSQL_CMAKE_SCRIPT_DIR}/configurable_file_content.in
  ${file}
  @ONLY)
ENDMACRO()

# Create libs from libs.
# Merges static libraries, creates shared libraries out of convenience libraries.
MACRO(MERGE_LIBRARIES_SHARED)
  MYSQL_PARSE_ARGUMENTS(ARG
    "EXPORTS;OUTPUT_NAME;COMPONENT"
    "SKIP_INSTALL"
    ${ARGN}
  )
  LIST(GET ARG_DEFAULT_ARGS 0 TARGET) 
  SET(LIBS ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT LIBS 0)
  
    SET(LIBTYPE SHARED)
    # check for non-PIC libraries
    IF(NOT _SKIP_PIC)
      FOREACH(LIB ${LIBS})
        GET_TARGET_PROPERTY(${LIB} TYPE LIBTYPE)
        IF(LIBTYPE STREQUAL "STATIC_LIBRARY")
          GET_TARGET_PROPERTY(LIB COMPILE_FLAGS LIB_COMPILE_FLAGS)
          IF(NOT LIB_COMPILE_FLAGS MATCHES "<PIC_FLAG>")
            MESSAGE(FATAL_ERROR 
            "Attempted to link non-PIC static library ${LIB} to shared library ${TARGET}\n"
            "Please use ADD_CONVENIENCE_LIBRARY, instead of ADD_LIBRARY for ${LIB}"
            )
          ENDIF()
        ENDIF()
      ENDFOREACH()
    ENDIF()
    CREATE_EXPORT_FILE(SRC ${TARGET} "${ARG_EXPORTS}")
    IF(UNIX)
      # Mark every export as explicitly needed, so that ld won't remove the .a files
      # containing them. This has a similar effect as --Wl,--no-whole-archive,
      # but is more focused.
      FOREACH(SYMBOL ${ARG_EXPORTS})
        IF(APPLE)
          SET(export_link_flags "${export_link_flags} -Wl,-u,_${SYMBOL}")
        ELSE()
          SET(export_link_flags "${export_link_flags} -Wl,-u,${SYMBOL}")
        ENDIF()
      ENDFOREACH()
    ENDIF()
    IF(NOT ARG_SKIP_INSTALL)
      ADD_VERSION_INFO(${TARGET} SHARED SRC)
    ENDIF()
    ADD_LIBRARY(${TARGET} ${LIBTYPE} ${SRC})
    TARGET_LINK_LIBRARIES(${TARGET} ${LIBS})
    IF(ARG_OUTPUT_NAME)
      SET_TARGET_PROPERTIES(
        ${TARGET} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
    ENDIF()
    SET_TARGET_PROPERTIES(
      ${TARGET} PROPERTIES LINK_FLAGS "${export_link_flags}")

  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT}) 
    ENDIF()

    MYSQL_INSTALL_TARGETS(${TARGET} DESTINATION "${INSTALL_LIBDIR}" ${COMP})

  ENDIF()
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_INTERFACE_LIBRARIES "")
ENDMACRO()


FUNCTION(GET_DEPENDEND_OS_LIBS target result)
  SET(deps ${${target}_LIB_DEPENDS})
  IF(deps)
   FOREACH(lib ${deps})
     # Filter out keywords for used for debug vs optimized builds
     IF(NOT lib MATCHES "general" AND
        NOT lib MATCHES "debug" AND
        NOT lib MATCHES "optimized")
      LIST(FIND KNOWN_CONVENIENCE_LIBRARIES ${lib} FOUNDIT)
      IF(FOUNDIT LESS 0)
        SET(ret ${ret} ${lib})
      ENDIF()
    ENDIF()
   ENDFOREACH()
  ENDIF()
  SET(${result} ${ret} PARENT_SCOPE)
ENDFUNCTION()


MACRO(MERGE_CONVENIENCE_LIBRARIES)
  MYSQL_PARSE_ARGUMENTS(ARG
    "OUTPUT_NAME;COMPONENT"
    "SKIP_INSTALL"
    ${ARGN}
    )
  LIST(GET ARG_DEFAULT_ARGS 0 TARGET)
  SET(LIBS ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT LIBS 0)

  SET(SOURCE_FILE
    ${CMAKE_BINARY_DIR}/archive_output_directory/${TARGET}_depends.c)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCE_FILE})
  IF(ARG_OUTPUT_NAME)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  ENDIF()

  # Collect all static libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

  # Go though the list of libraries.
  # Known convenience libraries should have type "STATIC_LIBRARY"
  # We assume that that unknown libraries (type "LIB_TYPE-NOTFOUND")
  # are operating system libraries, to be linked with TARGET
  SET(OSLIBS)
  SET(MYLIBS)
  FOREACH(LIB ${LIBS})
    GET_TARGET_PROPERTY(LIB_TYPE ${LIB} TYPE)
    IF(LIB_TYPE STREQUAL "STATIC_LIBRARY")
      LIST(FIND KNOWN_CONVENIENCE_LIBRARIES ${LIB} FOUNDIT)
      IF(FOUNDIT LESS 0)
        MESSAGE(STATUS "Known libs : ${KNOWN_CONVENIENCE_LIBRARIES}")
        MESSAGE(FATAL_ERROR "Unknown static library ${LIB} FOUNDIT ${FOUNDIT}")
      ELSE()
        ADD_DEPENDENCIES(${TARGET} ${LIB})
        GET_TARGET_PROPERTY(loc ${LIB} IMPORTED_LOCATION)
        IF(loc)
          ADD_DEPENDENCIES(${TARGET} ${LIB}_location)
        ENDIF()
        LIST(APPEND MYLIBS ${LIB})
        GET_DEPENDEND_OS_LIBS(${LIB} LIB_OSLIBS)
        IF(LIB_OSLIBS)
          # MESSAGE(STATUS "GET_DEPENDEND_OS_LIBS ${LIB} : ${LIB_OSLIBS}")
          LIST(APPEND OSLIBS ${LIB_OSLIBS})
        ENDIF()
      ENDIF()
    ELSE()
      # 3rd party library like libz.so. Make sure that everything
      # that links to our library links to this one as well.
      LIST(APPEND OSLIBS ${LIB})
    ENDIF()
    # MESSAGE(STATUS "LIB ${LIB} LIB_TYPE ${LIB_TYPE}")
  ENDFOREACH()

  IF(OSLIBS)
    LIST(REMOVE_DUPLICATES OSLIBS)
    TARGET_LINK_LIBRARIES(${TARGET} ${OSLIBS})
    MESSAGE(STATUS "Library ${TARGET} depends on OSLIBS ${OSLIBS}")
  ENDIF()

  # Make the generated dummy source file depended on all static input
  # libs. If input lib changes,the source file is touched
  # which causes the desired effect (relink).
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND}  -E touch ${SOURCE_FILE}
    DEPENDS ${MYLIBS}
    )

  MESSAGE(STATUS "MERGE_CONVENIENCE_LIBRARIES TARGET ${TARGET}")
  MESSAGE(STATUS "MERGE_CONVENIENCE_LIBRARIES LIBS ${LIBS}")
  MESSAGE(STATUS "MERGE_CONVENIENCE_LIBRARIES MYLIBS ${MYLIBS}")

  CONFIGURE_FILE(
    ${MYSQL_CMAKE_SCRIPT_DIR}/merge_archives.cmake.in
    ${CMAKE_BINARY_DIR}/archive_output_directory/lib_merge_${TARGET}.cmake
    @ONLY)
  ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -DTARGET_NAME=${TARGET}
    -DTARGET_LOC=$<TARGET_FILE:${TARGET}>
    -DCFG_INTDIR=${CMAKE_CFG_INTDIR}
    -P ${CMAKE_BINARY_DIR}/archive_output_directory/lib_merge_${TARGET}.cmake
    COMMENT "Merging library ${TARGET}"
    )

  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT})
    ENDIF()
    MYSQL_INSTALL_TARGETS(${TARGET} DESTINATION "${INSTALL_LIBDIR}" ${COMP})
  ENDIF()
ENDMACRO()
