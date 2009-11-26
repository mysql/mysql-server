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


# Build comp_sql - used for embedding SQL in C or C++ programs
IF(NOT CMAKE_CROSSCOMPILING)
 ADD_EXECUTABLE(comp_sql comp_sql.c)
 TARGET_LINK_LIBRARIES(comp_sql)
ENDIF()

SET(FIX_PRIVS_IN 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_fix.sql
)
SET(FIX_PRIVILEGES_SQL
 ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql
)

# Build mysql_fix_privilege_tables.sql (concatenate 2 sql scripts)
FILE(WRITE ${FIX_PRIVILEGES_SQL} "")
FOREACH(FILENAME ${FIX_PRIVS_IN})
  FILE(READ "${FILENAME}" CONTENTS)
  FILE(APPEND ${FIX_PRIVILEGES_SQL} "${CONTENTS}")
ENDFOREACH()


# Build mysql_fix_privilege_tables.c
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables_sql.c
  COMMAND comp_sql
  mysql_fix_privilege_tables
  mysql_fix_privilege_tables.sql
  mysql_fix_privilege_tables_sql.c
  DEPENDS comp_sql
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Add target for the above to be built
ADD_CUSTOM_TARGET(GenFixPrivs
  ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables_sql.c
)

IF(UNIX)
 FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution 
   "cd ${CMAKE_BINARY_DIR} && ${CMAKE_CPACK_COMMAND} -G TGZ --config CPackConfig.cmake" )
 EXECUTE_PROCESS(
  COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution
)
ENDIF()

INSTALL(FILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_test_data_timezone.sql
  DESTINATION share
)

# TCMalloc hacks
IF($ENV{MALLOC_LIB})
  SET(MALLOC_LIB $ENV{MALLOC_LIB} CACHE STRING "malloc library")
ENDIF()

IF(MALLOC_LIB)
  INSTALL(FILES ${MALLOC_LIB} DESTINATION lib OPTIONAL)
ENDIF()

# install_scripts.cmake delays configuring scripts (e.g mysql_install_db)
# until cpack runs (necessary to handle DESTDIR correctly)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/install_scripts.cmake.in 
  ${CMAKE_CURRENT_BINARY_DIR}/install_scripts.cmake  @ONLY)
INSTALL(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install_scripts.cmake)
