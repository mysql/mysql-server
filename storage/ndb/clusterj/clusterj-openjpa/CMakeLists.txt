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

SET(BASE_NAMES api core tie)

FOREACH(BASE_NAME ${BASE_NAMES})

  SET(BASE_DIR ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-${BASE_NAME})
  SET(BUNDLE ${BASE_DIR}/src/main/resources/com/mysql/clusterj/${BASE_NAME}/Bundle.properties)

  LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/target/classes" "com/mysql/clusterj")

  IF(EXISTS ${BASE_DIR}/src/main/resources/META-INF/services)
    LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/src/main/resources" "META-INF/services")
  ENDIF()
  IF(EXISTS ${BUNDLE})
    LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/src/main/resources" "com/mysql/clusterj/${BASE_NAME}/Bundle.properties")
  ENDIF()

ENDFOREACH(BASE_NAME ${BASE_NAMES})

LIST(APPEND JAVA_CLASS_DIRS "${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/target/classes" "com/mysql/ndbjtie")
LIST(APPEND JAVA_CLASS_DIRS "${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes" "com/mysql/jtie")

SET(CLUSTERJ_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation,com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie)

SET (JAVA_SOURCE_DIRS
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/com/mysql/clusterj/openjpa/*.java)

SET(CLASSPATH 
  target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-api/target/classes
  ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-core/target/classes
  ${WITH_CLASSPATH})

SET(CLUSTERJPA_EXPORTS "${CLUSTERJ_EXPORTS},com.mysql.clusterj.openjpa")
CREATE_MANIFEST(clusterjpa_manifest.mf ${CLUSTERJPA_EXPORTS} clusterjpa)
CREATE_JAR(clusterjpa ${JAVA_SOURCE_DIRS}
  CLASSPATH ${CLASSPATH}
  MANIFEST clusterjpa_manifest.mf
  DEPENDENCIES clusterj-tie.jar
  EXTRA_FILES ${JAVA_CLASS_DIRS})

