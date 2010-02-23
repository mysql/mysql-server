# Copyright (C) 2009 MySQL AB
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

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapi.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/FindJava.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/FindJNI.cmake)

SET (JAVA_SOURCE_DIRS
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/openjpa/*.java)

SET(CLASSPATH 
  target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${WITH_CLASSPATH})

SET(JAR_DEPENDENCIES 
  clusterj-api.jar
  clusterj-core.jar)

CREATE_JAR(cluster-openjpa.jar ${JAVA_SOURCE_DIRS} 
  CLASSPATH ${CLASSPATH} 
  DEPENDENCIES ${JAR_DEPENDENCIES})
