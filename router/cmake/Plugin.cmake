# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

# add_harness_plugin - Add a new plugin target and set install
#                      location
#
# add_harness_plugin(name [NO_INSTALL]
#                    LOG_DOMAIN domain
#                    SOURCES file1 ...
#                    INTERFACE directory
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
# the plugin from, while the files in the directory after INTERFACE
# will be installed alongside the header files for the harness.
#
# For plugins, it is necessary to set the RPATH so that the plugin can
# find other plugins when being loaded. This, unfortunately, means
# that the plugin path need to be set at compile time and cannot be
# changed after that.

FUNCTION(add_harness_plugin NAME)
  SET(_options NO_INSTALL)
  SET(_single_value LOG_DOMAIN INTERFACE DESTINATION OUTPUT_NAME)
  SET(_multi_value SOURCES REQUIRES)
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

  IF(NOT _option_NO_INSTALL)
    ADD_VERSION_INFO(SHARED _option_SOURCES Router)
  ENDIF()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before). We are using SHARED libraries since we
  # intend to link against it, which is something that MODULE does not
  # allow. On OSX, this means that the suffix for the library becomes
  # .dylib, which we do not want, so we reset it here.
  ADD_LIBRARY(${NAME} SHARED ${_option_SOURCES})
  TARGET_COMPILE_FEATURES(${NAME} PUBLIC cxx_std_20)

  # add plugin to build-all target
  ADD_DEPENDENCIES(mysqlrouter_all ${NAME})
  IF(_option_OUTPUT_NAME)
    SET_TARGET_PROPERTIES(${NAME}
      PROPERTIES OUTPUT_NAME ${_option_OUTPUT_NAME})
  ENDIF()
  TARGET_COMPILE_DEFINITIONS(${NAME} PRIVATE
    MYSQL_ROUTER_LOG_DOMAIN=${_option_LOG_DOMAIN})
  IF(NOT WIN32)
    SET_TARGET_PROPERTIES(${NAME} PROPERTIES
      PREFIX ""
      SUFFIX ".so")
  ENDIF()

  # Declare the interface directory for this plugin, if present. It
  # will be used both when compiling the plugin as well as as for any
  # dependent targets.
  IF(_option_INTERFACE)
    TARGET_INCLUDE_DIRECTORIES(${NAME}
      PUBLIC ${_option_INTERFACE})
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/${_option_INTERFACE}
        ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR})
  ENDIF()

  # Add a dependencies on interfaces for other plugins this plugin
  # requires.
  TARGET_LINK_LIBRARIES(${NAME}
    PUBLIC harness-library
    ${_option_REQUIRES})
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

    IF(_option_INTERFACE)
      FILE(GLOB interface_files ${_option_INTERFACE}/*.h)
      INSTALL(FILES ${interface_files}
        DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${HARNESS_NAME}
        COMPONENT Router)
    ENDIF()
  ENDIF()
ENDFUNCTION(add_harness_plugin)

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
    )
  SET(_multi_value
    INCLUDE_DIRECTORIES
    )
  CMAKE_PARSE_ARGUMENTS(_option
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  SET(ARGS ${_option_UNPARSED_ARGUMENTS})
  IF(NOT DEFINED _option_SOVERSION)
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

  ADD_SHARED_LIBRARY(${TARGET} ${ARGS}
    NAMELINK_SKIP
    )

  ADD_INSTALL_RPATH_FOR_OPENSSL(${TARGET})
  SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(${TARGET})

  IF(_option_PREFIX)
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES
      PREFIX "${_option_PREFIX}"
    )
  ENDIF()

  IF(NOT _option_NO_EXPORT_HEADER)
    TARGET_INCLUDE_DIRECTORIES(${TARGET}
      PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/
      ${CMAKE_CURRENT_BINARY_DIR}/../include/
      )
    GENERATE_EXPORT_HEADER(${TARGET}
      EXPORT_FILE_NAME
      ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/${TARGET}_export.h
      )
  ENDIF()

ENDFUNCTION()
