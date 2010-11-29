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

IF(HAVE_OPENJPA)
  LIST(APPEND BASE_NAMES openjpa)
ENDIF(HAVE_OPENJPA)
IF(HAVE_JUNIT)
  ADD_SUBDIRECTORY(clusterj-test)
  IF(HAVE_OPENJPA)
    ADD_SUBDIRECTORY(clusterj-jpatest)
  ENDIF(HAVE_OPENJPA)
ENDIF(HAVE_JUNIT)

FOREACH(BASE_NAME ${BASE_NAMES})

  ADD_SUBDIRECTORY(clusterj-${BASE_NAME})

  SET(BASE_DIR ${CMAKE_SOURCE_DIR}/storage/ndb/clusterj/clusterj-${BASE_NAME})
  SET(BUNDLE ${BASE_DIR}/src/main/resources/com/mysql/clusterj/${BASE_NAME}/Bundle.properties)

  LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/target/classes" "com/mysql/clusterj")

  IF(EXISTS ${BASE_DIR}/src/main/resources/META-INF/services)
    LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/src/main/resources" "META-INF/services")
  ENDIF()  
  IF(EXISTS ${BUNDLE})
    LIST(APPEND JAVA_CLASS_DIRS "${BASE_DIR}/src/main/resources" "com/mysql/clusterj/${BASE_NAME}/Bundle.properties")
  ENDIF() 

  # Generate clusterj.jar dependencies from the BASE_NAME
  IF(BASE_NAME STREQUAL openjpa)
    # Exception to confirm rule:
    # "jar built in clusterj-openjpa is called clusterjpa"
    SET(CLUSTERJ_JAR_DEPENDS ${CLUSTERJ_JAR_DEPENDS} clusterjpa.jar)
  ELSE()
    SET(CLUSTERJ_JAR_DEPENDS ${CLUSTERJ_JAR_DEPENDS} clusterj-${BASE_NAME}.jar)
  ENDIF()


ENDFOREACH(BASE_NAME ${BASE_NAMES})

LIST(APPEND JAVA_CLASS_DIRS "${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/target/classes" "com/mysql/ndbjtie")
LIST(APPEND JAVA_CLASS_DIRS "${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes" "com/mysql/jtie")

SET(CLUSTERJ_EXPORTS com.mysql.clusterj,com.mysql.clusterj.query,com.mysql.clusterj.annotation,com.mysql.clusterj.metadata,com.mysql.clusterj.spi,com.mysql.clusterj.store,com.mysql.clusterj.util,com.mysql.clusterj.tie)
CREATE_MANIFEST(clusterj_manifest.mf ${CLUSTERJ_EXPORTS} clusterj)
CREATE_JAR_FROM_CLASSES(clusterj ${JAVA_CLASS_DIRS}
  MANIFEST clusterj_manifest.mf
  DEPENDENCIES ndbjtie.jar jtie.jar ${CLUSTERJ_JAR_DEPENDS})


