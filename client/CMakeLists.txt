# Copyright (C) 2006 MySQL AB
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${ZLIB_INCLUDE_DIR}
  ${SSL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/strings
  ${READLINE_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

ADD_DEFINITIONS(${READLINE_DEFINES})
ADD_DEFINITIONS(${SSL_DEFINES})
ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc sql_string.cc)
TARGET_LINK_LIBRARIES(mysql mysqlclient)
IF(UNIX)
  TARGET_LINK_LIBRARIES(mysql ${READLINE_LIBRARY})
ENDIF(UNIX)

ADD_EXECUTABLE(mysqltest mysqltest.cc)
SET_SOURCE_FILES_PROPERTIES(mysqltest.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqltest mysqlclient regex)


ADD_EXECUTABLE(mysqlcheck mysqlcheck.c)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlclient)

ADD_EXECUTABLE(mysqldump mysqldump.c ../sql-common/my_user.c)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient)

ADD_EXECUTABLE(mysqlimport mysqlimport.c)
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient)

ADD_EXECUTABLE(mysql_upgrade mysql_upgrade.c)
TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient)
ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs)

ADD_EXECUTABLE(mysqlshow mysqlshow.c)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient)

ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient)

ADD_EXECUTABLE(mysqladmin mysqladmin.cc)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient)

ADD_EXECUTABLE(mysqlslap mysqlslap.c)
SET_SOURCE_FILES_PROPERTIES(mysqlslap.c PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlslap mysqlclient)

ADD_EXECUTABLE(echo echo.c)

SET_TARGET_PROPERTIES (mysqlcheck mysqldump mysqlimport mysql_upgrade mysqlshow mysqlslap 
PROPERTIES HAS_CXX TRUE)

SET(INSTALL_EXES 
 mysqlcheck 
 mysqldump 
 mysqlimport 
 mysql_upgrade 
 mysqlshow 
 mysqlslap 
 mysql 
 mysqladmin 
 mysqlbinlog 
 mysqltest)
 
INSTALL(TARGETS  ${INSTALL_EXES} DESTINATION bin)
INSTALL_DEBUG_SYMBOLS("${INSTALL_EXES}")

