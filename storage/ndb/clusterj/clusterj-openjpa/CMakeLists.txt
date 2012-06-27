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

SET(DEP ndbjtie.jar
        jtie.jar
        clusterj-api.jar
        clusterj-core.jar
        clusterj-tie.jar)

SET(JARS ${CMAKE_CURRENT_BINARY_DIR}/../../src/ndbjtie/ndbjtie.jar
         ${CMAKE_CURRENT_BINARY_DIR}/../../src/ndbjtie/jtie/jtie.jar
         ${CMAKE_CURRENT_BINARY_DIR}/../clusterj-api/clusterj-api.jar
         ${CMAKE_CURRENT_BINARY_DIR}/../clusterj-core/clusterj-core.jar
         ${CMAKE_CURRENT_BINARY_DIR}/../clusterj-tie/clusterj-tie.jar)

SET(CLUSTERJ_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation,com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie)

FILE(GLOB JAVA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/openjpa/*.java)

SET(CLASSPATH 
  target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${WITH_CLASSPATH})

SET(CLUSTERJPA_EXPORTS "${CLUSTERJ_EXPORTS},com.mysql.clusterj.openjpa")
CREATE_MANIFEST(clusterjpa_manifest.mf ${CLUSTERJPA_EXPORTS} clusterjpa)
CREATE_JAR(clusterjpa ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/clusterjpa_manifest.mf
  DEPENDENCIES ${DEP}
  EXTRA_FILES src/main/resources/META-INF
              src/main/resources/com
  MERGE_JARS ${JARS})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/clusterjpa-${MYSQL_CLUSTER_VERSION}.jar
        DESTINATION ${INSTALL_MYSQLSHAREDIR}/java COMPONENT ClusterJ)
