# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

SET ( JAVA_SOURCE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/metadata
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/query
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/spi
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/store
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/util)

SET ( BROKEN_JAVAC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/metadata/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/query/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/spi/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/store/*.java
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/core/util/*.java)

SET ( JAVA_SOURCES "")
FOREACH(D ${JAVA_SOURCE_DIRS})
  FILE(GLOB tmp ${D}/*.java)
  LIST(APPEND JAVA_SOURCES ${tmp})
ENDFOREACH()

SET ( CLASSPATH 
  target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes)

CREATE_JAR(clusterj-core ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  DEPENDENCIES clusterj-api.jar
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/com
  BROKEN_JAVAC ${BROKEN_JAVAC})
