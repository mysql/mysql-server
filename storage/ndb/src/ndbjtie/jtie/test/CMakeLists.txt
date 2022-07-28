# Copyright (c) 2010, 2022, Oracle and/or its affiliates.
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

# The jties tests intentionally checks more or less all permutations of
# const result/parameter types in order to see what is supported
# by C++ vs. Java. This causes warnings about "type qualifiers ignored
# on function return type" (which is turned on by -Wextra in maintainer.cmake).
# And also some shadow-field warnings with clang.
# Avoid the superfluous warnings by turning them off.
MY_CHECK_CXX_COMPILER_WARNING("ignored-qualifiers" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  MESSAGE(STATUS "Turning off ignored qualifier warnings for jtie tests")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("shadow-field" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  MESSAGE(STATUS "Turning off shadow field warnings for jtie tests")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

ADD_SUBDIRECTORY(myapi)
ADD_SUBDIRECTORY(myjapi)
ADD_SUBDIRECTORY(unload)
