# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/components/example)

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

MYSQL_ADD_COMPONENT(example_component1
  english_greeting_service_imp.cc
  example_component1.cc
  simple_example_math_imp.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(example_component2
  example_component2.cc
  polish_greeting_service_imp.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(example_component3
  example_component3.cc
  example_math_wrapping_imp.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_string_service
  test_string_service.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_string_service_long
  test_string_service_long.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_string_service_charset
  test_string_service_charset.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_backup_lock_service
  test_backup_lock_service.cc
  TEST MODULE)
