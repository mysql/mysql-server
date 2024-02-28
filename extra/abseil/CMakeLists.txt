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
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_MISSING_PROFILE}")
ENDIF()

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

# absl/crc/internal/crc_memcpy_x86_64.cc:91:34:
# warning: ignoring attributes on template argument __m128i [-Wignored-attributes]
#    91 |   std::array<__m128i, vec_regions> data
MY_CHECK_CXX_COMPILER_WARNING("-Wignored-attributes" HAS_WARN_FLAG)
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
IF(LINUX)
  SET(absl_BUILD_SHARED_LIBS ON)
ENDIF()

IF(MSVC AND (FPROFILE_GENERATE OR FPROFILE_USE))
  # The use of CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS is incompatible with
  # the MSVC /GL compiler option which is added by fprofile.cmake,
  # along with the linker flags /LTCG, /GENPROFILE and /USEPROFILE
  STRING(REPLACE "/GL" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  STRING(REPLACE "/GENPROFILE" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "/USEPROFILE" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "/LTCG" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
ENDIF()

# Suppress linker warnings generated as a consequence of combining
# CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS with __declspec(dllexport)
IF(MSVC)
  STRING(APPEND CMAKE_SHARED_LINKER_FLAGS " /IGNORE:4197")
ENDIF()

MACRO(do_not_install)
ENDMACRO()

# Avoid ODR violations when running protoc for grpc.
# This broke group replication.
IF(UNIX)
#  STRING_APPEND(CMAKE_C_FLAGS   " -fvisibility=hidden")
#  STRING_APPEND(CMAKE_CXX_FLAGS " -fvisibility=hidden")
ENDIF()

ADD_SUBDIRECTORY(abseil-cpp-20230802.1)
