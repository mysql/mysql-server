# Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(CheckCCompilerFlag)

# Setup GCC (GNU C compiler) warning options.
MACRO(SET_MYSQL_MAINTAINER_GNU_C_OPTIONS)
  SET(MY_MAINTAINER_WARNINGS
      "-Wall -Wextra -Wunused -Wwrite-strings -Wno-strict-aliasing  -Werror")

  CHECK_C_COMPILER_FLAG("-Wdeclaration-after-statement"
                        HAVE_DECLARATION_AFTER_STATEMENT)
  IF(HAVE_DECLARATION_AFTER_STATEMENT)
    SET(MY_MAINTAINER_DECLARATION_AFTER_STATEMENT
        "-Wdeclaration-after-statement")
  ENDIF()
  SET(MY_MAINTAINER_C_WARNINGS
      "${MY_MAINTAINER_WARNINGS} ${MY_MAINTAINER_DECLARATION_AFTER_STATEMENT}"
      CACHE STRING "C warning options used in maintainer builds.")
  # Do not make warnings in checks into errors.
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Wno-error")
ENDMACRO()


# Setup G++ (GNU C++ compiler) warning options.
MACRO(SET_MYSQL_MAINTAINER_GNU_CXX_OPTIONS)
  SET(MY_MAINTAINER_CXX_WARNINGS
      "${MY_MAINTAINER_WARNINGS} -Wno-unused-parameter -Woverloaded-virtual"
      CACHE STRING "C++ warning options used in maintainer builds.")
ENDMACRO()

# Setup ICC (Intel C Compiler) warning options.
MACRO(SET_MYSQL_MAINTAINER_INTEL_C_OPTIONS)
  SET(MY_MAINTAINER_WARNINGS "-Wcheck")
  SET(MY_MAINTAINER_C_WARNINGS "${MY_MAINTAINER_WARNINGS}"
      CACHE STRING "C warning options used in maintainer builds.")
ENDMACRO()

# Setup ICPC (Intel C++ Compiler) warning options.
MACRO(SET_MYSQL_MAINTAINER_INTEL_CXX_OPTIONS)
  SET(MY_MAINTAINER_CXX_WARNINGS "${MY_MAINTAINER_WARNINGS}"
      CACHE STRING "C++ warning options used in maintainer builds.")
ENDMACRO()

