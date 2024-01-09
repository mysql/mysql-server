# Copyright (c) 2023, Oracle and/or its affiliates.
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

MY_CHECK_CXX_COMPILER_WARNING("-Wchar-subscripts" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-lambda-capture" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wzero-as-null-pointer-constant" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

IF(FREEBSD)
  # Missing posix_memalign()
  ADD_DEFINITIONS(-D_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION)
ENDIF()

SET(ABSL_PROPAGATE_CXX_STD ON)

# The Clang build fails for shared protobuf/abseil builds:
# invalid application of 'alignof' to an incomplete type 'mapped_type'
# note: due to ..... being dllexported
IF(MSVC AND NOT WIN32_CLANG)
  SET(absl_BUILD_SHARED_LIBS ON)
ENDIF()

MACRO(do_not_install)
ENDMACRO()

ADD_SUBDIRECTORY(abseil-cpp-20230802.1)
