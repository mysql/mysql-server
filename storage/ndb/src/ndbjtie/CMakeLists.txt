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

# set common Java variables for ndbjtie unit tests
# neither wildcards (nor line breaks) for clean targets supported by cmake
#SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "*.log")
IF(HAVE_JAVA)

  # the native path to the JVM
  FILE(TO_NATIVE_PATH "${Java_JAVA_EXECUTABLE}" Java_JAVA_EXECUTABLE_PATH)

ENDIF(HAVE_JAVA)

ADD_SUBDIRECTORY(jtie)
ADD_SUBDIRECTORY(test)

IF(HAVE_JDK)

  INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})

  INCLUDE_DIRECTORIES(utils
                      jtie)

  ADD_CONVENIENCE_LIBRARY(ndbjtie ndbjtie_lib.cpp)

ENDIF(HAVE_JDK)

# the NdbJTie Java API
SET(JAVA_SOURCE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/mysql
  ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/mgmapi
  ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/ndbapi)

SET(JAVA_SOURCES "")
FOREACH(D ${JAVA_SOURCE_DIRS})
  FILE(GLOB tmp ${D}/*.java)
  LIST(APPEND JAVA_SOURCES ${tmp})
ENDFOREACH()

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes)

SET(JARS
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/jtie.jar)

CREATE_JAR(ndbjtie ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MERGE_JARS ${JARS}
  DEPENDENCIES jtie.jar)
