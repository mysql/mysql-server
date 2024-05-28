# Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

# Options for doing PGO (profile guided optimization) with gcc/clang.
#
# HowTo for gcc9 and above:
# Assuming we have two build directories
#   <some path>/build
#   <some path>/build-profile-data
#
# in build
#   cmake <path to source> -DFPROFILE_GENERATE=1
#   make
#   run whatever test suite is an appropriate training set
# now rename the build directory to something else, or simply 'rm -rf' it.
# 'mkdir build' note: same name
# in the new build
#   cmake <path to source> -DFPROFILE_USE=1
#   make
#
# HowTo for clang:
# Assuming we have three build directories
#   <some path>/build-gen
#   <some path>/build-use
#   <some path>/profile-data
#
# in build-gen
#   cmake <path to source> -DFPROFILE_GENERATE=1
#   make
#   run whatever test suite is an appropriate training set
# in profile-data
#   llvm-profdata merge -output=default.profdata .
# in build-use
#   cmake <path to source> -DFPROFILE_USE=1
#   make
#
#
# Your executables should hopefully be faster than a default build.
# In order to share the profile data, we turn on REPRODUCIBLE_BUILD.
# We also switch off USE_LD_LLD since it resulted in some linking problems.
#
# HowTo for MSVC:
# Prerequisites: The pgort140.dll file must be in the path. An easy way
# to ensure this is to use the Developer Command Prompt for VS 2019/2022.
#
# Assuming we have two directories
#  <some path>/build-normal
#  <some path>/build-pgo
#
# in build-normal, build everything as normal:
#  cmake <path to source>
#  cmake --build . --config RelWithDebInfo -- /m
#
# The "normal" build is required to create the mysqld.def file, as the
# PGO compilation options prevent its generation.
#
# In build-pgo
#  cmake <path to source> -DFPROFILE_GENERATE=1 -DMYSQLD_DEF_FILE=<some path>/build-normal/sql/relwithdebinfo/mysqld.default
#  cmake --build . --target mysqld --config RelWithDebInfo -- /m
#  copy build-pgo/runtime_output_directory/RelWithDebInfo/mysqld.exe build-normal/runtime_output_directory/RelWithDebInfo
#
# in build-normal (which now has copy of the instrumented mysqld.exe),
# run whatever test suite is an appropriate training set. Note the
# --mysqld=--no-monitor and --parallel=1 options must be used with
# the mysql-test-run.pl script to ensure only one mysqld.exe instance is
# active, otherwise the profile data will not be captured in mysqld*.pgc
# files.
#
# Having run sufficient training, the profile data in the mysqld*.pgc
# files must be merged into the mysqld.pgd file.
#
# In build-normal/runtime_output_directory/RelWithDebInfo
#  pgomgr -merge mysqld.pgd
#  copy build-normal/runtime_output_directory/RelWithDebInfo/mysqld.pgd  build-pgo/runtime_output_directory/RelWithDebInfo
#
# In build-pgo
#  del CMakeCache.txt
#  cmake <path to source> -DFPROFILE_USE=1 -DMYSQLD_DEF_FILE=<some path>/build-normal/sql/relwithdebinfo/mysqld.default
#  cmake --build . --target mysqld --config RelWithDebInfo -- /m
#  copy build-pgo/runtime_output_directory/RelWithDebInfo/mysqld.exe build-normal/runtime_output_directory/RelWithDebInfo
#  copy build-pgo/runtime_output_directory/RelWithDebInfo/mysqld.pdb build-normal/runtime_output_directory/RelWithDebInfo
#
# The mysqld.exe in build-normal is now the optimized build.
#

IF(MY_COMPILER_IS_GNU)
  SET(FPROFILE_DIR_DEFAULT "${CMAKE_BINARY_DIR}-profile-data")
ELSE()
  SET(FPROFILE_DIR_DEFAULT "${CMAKE_BINARY_DIR}/../profile-data")
ENDIF()

IF(NOT DEFINED FPROFILE_DIR)
  SET(FPROFILE_DIR "${FPROFILE_DIR_DEFAULT}")
ENDIF()

OPTION(FPROFILE_GENERATE "Add -fprofile-generate" OFF)
IF(FPROFILE_GENERATE)
  IF(MSVC)
    STRING_APPEND(CMAKE_C_FLAGS " /GL")
    STRING_APPEND(CMAKE_CXX_FLAGS " /GL")
    FOREACH(type EXE SHARED MODULE)
      FOREACH(config RELWITHDEBINFO RELEASE )
        SET(flag "CMAKE_${type}_LINKER_FLAGS_${config}")
        STRING_APPEND("${flag}" " /LTCG /GENPROFILE")
      ENDFOREACH()
    ENDFOREACH()
  ELSE()
    STRING_APPEND(CMAKE_C_FLAGS " -fprofile-generate=${FPROFILE_DIR}")
    STRING_APPEND(CMAKE_CXX_FLAGS " -fprofile-generate=${FPROFILE_DIR}")

    IF(MY_COMPILER_IS_GNU)
      STRING_APPEND(CMAKE_C_FLAGS " -fprofile-update=prefer-atomic")
      STRING_APPEND(CMAKE_CXX_FLAGS " -fprofile-update=prefer-atomic")
    ENDIF()
  ENDIF()
ENDIF()

OPTION(FPROFILE_USE "Add -fprofile-use" OFF)
IF(FPROFILE_USE)
  IF(MSVC)
    STRING_APPEND(CMAKE_C_FLAGS " /GL")
    STRING_APPEND(CMAKE_CXX_FLAGS " /GL")
    FOREACH(type EXE SHARED MODULE)
      FOREACH(config RELWITHDEBINFO RELEASE )
        SET(flag "CMAKE_${type}_LINKER_FLAGS_${config}")
        STRING_APPEND("${flag}" " /LTCG /USEPROFILE")
      ENDFOREACH()
    ENDFOREACH()
  ELSE()
    STRING_APPEND(CMAKE_C_FLAGS " -fprofile-use=${FPROFILE_DIR}")
    STRING_APPEND(CMAKE_CXX_FLAGS " -fprofile-use=${FPROFILE_DIR}")
    # Collection of profile data is not thread safe,
    # use -fprofile-correction for GCC
    IF(MY_COMPILER_IS_GNU)
      STRING_APPEND(CMAKE_C_FLAGS " -fprofile-correction")
      STRING_APPEND(CMAKE_CXX_FLAGS " -fprofile-correction")

      # With -fprofile-use all portions of programs not executed during
      # train run are optimized agressively for size rather than speed.
      # gcc10 has -fprofile-partial-training, which will ignore profile
      # feedback for functions not executed during the train run, leading them
      # to be optimized as if they were compiled without profile feedback.
      # This leads to better performance when train run is not representative
      # but also leads to significantly bigger code.
      STRING_APPEND(CMAKE_C_FLAGS " -fprofile-partial-training")
      STRING_APPEND(CMAKE_CXX_FLAGS " -fprofile-partial-training")

    ENDIF()
  ENDIF()
ENDIF()

IF(FPROFILE_GENERATE AND FPROFILE_USE)
  MESSAGE(FATAL_ERROR "Cannot combine -fprofile-generate and -fprofile-use")
ENDIF()

IF((FPROFILE_GENERATE OR FPROFILE_USE) AND NOT MSVC AND NOT
  (LINUX_ARM AND INSTALL_LAYOUT MATCHES "RPM"))
  SET(REPRODUCIBLE_BUILD ON CACHE INTERNAL "")
ENDIF()

IF(FPROFILE_USE AND NOT MSVC)
  # LTO combined with PGO boosts performance even more.
  SET(WITH_LTO_DEFAULT ON CACHE INTERNAL "")
ENDIF()

MACRO(DOWNGRADE_STRINGOP_WARNINGS target)
  IF(MY_COMPILER_IS_GNU AND WITH_LTO AND FPROFILE_USE)
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
      TARGET_LINK_OPTIONS(${target} PRIVATE
        -Wno-error=stringop-overflow
        -Wno-error=stringop-overread
      )
    ELSE()
      TARGET_LINK_OPTIONS(${target} PRIVATE
        -Wno-error=stringop-overflow
      )
    ENDIF()
  ENDIF()
ENDMACRO()
