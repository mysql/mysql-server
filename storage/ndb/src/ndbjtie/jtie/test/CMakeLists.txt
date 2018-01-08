# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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
# by C++ vs. Java. Tthis causes warnings about "type qualifiers ignored
# on function return type" (which is turned on by -Wextra in maintainer.cmake).
# Avoid the superfluous warnings by turning them off.
MY_CHECK_CXX_COMPILER_FLAG("-Wignored-qualifiers" HAVE_IGNORED_QUALIFIERS)
IF(HAVE_IGNORED_QUALIFIERS)
  MESSAGE(STATUS "Turning off ignored qualifier warnings for jtie tests")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-qualifiers")
ENDIF()

ADD_SUBDIRECTORY(myapi)
ADD_SUBDIRECTORY(myjapi)
ADD_SUBDIRECTORY(unload)
