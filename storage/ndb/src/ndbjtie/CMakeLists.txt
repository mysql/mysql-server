# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

# set common Java variables for ndbjtie unit tests
# neither wildcards (nor line breaks) for clean targets supported by cmake
#SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "*.log")

# the native path to the JVM
FILE(TO_NATIVE_PATH "${Java_JAVA_EXECUTABLE}" Java_JAVA_EXECUTABLE_PATH)

FOREACH(warning
    "unused-but-set-variable"
    "unused-parameter"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(jtie)
ADD_SUBDIRECTORY(test)

ADD_CONVENIENCE_LIBRARY(ndbjtie
  ndbjtie_lib.cpp
  INCLUDE_DIRECTORIES
  ${JNI_INCLUDE_DIRS}
  jtie
  utils
  )

# the NdbJTie Java API
SET(CLUSTERJ_NDBJTIE_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie)

SET(JAVA_SOURCES
  ${CLUSTERJ_NDBJTIE_PREFIX}/mgmapi/MGMAPI.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/mysql/CharsetMap.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/mysql/CharsetMapConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/mysql/Utils.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NDBAPI.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NDBAPIConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/Ndb.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbBlob.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbBlobConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbDictionary.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbError.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbErrorConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbEventOperation.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbEventOperationConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbIndexOperation.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbIndexOperationConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbIndexScanOperation.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbIndexScanOperationConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbInterpretedCode.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbInterpretedCodeConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbLockHandle.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbLockHandleConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbOperation.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbOperationConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbRecAttr.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbRecAttrConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbRecord.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbRecordConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbScanFilter.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbScanFilterConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbScanOperation.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbScanOperationConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbTransaction.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/NdbTransactionConst.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/Ndb_cluster_connection.java
  ${CLUSTERJ_NDBJTIE_PREFIX}/ndbapi/Ndb_cluster_connectionConst.java
  )

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes)

SET(JARS
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/jtie.jar)

CREATE_JAR(ndbjtie ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MERGE_JARS ${JARS}
  DEPENDENCIES jtie.jar)
