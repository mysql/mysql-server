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
INCLUDE("${PROJECT_SOURCE_DIR}/win/mysql_manifest.cmake")

# We use the "mysqlclient_notls" library here just as safety, in case
# any of the clients here would go beond the client API and access the
# Thread Local Storage directly.

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/strings)

ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc sql_string.cc ../mysys/my_conio.c)
TARGET_LINK_LIBRARIES(mysql mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqltest mysqltest.c ../mysys/my_getsystime.c
               ../mysys/my_copy.c ../mysys/my_mkdir.c)
TARGET_LINK_LIBRARIES(mysqltest mysqlclient_notls regex wsock32)

ADD_EXECUTABLE(mysqlcheck mysqlcheck.c)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqldump mysqldump.c ../sql-common/my_user.c ../mysys/mf_getdate.c)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqlimport mysqlimport.c)
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysql_upgrade mysql_upgrade.c ../mysys/my_getpagesize.c)
TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient_notls wsock32)
ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs)

ADD_EXECUTABLE(mysqlshow mysqlshow.c)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc
			   ../mysys/mf_tempdir.c
			   ../mysys/my_new.cc
			   ../mysys/my_bit.c
			   ../mysys/my_bitmap.c
			   ../mysys/my_vle.c
			   ../mysys/base64.c)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqladmin mysqladmin.cc)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient_notls wsock32)

ADD_EXECUTABLE(mysqlslap mysqlslap.c)
SET_SOURCE_FILES_PROPERTIES(mysqlslap.c PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlslap mysqlclient mysys zlib wsock32 dbug)

ADD_EXECUTABLE(echo echo.c)

IF(EMBED_MANIFESTS)
  MYSQL_EMBED_MANIFEST("mysql" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqltest" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlcheck" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqldump" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlimport" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysql_upgrade" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlshow" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlbinlog" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqladmin" "asInvoker")
  MYSQL_EMBED_MANIFEST("echo" "asInvoker")
ENDIF(EMBED_MANIFESTS)

