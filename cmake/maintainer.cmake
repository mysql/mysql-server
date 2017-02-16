# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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

# Common warning flags for GCC, G++, Clang and Clang++
SET(MY_WARNING_FLAGS "-Wall -Wextra -Wformat-security")
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

# Extra warning flags for Clang++
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wno-null-conversion -Wno-unused-private-field")
ENDIF()

# Turn on Werror (warning => error) when using maintainer mode.
IF(MYSQL_MAINTAINER_MODE MATCHES "ON")
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -DFORCE_INIT_OF_VARS -Werror")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -DFORCE_INIT_OF_VARS -Werror")
ENDIF()

# Set warning flags for GCC/Clang
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(MY_MAINTAINER_C_WARNINGS "${MY_C_WARNING_FLAGS}")
ENDIF()
# Set warning flags for G++/Clang++
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(MY_MAINTAINER_CXX_WARNINGS "${MY_CXX_WARNING_FLAGS}")
ENDIF()

IF(MYSQL_MAINTAINER_MODE MATCHES "ON")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_MAINTAINER_C_WARNINGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_MAINTAINER_CXX_WARNINGS}")
ELSEIF(MYSQL_MAINTAINER_MODE MATCHES "AUTO")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_MAINTAINER_C_WARNINGS}")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${MY_MAINTAINER_CXX_WARNINGS}")
ENDIF()
