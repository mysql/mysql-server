# Copyright (c) 2026, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(../src ${PROJECT_SOURCE_DIR}/tests/ ./)
ADD_DEFINITIONS(-DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")

ADD_TEST_FILE(test_plugin.cc
  EXTRA_SOURCES
    ../src/host_cache.cc
    ./test_host_cache.cc
  MODULE host_cache
  LIB_DEPENDS test-helpers
  )

ROUTERTEST_GET_TARGET(T "test_plugin.cc" "host_cache")
TARGET_INCLUDE_DIRECTORIES(${T}
  PRIVATE
    $<TARGET_PROPERTY:host_cache,INCLUDE_DIRECTORIES>
  )
TARGET_COMPILE_DEFINITIONS(${T}
  PRIVATE
  HOST_CACHE_STATIC_DEFINE=1
  )
