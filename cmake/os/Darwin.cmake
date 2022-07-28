# Copyright (c) 2010, 2022, Oracle and/or its affiliates.
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

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  SET(APPLE_ARM 1)
ENDIF()

# We require at least XCode 10.0
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(MY_COMPILER_IS_CLANG)
    CHECK_C_SOURCE_RUNS("
      int main()
      {
        return (__clang_major__ < 10);
      }" HAVE_SUPPORTED_CLANG_VERSION)
    IF(NOT HAVE_SUPPORTED_CLANG_VERSION)
      MESSAGE(FATAL_ERROR "XCode 10.0 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
  IF(APPLE_ARM)
    SET(MYSQL_MACHINE_TYPE "arm64")
  ELSE()
    SET(MYSQL_MACHINE_TYPE "x86_64")
  ENDIF()
ENDIF()

# CMAKE_CXX_ARCHIVE_CREATE is by default
#     "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>"
# And CMAKE_AR is "<toolchain>/usr/bin/ar"
#
# CMAKE_CXX_ARCHIVE_FINISH is by default
#     "<CMAKE_RANLIB> <TARGET>"
# and CMAKE_RANLIB is "<toolchain>/usr/bin/ranlib>"
#
# libtool has an option -no_warning_for_no_symbols
# but will generate lots of warnings for files with the same basename:
# /usr/bin/libtool: warning same member name (check_constraints.cc.o) ....
#
# To get a clean build, use 'ar' and ensure all source files are non-empty.
# Use this by default for Ninja and Makefiles.
IF(APPLE_XCODE)
  SET(WITH_LIBTOOL_DEFAULT ON)
ELSE()
  SET(WITH_LIBTOOL_DEFAULT OFF)
ENDIF()
OPTION(WITH_LIBTOOL
  "Use 'libtool' rather than 'ar' for creating static libraries"
  ${WITH_LIBTOOL_DEFAULT}
  )

IF(WITH_LIBTOOL)
  SET(CMAKE_C_CREATE_STATIC_LIBRARY
    "/usr/bin/libtool -static -no_warning_for_no_symbols -o <TARGET> <LINK_FLAGS> <OBJECTS> ")
  SET(CMAKE_CXX_CREATE_STATIC_LIBRARY
    "/usr/bin/libtool -static -no_warning_for_no_symbols -o <TARGET> <LINK_FLAGS> <OBJECTS> ")
ELSE()
  # This did not fix the "library.a(filename.cc.o) has no symbols" warnings.
  # 'ranlib' has the -no_warning_for_no_symbols option, but 'ar' does not.
  # STRING(REPLACE "<CMAKE_RANLIB>" "<CMAKE_RANLIB> -no_warning_for_no_symbols"
  #   CMAKE_C_ARCHIVE_FINISH "${CMAKE_C_ARCHIVE_FINISH}")
  # STRING(REPLACE "<CMAKE_RANLIB>" "<CMAKE_RANLIB> -no_warning_for_no_symbols"
  #   CMAKE_CXX_ARCHIVE_FINISH "${CMAKE_CXX_ARCHIVE_FINISH}")
ENDIF()
