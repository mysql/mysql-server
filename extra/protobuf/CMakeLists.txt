# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

# The files in protobuf-24.4/ are almost unmodified versions of google
# source files taken from protobuf-cpp-24.4.tar.gz archive.

# Xcode >= 14 makes noise about sprintf, so silence:
IF(APPLE)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-deprecated-declarations")
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-shorten-64-to-32")
ENDIF()

# Inhibit warning messages
IF(MSVC AND NOT WIN32_CLANG)
  # no suitable definition provided for explicit template instantiation
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4661")
  # 'inline' used more than once
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4141")
  # unsafe mix of type 'bool' and type 'uint64_t
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4805")
  # signed/unsigned mismatch
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4018")
  # __declspec(dllimport): ignored on left of ...  VS16 only
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4091")
  # non-dllinterface class .. used as base for dll-interface class .. VS16 only
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4275")
  # 'identifier' : class 'type' needs to have dll-interface VS16 only
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4251")
ELSE()
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-sign-compare")
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-function")
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
  STRING(REPLACE "/GENPROFILE" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "/USEPROFILE" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "/LTCG" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
         "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
ENDIF()

# Suppress linker warnings generated as a consequence of combining
# CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS with __declspec(dllexport)
IF(MSVC)
  STRING(APPEND CMAKE_SHARED_LINKER_FLAGS " /IGNORE:4197")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wzero-as-null-pointer-constant" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  # For WIN32_CLANG, replace it, since we do not use ICU_LINUX_COMPILE_OPTIONS.
  STRING(REPLACE "-Wzero-as-null-pointer-constant" ""
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-lambda-capture" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# always_inline function might not be inlinable
MY_CHECK_CXX_COMPILER_WARNING("-Wattributes" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# implicit capture of this via [=] is deprecated in C++20
# src/google/protobuf/map.cc
MY_CHECK_CXX_COMPILER_WARNING("-Wdeprecated" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_MISSING_PROFILE}")
ENDIF()

IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-inconsistent-missing-override")
ENDIF()

# For gcc9
MY_CHECK_CXX_COMPILER_WARNING("-Wdeprecated-copy" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wextra-semi" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wstringop-overflow" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmaybe-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-const-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

# memcpy accessing ... or more bytes at offsets ... may overlap
MY_CHECK_CXX_COMPILER_WARNING("-Wrestrict" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wchar-subscripts" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

SET(protobuf_BUILD_TESTS OFF CACHE INTERNAL "")
SET(protobuf_BUILD_EXAMPLES OFF CACHE INTERNAL "")

# protoc hangs in __static_initialization_and_destruction_0
# Look for: Force running AddDescriptors() at dynamic initialization time.
# Hangs at src/google/protobuf/compiler/plugin.pb.cc:193
# cmake/abseil-cpp.cmake assumes abseil is also a .dll
IF(SOLARIS OR WIN32_CLANG)
  SET(protobuf_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
ELSE()
  # With static C/C++ libraries, protoc with shared libs will segfault,
  # so use static libs instead.
  IF(CMAKE_CXX_FLAGS MATCHES "-static-libgcc")
    SET(protobuf_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
  ELSEIF(NOT DEFINED protobuf_BUILD_SHARED_LIBS)
    # Do not redefine to ON if already set to OFF
    SET(protobuf_BUILD_SHARED_LIBS ON CACHE INTERNAL "")
  ENDIF()
ENDIF()

SET(protobuf_MSVC_STATIC_RUNTIME OFF CACHE INTERNAL "")
SET(protobuf_WITH_ZLIB ON CACHE INTERNAL "")
SET(ZLIB_LIBRARIES ext::zlib)
ADD_SUBDIRECTORY(protobuf-24.4)
