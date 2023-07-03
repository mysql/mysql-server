# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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
  FIND_LIBRARY(MALLOC_LIBRARY ${library_name})
  IF(NOT MALLOC_LIBRARY)
    IF(library_name MATCHES "tcmalloc")
      WARN_MISSING_SYSTEM_TCMALLOC()
    ELSE()
      WARN_MISSING_SYSTEM_JEMALLOC()
    ENDIF()
    MESSAGE(FATAL_ERROR "Library ${library_name} not found")
  ENDIF()

  STRING_APPEND(CMAKE_C_FLAGS " -fno-builtin-malloc -fno-builtin-calloc")
  STRING_APPEND(CMAKE_C_FLAGS " -fno-builtin-realloc -fno-builtin-free")
  STRING_APPEND(CMAKE_CXX_FLAGS " -fno-builtin-malloc -fno-builtin-calloc")
  STRING_APPEND(CMAKE_CXX_FLAGS " -fno-builtin-realloc -fno-builtin-free")

  STRING_APPEND(CMAKE_EXE_LINKER_FLAGS " -l${library_name}")
  STRING_APPEND(CMAKE_MODULE_LINKER_FLAGS " -l${library_name}")
  STRING_APPEND(CMAKE_SHARED_LINKER_FLAGS " -l${library_name}")

  SET(FIND_MALLOC_LIBRARY_FLAG "-l${library_name}" CACHE STRING "" FORCE)

  FOREACH(flag
      CMAKE_C_FLAGS
      CMAKE_CXX_FLAGS
      CMAKE_EXE_LINKER_FLAGS
      CMAKE_MODULE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS
      )
    SET(${flag} "${${flag}}" PARENT_SCOPE)
  ENDFOREACH()
ENDFUNCTION()
