# Copyright (c) 2009, 2024, Oracle and/or its affiliates.
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


# This file exports macros that emulate some functionality found  in GNU libtool
# on Unix systems. One such feature is convenience libraries. In this context,
# convenience library is a static library that can be linked to shared library
# On systems that force position-independent code, linking into shared library
# normally requires compilation with a special flag (often -fPIC).
# Some systems, like Windows or OSX do not need special compilation (Windows
# never uses PIC and OSX always uses it).
#
# The intention behind convenience libraries is simplify the build and to
# reduce excessive recompiles.

# Except for convenience libraries, this file provides macros to merge static
# libraries (we need it for mysqlclient) and to create shared library out of
# convenience libraries(again, for mysqlclient)

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

# Create a STATIC library ${TARGET} and populate its properties
FUNCTION(ADD_STATIC_LIBRARY TARGET)
  SET(LIBRARY_OPTIONS
    EXCLUDE_FROM_ALL
    EXCLUDE_FROM_PGO
  )
  SET(LIBRARY_ONE_VALUE_KW
  )
  SET(LIBRARY_MULTI_VALUE_KW
    COMPILE_DEFINITIONS # for TARGET_COMPILE_DEFINITIONS
    COMPILE_OPTIONS     # for TARGET_COMPILE_OPTIONS
    COMPILE_FEATURES    # for TARGET_COMPILE_FEATURES
    INCLUDE_DIRECTORIES # for TARGET_INCLUDE_DIRECTORIES
    SYSTEM_INCLUDE_DIRECTORIES # for TARGET_SYSTEM_INCLUDE_DIRECTORIES
    LINK_LIBRARIES      # for TARGET_LINK_LIBRARIES
    DEPENDENCIES        # for ADD_DEPENDENCIES
  )

  CMAKE_PARSE_ARGUMENTS(ARG
    "${LIBRARY_OPTIONS}"
    "${LIBRARY_ONE_VALUE_KW}"
    "${LIBRARY_MULTI_VALUE_KW}"
    ${ARGN}
  )

  IF(ARG_EXCLUDE_FROM_PGO)
    IF(FPROFILE_GENERATE)
      RETURN()
    ENDIF()
  ENDIF()

  ADD_LIBRARY(${TARGET} STATIC)
  TARGET_SOURCES(${TARGET} PRIVATE ${ARG_UNPARSED_ARGUMENTS})
  TARGET_COMPILE_FEATURES(${TARGET} PUBLIC cxx_std_20)

  # Collect all static libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

  IF(ARG_EXCLUDE_FROM_ALL)
    SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  # Add COMPILE_DEFINITIONS
  IF(ARG_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${TARGET} ${ARG_COMPILE_DEFINITIONS})
  ENDIF()

  # Add COMPILE_OPTIONS
  IF(ARG_COMPILE_OPTIONS)
    TARGET_COMPILE_OPTIONS(${TARGET} ${ARG_COMPILE_OPTIONS})
  ENDIF()

  # Add COMPILE_FEATURES
  IF(ARG_COMPILE_FEATURES)
    TARGET_COMPILE_FEATURES(${TARGET} ${ARG_COMPILE_FEATURES})
  ENDIF()

  # Add INCLUDE_DIRECTORIES
  IF(ARG_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${TARGET} ${ARG_INCLUDE_DIRECTORIES})
  ENDIF()

  # Add SYSTEM INCLUDE_DIRECTORIES
  IF(ARG_SYSTEM_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${TARGET} SYSTEM
      ${ARG_SYSTEM_INCLUDE_DIRECTORIES})
  ENDIF()

  # Add LINK_LIBRARIES
  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET} ${ARG_LINK_LIBRARIES})
  ENDIF()

  # Add DEPENDENCIES
  IF(ARG_DEPENDENCIES)
    ADD_DEPENDENCIES(${TARGET} ${ARG_DEPENDENCIES})
  ENDIF()
ENDFUNCTION()

# ADD_CONVENIENCE_LIBRARY(name sources... options/keywords...)
# Create an OBJECT library ${name}_objlib containing all object files.
# Create a STATIC library ${name} which can be used for linking.
#
# We use the OBJECT libraries for merging in MERGE_CONVENIENCE_LIBRARIES.
# For APPLE, we create a STATIC library only,
# see comments in MERGE_CONVENIENCE_LIBRARIES for Xcode
#
MACRO(ADD_CONVENIENCE_LIBRARY TARGET_ARG)
  SET(LIBRARY_OPTIONS
    EXCLUDE_FROM_ALL
    EXCLUDE_FROM_PGO
    )
  SET(LIBRARY_ONE_VALUE_KW
    )
  SET(LIBRARY_MULTI_VALUE_KW
    COMPILE_DEFINITIONS # for TARGET_COMPILE_DEFINITIONS
    COMPILE_OPTIONS     # for TARGET_COMPILE_OPTIONS
    DEPENDENCIES        # for ADD_DEPENDENCIES
    INCLUDE_DIRECTORIES # for TARGET_INCLUDE_DIRECTORIES
    LINK_LIBRARIES      # for TARGET_LINK_LIBRARIES
    SYSTEM_INCLUDE_DIRECTORIES
    )

  CMAKE_PARSE_ARGUMENTS(ARG
    "${LIBRARY_OPTIONS}"
    "${LIBRARY_ONE_VALUE_KW}"
    "${LIBRARY_MULTI_VALUE_KW}"
    ${ARGN}
    )

  SET(TARGET ${TARGET_ARG})
  SET(SOURCES ${ARG_UNPARSED_ARGUMENTS})

  # For APPLE, we create a STATIC library only,
  IF(APPLE)
    SET(TARGET_LIB ${TARGET})
    ADD_LIBRARY(${TARGET} STATIC ${SOURCES})
  ELSE()
    SET(TARGET_LIB ${TARGET}_objlib)
    ADD_LIBRARY(${TARGET_LIB} OBJECT ${SOURCES})
    ADD_LIBRARY(${TARGET} STATIC $<TARGET_OBJECTS:${TARGET_LIB}>)
  ENDIF()

  # Collect all static libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

  IF(ARG_EXCLUDE_FROM_PGO)
    IF(FPROFILE_GENERATE)
      SET(ARG_EXCLUDE_FROM_ALL TRUE)
    ENDIF()
  ENDIF()

  IF(ARG_EXCLUDE_FROM_ALL)
    SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  # Add COMPILE_DEFINITIONS to _objlib
  IF(ARG_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${TARGET_LIB} PRIVATE
      ${ARG_COMPILE_DEFINITIONS})
  ENDIF()

  # Add COMPILE_OPTIONS to _objlib
  IF(ARG_COMPILE_OPTIONS)
    TARGET_COMPILE_OPTIONS(${TARGET_LIB} PRIVATE
      ${ARG_COMPILE_OPTIONS})
  ENDIF()

  # Add DEPENDENCIES to _objlib
  IF(ARG_DEPENDENCIES)
    ADD_DEPENDENCIES(${TARGET_LIB} ${ARG_DEPENDENCIES})
  ENDIF()

  # Add INCLUDE_DIRECTORIES to _objlib
  IF(ARG_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${TARGET_LIB} PRIVATE
      ${ARG_INCLUDE_DIRECTORIES})
  ENDIF()

  # Add SYSTEM INCLUDE_DIRECTORIES to _objlib
  IF(ARG_SYSTEM_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${TARGET_LIB} SYSTEM PRIVATE
      ${ARG_SYSTEM_INCLUDE_DIRECTORIES})
  ENDIF()

  # Add LINK_LIBRARIES to static lib
  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET} ${ARG_LINK_LIBRARIES})
    FOREACH(lib ${ARG_LINK_LIBRARIES})
      IF(lib MATCHES "ext::")
        MESSAGE(STATUS "${TARGET_LIB} depends on ${lib}")
        TARGET_INCLUDE_DIRECTORIES(${TARGET_LIB} SYSTEM PRIVATE
          $<TARGET_PROPERTY:${lib},INTERFACE_INCLUDE_DIRECTORIES>)
      ENDIF()
    ENDFOREACH()
  ENDIF()

  # Keep track of known convenience libraries, in a global scope.
  SET(KNOWN_CONVENIENCE_LIBRARIES
    ${KNOWN_CONVENIENCE_LIBRARIES} ${TARGET} CACHE INTERNAL "" FORCE)

ENDMACRO()


# Creates a shared library by merging static libraries.
# MERGE_LIBRARIES_SHARED(target options/keywords ... source libs ...)
MACRO(MERGE_LIBRARIES_SHARED TARGET_ARG)
  SET(SHLIB_OPTIONS
    EXCLUDE_FROM_ALL
    EXCLUDE_FROM_PGO # add target, but do not build for FPROFILE_GENERATE
    LINK_PUBLIC # All source libs become part of the PUBLIC interface of target.
                # See documentation for INTERFACE_LINK_LIBRARIES
                # The default is STATIC, i.e. the property
                #   INTERFACE_LINK_LIBRARIES for the target library is empty.
    SKIP_INSTALL# Do not install it.
                # By default it will be installed to ${INSTALL_LIBDIR}
    NAMELINK_SKIP
    )
  SET(SHLIB_ONE_VALUE_KW
    COMPONENT   # Installation COMPONENT.
    DESTINATION # Where to install
    OUTPUT_NAME # Target library output name.
    SOVERSION   # API version.
    VERSION     # Build version.
    )
  SET(SHLIB_MULTI_VALUE_KW
    EXPORTS     # Symbols to be exported by the target library.
                # We force these symbols to be imported from the source libs.
    LINK_LIBRARIES # for TARGET_LINK_LIBRARIES
    )

  CMAKE_PARSE_ARGUMENTS(ARG
    "${SHLIB_OPTIONS}"
    "${SHLIB_ONE_VALUE_KW}"
    "${SHLIB_MULTI_VALUE_KW}"
    ${ARGN}
    )

  SET(TARGET ${TARGET_ARG})
  SET(LIBS_TO_MERGE ${ARG_UNPARSED_ARGUMENTS})

  CREATE_EXPORT_FILE(SRC ${TARGET} "${ARG_EXPORTS}")
  IF(UNIX)
    SET(export_link_flags)
    # Mark every export as explicitly needed, so that ld won't remove the
    # .a files containing them. This has a similar effect as
    # --Wl,--no-whole-archive, but is more focused.
    FOREACH(SYMBOL ${ARG_EXPORTS})
      IF(APPLE)
        LIST(APPEND export_link_flags "-Wl,-u,_${SYMBOL}")
      ELSE()
        LIST(APPEND export_link_flags "-Wl,-u,${SYMBOL}")
      ENDIF()
    ENDFOREACH()
  ENDIF()

  IF(ARG_EXCLUDE_FROM_PGO)
    IF(FPROFILE_GENERATE)
      SET(ARG_EXCLUDE_FROM_ALL TRUE)
      SET(ARG_SKIP_INSTALL TRUE)
    ENDIF()
  ENDIF()

  IF(NOT ARG_SKIP_INSTALL)
    ADD_VERSION_INFO(SHARED SRC "${ARG_COMPONENT}")
  ENDIF()
  ADD_LIBRARY(${TARGET} SHARED ${SRC})

  IF(ARG_EXCLUDE_FROM_ALL)
    IF(NOT ARG_SKIP_INSTALL)
      MESSAGE(FATAL_ERROR "EXCLUDE_FROM_ALL requires SKIP_INSTALL")
    ENDIF()
    SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  # Collect all dynamic libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
    )

  IF(WIN32_CLANG AND WITH_ASAN)
    TARGET_LINK_LIBRARIES(${TARGET} PRIVATE
      "${ASAN_LIB_DIR}/clang_rt.asan_dll_thunk-x86_64.lib")
  ENDIF()

  IF(ARG_LINK_PUBLIC)
    SET(PUBLIC_OR_PRIVATE PUBLIC)
  ELSE()
    SET(PUBLIC_OR_PRIVATE PRIVATE)
  ENDIF()
  TARGET_LINK_LIBRARIES(${TARGET} ${PUBLIC_OR_PRIVATE} ${LIBS_TO_MERGE})

  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET} PRIVATE ${ARG_LINK_LIBRARIES})
  ENDIF()

  IF(ARG_OUTPUT_NAME)
    SET_TARGET_PROPERTIES(
      ${TARGET} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  ENDIF()
  IF(ARG_SOVERSION)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES SOVERSION "${ARG_SOVERSION}")
  ENDIF()
  IF(ARG_VERSION)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES VERSION "${ARG_VERSION}")
  ENDIF()

  IF(APPLE)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES MACOSX_RPATH ON)
  ENDIF()

  TARGET_LINK_OPTIONS(${TARGET} PRIVATE ${export_link_flags})

  IF(APPLE_WITH_CUSTOM_SSL)
    SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(${TARGET})
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

  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT})
    ENDIF()
    IF(ARG_DESTINATION)
      SET(DESTINATION "${ARG_DESTINATION}")
    ELSE()
      SET(DESTINATION "${INSTALL_LIBDIR}")
    ENDIF()
    IF(ARG_NAMELINK_SKIP)
      SET(INSTALL_ARGS NAMELINK_SKIP)
    ENDIF()
    MYSQL_INSTALL_TARGET(${TARGET} DESTINATION "${DESTINATION}" ${COMP}
      ${INSTALL_ARGS})
  ENDIF()

  IF(WIN32)
    SET(LIBRARY_DIR "${CMAKE_BINARY_DIR}/library_output_directory")
    SET(RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime_output_directory")
    ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${LIBRARY_DIR}/${CMAKE_CFG_INTDIR}/$<TARGET_FILE_NAME:${TARGET}>"
      "${RUNTIME_DIR}/${CMAKE_CFG_INTDIR}/$<TARGET_FILE_NAME:${TARGET}>"
      )
  ENDIF()

  IF(UNIX)
    ADD_INSTALL_RPATH_FOR_OPENSSL(${TARGET})
  ENDIF()

  ADD_OBJDUMP_TARGET(show_${TARGET} "$<TARGET_FILE:${TARGET}>"
    DEPENDS ${TARGET})

ENDMACRO(MERGE_LIBRARIES_SHARED)


FUNCTION(GET_DEPENDEND_OS_LIBS target result)
  GET_TARGET_PROPERTY(TARGET_LIB_DEPENDS ${target} LINK_LIBRARIES)
  SET(MY_DEPENDENT_OS_LIBS)
  IF(TARGET_LIB_DEPENDS)
    LIST(REMOVE_DUPLICATES TARGET_LIB_DEPENDS)
    FOREACH(lib ${TARGET_LIB_DEPENDS})
      IF(lib MATCHES "${CMAKE_BINARY_DIR}")
        # This is a "custom/imported" system lib (libssl libcrypto)
        # MESSAGE(STATUS "GET_DEPENDEND_OS_LIBS ignore imported ${lib}")
      ELSEIF(TARGET ${lib})
        # This is one of our own libraries
        # MESSAGE(STATUS "GET_DEPENDEND_OS_LIBS ignore our ${lib}")
      ELSE()
        LIST(APPEND MY_DEPENDENT_OS_LIBS ${lib})
      ENDIF()
    ENDFOREACH()
  ENDIF()
  SET(${result} ${MY_DEPENDENT_OS_LIBS} PARENT_SCOPE)
ENDFUNCTION(GET_DEPENDEND_OS_LIBS)


MACRO(MERGE_CONVENIENCE_LIBRARIES TARGET_ARG)
  CMAKE_PARSE_ARGUMENTS(ARG
    "EXCLUDE_FROM_ALL;SKIP_INSTALL"
    "COMPONENT;OUTPUT_NAME"
    "LINK_LIBRARIES"
    ${ARGN}
    )

  SET(TARGET ${TARGET_ARG})
  SET(LIBS ${ARG_UNPARSED_ARGUMENTS})

  # Add a dummy source file, with non-empty content, to avoid warning:
  # libjson_binlog_static.a(json_binlog_static_depends.c.o) has no symbols
  SET(SOURCE_FILE
    ${CMAKE_BINARY_DIR}/archive_output_directory/${TARGET}_depends.c)
  SET(SOURCE_FILE_CONTENT "void dummy_${TARGET}_function() {}")
  CONFIGURE_FILE_CONTENT("${SOURCE_FILE_CONTENT}" "${SOURCE_FILE}")

  ADD_LIBRARY(${TARGET} STATIC ${SOURCE_FILE})
  MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
  IF(FPROFILE_USE AND HAS_MISSING_PROFILE)
    ADD_COMPILE_FLAGS(${SOURCE_FILE} COMPILE_FLAGS ${HAS_MISSING_PROFILE})
  ENDIF()

  IF(ARG_EXCLUDE_FROM_ALL)
    IF(NOT ARG_SKIP_INSTALL)
      MESSAGE(FATAL_ERROR "EXCLUDE_FROM_ALL requires SKIP_INSTALL")
    ENDIF()
    SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  # Collect all static libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

  # Go though the list of libraries.
  # Known convenience libraries should have type "STATIC_LIBRARY"
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
        LIST(APPEND MYLIBS ${LIB})
        GET_DEPENDEND_OS_LIBS(${LIB} LIB_OSLIBS)
        IF(LIB_OSLIBS)
          # MESSAGE(STATUS "GET_DEPENDEND_OS_LIBS ${LIB} : ${LIB_OSLIBS}")
          LIST(APPEND OSLIBS ${LIB_OSLIBS})
        ENDIF()
      ENDIF()
    ELSE()
      # 3rd party library like libz.so. This is a usage error of this macro.
      MESSAGE(FATAL_ERROR "Unknown 3rd party lib ${LIB}")
    ENDIF()
    # MESSAGE(STATUS "LIB ${LIB} LIB_TYPE ${LIB_TYPE}")
  ENDFOREACH()

  # Make the generated dummy source file depended on all static input
  # libs. If input lib changes,the source file is touched
  # which causes the desired effect (relink).
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND}  -E touch ${SOURCE_FILE}
    DEPENDS ${MYLIBS}
    )

  # For Xcode the merging of TARGET_OBJECTS does not work.
  # Rather than having a special implementation for Xcode only,
  # we always use libtool directly for merging libraries.
  IF(APPLE)
    SET(STATIC_LIBS_STRING)
    FOREACH(LIB ${MYLIBS})
      STRING_APPEND(STATIC_LIBS_STRING " $<TARGET_FILE:${LIB}>")
    ENDFOREACH()
    # Convert string to list
    STRING(REGEX REPLACE "[ ]+" ";" STATIC_LIBS_STRING "${STATIC_LIBS_STRING}" )
    ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
      COMMAND rm $<TARGET_FILE:${TARGET}>
      COMMAND /usr/bin/libtool -static -no_warning_for_no_symbols
      -o $<TARGET_FILE:${TARGET}>
      ${STATIC_LIBS_STRING}
      )
  ELSE()
    FOREACH(LIB ${MYLIBS})
      TARGET_SOURCES(${TARGET} PRIVATE $<TARGET_OBJECTS:${LIB}_objlib>)
    ENDFOREACH()
  ENDIF()

  # On Windows, ssleay32.lib/libeay32.lib or libssl.lib/libcrypto.lib
  # must be merged into mysqlclient.lib
  IF(WIN32 AND ${TARGET} STREQUAL "mysqlclient")
    SET(LINKER_EXTRA_FLAGS "")
    FOREACH(LIB OpenSSL::SSL OpenSSL::Crypto)
      GET_TARGET_PROPERTY(dot_lib_file ${LIB} IMPORTED_LOCATION)
      STRING_APPEND(LINKER_EXTRA_FLAGS " \"${dot_lib_file}\"")
    ENDFOREACH()

    # __NULL_IMPORT_DESCRIPTOR already defined, second definition ignored
    # Same symbol from both libssl and libcrypto
    # But: Lib.exe has no /IGNORE option, see
    # https://docs.microsoft.com/en-us/cpp/build/reference/running-lib?view=msvc-160
    # STRING_APPEND(LINKER_EXTRA_FLAGS " /IGNORE:LNK4006")

    SET_TARGET_PROPERTIES(${TARGET}
      PROPERTIES STATIC_LIBRARY_FLAGS "${LINKER_EXTRA_FLAGS}")
  ENDIF()

  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET} PRIVATE ${ARG_LINK_LIBRARIES})
  ENDIF()

  IF(OSLIBS)
    LIST(REMOVE_DUPLICATES OSLIBS)
    TARGET_LINK_LIBRARIES(${TARGET} PRIVATE ${OSLIBS})
    MESSAGE(STATUS "Library ${TARGET} depends on OSLIBS ${OSLIBS}")
  ENDIF()

  MESSAGE(STATUS "MERGE_CONVENIENCE_LIBRARIES TARGET ${TARGET}")
  MESSAGE(STATUS "MERGE_CONVENIENCE_LIBRARIES LIBS ${LIBS}")

  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT})
    ENDIF()
    IF(INSTALL_STATIC_LIBRARIES)
      MYSQL_INSTALL_TARGET(${TARGET} DESTINATION "${INSTALL_LIBDIR}" ${COMP})
    ENDIF()
  ENDIF()
ENDMACRO(MERGE_CONVENIENCE_LIBRARIES)


FUNCTION(ADD_SHARED_LIBRARY TARGET_ARG)
  SET(LIBRARY_OPTIONS
    EXCLUDE_FROM_ALL
    NO_UNDEFINED
    SKIP_INSTALL
    NAMELINK_SKIP
  )
  SET(LIBRARY_ONE_VALUE_KW
    COMPONENT
    DESTINATION
    LINUX_VERSION_SCRIPT
    OUTPUT_NAME
    SOVERSION
    VERSION
    WIN_DEF_FILE
    )
  SET(LIBRARY_MULTI_VALUE_KW
    COMPILE_DEFINITIONS # for TARGET_COMPILE_DEFINITIONS
    COMPILE_OPTIONS     # for TARGET_COMPILE_OPTIONS
    DEPENDENCIES        # for ADD_DEPENDENCIES
    INCLUDE_DIRECTORIES # for TARGET_INCLUDE_DIRECTORIES
    LINK_LIBRARIES      # for TARGET_LINK_LIBRARIES
    )

  CMAKE_PARSE_ARGUMENTS(ARG
    "${LIBRARY_OPTIONS}"
    "${LIBRARY_ONE_VALUE_KW}"
    "${LIBRARY_MULTI_VALUE_KW}"
    ${ARGN}
    )

  SET(TARGET ${TARGET_ARG})
  SET(SOURCES ${ARG_UNPARSED_ARGUMENTS})

  IF(NOT ARG_SKIP_INSTALL AND ARG_COMPONENT STREQUAL "Router")
    # add the version-info resource file to SOURCES
    ADD_VERSION_INFO(SHARED SOURCES Router)
  ENDIF()

  ADD_LIBRARY(${TARGET} SHARED ${SOURCES})
  TARGET_COMPILE_FEATURES(${TARGET} PUBLIC cxx_std_20)

  # Collect all shared libraries in the same directory
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
    )

  IF(ARG_EXCLUDE_FROM_ALL)
    SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${TARGET} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  IF(ARG_NO_UNDEFINED AND LINK_FLAG_NO_UNDEFINED)
    TARGET_LINK_OPTIONS(${TARGET} PRIVATE ${LINK_FLAG_NO_UNDEFINED})
  ENDIF()

  IF(NOT ARG_SKIP_INSTALL)
    IF(ARG_DESTINATION)
      SET(DESTINATION "${ARG_DESTINATION}")
    ELSEIF(WIN32)
      SET(DESTINATION "${INSTALL_BINDIR}")
    ELSE()
      SET(DESTINATION "${INSTALL_LIBDIR}")
    ENDIF()
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT})
    ELSE()
      SET(COMP COMPONENT SharedLibraries)
    ENDIF()
    IF(ARG_NAMELINK_SKIP)
      SET(INSTALL_ARGS NAMELINK_SKIP)
    ENDIF()
    MYSQL_INSTALL_TARGET(${TARGET} DESTINATION "${DESTINATION}" ${COMP}
      ${INSTALL_ARGS})
  ENDIF()

  IF(ARG_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${TARGET} PRIVATE ${ARG_COMPILE_DEFINITIONS})
  ENDIF()
  IF(ARG_COMPILE_OPTIONS)
    TARGET_COMPILE_OPTIONS(${TARGET} PRIVATE ${ARG_COMPILE_OPTIONS})
  ENDIF()
  IF(ARG_DEPENDENCIES)
    ADD_DEPENDENCIES(${TARGET} ${ARG_DEPENDENCIES})
  ENDIF()
  IF(ARG_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${TARGET} PRIVATE ${ARG_INCLUDE_DIRECTORIES})
  ENDIF()
  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${TARGET} ${ARG_LINK_LIBRARIES})
  ENDIF()
  IF(ARG_OUTPUT_NAME)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  ENDIF()
  IF(ARG_SOVERSION)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES SOVERSION "${ARG_SOVERSION}")
  ENDIF()

  IF(LINUX AND ARG_LINUX_VERSION_SCRIPT)
    TARGET_LINK_OPTIONS(${TARGET} PRIVATE
      LINKER:--version-script=${ARG_LINUX_VERSION_SCRIPT})
    SET_TARGET_PROPERTIES(${TARGET}
      PROPERTIES LINK_DEPENDS ${ARG_LINUX_VERSION_SCRIPT}
      )
  ENDIF()

  IF(ARG_VERSION)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES VERSION "${ARG_VERSION}")
    # Bug in cmake Visual Studio generator:
    # https://gitlab.kitware.com/cmake/cmake/-/issues/19618
    IF(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
      TARGET_LINK_OPTIONS(${TARGET} PRIVATE /VERSION:${ARG_VERSION})
    ENDIF()
  ENDIF()

  IF(WIN32)
    SET(LIBRARY_DIR "${CMAKE_BINARY_DIR}/library_output_directory")
    SET(RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime_output_directory")
    ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${LIBRARY_DIR}/${CMAKE_CFG_INTDIR}/$<TARGET_FILE_NAME:${TARGET}>"
      "${RUNTIME_DIR}/${CMAKE_CFG_INTDIR}/$<TARGET_FILE_NAME:${TARGET}>"
      )
    IF(ARG_WIN_DEF_FILE)
      TARGET_LINK_OPTIONS(${TARGET} PRIVATE /DEF:${ARG_WIN_DEF_FILE})
    ENDIF()
  ENDIF()

  ADD_OBJDUMP_TARGET(show_${TARGET} "$<TARGET_FILE:${TARGET}>"
    DEPENDS ${TARGET})

ENDFUNCTION(ADD_SHARED_LIBRARY)
