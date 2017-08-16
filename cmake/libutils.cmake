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
# normally requires compilation with a special flag (often -fPIC).
# Some systems, like Windows or OSX do not need special compilation (Windows 
# never uses PIC and OSX always uses it). 
#
# The intention behind convenience libraries is simplify the build and to reduce
# excessive recompiles.

# Except for convenience libraries, this file provides macros to merge static 
# libraries (we need it for mysqlclient) and to create shared library out of 
# convenience libraries(again, for mysqlclient)

# Following macros are exported
# - ADD_CONVENIENCE_LIBRARY(target source1...sourceN)
# This macro creates convenience library. The functionality is similar to 
# ADD_LIBRARY(target STATIC source1...sourceN), the difference is that resulting 
# library can always be linked to shared library
# 
# - MERGE_LIBRARIES(target [STATIC|SHARED|MODULE]  [linklib1 .... linklibN]
#  [EXPORTS exported_func1 .... exported_func_N]
#  [OUTPUT_NAME output_name]
# This macro merges several static libraries into a single one or creates a shared
# library from several convenience libraries

# Important global flags 
#
# - DISABLE_SHARED: If set, it is assumed that shared libraries are not produced
# during the build. ADD_CONVENIENCE_LIBRARY does not add anything to compile flags


GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
IF(WIN32 OR APPLE OR DISABLE_SHARED)
 SET(_SKIP_PIC 1)
ENDIF()

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)
# CREATE_EXPORT_FILE (VAR target api_functions)
# Internal macro, used on Windows to export API functions as dllexport.
# Returns a list of extra files that should be linked into the library
# (in the variable pointed to by VAR).
MACRO(CREATE_EXPORT_FILE VAR TARGET API_FUNCTIONS)
  SET(DUMMY ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_dummy.c)
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


# MYSQL_ADD_CONVENIENCE_LIBRARY(name source1...sourceN)
# Create static library that can be linked to shared library.
# On systems that force position-independent code, adds -fPIC or 
# equivalent flag to compile flags.
MACRO(ADD_CONVENIENCE_LIBRARY)
  SET(TARGET ${ARGV0})
  SET(SOURCES ${ARGN})
  LIST(REMOVE_AT SOURCES 0)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCES})
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

# Merge static libraries into a big static lib. The resulting library 
# should not not have dependencies on other static libraries.
# We use it in MySQL to merge mysys,dbug,vio etc into mysqlclient

MACRO(MERGE_STATIC_LIBS TARGET OUTPUT_NAME LIBS_TO_MERGE)
  # To produce a library we need at least one source file.
  # It is created by ADD_CUSTOM_COMMAND below and will
  # also help to track dependencies.
  SET(SOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_depends.c)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCE_FILE})
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

  SET(OSLIBS)
  FOREACH(LIB ${LIBS_TO_MERGE})
    GET_TARGET_PROPERTY(LIB_LOCATION ${LIB} LOCATION)
    GET_TARGET_PROPERTY(LIB_TYPE ${LIB} TYPE)
    IF(NOT LIB_LOCATION)
       # 3rd party library like libz.so. Make sure that everything
       # that links to our library links to this one as well.
       LIST(APPEND OSLIBS ${LIB})
    ELSE()
      # This is a target in current project
      # (can be a static or shared lib)
      IF(LIB_TYPE STREQUAL "STATIC_LIBRARY")
        SET(STATIC_LIBS ${STATIC_LIBS} ${LIB_LOCATION})
        ADD_DEPENDENCIES(${TARGET} ${LIB})
        # Extract dependend OS libraries
        GET_DEPENDEND_OS_LIBS(${LIB} LIB_OSLIBS)
        LIST(APPEND OSLIBS ${LIB_OSLIBS})
      ELSE()
        # This is a shared library our static lib depends on.
        LIST(APPEND OSLIBS ${LIB})
      ENDIF()
    ENDIF()
  ENDFOREACH()

  IF(OSLIBS)
    LIST(REMOVE_DUPLICATES OSLIBS)
    TARGET_LINK_LIBRARIES(${TARGET} ${OSLIBS})
    MESSAGE(STATUS "Library ${TARGET} depends on OSLIBS ${OSLIBS}")
  ENDIF()

  IF(STATIC_LIBS)
    LIST(REMOVE_DUPLICATES STATIC_LIBS)
  ENDIF()

  # Make the generated dummy source file depended on all static input
  # libs. If input lib changes,the source file is touched
  # which causes the desired effect (relink).
  ADD_CUSTOM_COMMAND( 
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND}  -E touch ${SOURCE_FILE}
    DEPENDS ${STATIC_LIBS})

  IF(MSVC)
    # To merge libs, just pass them to lib.exe command line.
    SET(LINKER_EXTRA_FLAGS "")
    FOREACH(LIB ${STATIC_LIBS})
      SET(LINKER_EXTRA_FLAGS "${LINKER_EXTRA_FLAGS} ${LIB}")
    ENDFOREACH()
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES STATIC_LIBRARY_FLAGS 
      "${LINKER_EXTRA_FLAGS}")
  ELSE()
    GET_TARGET_PROPERTY(TARGET_LOCATION ${TARGET} LOCATION)  
    IF(APPLE)
      # Use OSX's libtool to merge archives (ihandles universal 
      # binaries properly)
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND rm ${TARGET_LOCATION}
        COMMAND /usr/bin/libtool -static -o ${TARGET_LOCATION} 
        ${STATIC_LIBS}
      )  
    ELSE()
      # Generic Unix or MinGW. In post-build step, call
      # script, that extracts objects from archives with "ar x" 
      # and repacks them with "ar r"
      SET(TARGET ${TARGET})
      CONFIGURE_FILE(
        ${MYSQL_CMAKE_SCRIPT_DIR}/merge_archives_unix.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake 
        @ONLY
      )
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND rm ${TARGET_LOCATION}
        COMMAND ${CMAKE_COMMAND} -P 
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake
      )
    ENDIF()
  ENDIF()
ENDMACRO()

# Create libs from libs.
# Merges static libraries, creates shared libraries out of convenience libraries.
# MERGE_LIBRARIES(target [STATIC|SHARED|MODULE] 
#  [linklib1 .... linklibN]
#  [EXPORTS exported_func1 .... exportedFuncN]
#  [OUTPUT_NAME output_name]
#)
MACRO(MERGE_LIBRARIES)
  MYSQL_PARSE_ARGUMENTS(ARG
    "EXPORTS;OUTPUT_NAME;COMPONENT"
    "STATIC;SHARED;MODULE;SKIP_INSTALL"
    ${ARGN}
  )
  LIST(GET ARG_DEFAULT_ARGS 0 TARGET) 
  SET(LIBS ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT LIBS 0)
  IF(ARG_STATIC)
    IF (NOT ARG_OUTPUT_NAME)
      SET(ARG_OUTPUT_NAME ${TARGET})
    ENDIF()
    MERGE_STATIC_LIBS(${TARGET} ${ARG_OUTPUT_NAME} "${LIBS}") 
  ELSEIF(ARG_SHARED OR ARG_MODULE)
    IF(ARG_SHARED)
      SET(LIBTYPE SHARED)
    ELSE()
      SET(LIBTYPE MODULE)
    ENDIF()
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

    # Collect all dynamic libraries in the same directory
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

    TARGET_LINK_LIBRARIES(${TARGET} ${LIBS})
    IF(ARG_OUTPUT_NAME)
      SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
    ENDIF()
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES LINK_FLAGS "${export_link_flags}")
    IF(APPLE AND HAVE_CRYPTO_DYLIB AND HAVE_OPENSSL_DYLIB)
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND install_name_tool -change
                "${CRYPTO_VERSION}" "@loader_path/${CRYPTO_VERSION}"
                $<TARGET_SONAME_FILE:${TARGET}>
        COMMAND install_name_tool -change
                "${OPENSSL_VERSION}" "@loader_path/${OPENSSL_VERSION}"
                $<TARGET_SONAME_FILE:${TARGET}>
        COMMAND install_name_tool -id "$<TARGET_SONAME_FILE_NAME:${TARGET}>"
                $<TARGET_SONAME_FILE:${TARGET}>
        )
      # All executables have dependencies:  "@loader_path/../lib/xxx.dylib
      # Create a symlink so that this works for Xcode also.
      IF(NOT BUILD_IS_SINGLE_CONFIG)
        ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E create_symlink
                  $<TARGET_SONAME_FILE_DIR:${TARGET}> lib
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
          )
      ENDIF()
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unknown library type")
  ENDIF()
  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT}) 
    ENDIF()
    IF(ARG_SHARED OR ARG_MODULE OR INSTALL_STATIC_LIBRARIES)
      MYSQL_INSTALL_TARGETS(${TARGET} DESTINATION "${INSTALL_LIBDIR}" ${COMP})
    ENDIF()
  ENDIF()
ENDMACRO()

FUNCTION(GET_DEPENDEND_OS_LIBS target result)
  SET(deps ${${target}_LIB_DEPENDS})
  IF(deps)
   FOREACH(lib ${deps})
    # Filter out keywords for used for debug vs optimized builds
    IF(NOT lib MATCHES "general" AND NOT lib MATCHES "debug" AND NOT lib MATCHES "optimized")
      GET_TARGET_PROPERTY(lib_location ${lib} LOCATION)
      IF(NOT lib_location)
        SET(ret ${ret} ${lib})
      ENDIF()
    ENDIF()
   ENDFOREACH()
  ENDIF()
  SET(${result} ${ret} PARENT_SCOPE)
ENDFUNCTION()
