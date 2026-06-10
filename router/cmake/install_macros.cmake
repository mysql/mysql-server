# Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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


# Move the custom shared library and symlinks to library_output_directory.
#
# We ensure that the copied custom libraries have the execute bit set.
# The following macro is duplicated in `cmake/install_macros.cmake`,
# where this instance is being adjusted to handle JIT external
# library, used by router.
#
# Set ${OUTPUT_LIBRARY_NAME} to the new location.
# Set ${OUTPUT_TARGET_NAME} to the name of a target which will do the copying.
# Add an INSTALL(FILES ....) rule to install library and symlinks into
#   ${INSTALL_PRIV_LIBDIR}
FUNCTION(ROUTER_COPY_CUSTOM_SHARED_LIBRARY library_full_filename
    OUTPUT_LIBRARY_NAME
    OUTPUT_TARGET_NAME
    )
  GET_FILENAME_COMPONENT(LIBRARY_EXT "${library_full_filename}" EXT)
  IF(NOT LIBRARY_EXT STREQUAL ".so")
    RETURN()
  ENDIF()
  GET_FILENAME_COMPONENT(library_directory "${library_full_filename}" DIRECTORY)
  GET_FILENAME_COMPONENT(library_name "${library_full_filename}" NAME)
  GET_FILENAME_COMPONENT(library_name_we "${library_full_filename}" NAME_WE)

  EXECUTE_PROCESS(
    COMMAND readlink "${library_full_filename}" OUTPUT_VARIABLE library_version
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  FIND_SONAME(${library_full_filename} library_soname)
  FIND_OBJECT_DEPENDENCIES(${library_full_filename} library_dependencies)

  if("${library_version}" STREQUAL "")
      SET(library_version ${library_name})
  ENDIF()
  IF("${library_soname}" STREQUAL "")
      SET(library_soname ${library_name})
  ENDIF()

  MESSAGE(STATUS "CUSTOM library ${library_full_filename}")
  #MESSAGE(STATUS "CUSTOM version ${library_version}")
  #MESSAGE(STATUS "CUSTOM directory ${library_directory}")
  #MESSAGE(STATUS "CUSTOM name ${library_name}")
  #MESSAGE(STATUS "CUSTOM name_we ${library_name_we}")
  #MESSAGE(STATUS "CUSTOM soname ${library_soname}")

  SET(COPIED_LIBRARY_NAME
    "${CMAKE_BINARY_DIR}/library_output_directory/${library_name}")
  SET(COPY_TARGET_NAME "copy_${library_name_we}_dll")

  # Keep track of libraries and dependencies.
  SET(SONAME_${library_name_we} "${library_soname}"
    CACHE INTERNAL "SONAME for ${library_name_we}" FORCE)
  SET(NEEDED_${library_name_we} "${library_dependencies}"
    CACHE INTERNAL "" FORCE)

  # Do copying and patching in a sub-process, so that we can skip it if
  # already done. The BYPRODUCTS arguments is needed by Ninja, and is
  # ignored on non-Ninja generators except to mark byproducts GENERATED.
  ADD_CUSTOM_TARGET(${COPY_TARGET_NAME} ALL
    COMMAND ${CMAKE_COMMAND}
    -Dlibrary_directory="${library_directory}"
    -Dlibrary_name="${library_name}"
    -Dlibrary_soname="${library_soname}"
    -Dlibrary_version="${library_version}"
    -P ${CMAKE_SOURCE_DIR}/router/cmake/copy_custom_library.cmake

    BYPRODUCTS
    "${CMAKE_BINARY_DIR}/library_output_directory/${library_name}"

    WORKING_DIRECTORY
    "${CMAKE_BINARY_DIR}/library_output_directory/"
    )

  # Link with the copied library, rather than the original one.
  SET(${OUTPUT_LIBRARY_NAME} "${COPIED_LIBRARY_NAME}" PARENT_SCOPE)
  SET(${OUTPUT_TARGET_NAME} "${COPY_TARGET_NAME}" PARENT_SCOPE)

  ADD_DEPENDENCIES(copy_custom_libraries ${COPY_TARGET_NAME})

  MESSAGE(STATUS "INSTALL ${library_name} to ${INSTALL_PRIV_LIBDIR}")

  # Cannot use INSTALL_PRIVATE_LIBRARY because these are not targets.
  INSTALL(FILES
    ${CMAKE_BINARY_DIR}/library_output_directory/${library_name}
    ${CMAKE_BINARY_DIR}/library_output_directory/${library_soname}
    ${CMAKE_BINARY_DIR}/library_output_directory/${library_version}
    DESTINATION "${ROUTER_INSTALL_LIBDIR}" COMPONENT Router
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    )
ENDFUNCTION(ROUTER_COPY_CUSTOM_SHARED_LIBRARY)


# For 3rd party .dlls on Windows.
# Adds a target which copies the .dll to library_output_directory
# Adds INSTALL(FILES ....) rule to install the .dll to ${ROUTER_INSTALL_PLUGINDIR}.
# Looks for matching .pdb file, and installs it if found.
# The following macro is duplicated in `cmake/install_macros.cmake`,
# where this instance is being adjusted to handle JIT external
# library, used by router.
#
# Sets ${OUTPUT_TARGET_NAME} to the name of a target which will do the copying.
FUNCTION(ROUTER_COPY_CUSTOM_DLL library_full_filename OUTPUT_LIBRARY_NAME OUTPUT_TARGET_NAME)
  IF(NOT WIN32)
    RETURN()
  ENDIF()
  GET_FILENAME_COMPONENT(library_directory "${library_full_filename}" DIRECTORY)
  GET_FILENAME_COMPONENT(library_name "${library_full_filename}" NAME)
  GET_FILENAME_COMPONENT(library_name_we "${library_full_filename}" NAME_WE)

  SET(LIBRARY_DIR "${CMAKE_BINARY_DIR}/plugin_output_directory")
  SET(SOURCE_DLL_NAME "${library_directory}/${library_name_we}.dll")
  SET(COPIED_LIBRARY_NAME "${LIBRARY_DIR}/${CMAKE_CFG_INTDIR}/${library_name_we}.dll")
  SET(COPY_TARGET_NAME "copy_${library_name_we}_dll")

  ADD_CUSTOM_COMMAND(
    OUTPUT "${COPIED_LIBRARY_NAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SOURCE_DLL_NAME}" "${COPIED_LIBRARY_NAME}"
    )
  MY_ADD_CUSTOM_TARGET(${COPY_TARGET_NAME} ALL
    DEPENDS "${COPIED_LIBRARY_NAME}"
    )

  # Install the original file, to avoid referring to CMAKE_CFG_INTDIR.
  MESSAGE(STATUS "INSTALL ${SOURCE_DLL_NAME} to ${ROUTER_INSTALL_PLUGINDIR}")
  INSTALL(FILES "${SOURCE_DLL_NAME}"
    DESTINATION "${ROUTER_INSTALL_PLUGINDIR}" COMPONENT Router
    )

  SET(${OUTPUT_LIBRARY_NAME} "${library_full_filename}" PARENT_SCOPE)
  SET(${OUTPUT_TARGET_NAME} "${COPY_TARGET_NAME}" PARENT_SCOPE)

  FIND_FILE(HAVE_${library_name_we}_PDB
    NAMES "${library_name_we}.pdb"
    PATHS "${library_directory}"
    NO_DEFAULT_PATH
    )
  IF(HAVE_${library_name_we}_PDB)
    SET(COPIED_PDB_NAME
      "${LIBRARY_DIR}/${CMAKE_CFG_INTDIR}/${library_name_we}.pdb")
    SET(COPY_TARGET_PDB_NAME "copy_${library_name_we}_pdb")

    ADD_CUSTOM_COMMAND(
      OUTPUT "${COPIED_PDB_NAME}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${HAVE_${library_name_we}_PDB}" "${COPIED_PDB_NAME}"
      )
    MY_ADD_CUSTOM_TARGET(${COPY_TARGET_PDB_NAME} ALL
      DEPENDS "${COPIED_PDB_NAME}"
      )

    MESSAGE(STATUS
      "INSTALL ${HAVE_${library_name_we}_PDB} to ${ROUTER_INSTALL_PLUGINDIR}")
    INSTALL(FILES "${HAVE_${library_name_we}_PDB}"
      DESTINATION "${ROUTER_INSTALL_PLUGINDIR}" COMPONENT Router
      )
  ENDIF()
ENDFUNCTION(ROUTER_COPY_CUSTOM_DLL)

# For 3rd party .dylib on MACOS.
# Adds a target which copies the .dylib to library_output_directory
# Adds INSTALL(FILES ....) rule to install the .dylib to
#   ${ROUTER_INSTALL_PLUGINDIR}.
# Sets ${OUTPUT_TARGET_NAME} to the name of a target which will do the copying.
FUNCTION(ROUTER_COPY_CUSTOM_DYLIB library_full_filename
    OUTPUT_LIBRARY_NAME OUTPUT_TARGET_NAME)
  IF(NOT APPLE)
    RETURN()
  ENDIF()
  FIND_OBJECT_DEPENDENCIES("${library_full_filename}" LIBRARY_DEPS)

  GET_FILENAME_COMPONENT(library_directory "${library_full_filename}" DIRECTORY)
  GET_FILENAME_COMPONENT(library_name "${library_full_filename}" NAME)
  GET_FILENAME_COMPONENT(library_name_we "${library_full_filename}" NAME_WE)

  SET(LIBRARY_DIR "${CMAKE_BINARY_DIR}/library_output_directory")
  SET(PLUGIN_DIR "${CMAKE_BINARY_DIR}/plugin_output_directory")

  IF(BUILD_IS_SINGLE_CONFIG)
    SET(COPIED_LIBRARY_NAME "${LIBRARY_DIR}/${library_name}")
    SET(LINK_PLUGIN_DIR "${PLUGIN_DIR}")
  ELSE()
    SET(COPIED_LIBRARY_NAME "${LIBRARY_DIR}/${CMAKE_CFG_INTDIR}/${library_name}")
    SET(LINK_PLUGIN_DIR "${PLUGIN_DIR}/${CMAKE_CFG_INTDIR}")
  ENDIF()

  SET(COPY_TARGET_NAME "copy_${library_name_we}_dylib")
  SET(LINK_TARGET_NAME "link_${library_name_we}_dylib")
  SET(BIND_TARGET_NAME "BIND_${library_name_we}_dylib")


  ADD_CUSTOM_TARGET(${COPY_TARGET_NAME} ALL
    COMMAND ${CMAKE_COMMAND}
    -Dlibrary_full_filename="${library_full_filename}"
    -Dlibrary_directory="${library_directory}"
    -Dlibrary_name="${library_name}"
    -Dlibrary_version="${library_name}"
    -Dsubdir=""
    -DLIBRARY_DEPS="${LIBRARY_DEPS}"
    -DPLUGIN_DIR="${CMAKE_BINARY_DIR}/plugin_output_directory"

    -P ${CMAKE_SOURCE_DIR}/cmake/copy_custom_dylib.cmake

    BYPRODUCTS
    ${COPIED_LIBRARY_NAME}

    WORKING_DIRECTORY
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}"
    )

  MY_ADD_CUSTOM_TARGET(${BIND_TARGET_NAME} ALL
    DEPENDS "${COPIED_LIBRARY_NAME}"
    )

  # It seems this is not actually needed:
  IF(NOT BUILD_IS_SINGLE_CONFIG)
    ADD_CUSTOM_TARGET(${LINK_TARGET_NAME} ALL
      COMMAND ${CMAKE_COMMAND} -E create_symlink
      "${COPIED_LIBRARY_NAME}" "${library_name}"
      WORKING_DIRECTORY "${LINK_PLUGIN_DIR}"
      )
    ADD_DEPENDENCIES(${BIND_TARGET_NAME} ${LINK_TARGET_NAME})
  ENDIF()

  MESSAGE(STATUS
    "INSTALL ${library_full_filename} to ${ROUTER_INSTALL_PLUGINDIR}")
  INSTALL(FILES "${COPIED_LIBRARY_NAME}"
    DESTINATION "${ROUTER_INSTALL_PLUGINDIR}" COMPONENT Router
    )

  SET(${OUTPUT_LIBRARY_NAME} "${COPIED_LIBRARY_NAME}" PARENT_SCOPE)
  SET(${OUTPUT_TARGET_NAME} "${BIND_TARGET_NAME}" PARENT_SCOPE)
ENDFUNCTION(ROUTER_COPY_CUSTOM_DYLIB)
