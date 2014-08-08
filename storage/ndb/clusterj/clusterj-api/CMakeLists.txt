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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET ( JAVA_SOURCE_DIRS
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/annotation
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/query)

SET ( JAVA_SOURCES "")
FOREACH(D ${JAVA_SOURCE_DIRS})
  FILE(GLOB tmp ${D}/*.java)
  MESSAGE(STATUS "java files found in ${D} -> ${tmp}")
  LIST(APPEND JAVA_SOURCES ${tmp})
ENDFOREACH()

SET(CLUSTERJ_API_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation)
CREATE_MANIFEST(manifest.mf ${CLUSTERJ_API_EXPORTS} clusterj-api)

CREATE_JAR(clusterj-api ${JAVA_SOURCES}
  CLASSPATH target/classes
  MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.mf)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterj-api-${JAVA_NDB_VERSION}.jar
        DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
