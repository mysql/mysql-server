# Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/libmysqld/include
                    ${CMAKE_SOURCE_DIR}/regex 
                    ${CMAKE_SOURCE_DIR}/tests
                    ${READLINE_INCLUDE_DIR}                 
                    )


ADD_DEFINITIONS(-DEMBEDDED_LIBRARY -UMYSQL_CLIENT)


MYSQL_ADD_EXECUTABLE(mysql_embedded ../../client/completion_hash.cc
               ../../client/mysql.cc ../../client/readline.cc)
TARGET_LINK_LIBRARIES(mysql_embedded mysqlserver)
IF(UNIX)
  ADD_DEFINITIONS(${READLINE_DEFINES})
  TARGET_LINK_LIBRARIES(mysql_embedded ${READLINE_LIBRARY})
  SET_TARGET_PROPERTIES(mysql_embedded PROPERTIES ENABLE_EXPORTS TRUE)
ENDIF(UNIX)

IF(VERSION MATCHES "-ndb-" AND INSTALL_LAYOUT MATCHES "SVR4")
  # The mysqltest_embedded and mysql_client_test_embedded are big, and at
  # present may cause the Solaris SVR4 packages for MySQL Cluster to be
  # too big (>2GB) for the packaging tools to handle.
  # Since these binaries are only used for testing the embedded server 
  # they can safely be excluded from the build.
  RETURN()
ENDIF()



MYSQL_ADD_EXECUTABLE(mysqltest_embedded ../../client/mysqltest.cc)
TARGET_LINK_LIBRARIES(mysqltest_embedded mysqlserver)


IF(CMAKE_GENERATOR MATCHES "Xcode")
# It does not seem possible to tell Xcode the resulting target might need
# to be linked with C++ runtime. The project needs to have at least one C++
# file. Add a dummy one.
  ADD_CUSTOM_COMMAND(OUTPUT  
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_client_test_embedded_dummy.cc
   COMMAND ${CMAKE_COMMAND} -E touch 
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_client_test_embedded_dummy.cc
  )
  MYSQL_ADD_EXECUTABLE(mysql_client_test_embedded
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_client_test_embedded_dummy.cc
    ../../tests/mysql_client_test.c)
ELSE()
#
# SVR4 packaging has a hard limit of 2Gb at least on Solaris 10, so
# remove the big mysql_client_test_embedded which is of little use
#
  IF(NOT INSTALL_LAYOUT MATCHES "SVR4")
    MYSQL_ADD_EXECUTABLE(mysql_client_test_embedded ../../tests/mysql_client_test.c)
    SET_TARGET_PROPERTIES(mysql_client_test_embedded  PROPERTIES HAS_CXX TRUE)
  ENDIF()
ENDIF()
IF(NOT INSTALL_LAYOUT MATCHES "SVR4")
  TARGET_LINK_LIBRARIES(mysql_client_test_embedded mysqlserver)
ENDIF()

IF(UNIX)
SET_TARGET_PROPERTIES(mysqltest_embedded PROPERTIES ENABLE_EXPORTS TRUE)
IF(NOT INSTALL_LAYOUT MATCHES "SVR4")
  SET_TARGET_PROPERTIES(mysql_client_test_embedded PROPERTIES ENABLE_EXPORTS TRUE)
  ENDIF()
ENDIF()
