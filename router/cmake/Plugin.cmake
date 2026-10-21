# Copyright (c) 2015, 2026, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

FUNCTION(harness_normalize_target_name INPUT OUTPUT_VARIABLE)
  STRING(REPLACE - _ _out ${INPUT})
  SET(${OUTPUT_VARIABLE} "${_out}" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(harness_normalize_target_name INPUT OUTPUT_VARIABLE)
  STRING(REPLACE - _ _out ${INPUT})
  SET(${OUTPUT_VARIABLE} "${_out}" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(harness_target_dummy_file GENERATED_FILE_CC PLUGIN_VERSION PLUGIN_EXPORT_HEADER_FULL PLUGIN_EXPORT_MACRO_LOWER)
  STRING(TOUPPER "${PLUGIN_EXPORT_MACRO_LOWER}" PLUGIN_EXPORT_MACRO)

  SET(_INCLUDE_PATH "include")
  STRING(LENGTH "${_INCLUDE_PATH}" _INCLUDE_LENGTH)
  STRING(FIND "${PLUGIN_EXPORT_HEADER_FULL}" "${_INCLUDE_PATH}" POS)
  IF(NOT POS EQUAL -1)
      MATH(EXPR START_POS "${POS} + ${_INCLUDE_LENGTH} + 1")
      STRING(SUBSTRING "${PLUGIN_EXPORT_HEADER_FULL}" ${START_POS} -1 PLUGIN_EXPORT_HEADER)
  ELSE()
      SET(PLUGIN_EXPORT_HEADER "${PLUGIN_EXPORT_HEADER_FULL}")
  ENDIF()

  CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/plugin_version.cc.in ${GENERATED_FILE_CC} @ONLY)
ENDFUNCTION()

FUNCTION(harness_target_object_name TARGET output_var)
  SET(${output_var} "${TARGET}_object" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(harness_target_interface_name TARGET output_var)
  SET(${output_var} "${TARGET}_interface" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(harness_target_required_name TARGET output_var)
  SET(${output_var} "${TARGET}_required" PARENT_SCOPE)
ENDFUNCTION()


FUNCTION(harness_whitelisted_library NAME RESULT)
  SET(${RESULT} TRUE PARENT_SCOPE)
  # TODO(lkotula): switch to object/harness_stdx
  SET(allowed harness_stdx)
  FOREACH(item ${allowed})
    IF("${item}" STREQUAL "${NAME}")
      SET(${RESULT} TRUE PARENT_SCOPE)
    ENDIF()
  ENDFOREACH()
ENDFUNCTION()

FUNCTION(is_cmake_keyword NAME RESULT)
  SET(${RESULT} FALSE PARENT_SCOPE)
  IF("PUBLIC" STREQUAL "${NAME}")
    SET(${RESULT} TRUE PARENT_SCOPE)
  ELSEIF("PRIVATE" STREQUAL "${NAME}")
    SET(${RESULT} TRUE PARENT_SCOPE)
  ELSEIF("SYSTEM" STREQUAL "${NAME}")
    SET(${RESULT} TRUE PARENT_SCOPE)
  ELSEIF("LINK_LIBRARIES" STREQUAL "${NAME}")
    SET(${RESULT} TRUE PARENT_SCOPE)
  ENDIF()

ENDFUNCTION()


# add_harness_plugin - Add a new plugin target and set install
#                      location
#
# add_harness_plugin(name [NO_INSTALL]
#                    LOG_DOMAIN domain
#                    SOURCES file1 ...
#                    DESTINATION directory
#                    REQUIRES plugin ...)
#
# The add_harness_plugin command will set up a new plugin target and
# also set the install location of the target correctly.
#
# Plugins that are normally put under the "lib" directory of the build
# root, but see the caveat in the next paragraph.
#
# If NO_INSTALL is provided, it will not be installed, which is useful
# if the plugin is only for testing purposes. These plugins are also
# left in their default location and not moved to the "lib"
# directory. If you want to move the plugin to some specific
# directory, you have to set the target property
# LIBRARY_OUTPUT_DIRECTORY yourself.
#
# If LOG_DOMAIN is given, it will be used as the log domain for the
# plugin. If no LOG_DOMAIN is given, the log domain will be the name
# of the plugin.
#
# DESTINATION specifies the directory where plugins will be installed.
# Providing it is compulsory unless NO_INSTALL is provided.
#
# Files provided after the SOURCES keyword are the sources to build
# the plugin from.
#
# For plugins, it is necessary to set the RPATH so that the plugin can
# find other plugins when being loaded. This, unfortunately, means
# that the plugin path need to be set at compile time and cannot be
# changed after that.

FUNCTION(add_harness_plugin NAME)
  SET(_options NO_INSTALL NO_EXPORT_HEADER)
  SET(_single_value LOG_DOMAIN DESTINATION OUTPUT_NAME EXPORT_HEADER EXPORT_BASE_NAME)
  SET(_multi_value SOURCES REQUIRES REQUIRES_IF INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES_IF)
  CMAKE_PARSE_ARGUMENTS(_option
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  IF(_option_UNPARSED_ARGUMENTS)
    MESSAGE(AUTHOR_WARNING
      "Unrecognized arguments: ${_option_UNPARSED_ARGUMENTS}")
  ENDIF()

  # Set the log domain to the name of the plugin unless an explicit
  # log domain was given.
  IF(NOT _option_LOG_DOMAIN)
    SET(_option_LOG_DOMAIN "\"${NAME}\"")
  ENDIF()

  SET(LIB_FILE_LIST "")

  IF(NOT _option_NO_INSTALL)
    ADD_VERSION_INFO(SHARED LIB_FILE_LIST Router)
  ENDIF()

  IF(NOT _option_EXPORT_HEADER)
    SET(_option_EXPORT_HEADER
      "${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/${NAME}_export.h")
  ENDIF()

  IF(NOT _option_EXPORT_BASE_NAME)
    SET(_option_EXPORT_BASE_NAME "${NAME}")
  ENDIF()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before). We are using SHARED libraries since we
  # intend to link against it, which is something that MODULE does not
  # allow. On OSX, this means that the suffix for the library becomes
  # .dylib, which we do not want, so we reset it here.
  SET(DUMMY_FILE "${NAME}_harness_plugin.cc")
  HARNESS_TARGET_DUMMY_FILE(${DUMMY_FILE} 1 ${_option_EXPORT_HEADER} ${_option_EXPORT_BASE_NAME})
  LIST(APPEND  LIB_FILE_LIST ${DUMMY_FILE})

  HARNESS_TARGET_OBJECT_NAME(${NAME} OBJ_NAME)
  HARNESS_TARGET_INTERFACE_NAME(${NAME} IF_NAME)

  ADD_LIBRARY(${IF_NAME} INTERFACE)
  ADD_LIBRARY(${OBJ_NAME} OBJECT ${_option_SOURCES})
  ADD_LIBRARY(${NAME} SHARED
    ${LIB_FILE_LIST}
    $<TARGET_OBJECTS:${OBJ_NAME}>)

  TARGET_COMPILE_FEATURES(${IF_NAME} INTERFACE cxx_std_20)
  TARGET_COMPILE_FEATURES(${OBJ_NAME} PUBLIC cxx_std_20)
  TARGET_COMPILE_FEATURES(${NAME} PUBLIC cxx_std_20)
  IF(APPLE)
    TARGET_LINK_OPTIONS(${NAME} PRIVATE LINKER:-no_warn_duplicate_libraries)
  ENDIF()

  # add plugin to build-all target
  ADD_DEPENDENCIES(mysqlrouter_all ${NAME})
  IF(_option_OUTPUT_NAME)
    SET_TARGET_PROPERTIES(${NAME}
      PROPERTIES OUTPUT_NAME ${_option_OUTPUT_NAME})
  ENDIF()
  TARGET_COMPILE_DEFINITIONS(${OBJ_NAME} PRIVATE
    MYSQL_ROUTER_LOG_DOMAIN=${_option_LOG_DOMAIN} ${NAME}_EXPORTS=1  ${OBJ_NAME}_EXPORTS=1)
  TARGET_COMPILE_DEFINITIONS(${NAME} PRIVATE
    ${NAME}_EXPORTS=1  ${OBJ_NAME}_EXPORTS=1)
 IF(NOT WIN32)
    SET_TARGET_PROPERTIES(${NAME} PROPERTIES
      PREFIX ""
      SUFFIX ".so")
  ENDIF()

  # Add a dependencies on interfaces for other plugins this plugin
  # requires.
  FOREACH(item ${_option_REQUIRES})
    harness_whitelisted_library(${item} RESULT)
    IF(RESULT)
    ELSEIF(item MATCHES "_interface$")
    ELSE()
      MESSAGE(FATAL_ERROR "While processing plugin '${NAME}', following library '${item}' is neither whitelisted nor ends with '_interface'.")
    ENDIF()
  ENDFOREACH()
  TARGET_LINK_LIBRARIES(${OBJ_NAME}
    PUBLIC harness-library_interface
    PUBLIC ${_option_REQUIRES_IF}
    ${IF_NAME} )
  TARGET_LINK_LIBRARIES(${NAME}
    PUBLIC ${OBJ_NAME} harness-library
    ${_option_REQUIRES})

  IF(_option_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${OBJ_NAME} ${_option_INCLUDE_DIRECTORIES})
  ENDIF()

  IF(_option_INCLUDE_DIRECTORIES_IF)
    TARGET_INCLUDE_DIRECTORIES(${IF_NAME} INTERFACE ${_option_INCLUDE_DIRECTORIES_IF})
  ENDIF()

  # Need to be able to link plugins with each other
  IF(APPLE)
    SET_TARGET_PROPERTIES(${NAME} PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup")
  ENDIF()

  SET_TARGET_PROPERTIES(${NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
  )

  IF(APPLE_WITH_CUSTOM_SSL)
    ADD_CUSTOM_COMMAND(TARGET ${NAME} POST_BUILD
      COMMAND install_name_tool -change
              "${CRYPTO_VERSION}" "@loader_path/${CRYPTO_VERSION}"
               $<TARGET_FILE_NAME:${NAME}>
      COMMAND install_name_tool -change
              "${OPENSSL_VERSION}" "@loader_path/${OPENSSL_VERSION}"
               $<TARGET_FILE_NAME:${NAME}>
      WORKING_DIRECTORY
      ${CMAKE_BINARY_DIR}/plugin_output_directory/${CMAKE_CFG_INTDIR}
      )
  ENDIF()

  # Add install rules to install the interface header files and the
  # plugin correctly.
  IF(NOT _option_NO_INSTALL)
    IF(NOT _option_DESTINATION)
      MESSAGE(ERROR "Parameter 'DESTINATION' is mandatory unless 'NO_INSTALL' is provided.")
    ENDIF()

    IF(WIN32)
      INSTALL(TARGETS ${NAME}
        RUNTIME DESTINATION ${_option_DESTINATION}
        COMPONENT Router)
      INSTALL(FILES $<TARGET_PDB_FILE:${NAME}>
        DESTINATION ${_option_DESTINATION}
        COMPONENT Router)
    ELSE()
      INSTALL(TARGETS ${NAME}
        LIBRARY DESTINATION ${_option_DESTINATION}
        COMPONENT Router)
    ENDIF()
  ENDIF()

  #IF(NOT _option_NO_EXPORT_HEADER)
    IF(_option_EXPORT_BASE_NAME)
      LIST(APPEND extra_args BASE_NAME "${_option_EXPORT_BASE_NAME}")
    ELSE()
      LIST(APPEND extra_args BASE_NAME "${NAME}")
    ENDIF()
    GENERATE_EXPORT_HEADER(${OBJ_NAME}
      ${extra_args}
      EXPORT_FILE_NAME
      "${_option_EXPORT_HEADER}"
      )
    IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
      TARGET_INCLUDE_DIRECTORIES(${IF_NAME}
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/
        )
    ENDIF()
    TARGET_INCLUDE_DIRECTORIES(${IF_NAME}
      INTERFACE
      ${CMAKE_CURRENT_BINARY_DIR}/../include/
      )
   #ENDIF()
ENDFUNCTION(add_harness_plugin)


FUNCTION(harness_plugin_include_directories NAME)
  HARNESS_TARGET_OBJECT_NAME(${NAME} OBJ_NAME)

  TARGET_INCLUDE_DIRECTORIES(${OBJ_NAME} ${ARGN})
ENDFUNCTION(harness_plugin_include_directories)

#
# ROUTER_ADD_SHARED_LIBRARY(target sources [opts])
#
# @param target  targetname of the created shared library.
# @param sources source files of the shared library target.
# @param opts    extra options
#
# - creates a shared library "target"
# - with OUTPUT_NAME (default target-name)
# - installs into $ROUTER_INSTALL_LIBDIR (if SKIP_INSTALL is not specified)
# - generates a export-header in bindir/../include/mysqlrouter/{target}_export.h
#
# Options
# =======
#
# All options of ADD_SHARED_LIBRARY() are supported.
#
# Additionally,
#
# NO_EXPORT_HEADER
# :  do not generate a export-headers for the shared library.
#    default: generate a export-header
#
# PREFIX
# :  PREFIX of the shared library name (cmake property)
#
# OUTPUT_NAME
# :  OUTPUT_NAME of the shared library (cmake property)
#    default: targetname
#
# SOVERSION
# :  SOVERSION of the shared library (cmake property)
#    default: 1
#
# DESTINATION
# :  DESTINATION of the shared library (see MYSQL_INSTALL_TARGET)
#    default: $ROUTER_INSTALL_BINDIR on windows, $ROUTER_INSTALL_LIBDIR otherwise


FUNCTION(ROUTER_ADD_SHARED_LIBRARY TARGET)
  SET(_options
    NO_EXPORT_HEADER
    )
  SET(_single_value
    COMPONENT
    DESTINATION
    PREFIX
    SOVERSION
    EXPORT_HEADER
    EXPORT_BASE_NAME
    OUTPUT_NAME
    )
  SET(_multi_value
    INCLUDE_DIRECTORIES
    INCLUDE_DIRECTORIES_IF
    SOURCES
    LINK_LIBRARIES
    LINK_LIBRARIES_IF
    DEPENDENCIES
    COMPILE_DEFINITIONS
    )
  CMAKE_PARSE_ARGUMENTS(_option
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  SET(ARGS ${_option_UNPARSED_ARGUMENTS})
  IF(NOT DEFINED _option_SOVERSION)
    SET(_option_SOVERSION 1)
    LIST(APPEND ARGS SOVERSION 1)
  ELSE()
    LIST(APPEND ARGS SOVERSION ${_option_SOVERSION})
  ENDIF()
  IF(NOT DEFINED _option_COMPONENT)
    LIST(APPEND ARGS COMPONENT Router)
  ELSE()
    LIST(APPEND ARGS COMPONENT ${_option_COMPONENT})
  ENDIF()
  IF(NOT DEFINED _option_INCLUDE_DIRECTORIES)
    LIST(APPEND ARGS INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
    SET(_option_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
  ELSE()
    LIST(APPEND ARGS INCLUDE_DIRECTORIES ${_option_INCLUDE_DIRECTORIES})
  ENDIF()
  IF(NOT DEFINED _option_DESTINATION)
    # ADD_SHARED_LIBRARY calls MYSQL_INSTALL_TARGET which only knows
    # about a single DESTINATIONS, but we want
    # - .dll's in the BINDIR and
    # - .so in the LIBDIR.
    IF(WIN32)
      LIST(APPEND ARGS DESTINATION ${ROUTER_INSTALL_BINDIR})
    ELSE()
      LIST(APPEND ARGS DESTINATION ${ROUTER_INSTALL_LIBDIR})
    ENDIF()
  ELSE()
    LIST(APPEND ARGS DESTINATION ${_option_DESTINATION})
  ENDIF()
  IF(_option_OUTPUT_NAME)
    SET(_option_OUTPUT_NAME OUTPUT_NAME ${_option_OUTPUT_NAME})
  ENDIF()
  IF(_option_LINK_LIBRARIES)
    SET(_option_LINK_LIBRARIES LINK_LIBRARIES ${_option_LINK_LIBRARIES})
  ENDIF()
  SET(_other_target_DEPENDENCIES)
  IF(_option_DEPENDENCIES)
    SET(_other_target_DEPENDENCIES DEPENDENCIES ${_option_DEPENDENCIES})
  ENDIF()
  IF(NOT _option_EXPORT_HEADER)
    SET(_option_EXPORT_HEADER
      "${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/${TARGET}_export.h")
  ENDIF()
  IF(NOT _option_EXPORT_BASE_NAME)
    SET(_option_EXPORT_BASE_NAME "${TARGET}")
  ENDIF()

  SET(DUMMY_FILE "${TARGET}_library.cc")
  HARNESS_TARGET_DUMMY_FILE(${DUMMY_FILE} ${_option_SOVERSION} ${_option_EXPORT_HEADER} ${_option_EXPORT_BASE_NAME})

  HARNESS_TARGET_OBJECT_NAME(${TARGET} OBJ_NAME)
  HARNESS_TARGET_INTERFACE_NAME(${TARGET} IF_NAME)
  HARNESS_TARGET_REQUIRED_NAME(${TARGET} REQ_NAME)

  ADD_LIBRARY(${IF_NAME} INTERFACE)
  ADD_LIBRARY(${REQ_NAME} INTERFACE)
  ADD_LIBRARY(${OBJ_NAME} OBJECT ${_option_SOURCES})
  ADD_SHARED_LIBRARY(${TARGET}
    ${DUMMY_FILE}
    $<TARGET_OBJECTS:${OBJ_NAME}>
    ${_other_target_DEPENDENCIES}
    ${_option_LINK_LIBRARIES}
    ${_option_OUTPUT_NAME}
    ${ARGS}
    NAMELINK_SKIP
    )

  TARGET_COMPILE_FEATURES(${IF_NAME} INTERFACE cxx_std_20)
  TARGET_COMPILE_FEATURES(${OBJ_NAME} PUBLIC cxx_std_20)
  TARGET_COMPILE_FEATURES(${TARGET} PUBLIC cxx_std_20)

  #TODO(lktoula): use it directy in TARGET ?
  IF(_option_LINK_LIBRARIES)
    FOREACH(item ${_option_LINK_LIBRARIES})
      is_cmake_keyword(${item} _is_cmake_keyword)
      IF(NOT ${_is_cmake_keyword})
        TARGET_LINK_LIBRARIES(${REQ_NAME}
          INTERFACE ${item})
      ENDIF()
    ENDFOREACH()
  ENDIF()

  IF(_option_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${OBJ_NAME} PRIVATE
      ${_option_COMPILE_DEFINITIONS})
  ENDIF()

  harness_normalize_target_name("${OBJ_NAME}" _NORMALIZED_NAME)

  TARGET_COMPILE_DEFINITIONS(${OBJ_NAME} PRIVATE
    ${_NORMALIZED_NAME}_EXPORTS=1)
  TARGET_COMPILE_DEFINITIONS(${TARGET} PRIVATE
    ${_NORMALIZED_NAME}_EXPORTS=1)

  IF(_option_DEPENDENCIES)
    ADD_DEPENDENCIES(${IF_NAME} ${_option_DEPENDENCIES})
  ENDIF()

  IF(_option_INCLUDE_DIRECTORIES_IF)
    TARGET_INCLUDE_DIRECTORIES(${IF_NAME} INTERFACE ${_option_INCLUDE_DIRECTORIES_IF})
  ENDIF()

  IF(_option_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${OBJ_NAME} PRIVATE ${_option_INCLUDE_DIRECTORIES})
  ENDIF()

  IF(_option_LINK_LIBRARIES_IF)
    TARGET_LINK_LIBRARIES(${IF_NAME}
      INTERFACE ${_option_LINK_LIBRARIES_IF})
  ENDIF()

  TARGET_LINK_LIBRARIES(${OBJ_NAME}
    PUBLIC ${IF_NAME})

  TARGET_LINK_LIBRARIES(${TARGET}
    PUBLIC ${OBJ_NAME})

  ADD_INSTALL_RPATH_FOR_OPENSSL(${TARGET})
  SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(${TARGET})

  IF(_option_PREFIX)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
      PREFIX "${_option_PREFIX}"
    )
  ENDIF()

  # Always generate.
  #IF(NOT _option_NO_EXPORT_HEADER)
    SET(extra_args)
    IF(_option_EXPORT_BASE_NAME)
      LIST(APPEND extra_args BASE_NAME "${_option_EXPORT_BASE_NAME}")
    ELSE()
      LIST(APPEND extra_args BASE_NAME "${TARGET}")
    ENDIF()

    IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
      TARGET_INCLUDE_DIRECTORIES(${IF_NAME}
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/
        )
    ENDIF()

    TARGET_INCLUDE_DIRECTORIES(${IF_NAME}
      INTERFACE
      ${CMAKE_CURRENT_BINARY_DIR}/../include/
      )
    GENERATE_EXPORT_HEADER(${OBJ_NAME}
      ${extra_args}
      EXPORT_FILE_NAME
      "${_option_EXPORT_HEADER}"
      )
  #ENDIF()

ENDFUNCTION()
