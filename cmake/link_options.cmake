# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

# As of cmake 3.13 there is a new target_link_options(....) function.
# This adds link options to executables or shared libraries.

# For example, "LINKER:-z,defs" becomes
# for gcc   : -Wl,-z,defs
# for clang : -Xlinker -z -Xlinker defs
# for SunPro: -Qoption ld -z,defs

# This is a home-grown replacement for target_link_options, since
# we need to support older versions of cmake.

IF(UNIX)
  SET(MY_LINKER_WRAPPER_FLAG "-Wl,")
ELSE()
  SET(MY_LINKER_WRAPPER_FLAG "")
ENDIF()

# We currently only need -Wl,<comma separated option list>
# so we can simply do STRING_APPEND on the appropriate LINK_FLAGS.
FUNCTION(MY_TARGET_LINK_OPTIONS target option_string)
  IF(option_string STREQUAL "")
    RETURN()
  ENDIF()
  GET_TARGET_PROPERTY(target_link_flags ${target} LINK_FLAGS)
  IF(NOT target_link_flags)
    SET(target_link_flags)
  ENDIF()
  IF(option_string MATCHES "LINKER:")
    STRING(REPLACE "LINKER:" "" option_string "${option_string}")
    STRING_APPEND(target_link_flags
      " ${MY_LINKER_WRAPPER_FLAG}${option_string}")
  ELSE()
    STRING_APPEND(target_link_flags " ${option_string}")
  ENDIF()
  SET_TARGET_PROPERTIES(${target} PROPERTIES LINK_FLAGS ${target_link_flags})
ENDFUNCTION()
