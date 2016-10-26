# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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


#
# Some mcrios to for simplifying the cmake script
#
MACRO(SET_C_FLAGS_NOT_EXIST)
  FOREACH(FLAG ${ARGN})
    IF (NOT CMAKE_C_FLAGS MATCHES ${FLAG} )
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
    ENDIF()
  ENDFOREACH()
ENDMACRO()

MACRO(SET_CXX_FLAGS_NOT_EXIST)
  FOREACH(FLAG ${ARGN})
    IF (NOT CMAKE_CXX_FLAGS MATCHES "${FLAG}")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}")
    ENDIF()
  ENDFOREACH()
ENDMACRO()
