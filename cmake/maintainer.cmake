# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

# Common warning flags for GCC, G++, Clang and Clang++
SET(MY_WARNING_FLAGS "-Wall -Wextra -Wformat-security")

# The default =3 given by -Wextra is a bit too strict for our code.
IF(CMAKE_COMPILER_IS_GNUCXX)
  MY_CHECK_CXX_COMPILER_FLAG("-Wimplicit-fallthrough=2"
    HAVE_IMPLICIT_FALLTHROUGH)
  IF(HAVE_IMPLICIT_FALLTHROUGH)
    SET(MY_WARNING_FLAGS "${MY_WARNING_FLAGS} -Wimplicit-fallthrough=0")
  ENDIF()
ENDIF()

MY_CHECK_C_COMPILER_FLAG("-Wvla" HAVE_WVLA) # Requires GCC 4.3+ or Clang
IF(HAVE_WVLA)
  SET(MY_WARNING_FLAGS "${MY_WARNING_FLAGS} -Wvla")
ENDIF()

# Common warning flags for GCC and Clang
SET(MY_C_WARNING_FLAGS
    "${MY_WARNING_FLAGS} -Wwrite-strings -Wdeclaration-after-statement")

# Common warning flags for G++ and Clang++
SET(MY_CXX_WARNING_FLAGS
    "${MY_WARNING_FLAGS} -Woverloaded-virtual -Wno-unused-parameter")

MY_CHECK_CXX_COMPILER_FLAG("-Wint-in-bool-context" HAVE_INT_IN_BOOL_CONTEXT)
IF(HAVE_INT_IN_BOOL_CONTEXT)
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-int-in-bool-context")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Waligned-new" HAVE_ALIGNED_NEW)
IF(HAVE_ALIGNED_NEW)
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-aligned-new")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wtautological-pointer-compare"
  HAVE_TAUTOLOGICAL_POINTER_COMPARE)
IF(HAVE_TAUTOLOGICAL_POINTER_COMPARE)
  SET(MY_CXX_WARNING_FLAGS
    "${MY_CXX_WARNING_FLAGS} -Wno-tautological-pointer-compare")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wclass-memaccess" HAVE_CLASS_MEMACCESS)
IF(HAVE_CLASS_MEMACCESS)
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-class-memaccess")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wcast-function-type" HAVE_CAST_FUNCTION_TYPE)
IF(HAVE_CAST_FUNCTION_TYPE)
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -Wno-cast-function-type")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-cast-function-type")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wformat-overflow" HAVE_FORMAT_OVERFLOW)
IF(HAVE_FORMAT_OVERFLOW)
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -Wno-format-overflow")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-format-overflow")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wformat-truncation" HAVE_FORMAT_TRUNCATION)
IF(HAVE_FORMAT_TRUNCATION)
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -Wno-format-truncation")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-format-truncation")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wignored-qualifiers" HAVE_IGNORED_QUALIFIERS)
IF(HAVE_IGNORED_QUALIFIERS)
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Wno-ignored-qualifiers")
ENDIF()

# Extra warning flags for Clang++
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wno-null-conversion -Wno-unused-private-field")
ENDIF()

# Turn on Werror (warning => error) when using maintainer mode.
IF(MYSQL_MAINTAINER_MODE)
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -Werror")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Werror")
  SET(COMPILE_FLAG_WERROR 1)
ENDIF()

# Set warning flags for GCC/Clang
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_WARNING_FLAGS}")
ENDIF()
# Set warning flags for G++/Clang++
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")
ENDIF()
