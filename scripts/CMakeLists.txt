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

IF(UNIX)
  # FIND_PROC and CHECK_PID are used by mysqld_safe
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET (FIND_PROC 
    "ps wwwp $PID | grep -v \" grep\" | grep -v mysqld_safe | grep -- \"$MYSQLD\" > /dev/null")
ENDIF()
IF(NOT FIND_PROC AND CMAKE_SYSTEM_NAME MATCHES "SunOS")
  SET (FIND_PROC 
    "ps -p $PID | grep -v \" grep\" | grep -v mysqld_safe | grep -- \"$MYSQLD\" > /dev/null")
ENDIF()

IF(NOT FIND_PROC)
  # BSD style
  EXECUTE_PROCESS(COMMAND ps -uaxww OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result)
  IF(result MATCHES 0)
     SET( FIND_PROC
       "ps -uaxww | grep -v \" grep\" | grep -v mysqld_safe | grep -- "\$MYSQLD\" | grep \" $PID \" > /dev/null")
  ENDIF()
ENDIF()

IF(NOT FIND_PROC)
  # SysV style
  EXECUTE_PROCESS(COMMAND ps -ef OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result)
  IF(result MATCHES 0)
    SET( FIND_PROC "ps -ef | grep -v \" grep\" | grep -v mysqld_safe | grep -- "\$MYSQLD\" | grep \" $PID \" > /dev/null")
  ENDIF()
ENDIF()
  
EXECUTE_PROCESS(COMMAND sh -c "kill -0 $$" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result2)
IF(result3 MATCHES 0)
  SET(CHECK_PID "kill -0 $PID > /dev/null 2> /dev/null")
ELSE()
  EXECUTE_PROCESS(COMMAND sh -c "kill -s 0 $$" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result3)
  IF(result4 MATCHES 0)
    SET(CHECK_PID "kill -s 0 $PID > /dev/null 2> /dev/null")
  ELSE()
    SET(CHECK_PID "kill -s SIGCONT $PID  > /dev/null 2> /dev/null")
  ENDIF()
ENDIF()

ENDIF(UNIX)

# some scripts use  @TARGET_LINUX@
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(TARGET_LINUX 1)
ELSE()
  SET(TARGET_LINUX 0)
ENDIF()
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

# ----------------------------------------------------------------------
# Replace some variables @foo@ in the .in/.sh file, and write the new script
# ----------------------------------------------------------------------

SET(CFLAGS "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
SET(CXXFLAGS "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
SET(LDFLAGS "${CMAKE_SHARED_LIBRARY_LINK_FLAGS}")

IF(WIN32)
  SET(prefix "${CMAKE_INSTALL_PREFIX}/MySQL Server ${MYSQL_BASE_VERSION}")
ELSE()
  set(prefix "${CMAKE_INSTALL_PREFIX}")
ENDIF()

SET(sysconfdir ${prefix})
SET(bindir ${prefix}/bin)
SET(libexecdir ${prefix}/bin)
SET(scriptdir ${prefix}/bin)
SET(datadir ${prefix}/share)
SET(pkgdatadir ${prefix}/share)
SET(pkgincludedir ${prefix}/include)
SET(pkglibdir ${prefix}/lib)
SET(pkgplugindir ${prefix}/lib/plugin)
SET(localstatedir ${prefix}/data)

# Use cmake variables to inspect dependencies for 
# mysqlclient library
SET(CLIENT_LIBS "")
SET(LIBS "")
FOREACH(lib ${mysqlclient_LIB_DEPENDS})
  # Filter out general, it is CMake hint
  # not real
  IF(NOT lib STREQUAL "general" AND NOT CLIENT_LIBS MATCHES "-l${lib} ")
     SET(CLIENT_LIBS "${CLIENT_LIBS}-l${lib} " ) 
  ENDIF()
ENDFOREACH()
FOREACH(lib ${mysqlserver_LIB_DEPENDS})
  IF(NOT lib STREQUAL "general" AND NOT LIBS MATCHES "-l${lib} ")
     SET(LIBS "${LIBS}-l${lib} " ) 
  ENDIF()
ENDFOREACH()
IF(MSVC)
  STRING(REPLACE "-l" "" CLIENT_LIBS "${CLIENT_LIBS}")
  STRING(REPLACE "-l" "" LIBS "${LIBS}" )
ENDIF()

SET(NON_THREADED_LIBS ${CLIENT_LIBS})

IF(WIN32)
  # On Windows, some .sh and some .pl.in files are configured 
  # The resulting files will have .pl extension (those are perl scripts)

  # Input files with pl.in extension
  SET(PLIN_FILES mysql_config mysql_secure_installation)
  # Input files with .sh extension
  SET(SH_FILES mysql_convert_table_format mysqld_multi)

  FOREACH(file ${PLIN_FILES})
    CONFIGURE_FILE(${file}.pl.in
      ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl ESCAPE_QUOTES @ONLY)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl DESTINATION scripts)
  ENDFOREACH()

  FOREACH(file ${SH_FILES})
    CONFIGURE_FILE(${file}.sh
       ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl ESCAPE_QUOTES @ONLY)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl DESTINATION scripts)
  ENDFOREACH()
ELSE()
  # On Unix, most of the files end up in the bin directory
  SET(BIN_SCRIPTS
    msql2mysql
    mysql_config
    mysql_fix_extensions
    mysql_setpermission
    mysql_secure_installation
    mysql_zap
    mysqlaccess
    mysqlbug
    mysql_convert_table_format
    mysql_find_rows
    mysqlhotcopy
    mysqldumpslow
    mysqld_multi
    mysqlaccess
    mysqlaccess.conf
    mysql_install_db
  )
  FOREACH(file ${BIN_SCRIPTS})
    IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${file}.sh)
      CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${file}.sh
        ${CMAKE_CURRENT_BINARY_DIR}/${file} ESCAPE_QUOTES @ONLY)
    ELSEIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${file})
      CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${file}
         ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
    ELSE()
      MESSAGE(FATAL_ERROR "Can not find ${file}.sh or ${file} in "
        "${CMAKE_CURRENT_SOURCE_DIR}" )
    ENDIF()

    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${file} DESTINATION bin
       PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
       GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  ENDFOREACH()

  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysqld_safe.sh
    ${CMAKE_CURRENT_BINARY_DIR}/mysqld_safe  @ONLY)

  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysqld_safe DESTINATION bin 
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ 
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  # For some reason, mysqld_safe needs to be also in scripts directory
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql_install_db 
    DESTINATION scripts 
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
    GROUP_EXECUTE  WORLD_READ WORLD_EXECUTE
  )
ENDIF()

INSTALL(FILES mysql_test_data_timezone.sql DESTINATION share)

IF(UNIX)
 FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution 
   "cd ${CMAKE_BINARY_DIR} && ${CMAKE_CPACK_COMMAND} -G TGZ --config CPackConfig.cmake" )
 EXECUTE_PROCESS(
  COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution
)
ENDIF()

# Install libgcc as  mylibgcc.a
IF(CMAKE_COMPILER_IS_GNUCXX)
  IF(NOT LIBGCC_LOCATION)
    EXECUTE_PROCESS (
     COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1}
       ${CMAKE_CXX_FLAGS} --print-libgcc
       OUTPUT_VARIABLE  LIBGCC_LOCATION
       RESULT_VARIABLE RESULT
       OUTPUT_STRIP_TRAILING_WHITESPACE
       ERROR_QUIET
    )
    IF(${RESULT} EQUAL 0 AND EXISTS ${LIBGCC_LOCATION})
      SET(LIBGCC_LOCATION "${LIBGCC_LOCATION}" CACHE INTERNAL 
         "location of libgcc" )
    ENDIF()
  ENDIF()
  
  IF(LIBGCC_LOCATION)
    INSTALL (CODE "CONFIGURE_FILE (${LIBGCC_LOCATION}
    ${CMAKE_CURRENT_BINARY_DIR}/libmygcc.a COPYONLY)")
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/libmygcc.a DESTINATION lib 
    OPTIONAL) 
  ENDIF()
ENDIF()


INSTALL (FILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
  DESTINATION share
)

# TCMalloc hacks
IF($ENV{MALLOC_LIB})
  SET(MALLOC_LIB $ENV{MALLOC_LIB} CACHE STRING "malloc library")
ENDIF()

IF(MALLOC_LIB)
  INSTALL(FILES ${MALLOC_LIB} DESTINATION lib OPTIONAL)
ENDIF()
