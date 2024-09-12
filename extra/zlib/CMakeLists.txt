# Copyright (c) 2024, Oracle and/or its affiliates.
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

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  STRING_APPEND(CMAKE_C_FLAGS " ${HAS_MISSING_PROFILE}")
ENDIF()

MY_CHECK_C_COMPILER_FLAG("-Wdeprecated-non-prototype"
  HAS_DEPRECATED_NON_PROTOTYPE)
IF(HAS_DEPRECATED_NON_PROTOTYPE)
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-deprecated-non-prototype")
ENDIF()

IF(NOT WIN32)
  STRING_APPEND(CMAKE_C_FLAGS " -fvisibility=hidden")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_C_FLAGS "${MY_COMPILER_FLAG_FLTO}")
ENDIF()

# Xcode >= 14 makes noise about sprintf, and loss of precision
# when assigning integers from 64 bits to 32 bits, so silence:
IF(APPLE)
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-deprecated-declarations")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-shorten-64-to-32")
ENDIF()

ADD_SUBDIRECTORY(${ZLIB_VERSION_DIR})
