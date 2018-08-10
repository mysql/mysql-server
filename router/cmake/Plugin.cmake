# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
#                    DESTINATION string
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
# If DESTINATION is provided it will be used as destination directory
# for plugins for install commands. DESTINATION is optional and
# defaults to ${HARNESS_INSTALL_LIBRARY_DIR}/${HARNESS_NAME}
# Note: on Windows ${HARNESS_INSTALL_LIBRARY_DIR} is always used
# as DESTINATION for plugins.
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
  cmake_parse_arguments(_option
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  IF(_option_UNPARSED_ARGUMENTS)
    MESSAGE(AUTHOR_WARNING
      "Unrecognized arguments: ${_option_UNPARSED_ARGUMENTS}")
  ENDIF()

  # Set default values
  IF(NOT _option_DESTINATION)
    SET(_option_DESTINATION "${HARNESS_INSTALL_LIBRARY_DIR}/${HARNESS_NAME}")
  ENDIF()

  # Set the log domain to the name of the plugin unless an explicit
  # log domain was given.
  IF(NOT _option_LOG_DOMAIN)
    SET(_option_LOG_DOMAIN "\"${NAME}\"")
  ENDIF()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before). We are using SHARED libraries since we
  # intend to link against it, which is something that MODULE does not
  # allow. On OSX, this means that the suffix for the library becomes
  # .dylib, which we do not want, so we reset it here.
  ADD_LIBRARY(${NAME} SHARED ${_option_SOURCES})
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

  SET_PATH_TO_SSL(${NAME} ${CMAKE_BINARY_DIR}/plugin_output_directory)

  # Add install rules to install the interface header files and the
  # plugin correctly.
  IF(NOT _option_NO_INSTALL AND HARNESS_INSTALL_PLUGINS)
    IF(WIN32)
      INSTALL(TARGETS ${NAME}
        RUNTIME DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
        COMPONENT Router)
      INSTALL(FILES $<TARGET_PDB_FILE:${NAME}>
            DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
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
