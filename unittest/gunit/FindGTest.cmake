# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Locate the Google C++ Testing Framework.
#
# Defines the following variables:
#
#   GTEST_FOUND - Found the Google Testing framework
#   GTEST_INCLUDE_DIRS - Include directories
#
# Also defines the library variables below as normal
# variables.  These contain debug/optimized keywords when
# a debugging library is found.
#
#   GTEST_BOTH_LIBRARIES - Both libgtest & libgtest-main
#   GTEST_LIBRARIES - libgtest
#   GTEST_MAIN_LIBRARIES - libgtest-main
#
# Accepts the following variables as input:
#
#   GTEST_ROOT - (as CMake or env. variable)
#                The root directory of the gtest install prefix
#
#   GTEST_MSVC_SEARCH - If on MSVC, enables searching the build tree of
#                       GTest if set to MD or MT (defaults: MD)
#
#-----------------------
# Example Usage:
#
#    enable_testing(true)
#    find_package(GTest REQUIRED)
#    include_directories(${GTEST_INCLUDE_DIRS})
#
#    add_executable(foo foo.cc)
#    target_link_libraries(foo ${GTEST_BOTH_LIBRARIES})
#
#    add_test(AllTestsInFoo foo)
#
#-----------------------
#
# If you would like each Google test to show up in CTest as
# a test you may use the following macro.  NOTE: It WILL slow
# down your tests, so be warned.
#
# GTEST_ADD_TESTS(executable extra_args ARGN)
#    executable = The path to the test executable
#    extra_args = Pass a list of extra arguments to be passed to
#                 executable enclosed in quotes (or "" for none)
#    ARGN =       A list of source files to search for tests & test
#                 fixtures.
#
#  Example:
#     set(FooTestArgs --foo 1 --bar 2)
#     add_executable(FooTest FooUnitTest.cc)
#     GTEST_ADD_TESTS(FooTest "${FooTestArgs}" FooUnitTest.cc)

#=============================================================================
# Copyright 2009 Kitware, Inc.
# Copyright 2009 Philip Lowman <philip@yhbt.com>
# Copyright 2009 Daniel Blezek <blezek@gmail.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# CMake - Cross Platform Makefile Generator
# Copyright 2000-2009 Kitware, Inc., Insight Software Consortium
# All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# * Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#  notice, this list of conditions and the following disclaimer in the
#  documentation and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
#  nor the names of their contributors may be used to endorse or promote
#  products derived from this software without specific prior written
#  permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ------------------------------------------------------------------------------
# 
# The above copyright and license notice applies to distributions of
# CMake in source and binary form.  Some source files contain additional
# notices of original copyright by their contributors; see each source
# for details.  Third-party software packages supplied with CMake under
# compatible licenses provide their own copyright notices documented in
# corresponding subdirectories.
#----------------------------------------------------------------------------

# CMake was initially developed by Kitware with the following sponsorship:
#
# * National Library of Medicine at the National Institutes of Health
#   as part of the Insight Segmentation and Registration Toolkit (ITK).
#
# * US National Labs (Los Alamos, Livermore, Sandia) ASC Parallel
#   Visualization Initiative.
#
# * National Alliance for Medical Image Computing (NAMIC) is funded by the
#   National Institutes of Health through the NIH Roadmap for Medical Research,
#   Grant U54 EB005149.
#
# * Kitware, Inc.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
#
# Thanks to Daniel Blezek <blezek@gmail.com> for the GTEST_ADD_TESTS code

function(GTEST_ADD_TESTS executable extra_args)
    if(NOT ARGN)
        message(FATAL_ERROR "Missing ARGN: Read the documentation for GTEST_ADD_TESTS")
    endif()
    foreach(source ${ARGN})
        file(READ "${source}" contents)
        string(REGEX MATCHALL "TEST_?F?\\(([A-Za-z_0-9 ,]+)\\)" found_tests ${contents})
        foreach(hit ${found_tests})
            string(REGEX REPLACE ".*\\(([A-Za-z_0-9]+)[, ]*([A-Za-z_0-9]+)\\).*" "\\1.\\2" test_name ${hit})
            add_test(${test_name} ${executable} --gtest_filter=${test_name} ${extra_args})
        endforeach()
    endforeach()
endfunction()

function(_gtest_append_debugs _endvar _library)
    if(${_library} AND ${_library}_DEBUG)
        set(_output optimized ${${_library}} debug ${${_library}_DEBUG})
    else()
        set(_output ${${_library}})
    endif()
    set(${_endvar} ${_output} PARENT_SCOPE)
endfunction()

function(_gtest_find_library _name)
    find_library(${_name}
        NAMES ${ARGN}
        HINTS
            $ENV{GTEST_ROOT}
            ${GTEST_ROOT}
        PATH_SUFFIXES ${_gtest_libpath_suffixes}
    )
    mark_as_advanced(${_name})
endfunction()

#

if(NOT DEFINED GTEST_MSVC_SEARCH)
    set(GTEST_MSVC_SEARCH MD)
endif()

set(_gtest_libpath_suffixes lib)
if(MSVC)
    if(GTEST_MSVC_SEARCH STREQUAL "MD")
        list(APPEND _gtest_libpath_suffixes
            msvc/gtest-md/Debug
            msvc/gtest-md/Release)
    elseif(GTEST_MSVC_SEARCH STREQUAL "MT")
        list(APPEND _gtest_libpath_suffixes
            msvc/gtest/Debug
            msvc/gtest/Release)
    endif()
endif()


find_path(GTEST_INCLUDE_DIR gtest/gtest.h
    HINTS
        $ENV{GTEST_ROOT}/include
        ${GTEST_ROOT}/include
)
mark_as_advanced(GTEST_INCLUDE_DIR)

if(MSVC AND GTEST_MSVC_SEARCH STREQUAL "MD")
    # The provided /MD project files for Google Test add -md suffixes to the
    # library names.
    _gtest_find_library(GTEST_LIBRARY            gtest-md  gtest)
    _gtest_find_library(GTEST_LIBRARY_DEBUG      gtest-mdd gtestd)
    _gtest_find_library(GTEST_MAIN_LIBRARY       gtest_main-md  gtest_main)
    _gtest_find_library(GTEST_MAIN_LIBRARY_DEBUG gtest_main-mdd gtest_maind)
else()
    _gtest_find_library(GTEST_LIBRARY            gtest)
    _gtest_find_library(GTEST_LIBRARY_DEBUG      gtestd)
    _gtest_find_library(GTEST_MAIN_LIBRARY       gtest_main)
    _gtest_find_library(GTEST_MAIN_LIBRARY_DEBUG gtest_maind)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTest DEFAULT_MSG GTEST_LIBRARY GTEST_INCLUDE_DIR GTEST_MAIN_LIBRARY)

if(GTEST_FOUND)
    set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
    _gtest_append_debugs(GTEST_LIBRARIES      GTEST_LIBRARY)
    _gtest_append_debugs(GTEST_MAIN_LIBRARIES GTEST_MAIN_LIBRARY)
    set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
endif()

