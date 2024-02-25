# Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(cmake/version.cmake)
SET(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# enable MACOSX_RPATH
CMAKE_POLICY(SET CMP0042 NEW)
# warn about non-existing dependencies in add_dependencies()
CMAKE_POLICY(SET CMP0046 NEW)
# set the VERSION as documented in PROJECT() command
CMAKE_POLICY(SET CMP0048 NEW)
# only interpret if() arguments as variables or keywords when unquoted
CMAKE_POLICY(SET CMP0054 NEW)
PROJECT("MySQLRouter" VERSION ${PROJECT_VERSION_TEXT} LANGUAGES C CXX)
MSVC_CPPCHECK_DISABLE()

# In CMake 3.12 and above, the
#
# * ``check_include_file`` macro in the ``CheckIncludeFile`` module, the
# * ``check_include_file_cxx`` macro in the
#   ``CheckIncludeFileCXX`` module, and the
# * ``check_include_files`` macro in the ``CheckIncludeFiles`` module
#
# now prefer to link the check executable to the libraries listed in the
# ``CMAKE_REQUIRED_LIBRARIES`` variable.
IF(POLICY CMP0075)
  CMAKE_POLICY(SET CMP0075 NEW)
ENDIF()

IF(SOLARIS)
  # disable rapidjson optimisation on Solaris as it breaks
  # shared objects that build with -fPIC
  ADD_DEFINITIONS(-DRAPIDJSON_48BITPOINTER_OPTIMIZATION=0)

  # MD5_Init() and others are deprecated.
  IF(MY_COMPILER_IS_GNU_OR_CLANG)
    ADD_COMPILE_OPTIONS("-Wno-deprecated-declarations")
  ENDIF()
ENDIF()

# ld.lld: error: corrupt input file:
# version definition index 0 for symbol __gcov_var is out of bounds
IF(ENABLE_GCOV)
  STRING(REPLACE "-fuse-ld=lld" ""
    CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS}")
  STRING(REPLACE "-fuse-ld=lld" ""
    CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  STRING(REPLACE "-Wl,--gdb-index" ""
    CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS}")
  STRING(REPLACE "-Wl,--gdb-index" ""
    CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

IF(WIN32)
  # Activate necessary dllexport/dllimport declarations.
  ADD_DEFINITIONS(-DPROTOBUF_USE_DLLS)
ENDIF()

IF(WIN32)
  # 'identifier' : class 'type' needs to have dll-interface to be used
  # by clients of class 'type2'
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
ENDIF()

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/rapidjson.cmake)

SET(ROUTER_LICENSE_FILE "${CMAKE_SOURCE_DIR}/router/LICENSE.router")
SET(ROUTER_README_FILE "${CMAKE_SOURCE_DIR}/router/README.router")

SET(DOC_DESTINATION ".")
IF(NOT INSTALL_LAYOUT MATCHES "RPM")
  INSTALL(FILES
      ${ROUTER_LICENSE_FILE}
      ${ROUTER_README_FILE}
      DESTINATION ${DOC_DESTINATION} COMPONENT Router OPTIONAL)
ENDIF()

INCLUDE(cmake/settings.cmake)
INCLUDE(cmake/set_rpath.cmake)

# Required tools, libraries, etc..
INCLUDE(cmake/testing.cmake)  # does not enable testing
INCLUDE(cmake/configure.cmake)
INCLUDE(cmake/packaging.cmake)
INCLUDE(cmake/Plugin.cmake)
INCLUDE(cmake/fuzzer.cmake)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/include)

# a meta-target to allow build everything router related, but nothing that
# it doesn't depend on
#
# each target needs to add itself via
#
#   ADD_DEPENDENCIES(mysqlrouter_all ...)
#
ADD_CUSTOM_TARGET(mysqlrouter_all)

# Load all modules, including plugins
ADD_SUBDIRECTORY(src)

# Enable testing


IF(WITH_UNIT_TESTS)
  ADD_SUBDIRECTORY(tests)
ENDIF()
