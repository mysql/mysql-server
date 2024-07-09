# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

# This file includes Linux specific options and quirks, related to system checks

INCLUDE(CheckSymbolExists)
INCLUDE(CheckCSourceRuns)

SET(LINUX 1)

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  SET(LINUX_ARM 1)
ENDIF()

# OS display name (version_compile_os etc).
# Used by the test suite to ignore bugs on some platforms.
SET(SYSTEM_TYPE "Linux")

IF(EXISTS "/etc/alpine-release")
  SET(LINUX_ALPINE 1)
ENDIF()

IF(EXISTS "/etc/fedora-release")
  SET(LINUX_FEDORA 1)
ENDIF()

# Use dpkg-buildflags --get CPPFLAGS | CFLAGS | CXXFLAGS | LDFLAGS
# to get flags for this platform.
IF(LINUX_DEBIAN OR LINUX_UBUNTU)
  SET(LINUX_DEB_PLATFORM 1)
ENDIF()

# Use CMAKE_C_FLAGS | CMAKE_CXX_FLAGS = rpm --eval %optflags
# to get flags for this platform.
IF(LINUX_FEDORA OR LINUX_RHEL OR LINUX_SUSE)
  SET(LINUX_RPM_PLATFORM 1)
ENDIF()

# We require at least GCC 10 Clang 12
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(MY_COMPILER_IS_GNU)
    # gcc9 is known to fail
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
      MESSAGE(FATAL_ERROR "GCC 10 or newer is required")
    ENDIF()
  ELSEIF(MY_COMPILER_IS_CLANG)
    # This is the lowest version tested
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
      MESSAGE(FATAL_ERROR "Clang 12 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# ISO C89, ISO C99, POSIX.1, POSIX.2, BSD, SVID, X/Open, LFS, and GNU extensions.
ADD_DEFINITIONS(-D_GNU_SOURCE)

# 64 bit file offset support flag
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

# Ensure we have clean build for shared libraries
# without unresolved symbols
# Not supported with Sanitizers
IF(NOT WITH_ASAN AND
   NOT WITH_LSAN AND
   NOT WITH_MSAN AND
   NOT WITH_TSAN AND
   NOT WITH_UBSAN)
  SET(LINK_FLAG_NO_UNDEFINED "-Wl,--no-undefined")
  SET(LINK_FLAG_Z_DEFS "-z,defs")
ENDIF()

# Linux specific HUGETLB /large page support
CHECK_SYMBOL_EXISTS(SHM_HUGETLB sys/shm.h HAVE_LINUX_LARGE_PAGES)
