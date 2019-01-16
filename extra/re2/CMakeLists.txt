# Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(.)

SET(RE2_SOURCES
    util/arena.cc
    util/hash.cc
    util/rune.cc
    util/stringpiece.cc
    util/stringprintf.cc
    util/strutil.cc
    util/arena.h
    util/atomicops.h
    util/benchmark.h
    util/flags.h
    util/logging.h
    util/valgrind.cc
    util/valgrind.h
    re2/stringpiece.h
    re2/bitstate.cc
    re2/compile.cc
    re2/dfa.cc
    re2/filtered_re2.cc
    re2/mimics_pcre.cc
    re2/nfa.cc
    re2/onepass.cc
    re2/parse.cc
    re2/perl_groups.cc
    re2/prefilter.cc
    re2/prefilter_tree.cc
    re2/prog.cc
    re2/re2.cc
    re2/re2.h
    re2/regexp.cc
    re2/set.cc
    re2/simplify.cc
    re2/tostring.cc
    re2/unicode_casefold.cc
    re2/unicode_groups.cc
    re2/variadic_function.h
    )

ADD_LIBRARY(re2_lib STATIC ${RE2_SOURCES})

#
# Disabling of various compiler warnings for GCC/Clang
#

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "-Wno-sign-compare")
  IF(CMAKE_COMPILER_IS_GNUCXX)
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "-Wno-class-memaccess")
  ENDIF()

  MY_CHECK_CXX_COMPILER_FLAG("-Wtautological-undefined-compare"
                             HAVE_TAUTOLOGICAL_UNDEFINED_COMPARE)
  IF(HAVE_TAUTOLOGICAL_UNDEFINED_COMPARE)
    TARGET_COMPILE_OPTIONS(re2_lib
      PRIVATE "-Wno-tautological-undefined-compare")
  ENDIF()
  MY_CHECK_CXX_COMPILER_FLAG("-Wunused-local-typedefs" HAVE_UNUSED_TYPEDEFS)
  IF(HAVE_UNUSED_TYPEDEFS)
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "-Wno-unused-local-typedefs")
  ENDIF()
  MY_CHECK_CXX_COMPILER_FLAG("-Wnonnull-compare" HAVE_NONNULL_COMPARE)
  IF(HAVE_NONNULL_COMPARE)
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "-Wno-nonnull-compare")
  ENDIF()

  MY_CHECK_CXX_COMPILER_FLAG("-Wextra-semi" HAVE_EXTRA_SEMI)
  IF(HAVE_EXTRA_SEMI)
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "-Wno-extra-semi")
  ENDIF()
ENDIF()

IF(DEFINED WIN32)
  IF(MSVC)
    # 'identifier' : macro redefinition
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "/wd4005")
    # 'expression' : signed/unsigned mismatch
    TARGET_COMPILE_OPTIONS(re2_lib PRIVATE "/wd4018")
    ADD_DEFINITIONS(-DSTL_MSVC)
  ENDIF()
ENDIF()
