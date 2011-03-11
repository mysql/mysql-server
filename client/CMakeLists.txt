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
# any of the clients here would go beyond the client API and access the
# Thread Local Storage directly.


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/strings
					${CMAKE_CURRENT_BINARY_DIR})

MYSQL_ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc sql_string.cc ../mysys/my_conio.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysql mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqltest mysqltest.cc DESTINATION bin)
SET_SOURCE_FILES_PROPERTIES(mysqltest.cc PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqltest mysqlclient mysys regex wsock32 dbug)

MYSQL_ADD_EXECUTABLE(mysqlcheck mysqlcheck.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqldump mysqldump.c ../sql-common/my_user.c ../mysys/mf_getdate.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqlimport mysqlimport.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysql_upgrade mysql_upgrade.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient_notls wsock32)
ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs)

MYSQL_ADD_EXECUTABLE(mysqlshow mysqlshow.c DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc
			   ../mysys/mf_tempdir.c
			   ../mysys/my_new.cc
			   ../mysys/my_bit.c
			   ../mysys/my_bitmap.c
			   ../mysys/my_vle.c
			   ../mysys/base64.c
			   DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqladmin mysqladmin.cc DESTINATION bin)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient_notls wsock32)

MYSQL_ADD_EXECUTABLE(mysqlslap mysqlslap.c DESTINATION bin)
SET_SOURCE_FILES_PROPERTIES(mysqlslap.c PROPERTIES COMPILE_FLAGS "-DTHREADS")
TARGET_LINK_LIBRARIES(mysqlslap mysqlclient mysys zlib wsock32 dbug)


MYSQL_ADD_EXECUTABLE(echo echo.c COMPONENT Test)
