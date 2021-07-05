# Copyright (c) 2020, 2021, Oracle and/or its affiliates.
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

# cmake -DWITH_FIDO=bundled|system

# check for dependent system libraries like libudev, hidapi libraries
MACRO (FIND_SYSTEM_UDEV_OR_HID)
  IF (LINUX)
    FIND_LIBRARY(UDEV_SYSTEM_LIBRARY NAMES udev)
    IF (NOT UDEV_SYSTEM_LIBRARY)
      MESSAGE(WARNING "Cannot find development libraries. "
        "You need to install the required packages:\n"
        "  Debian/Ubuntu:              apt install libudev-dev\n"
        "  RedHat/Fedora/Oracle Linux: yum install libudev-devel\n"
        "  SuSE:                       zypper install libudev-devel\n"
      )
    ENDIF()
  ELSEIF(FREEBSD)
    FIND_LIBRARY(HID_LIBRARY NAMES hidapi)
    IF (NOT HID_LIBRARY)
      MESSAGE(WARNING "Cannot find development libraries. "
        "You need to install the required packages:\n"
        "FreeBSD:     pkg install hidapi\n"
      )
    ENDIF()
  ENDIF()
ENDMACRO()

# Look for system fido2. If we find it, there is no need to look for libudev.
MACRO (FIND_SYSTEM_FIDO)
  FIND_PATH(FIDO_INCLUDE_DIR fido.h PATHS ${INCLUDE_PATH})
  IF (NOT FIDO_INCLUDE_DIR)
    MESSAGE(WARNING "Cannot find development libraries. "
      "You need to install the required packages:\n"
      "  Debian/Ubuntu:              apt install libfido2-dev\n"
      "  RedHat/Fedora/Oracle Linux: yum install libfido2-devel\n"
      "  SuSE:                       zypper install libfido2-devel\n"
      )

  ENDIF()

  FIND_LIBRARY(FIDO_LIBRARY fido2)

  IF (FIDO_LIBRARY AND FIDO_INCLUDE_DIR)
    SET(FIDO_FOUND TRUE)
  ENDIF()
ENDMACRO()

MACRO(MYSQL_USE_BUNDLED_FIDO)
  SET(WITH_FIDO "bundled" CACHE STRING
    "Bundled fido2 library")

  FIND_SYSTEM_UDEV_OR_HID()

  SET(CBOR_BUNDLE_SRC_PATH "internal/extra/libcbor")

  SET(FIDO_BUNDLE_SRC_PATH "internal/extra/libfido2")
  SET(FIDO_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/${FIDO_BUNDLE_SRC_PATH}/src)

  UNSET(CBOR_INCLUDE_DIR)
  UNSET(CBOR_INCLUDE_DIR CACHE)
  UNSET(FIDO_INCLUDE_DIR)
  UNSET(FIDO_INCLUDE_DIR CACHE)

  SET(FIDO_FOUND TRUE)
  SET(FIDO_LIBRARY fido2 CACHE INTERNAL "Bundled fido2 library")
ENDMACRO()

MACRO(MYSQL_CHECK_FIDO)
  IF (NOT WITH_FIDO)
    SET(WITH_FIDO "bundled"
      CACHE STRING "By default use bundled libfido2.")
  ENDIF()

  IF(WITH_FIDO STREQUAL "bundled")
    MYSQL_USE_BUNDLED_FIDO()
  ELSEIF(WITH_FIDO STREQUAL "system")
    FIND_SYSTEM_FIDO()
    IF(NOT FIDO_FOUND)
      MESSAGE(WARNING "Cannot find system fido2 libraries.")
    ENDIF()
  ELSE()
    MESSAGE(WARNING "WITH_FIDO must be bundled or system")
  ENDIF()

ENDMACRO()
