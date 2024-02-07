# Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

# cmake -DWITH_FIDO=bundled|system

# Version 1.3.1 on Ubuntu 20.04 does *not* work.
# Version 1.4.0 on Fedora 33 passes all tests.
SET(MIN_FIDO_VERSION_REQUIRED "1.4.0")
SET(FIDO_BUNDLED_VERSION "1.13.0")
SET(CBOR_BUNDLED_VERSION "0.10.2")

MACRO(FIND_FIDO_VERSION)
  IF(WITH_FIDO STREQUAL "bundled")
    SET(FIDO_VERSION "${FIDO_BUNDLED_VERSION}")
  ELSE()
    # This does not set any version information:
    # PKG_CHECK_MODULES(SYSTEM_FIDO fido2)

    IF(APPLE)
      EXECUTE_PROCESS(
        COMMAND otool -L "${FIDO_LIBRARY}"
        OUTPUT_VARIABLE OTOOL_FIDO_DEPS)
      STRING(REPLACE "\n" ";" DEPS_LIST ${OTOOL_FIDO_DEPS})
      FOREACH(LINE ${DEPS_LIST})
        STRING(REGEX MATCH
          ".*libfido2.*current version ([.0-9]+).*" UNUSED ${LINE})
        IF(CMAKE_MATCH_1)
          SET(FIDO_VERSION "${CMAKE_MATCH_1}")
          BREAK()
        ENDIF()
      ENDFOREACH()
    ELSE()
      MYSQL_CHECK_PKGCONFIG()
      EXECUTE_PROCESS(
        COMMAND ${MY_PKG_CONFIG_EXECUTABLE} --modversion libfido2
        OUTPUT_VARIABLE MY_FIDO_MODVERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE MY_MODVERSION_RESULT
        )
      IF(MY_MODVERSION_RESULT EQUAL 0)
        SET(FIDO_VERSION ${MY_FIDO_MODVERSION})
      ENDIF()
    ENDIF()
  ENDIF()
  MESSAGE(STATUS "FIDO_VERSION (${WITH_FIDO}) is ${FIDO_VERSION}")
  MESSAGE(STATUS "FIDO_INCLUDE_DIR ${FIDO_INCLUDE_DIR}")
ENDMACRO(FIND_FIDO_VERSION)

# libudev is needed on Linux only.
FUNCTION(WARN_MISSING_SYSTEM_UDEV OUTPUT_WARNING)
  IF(LINUX AND WITH_FIDO STREQUAL "bundled" AND NOT LIBUDEV_DEVEL_FOUND)
    MESSAGE(WARNING "Cannot find development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libudev-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libudev-devel\n"
      "  SuSE:                       zypper install libudev-devel\n"
      )
    SET(${OUTPUT_WARNING} 1 PARENT_SCOPE)
  ENDIF()
  IF(SOLARIS)
    MESSAGE(STATUS "No known libudev on SOLARIS")
    SET(${OUTPUT_WARNING} 1 PARENT_SCOPE)
  ENDIF()
ENDFUNCTION(WARN_MISSING_SYSTEM_UDEV)

# Bundled FIDO requires libudev.
FUNCTION(FIND_SYSTEM_UDEV_OR_HID)
  IF(LINUX)
    FIND_LIBRARY(UDEV_SYSTEM_LIBRARY NAMES udev)
    CHECK_INCLUDE_FILE(libudev.h HAVE_LIBUDEV_H)
    IF(UDEV_SYSTEM_LIBRARY AND HAVE_LIBUDEV_H)
      SET(LIBUDEV_DEVEL_FOUND 1 CACHE INTERNAL "")
      SET(UDEV_LIBRARIES ${UDEV_SYSTEM_LIBRARY} CACHE INTERNAL "")
      MESSAGE(STATUS "UDEV_SYSTEM_LIBRARY ${UDEV_SYSTEM_LIBRARY}")
    ENDIF()
  ELSEIF(FREEBSD)
    FIND_LIBRARY(HID_LIBRARY NAMES hidapi)
    IF(HID_LIBRARY)
      MESSAGE(STATUS "HID_LIBRARY ${HID_LIBRARY}")
    ELSE()
      MESSAGE(WARNING "Cannot find development libraries. "
        "You need to install the required packages:\n"
        "FreeBSD:     pkg install hidapi\n"
      )
    ENDIF()
  ENDIF()
ENDFUNCTION(FIND_SYSTEM_UDEV_OR_HID)

FUNCTION(WARN_MISSING_SYSTEM_FIDO OUTPUT_WARNING)
  IF(WITH_FIDO STREQUAL "system" AND NOT FIDO_FOUND)
    MESSAGE(WARNING "Cannot find development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libfido2-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libfido2-devel\n"
      "  SuSE:                       zypper install libfido2-devel\n"
      )
    SET(${OUTPUT_WARNING} 1 PARENT_SCOPE)
  ENDIF()
ENDFUNCTION(WARN_MISSING_SYSTEM_FIDO)

# Look for system fido2. If we find it, there is no need to look for libudev.
FUNCTION(FIND_SYSTEM_FIDO)
  IF(APPLE)
    SET(CMAKE_REQUIRED_INCLUDES
      "${HOMEBREW_HOME}/include"
      "${HOMEBREW_HOME}/libfido2/include"
      "${OPENSSL_INCLUDE_DIR}"
      )
  ENDIF()

  CHECK_INCLUDE_FILE(fido.h HAVE_FIDO_H)
  FIND_LIBRARY(FIDO_LIBRARY fido2)
  IF (FIDO_LIBRARY AND HAVE_FIDO_H)
    SET(FIDO_FOUND TRUE)
    SET(FIDO_FOUND TRUE PARENT_SCOPE)
    FIND_PATH(FIDO_INCLUDE_DIR
      NAMES fido.h
      HINTS ${CMAKE_REQUIRED_INCLUDES}
      )
    ADD_LIBRARY(fido_interface INTERFACE)
    TARGET_LINK_LIBRARIES(fido_interface INTERFACE ${FIDO_LIBRARY})
    IF(NOT FIDO_INCLUDE_DIR STREQUAL "/usr/include")
      TARGET_INCLUDE_DIRECTORIES(fido_interface SYSTEM INTERFACE
        ${FIDO_INCLUDE_DIR})
    ENDIF()
  ENDIF()
ENDFUNCTION(FIND_SYSTEM_FIDO)

SET(FIDO_BUNDLE_SRC_PATH "extra/libfido2/libfido2-")
STRING_APPEND(FIDO_BUNDLE_SRC_PATH "${FIDO_BUNDLED_VERSION}")
SET(CBOR_BUNDLE_SRC_PATH "extra/libcbor/libcbor-")
STRING_APPEND(CBOR_BUNDLE_SRC_PATH "${CBOR_BUNDLED_VERSION}")

FUNCTION(MYSQL_USE_BUNDLED_FIDO)

  FIND_SYSTEM_UDEV_OR_HID()

  SET(FIDO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/${FIDO_BUNDLE_SRC_PATH}/src)
  SET(FIDO_INCLUDE_DIR ${FIDO_INCLUDE_DIR} PARENT_SCOPE)

  # We use the bundled version, so:
  SET(FIDO_FOUND TRUE)
  SET(FIDO_FOUND TRUE PARENT_SCOPE)

  # Mark it as not found if libudev is missing, so we can give proper warnings.
  IF(LINUX AND NOT LIBUDEV_DEVEL_FOUND)
    SET(FIDO_FOUND FALSE)
    SET(FIDO_FOUND FALSE PARENT_SCOPE)
  ENDIF()
  # So that we skip authentication_webauthn_client.so
  IF(SOLARIS)
    SET(FIDO_FOUND FALSE)
    SET(FIDO_FOUND FALSE PARENT_SCOPE)
  ENDIF()

  IF(FIDO_FOUND)
    ADD_LIBRARY(fido_interface INTERFACE)
    # Our own libfido2.so will be built later, see top level CMakeLists.txt:
    # ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/${FIDO_BUNDLE_SRC_PATH})
    TARGET_LINK_LIBRARIES(fido_interface INTERFACE fido2)
    TARGET_INCLUDE_DIRECTORIES(fido_interface SYSTEM BEFORE INTERFACE
      ${FIDO_INCLUDE_DIR})
  ENDIF()
ENDFUNCTION(MYSQL_USE_BUNDLED_FIDO)

MACRO(MYSQL_CHECK_FIDO)
  IF("${OPENSSL_MAJOR_MINOR_FIX_VERSION}" VERSION_GREATER "1.1.0")
    SET(OPENSSL_IS_COMPATIBLE_WITH_BUNDLED_FIDO ON)
  ELSE()
    SET(OPENSSL_IS_COMPATIBLE_WITH_BUNDLED_FIDO OFF)
    SET(WITH_AUTHENTICATION_WEBAUTHN OFF)
  ENDIF()

  IF (NOT WITH_FIDO)
    IF(OPENSSL_IS_COMPATIBLE_WITH_BUNDLED_FIDO)
      SET(WITH_FIDO "bundled"
        CACHE STRING "By default use bundled libfido2.")
    ELSE()
      SET(WITH_FIDO "none"
        CACHE STRING "Do not compile libfido2 for OpenSSL < 1.1.1")
    ENDIF()
  ENDIF()

  IF(WITH_FIDO STREQUAL "none")
    MESSAGE(WARNING
      "WITH_FIDO is set to \"none\". \
       FIDO based authentication plugins will be skipped.")
  ELSE()
    IF(WITH_FIDO STREQUAL "bundled")
      IF(OPENSSL_IS_COMPATIBLE_WITH_BUNDLED_FIDO)
        MYSQL_USE_BUNDLED_FIDO()
      ELSE()
        MESSAGE(FATAL_ERROR
          "Bundled libfido2 requires OpenSSL version >= 1.1.1")
      ENDIF()
    ELSEIF(WITH_FIDO STREQUAL "system")
      FIND_SYSTEM_FIDO()
      IF(NOT FIDO_FOUND)
        MESSAGE(WARNING "Cannot find system fido2 libraries/headers.")
      ENDIF()
    ELSE()
      MESSAGE(WARNING "WITH_FIDO must be none, bundled or system")
    ENDIF()

    FIND_FIDO_VERSION()
    IF(FIDO_VERSION VERSION_LESS MIN_FIDO_VERSION_REQUIRED)
      MESSAGE(FATAL_ERROR
        "FIDO version must be at least ${MIN_FIDO_VERSION_REQUIRED}, "
        "found ${FIDO_VERSION}.\nPlease use -DWITH_FIDO=bundled")
    ENDIF()
    IF(FIDO_FOUND)
      ADD_LIBRARY(ext::fido ALIAS fido_interface)
    ENDIF()
  ENDIF()
ENDMACRO()
