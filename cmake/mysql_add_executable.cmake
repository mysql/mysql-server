# Copyright (c) 2009, 2022, Oracle and/or its affiliates.
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

IF(APPLE)
  SET(DEV_ENTITLEMENT_FILE ${CMAKE_BINARY_DIR}/dev.entitlements)

  # use PlistBuddy to create the dev.entitlements file
  # if it doesn't exist.
  #
  # - get-task-allow allows a debugger to attach to a binary
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DEV_ENTITLEMENT_FILE}
    COMMAND /usr/libexec/PlistBuddy
    -c "Add :com.apple.security.get-task-allow bool true"
    ${DEV_ENTITLEMENT_FILE}
    )

  ADD_CUSTOM_TARGET(GenerateDevEntitlements
    DEPENDS ${DEV_ENTITLEMENT_FILE}
    )
ENDIF()


# add developer specific entitlements to the target.
#
# - allow debugger to attach.
#
# @param TGT targetname
FUNCTION(MACOS_ADD_DEVELOPER_ENTITLEMENTS TGT)
  # Ensure the dev.entitlement file is created before codesign is called.
  ADD_DEPENDENCIES(${TGT} GenerateDevEntitlements)

  # Use 'codesign' to add the dev.entitlements to the target
  ADD_CUSTOM_COMMAND(TARGET ${TGT} POST_BUILD
    COMMAND codesign
    ARGS
      --sign -
      --preserve-metadata=entitlements
      --force
      --entitlements ${DEV_ENTITLEMENT_FILE}
      $<TARGET_FILE:${TGT}>
    )
ENDFUNCTION()


# MYSQL_ADD_EXECUTABLE(target sources... options/keywords...)
#
# All executables are built in ${CMAKE_BINARY_DIR}/runtime_output_directory
# (can be overridden by the RUNTIME_OUTPUT_DIRECTORY option).
# This is primarily done to simplify usage of dynamic libraries on Windows.
# It also simplifies test tools like mtr, which have to locate executables in
# order to run them during testing.

FUNCTION(MYSQL_ADD_EXECUTABLE target_arg)
  SET(EXECUTABLE_OPTIONS
    ENABLE_EXPORTS
    EXCLUDE_FROM_ALL   # add target, but do not build it by default
    EXCLUDE_FROM_PGO   # add target, but do not build for PGO
    SKIP_INSTALL       # do not install it
    )
  SET(EXECUTABLE_ONE_VALUE_KW
    ADD_TEST           # add unit test, sets SKIP_INSTALL
    COMPONENT
    DESTINATION        # install destination, defaults to ${INSTALL_BINDIR}
    RUNTIME_OUTPUT_DIRECTORY
    )
  SET(EXECUTABLE_MULTI_VALUE_KW
    COMPILE_DEFINITIONS # for TARGET_COMPILE_DEFINITIONS
    COMPILE_OPTIONS     # for TARGET_COMPILE_OPTIONS
    DEPENDENCIES
    INCLUDE_DIRECTORIES # for TARGET_INCLUDE_DIRECTORIES
    LINK_LIBRARIES
    )
  CMAKE_PARSE_ARGUMENTS(ARG
    "${EXECUTABLE_OPTIONS}"
    "${EXECUTABLE_ONE_VALUE_KW}"
    "${EXECUTABLE_MULTI_VALUE_KW}"
    ${ARGN}
    )

  SET(target ${target_arg})
  SET(sources ${ARG_UNPARSED_ARGUMENTS})

  # Collect all executables in the same directory
  IF(ARG_RUNTIME_OUTPUT_DIRECTORY)
    SET(TARGET_RUNTIME_OUTPUT_DIRECTORY ${ARG_RUNTIME_OUTPUT_DIRECTORY})
  ELSE()
    SET(TARGET_RUNTIME_OUTPUT_DIRECTORY
      ${CMAKE_BINARY_DIR}/runtime_output_directory)
  ENDIF()

  ADD_VERSION_INFO(${target} EXECUTABLE sources)

  ADD_EXECUTABLE(${target} ${sources})

  SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(${target})

  IF(ARG_DEPENDENCIES)
    ADD_DEPENDENCIES(${target} ${ARG_DEPENDENCIES})
  ENDIF()
  IF(ARG_COMPONENT STREQUAL "Router")
    ADD_DEPENDENCIES(mysqlrouter_all ${target})
  ENDIF()
  IF(NOT ARG_EXCLUDE_FROM_ALL)
    ADD_DEPENDENCIES(executable_all ${target})
  ENDIF()

  IF(ARG_INCLUDE_DIRECTORIES)
    TARGET_INCLUDE_DIRECTORIES(${target} PRIVATE ${ARG_INCLUDE_DIRECTORIES})
  ENDIF()
  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${target} ${ARG_LINK_LIBRARIES})
  ENDIF()

  IF(ARG_EXCLUDE_FROM_PGO)
    IF(FPROFILE_GENERATE OR FPROFILE_USE)
      SET(ARG_EXCLUDE_FROM_ALL TRUE)
      SET(ARG_SKIP_INSTALL TRUE)
      UNSET(ARG_ADD_TEST)
    ENDIF()
  ENDIF()

  IF(ARG_ENABLE_EXPORTS)
    SET_TARGET_PROPERTIES(${target} PROPERTIES ENABLE_EXPORTS TRUE)
  ENDIF()

  IF(ARG_EXCLUDE_FROM_ALL)
#   MESSAGE(STATUS "EXCLUDE_FROM_ALL ${target}")
    SET_PROPERTY(TARGET ${target} PROPERTY EXCLUDE_FROM_ALL TRUE)
    IF(WIN32)
      SET_PROPERTY(TARGET ${target} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
  ENDIF()

  SET_TARGET_PROPERTIES(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${TARGET_RUNTIME_OUTPUT_DIRECTORY})

  IF(ARG_COMPILE_DEFINITIONS)
    TARGET_COMPILE_DEFINITIONS(${target} PRIVATE ${ARG_COMPILE_DEFINITIONS})
  ENDIF()

  IF(ARG_COMPILE_OPTIONS)
    TARGET_COMPILE_OPTIONS(${target} PRIVATE ${ARG_COMPILE_OPTIONS})
  ENDIF()

  IF(APPLE AND WITH_DEVELOPER_ENTITLEMENTS)
    MACOS_ADD_DEVELOPER_ENTITLEMENTS(${target})
  ENDIF()

  IF(WIN32_CLANG AND WITH_ASAN)
    TARGET_LINK_LIBRARIES(${target} "${ASAN_LIB_DIR}/clang_rt.asan-x86_64.lib")
    TARGET_LINK_LIBRARIES(${target} "${ASAN_LIB_DIR}/clang_rt.asan_cxx-x86_64.lib")
    SET_TARGET_PROPERTIES(${target} PROPERTIES LINK_FLAGS
      "/wholearchive:\"${ASAN_LIB_DIR}/clang_rt.asan-x86_64.lib\" /wholearchive:\"${ASAN_LIB_DIR}/clang_rt.asan_cxx-x86_64.lib\"")
  ENDIF()

  # Add unit test, do not install it.
  IF (ARG_ADD_TEST)
    ADD_DEPENDENCIES(unittest_all ${target})
    ADD_TEST(${ARG_ADD_TEST}
      ${TARGET_RUNTIME_OUTPUT_DIRECTORY}/${target})
    SET(ARG_SKIP_INSTALL TRUE)
  ENDIF()

  IF(COMPRESS_DEBUG_SECTIONS)
    MY_TARGET_LINK_OPTIONS(${target}
      "LINKER:--compress-debug-sections=zlib")
  ENDIF()

  IF(HAVE_BUILD_ID_SUPPORT)
    MY_TARGET_LINK_OPTIONS(${target} "LINKER:--build-id=sha1")
  ENDIF()

  # tell CPack where to install
  IF(NOT ARG_SKIP_INSTALL)
    IF(NOT ARG_DESTINATION)
      SET(ARG_DESTINATION ${INSTALL_BINDIR})
    ENDIF()
    IF(ARG_COMPONENT)
      SET(COMP COMPONENT ${ARG_COMPONENT})
    ELSE()
      SET(COMP COMPONENT Client)
    ENDIF()
    ADD_INSTALL_RPATH_FOR_OPENSSL(${target})
    MYSQL_INSTALL_TARGET(${target} DESTINATION ${ARG_DESTINATION} ${COMP})
  ENDIF()
ENDFUNCTION()
