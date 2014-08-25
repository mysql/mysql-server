# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 

# This file includes OSX specific options and quirks, related to system checks

INCLUDE(CheckCSourceRuns)

# We require at least Clang 3.3 (XCode 5).
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    CHECK_C_SOURCE_RUNS("
      int main()
      {
        return (__clang_major__ < 3) ||
               (__clang_major__ == 3 && __clang_minor__ < 3);
      }" HAVE_SUPPORTED_CLANG_VERSION)
    IF(NOT HAVE_SUPPORTED_CLANG_VERSION)
      MESSAGE(FATAL_ERROR "Clang 3.3 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()
