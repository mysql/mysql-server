# Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

# The files in protobuf-3.11.4/ are almost unmodified versions of google
# source files taken from protobuf-cpp-3.11.4.tar.gz archive.
# Changes:
# - remove directories and files
#    protobuf-3.11.4/benchmarks/
#    protobuf-3.11.4/conformance/
#    protobuf-3.11.4/editors/
#    protobuf-3.11.4/examples/
#    protobuf-3.11.4/m4/
#    protobuf-3.11.4/objectivec/
#    protobuf-3.11.4/python/
#    protobuf-3.11.4/third_party/
#    protobuf-3.11.4/util/
#    protobuf-3.11.4/ar-lib
#    protobuf-3.11.4/compile
#    protobuf-3.11.4/config.guess
#    protobuf-3.11.4/config.h.in
#    protobuf-3.11.4/config.sub
#    protobuf-3.11.4/configure
#    protobuf-3.11.4/depcomp
#    protobuf-3.11.4/install-sh
#    protobuf-3.11.4/ltmain.sh
#    protobuf-3.11.4/missing
#    protobuf-3.11.4/test-driver
#    protobuf-3.11.4/update_file_lists.sh
# - disable configuration check of cmake's mimimum version
# - disable configuration zlib check
# - disable installation script
# - fix minor compilation issues on SunPro 5.14.0

# Inhibit warning messages
IF(MSVC AND NOT WIN32_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4005")
ELSE()
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-sign-compare")
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-function")
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

SET(protobuf_BUILD_TESTS OFF CACHE INTERNAL "")
SET(protobuf_BUILD_EXAMPLES OFF CACHE INTERNAL "")
SET(protobuf_BUILD_SHARED_LIBS ON CACHE INTERNAL "")
SET(protobuf_MSVC_STATIC_RUNTIME OFF CACHE INTERNAL "")
SET(protobuf_WITH_ZLIB ON CACHE INTERNAL "")
SET(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
ADD_SUBDIRECTORY(protobuf-3.11.4/cmake)
