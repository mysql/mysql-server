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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/tests/)

# Google-test TYPED_TEST doesn't override the virtual method in derived class.
STRING(REPLACE "-Wsuggest-override" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

ADD_GOOGLETEST_FILE(test_tls.cc
  MODULE openssl
  LIB_DEPENDS
    routertest_helpers
    router_lib
    harness_net_ts
    harness_tls
    harness-library
  )

TARGET_INCLUDE_DIRECTORIES(routertest_openssl_tls
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  ${CMAKE_CURRENT_SOURCE_DIR}/
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  )
