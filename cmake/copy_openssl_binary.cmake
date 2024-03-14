# Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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

SET(MSG_TXT
  "Copied OPENSSL_EXECUTABLE = ${executable_full_filename} to")
IF(BUILD_IS_SINGLE_CONFIG)
  IF(EXISTS "${executable_name}")
#   MESSAGE(STATUS "${executable_name} already copied")
    RETURN()
  ENDIF()
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E copy
    "${executable_full_filename}" "${executable_name}"
    )
  SET(MSG_TXT "${MSG_TXT} ${CWD}/${executable_name}")
  MESSAGE(STATUS "${MSG_TXT}")
ELSE()
  IF(EXISTS "./${CMAKE_CFG_INTDIR}/${executable_name}")
#   MESSAGE(STATUS "${CMAKE_CFG_INTDIR}/${executable_name} already copied")
    RETURN()
  ENDIF()
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E copy
    "${executable_full_filename}" "${CMAKE_CFG_INTDIR}/${executable_name}"
    )
  SET(MSG_TXT "${MSG_TXT} ${CWD}/${CMAKE_CFG_INTDIR}/${executable_name}")
  MESSAGE(STATUS "${MSG_TXT}")
ENDIF()

IF(LINUX)
  EXECUTE_PROCESS(
    COMMAND ${PATCHELF_EXECUTABLE} --version
    OUTPUT_VARIABLE PATCHELF_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  STRING(REPLACE "patchelf" "" PATCHELF_VERSION "${PATCHELF_VERSION}")

  IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" AND
      PATCHELF_VERSION VERSION_LESS "0.14.5")
    SET(PATCHELF_PAGE_SIZE_ARGS --page-size 65536)
  ENDIF()

  EXECUTE_PROCESS(
    COMMAND ${PATCHELF_EXECUTABLE} ${PATCHELF_PAGE_SIZE_ARGS}
    --set-rpath "$ORIGIN/../lib:$ORIGIN/../${INSTALL_PRIV_LIBDIR}"
    "./${executable_name}"
    )
ENDIF(LINUX)

IF(APPLE)
  MESSAGE(STATUS "CRYPTO_VERSION is ${CRYPTO_VERSION}")
  MESSAGE(STATUS "OPENSSL_VERSION is ${OPENSSL_VERSION}")
  EXECUTE_PROCESS(
    COMMAND otool -L ${CMAKE_CFG_INTDIR}/${executable_name}
    OUTPUT_VARIABLE OTOOL_OPENSSL_DEPS
    )

  STRING(REPLACE "\n" ";" DEPS_LIST ${OTOOL_OPENSSL_DEPS})
  FOREACH(LINE ${DEPS_LIST})
    IF(LINE MATCHES "libssl")
      STRING(REGEX MATCH "[ ]*([.a-zA-Z0-9/@_]+.dylib).*" UNUSED "${LINE}")
      MESSAGE(STATUS "dependency ${CMAKE_MATCH_1}")
      SET(LIBSSL_MATCH "${CMAKE_MATCH_1}")
    ENDIF()
    IF(LINE MATCHES "libcrypto")
      STRING(REGEX MATCH "[ ]*([.a-zA-Z0-9/@_]+.dylib).*" UNUSED "${LINE}")
      MESSAGE(STATUS "dependency ${CMAKE_MATCH_1}")
      SET(LIBCRYPTO_MATCH "${CMAKE_MATCH_1}")
    ENDIF()
  ENDFOREACH()

  IF(BUILD_IS_SINGLE_CONFIG)
    # install_name_tool -change old new file
    EXECUTE_PROCESS(COMMAND install_name_tool -change
      "${LIBSSL_MATCH}" "@loader_path/../lib/${OPENSSL_VERSION}"
      "./${executable_name}"
      )
    EXECUTE_PROCESS(COMMAND install_name_tool -change
      "${LIBCRYPTO_MATCH}" "@loader_path/../lib/${CRYPTO_VERSION}"
      "./${executable_name}"
      )
    EXECUTE_PROCESS(
      COMMAND chmod +w "./${executable_name}"
      )
  ELSE()
    # install_name_tool -change old new file
    EXECUTE_PROCESS(COMMAND install_name_tool -change
      "${LIBSSL_MATCH}"
      "@loader_path/../../lib/${CMAKE_CFG_INTDIR}/${OPENSSL_VERSION}"
      "./${CMAKE_CFG_INTDIR}/${executable_name}"
      )
    EXECUTE_PROCESS(COMMAND install_name_tool -change
      "${LIBCRYPTO_MATCH}"
      "@loader_path/../../lib/${CMAKE_CFG_INTDIR}/${CRYPTO_VERSION}"
      "./${CMAKE_CFG_INTDIR}/${executable_name}"
      )
    EXECUTE_PROCESS(
      COMMAND chmod +w "./${CMAKE_CFG_INTDIR}/${executable_name}"
      )
  ENDIF()
ENDIF(APPLE)
