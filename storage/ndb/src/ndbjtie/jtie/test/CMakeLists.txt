# Copyright (c) 2010, 2012 Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

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

ADD_EXECUTABLE(jtie_unit_tests-t jtie_unit_tests-t.cpp)
