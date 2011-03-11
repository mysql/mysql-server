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

# Build mysql_fix_privilege_tables.sql
FILE(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql
   native_outfile )
ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_BINARY_DIR}/scripts/mysql_fix_privilege_tables.sql
                   COMMAND  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR} 
				   cmd /c copy /b mysql_system_tables.sql + mysql_system_tables_fix.sql
                   ${native_outfile}
                   DEPENDS
                   ${PROJECT_SOURCE_DIR}/scripts/mysql_system_tables.sql
                   ${PROJECT_SOURCE_DIR}/scripts/mysql_system_tables_fix.sql)

# Build comp_sql - used for embedding SQL in C or C++ programs
ADD_EXECUTABLE(comp_sql comp_sql.c)
TARGET_LINK_LIBRARIES(comp_sql debug dbug mysys strings)

# Use comp_sql to build mysql_fix_privilege_tables_sql.c
GET_TARGET_PROPERTY(COMP_SQL_EXE comp_sql LOCATION)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_BINARY_DIR}/scripts/mysql_fix_privilege_tables_sql.c
                   COMMAND ${COMP_SQL_EXE}
                   mysql_fix_privilege_tables
                   ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql
                   mysql_fix_privilege_tables_sql.c
				   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS comp_sql
                   ${CMAKE_BINARY_DIR}/scripts/mysql_fix_privilege_tables.sql)

# Add dummy target for the above to be built
ADD_CUSTOM_TARGET(GenFixPrivs
                  ALL
                  DEPENDS ${CMAKE_BINARY_DIR}/scripts/mysql_fix_privilege_tables_sql.c)

# ----------------------------------------------------------------------
# Replace some variables @foo@ in the .in/.sh file, and write the new script
# ----------------------------------------------------------------------

SET(CFLAGS             "-D_WINDOWS ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
SET(prefix             "${CMAKE_INSTALL_PREFIX}")
SET(sysconfdir          ${prefix})
SET(bindir              ${prefix}/bin)
SET(libexecdir          ${prefix}/bin)
SET(scriptdir           ${prefix}/bin)
SET(datadir             ${prefix}/share)
SET(pkgdatadir          ${prefix}/share)
SET(localstatedir       ${prefix}/data)

CONFIGURE_FILE(mysql_config.pl.in
               ${CMAKE_BINARY_DIR}/scripts/mysql_config.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysql_convert_table_format.sh
               ${CMAKE_BINARY_DIR}/scripts/mysql_convert_table_format.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysql_install_db.pl.in
               ${CMAKE_BINARY_DIR}/scripts/mysql_install_db.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysql_secure_installation.pl.in
               ${CMAKE_BINARY_DIR}/scripts/mysql_secure_installation.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysqld_multi.sh
               ${CMAKE_BINARY_DIR}/scripts/mysqld_multi.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysqldumpslow.sh
               ${CMAKE_BINARY_DIR}/scripts/mysqldumpslow.pl ESCAPE_QUOTES @ONLY)

CONFIGURE_FILE(mysqlhotcopy.sh
               ${CMAKE_BINARY_DIR}/scripts/mysqlhotcopy.pl ESCAPE_QUOTES @ONLY)

FOREACH(f mysqldumpslow.pl mysqlhotcopy.pl mysql_config.pl
	mysql_convert_table_format.pl mysql_install_db.pl
	mysql_secure_installation.pl mysqld_multi.pl)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${f}
	DESTINATION scripts COMPONENT Server_Scripts)
ENDFOREACH()

INSTALL(FILES fill_help_tables.sql mysql_system_tables.sql
	mysql_system_tables_data.sql mysql_system_tables_fix.sql mysql_test_data_timezone.sql
	DESTINATION share COMPONENT Server)
INSTALL(FILES  ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql DESTINATION share COMPONENT Server)