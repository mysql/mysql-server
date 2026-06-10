# Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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

DISABLE_MISSING_PROFILE_WARNING()

SET(MY_SYSTEM_SOURCES
  my_system.cc
  my_system_api/my_system_api_common.cc
)

IF(UNIX)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_cgroup.cc)
ENDIF()

IF(LINUX)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_linux.cc)
ELSEIF(APPLE)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_apple.cc)
ELSEIF(FREEBSD)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_freebsd.cc)
ELSEIF(SOLARIS)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_solaris.cc)
ELSEIF(WIN32)
  LIST(APPEND MY_SYSTEM_SOURCES
    my_system_api/my_system_api_win.cc)
ENDIF()

ADD_LIBRARY(library_mysys STATIC
  my_memory.cc
  my_hex_tools.cc
  my_base64_encode.cc
  option_tracker_usage.cc
  ${MY_SYSTEM_SOURCES}
)
ADD_DEPENDENCIES(library_mysys GenError)

IF(UNIX)
  TARGET_COMPILE_OPTIONS(library_mysys PRIVATE "-fvisibility=hidden")
ENDIF()

# Use default value for -ftls-model which is global-dynamic with -fPIC
REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_C_FLAGS "-ftls-model=initial-exec")
REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_CXX_FLAGS "-ftls-model=initial-exec")

TARGET_LINK_LIBRARIES(library_mysys rapidjson)
