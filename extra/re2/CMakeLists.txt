# 
# CMake specification file for building re2 library.
# 
# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#

INCLUDE_DIRECTORIES(.)

# disable warnings about signed/unsigned comparison
#SET(WARNFLAGS "-Wall -Wno-sign-compare")
#SET(CMAKE_CXX_FLAGS
#  "${CMAKE_CXX_FLAGS} ${OPTFLAGS} ${WARNFLAGS}")


IF(DEFINED WIN32)
   IF(MSVC)
       ADD_DEFINITIONS(-DSTL_MSVC)
   ENDIF()
ENDIF()


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
