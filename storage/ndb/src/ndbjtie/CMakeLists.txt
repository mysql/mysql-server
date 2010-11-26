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

ADD_SUBDIRECTORY(jtie)
ADD_SUBDIRECTORY(mysql)
ADD_SUBDIRECTORY(test)

IF(HAVE_JDK)

  INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})

  INCLUDE_DIRECTORIES(mysql 
                      utils 
                      jtie)

  ADD_LIBRARY(ndbjtie STATIC ndbjtie_lib.cpp)

ENDIF(HAVE_JDK)

SET ( JAVA_SOURCE_DIRS
 ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/mysql/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/mgmapi/*.java
 ${CMAKE_CURRENT_SOURCE_DIR}/com/mysql/ndbjtie/ndbapi/*.java)

SET ( CLASSPATH jtie/target/classes)

CREATE_JAR(ndbjtie ${JAVA_SOURCE_DIRS} 
  CLASSPATH ${CLASSPATH} 
  DEPENDENCIES jtie.jar)

