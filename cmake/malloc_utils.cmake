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

FUNCTION(WARN_MISSING_SYSTEM_TCMALLOC)
  MESSAGE(WARNING "Cannot find tcmalloc development libraries. "
    "You need to install the required packages:\n"
    "  Debian/Ubuntu:              apt install libgoogle-perftools-dev\n"
    "  RedHat/Fedora/Oracle Linux: yum install gperftools-devel\n"
    "  SuSE:                       zypper install gperftools-devel\n"
    )
ENDFUNCTION()

FUNCTION(WARN_MISSING_SYSTEM_JEMALLOC)
  MESSAGE(WARNING "Cannot find jemalloc development libraries. "
    "You need to install the required packages:\n"
    "  Debian/Ubuntu:              apt install libjemalloc-dev\n"
    "  RedHat/Fedora/Oracle Linux: yum install jemalloc-devel\n"
    "  SuSE:                       zypper install jemalloc-devel\n"
    )
ENDFUNCTION()

FUNCTION(FIND_MALLOC_LIBRARY library_name)
  # Skip system tcmalloc, and build the bundled version instead.
  IF(WITH_TCMALLOC STREQUAL "bundled")
    RETURN()
  ENDIF()
  FIND_LIBRARY(MALLOC_LIBRARY ${library_name})
  IF(NOT MALLOC_LIBRARY)
    IF(library_name MATCHES "tcmalloc")
      WARN_MISSING_SYSTEM_TCMALLOC()
    ELSE()
      WARN_MISSING_SYSTEM_JEMALLOC()
    ENDIF()
    MESSAGE(FATAL_ERROR "Library ${library_name} not found")
  ENDIF()

  # There are some special considerations when using the RedHat gcc toolsets:
  # https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/
  #   html/developing_c_and_cpp_applications_in_rhel_8/
  #   additional-toolsets-for-development_developing-applications
  #
  # For mysqld (and possibly other binaries),
  #   we need tcmalloc to be the last library on the link line.
  # Or more specifically:
  #   after any library that might pull in libstdc++_nonshared.a
  # Cmake will analyze target dependencies, and will add -ltcmalloc
  #   somewhere on the compiler/linker command line accordingly.
  # There is no way to tell cmake to "link this library last",
  #   so we modify CMAKE_CXX_LINK_EXECUTABLE instead.
  # For the original contents of CMAKE_CXX_LINK_EXECUTABLE,
  #   see e.g. cmake-3.20.1/Modules/CMakeCXXInformation.cmake
  SET(ORIGINAL_CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -l${library_name}")

  SET(ORIGINAL_CMAKE_CXX_LINK_EXECUTABLE "${ORIGINAL_CMAKE_CXX_LINK_EXECUTABLE}" PARENT_SCOPE)
  SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}" PARENT_SCOPE)
ENDFUNCTION()
