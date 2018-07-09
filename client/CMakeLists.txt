# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/client
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/mysys_ssl
  ${LZ4_INCLUDE_DIR}
  ${SSL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/strings
  ${EDITLINE_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

## Subdirectory with common client code.
ADD_SUBDIRECTORY(base)
## Subdirectory for mysqlpump code.
ADD_SUBDIRECTORY(dump)

## We will need libeay32.dll and ssleay32.dll when running client executables.
COPY_OPENSSL_DLLS(copy_openssl_client)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

ADD_DEFINITIONS(${SSL_DEFINES})
MYSQL_ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc ../sql-common/sql_string.cc)
TARGET_LINK_LIBRARIES(mysql mysqlclient)
IF(UNIX)
  TARGET_LINK_LIBRARIES(mysql ${EDITLINE_LIBRARY})
ENDIF(UNIX)

IF(NOT WITHOUT_SERVER)
  MYSQL_ADD_EXECUTABLE(mysql_upgrade
    upgrade/program.cc
  )
ADD_COMPILE_FLAGS(
  upgrade/program.cc COMPILE_FLAGS -I${BOOST_PATCHES_DIR} -I${BOOST_INCLUDE_DIR}
)
TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient client_base mysqlcheck_core)
ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs GenSysSchema)
ENDIF()

MYSQL_ADD_EXECUTABLE(mysqltest mysqltest.cc COMPONENT Test)
SET_SOURCE_FILES_PROPERTIES(mysqltest.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqltest mysqlclient regex)
ADD_DEPENDENCIES(mysqltest GenError)

ADD_CONVENIENCE_LIBRARY(mysqlcheck_core check/mysqlcheck_core.cc)
TARGET_LINK_LIBRARIES(mysqlcheck_core mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlcheck check/mysqlcheck.cc)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlcheck_core)

MYSQL_ADD_EXECUTABLE(mysqldump mysqldump.c ../sql-common/my_user.c)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlimport mysqlimport.c)
SET_SOURCE_FILES_PROPERTIES(mysqlimport.c PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlshow mysqlshow.c)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_plugin mysql_plugin.c)
TARGET_LINK_LIBRARIES(mysql_plugin mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc)
ADD_COMPILE_FLAGS(
  mysqlbinlog.cc
  COMPILE_FLAGS "-I${CMAKE_SOURCE_DIR}/sql" "-DHAVE_REPLICATION" "-DDISABLE_PSI_MUTEX"
)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient binlogevents_static)

MYSQL_ADD_EXECUTABLE(mysqladmin mysqladmin.cc)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient)

MYSQL_ADD_EXECUTABLE(mysqlslap mysqlslap.cc)
SET_SOURCE_FILES_PROPERTIES(mysqlslap.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlslap mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_config_editor mysql_config_editor.cc)
TARGET_LINK_LIBRARIES(mysql_config_editor mysqlclient)

MYSQL_ADD_EXECUTABLE(mysql_secure_installation mysql_secure_installation.cc)
TARGET_LINK_LIBRARIES(mysql_secure_installation mysqlclient)

IF(UNIX AND NOT WITHOUT_SERVER)
  MYSQL_ADD_EXECUTABLE(mysql_install_db
    mysql_install_db.cc auth_utils.cc path.cc logger.cc)
  TARGET_LINK_LIBRARIES(mysql_install_db mysqlclient)
  ADD_COMPILE_FLAGS(
    auth_utils.cc mysql_install_db.cc
    COMPILE_FLAGS "-I${CMAKE_SOURCE_DIR}/sql/auth"
    )
  ADD_DEPENDENCIES(mysql_install_db GenBootstrapPriv GenSysSchema)
ENDIF()

MYSQL_ADD_EXECUTABLE(mysql_ssl_rsa_setup mysql_ssl_rsa_setup.cc path.cc logger.cc)
TARGET_LINK_LIBRARIES(mysql_ssl_rsa_setup mysys mysys_ssl)
SET_TARGET_PROPERTIES(mysql_ssl_rsa_setup PROPERTIES LINKER_LANGUAGE CXX)

# "WIN32" also covers 64 bit. "echo" is used in some files below "mysql-test/".
IF(WIN32)
  MYSQL_ADD_EXECUTABLE(echo echo.c)
ENDIF(WIN32)

SET_TARGET_PROPERTIES (
  mysql_plugin
  mysqlcheck
  mysqldump
  mysqlimport
  mysqlshow
  mysqlslap
PROPERTIES HAS_CXX TRUE)

ADD_DEFINITIONS(-DHAVE_DLOPEN)

