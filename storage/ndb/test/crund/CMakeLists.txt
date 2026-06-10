# Copyright (c) 2010, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/helpers)

#
## runCrund
###
NDB_ADD_EXECUTABLE(runCrund runCrund.cpp NDBCLIENT)
TARGET_COMPILE_DEFINITIONS(runCrund PRIVATE
                           CMAKE_BINARY_DIR=\"${CMAKE_BINARY_DIR}\"
                           WITH_CLASSPATH=\"${WITH_CLASSPATH}\"
                          )

#
## C++ Crund Helpers
###
SET (C_HELPER_SOURCES
     helpers/src/hrt_stopwatch.cpp
     helpers/src/hrt_gstopwatch.cpp
     helpers/src/hrt_utils.cpp
     )

ADD_CONVENIENCE_LIBRARY(crund_helpers ${C_HELPER_SOURCES})

NDB_ADD_TEST(hrt_utils_test helpers/src/hrt_utils_test.cpp LIBS crund_helpers)
NDB_ADD_TEST(hrt_stopwatch_test helpers/src/hrt_stopwatch_test.cpp
             LIBS crund_helpers)
NDB_ADD_TEST(Properties_test helpers/src/Properties_test.cpp)

#
## C++ Crund (AB load for ndbapi)
###
ADD_SUBDIRECTORY(cpp)

#
## Java Crund (AB & S loads for ClusterJ, Ndbjtie, and JDBC)
###
IF(WITH_NDB_JAVA)
  ADD_SUBDIRECTORY(java)
ENDIF()
