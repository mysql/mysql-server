# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

# This file includes OSX specific options and quirks, related to system checks

INCLUDE(CheckCSourceRuns)

# We require at least Clang 3.6 (XCode 7).
# Bug #26279510 OS X: ASSERTION FAILED: (TABLE_SHARE->TMP_TABLE !=
# Clang 8.0.0 generates wrong code.
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    CHECK_C_SOURCE_RUNS("
      int main()
      {
        return !(__clang_major__ == 8 && __clang_minor__ == 0);
      }"
    HAVE_BUGGY_CLANG_VERSION)
    IF(HAVE_BUGGY_CLANG_VERSION)
      MESSAGE(FATAL_ERROR
        "Clang version 8.0 is known to generate wrong code for MySQL, you should upgrade Xcode")
    ENDIF()

    CHECK_C_SOURCE_RUNS("
      int main()
      {
        return (__clang_major__ < 3) ||
               (__clang_major__ == 3 && __clang_minor__ < 6);
      }" HAVE_SUPPORTED_CLANG_VERSION)
    IF(NOT HAVE_SUPPORTED_CLANG_VERSION)
      MESSAGE(FATAL_ERROR "Clang 3.6 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()
