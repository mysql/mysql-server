# Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
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


# NDB_REQUIRE_VARIABLE
#
# Check that the variable with given name is defined
#
MACRO(NDB_REQUIRE_VARIABLE variable_name)
  #MESSAGE(STATUS "Checking variable ${variable_name} required by NDB")
  IF("${${variable_name}}" STREQUAL "")
    MESSAGE(FATAL_ERROR "The variable ${variable_name} is required "
                         "to build NDB")
  ENDIF()
ENDMACRO()
