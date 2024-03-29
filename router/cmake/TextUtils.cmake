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

# This function can be used to output a range of elements using a
# serial comma (also known as the Oxford comma).
# For example calling it with parameters (2 3 5 7 11) will produce result:
# "2, 3, 5, 7, and 11":
FUNCTION(oxford_comma _var)
  IF(ARGC EQUAL 2)
    SET(${_var} "${ARGV1}" PARENT_SCOPE)
  ELSEIF(ARGC EQUAL 3)
    SET(${_var} "${ARGV1} and ${ARGV2}" PARENT_SCOPE)
  ELSE()
    SET(_count 3)
    SET(_glue)
    SET(_result)
    FOREACH(_arg ${ARGN})
      SET(_result "${_result}${_glue}${_arg}")
      IF(_count LESS ARGC)
        SET(_glue ", ")
      ELSE()
        SET(_glue ", and ")
      ENDIF()
      MATH(EXPR _count "${_count}+1")
    ENDFOREACH()
    SET(${_var} "${_result}" PARENT_SCOPE)
  ENDIF()
ENDFUNCTION()
