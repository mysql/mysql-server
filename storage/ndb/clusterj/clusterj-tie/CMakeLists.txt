# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

FILE(GLOB JAVA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/tie/*.java)

SET ( CLASSPATH 
  target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes)

SET ( JAR_DEPENDENCIES
  clusterj-core.jar
  ndbjtie.jar)

CREATE_JAR(clusterj-tie ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  DEPENDENCIES ${JAR_DEPENDENCIES}
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/com)
