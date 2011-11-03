# Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.
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

# Build comp_sql - used for embedding SQL in C or C++ programs
IF(NOT CMAKE_CROSSCOMPILING)
 ADD_EXECUTABLE(comp_sql comp_sql.c)
 TARGET_LINK_LIBRARIES(comp_sql)
ENDIF()


# Build mysql_fix_privilege_tables.sql (concatenate 2 sql scripts)
IF(NOT WIN32 OR CMAKE_CROSSCOMPILING)
  FIND_PROGRAM(CAT_EXECUTABLE cat DOC "path to the executable")
  MARK_AS_ADVANCED(CAT_EXECUTABLE)
ENDIF()

IF(CAT_EXECUTABLE)
  SET(CAT_COMMAND COMMAND 
    ${CMAKE_COMMAND} -E chdir  ${CMAKE_CURRENT_SOURCE_DIR}
    ${CAT_EXECUTABLE} mysql_system_tables.sql mysql_system_tables_fix.sql >
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql
  )
ELSEIF(WIN32)
  FILE(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables.sql
   native_outfile )
  SET(CAT_COMMAND
   COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}
   cmd /c copy /b mysql_system_tables.sql + mysql_system_tables_fix.sql
   ${native_outfile} )
ELSE()
  MESSAGE(FATAL_ERROR "Cannot concatenate files")
ENDIF()

# Build mysql_fix_privilege_tables.c
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables_sql.c
  ${CAT_COMMAND}
  COMMAND comp_sql
  mysql_fix_privilege_tables
  mysql_fix_privilege_tables.sql
  mysql_fix_privilege_tables_sql.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS comp_sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_fix.sql
)

# Add target for the above to be built
ADD_CUSTOM_TARGET(GenFixPrivs
  ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mysql_fix_privilege_tables_sql.c
)

IF(UNIX)
 FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution 
   "cd ${CMAKE_BINARY_DIR} && '${CMAKE_CPACK_COMMAND}' -G TGZ --config CPackConfig.cmake\n" )
 EXECUTE_PROCESS(
  COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution
)
ENDIF()

INSTALL(FILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql 
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_test_data_timezone.sql
  ${FIX_PRIVILEGES_SQL}
  DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Server
)

# TCMalloc hacks
IF(MALLOC_LIB)
  MESSAGE("Using tcmalloc '${MALLOC_LIB}'")
  INSTALL(FILES ${MALLOC_LIB} DESTINATION ${INSTALL_LIBDIR} OPTIONAL)
ENDIF()

IF(CMAKE_GENERATOR MATCHES "Makefiles")
  # Strip maintainer mode options if necessary
  STRING(REPLACE "${MY_MAINTAINER_C_WARNINGS}" "" CFLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "${MY_MAINTAINER_CXX_WARNINGS}" "" CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  FOREACH(ARCH ${CMAKE_OSX_ARCHITECTURES})
    SET(CFLAGS   "${CFLAGS} -arch ${ARCH}")
    SET(CXXFLAGS "${CXXFLAGS} -arch ${ARCH}")
  ENDFOREACH()
ELSE()
  # Strip maintainer mode options if necessary
  STRING(REPLACE "${MY_MAINTAINER_C_WARNINGS}" "" CFLAGS "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  STRING(REPLACE "${MY_MAINTAINER_CXX_WARNINGS}" "" CXXFLAGS "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
ENDIF()

IF(UNIX)
  # FIND_PROC and CHECK_PID are used by mysqld_safe
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET (FIND_PROC 
    "ps wwwp $PID | grep -v mysqld_safe | grep -- $MYSQLD > /dev/null")
ENDIF()
IF(NOT FIND_PROC AND CMAKE_SYSTEM_NAME MATCHES "SunOS")
  SET (FIND_PROC 
    "ps -p $PID | grep -v mysqld_safe | grep -- $MYSQLD > /dev/null")
ENDIF()

IF(NOT FIND_PROC)
  # BSD style
  EXECUTE_PROCESS(COMMAND ps -uaxww OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result)
  IF(result MATCHES 0) 
     SET( FIND_PROC
       "ps -uaxww | grep -v mysqld_safe | grep -- $MYSQLD | grep $PID > /dev/null")
  ENDIF()
ENDIF()

IF(NOT FIND_PROC)
  # SysV style
  EXECUTE_PROCESS(COMMAND ps -ef OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result)
  IF(result MATCHES 0)
    SET( FIND_PROC "ps -ef | grep -v mysqld_safe | grep -- $MYSQLD | grep $PID > /dev/null")
  ENDIF()
ENDIF()
  
EXECUTE_PROCESS(COMMAND sh -c "kill -0 $$" OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE result)
IF(result MATCHES 0)
  SET(CHECK_PID "kill -0 $PID > /dev/null 2> /dev/null")
ELSE()
  SET(CHECK_PID "kill -s SIGCONT $PID  > /dev/null 2> /dev/null")
ENDIF()

SET(HOSTNAME "hostname")
SET(MYSQLD_USER "mysql")

# Required for mysqlbug until autotools are deprecated, once done remove these
# and expand default cmake variables
SET(CC ${CMAKE_C_COMPILER})
SET(CXX ${CMAKE_CXX_COMPILER})
SET(SAVE_CC ${CMAKE_C_COMPILER})
SET(SAVE_CXX ${CMAKE_CXX_COMPILER})
SET(SAVE_CFLAGS ${CFLAGS})
SET(SAVE_CXXFLAGS ${CXXFLAGS})
# XXX no cmake equivalent for this, just make one up
SET(CONFIGURE_LINE "Built using CMake")

# Also required for mysqlbug, autoconf only supports --version so for now we
# just explicitly require GNU
IF(CMAKE_COMPILER_IS_GNUCC)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1} --version
    COMMAND sed 1q
    OUTPUT_VARIABLE CC_VERSION)
ELSE()
  SET(CC_VERSION "")
ENDIF()
IF(CMAKE_COMPILER_IS_GNUCXX)
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version
    COMMAND sed 1q
    OUTPUT_VARIABLE CXX_VERSION)
ELSE()
  SET(CXX_VERSION "")
ENDIF()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysqlbug.sh
  ${CMAKE_CURRENT_BINARY_DIR}/mysqlbug ESCAPE_QUOTES @ONLY)
INSTALL_SCRIPT(${CMAKE_CURRENT_BINARY_DIR}/mysqlbug
  DESTINATION ${INSTALL_BINDIR}
  COMPONENT Server
  )

ENDIF(UNIX)

# Really ugly, one script, "mysql_install_db", needs prefix set to ".",
# i.e. makes access relative the current directory. This matches
# the documentation, so better not change this.

IF(INSTALL_LAYOUT MATCHES "STANDALONE")
  SET(prefix ".")
ELSE()
  SET(prefix "${CMAKE_INSTALL_PREFIX}")
ENDIF()

SET(bindir ${prefix}/${INSTALL_BINDIR})
SET(sbindir ${prefix}/${INSTALL_SBINDIR})
SET(scriptdir ${prefix}/${INSTALL_BINDIR})
SET(libexecdir ${prefix}/${INSTALL_SBINDIR})
SET(pkgdatadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
IF(INSTALL_LAYOUT MATCHES "STANDALONE")
  SET(localstatedir ${prefix}/data)
ELSE()
  SET(localstatedir ${MYSQL_DATADIR})
ENDIF()

IF(UNIX)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_install_db.sh
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_install_db ESCAPE_QUOTES @ONLY)
  SET(DEST ${INSTALL_SCRIPTDIR})
  SET(EXT)
ELSE()
 CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_install_db.pl.in
   ${CMAKE_CURRENT_BINARY_DIR}/mysql_install_db.pl ESCAPE_QUOTES @ONLY)
 SET(DEST ${INSTALL_SCRIPTDIR})
 SET(EXT ".pl")
ENDIF()

INSTALL_SCRIPT(
 "${CMAKE_CURRENT_BINARY_DIR}/mysql_install_db${EXT}"
  DESTINATION ${DEST}
  COMPONENT Server
  )


SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(sysconfdir ${prefix})
SET(bindir ${prefix}/${INSTALL_BINDIR})
SET(libexecdir ${prefix}/${INSTALL_SBINDIR})
SET(scriptdir ${prefix}/${INSTALL_BINDIR})
SET(datadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
SET(pkgdatadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
SET(pkgincludedir ${prefix}/${INSTALL_INCLUDEDIR})
SET(pkglibdir ${prefix}/${INSTALL_LIBDIR})
SET(pkgplugindir ${prefix}/${INSTALL_PLUGINDIR})
SET(localstatedir ${MYSQL_DATADIR})

# some scripts use  @TARGET_LINUX@
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(TARGET_LINUX "true")
ELSE()
  SET(TARGET_LINUX "false")
ENDIF()

# Use cmake variables to inspect dependencies for 
# mysqlclient library (add -l stuff)
SET(CLIENT_LIBS "")
SET(LIBS "")

# Avoid compatibility warning about lists with empty elements
IF(POLICY CMP0011)
  CMAKE_POLICY(SET CMP0011 NEW)
ENDIF()
IF(POLICY CMP0007)
  CMAKE_POLICY(SET CMP0007 OLD)
ENDIF()

# Extract dependencies using CMake's internal ${target}_LIB_DEPENDS variable
# returned string in ${var} is can be passed to linker's command line
MACRO(EXTRACT_LINK_LIBRARIES target var)
  IF(${target}_LIB_DEPENDS)
    LIST(REMOVE_ITEM ${target}_LIB_DEPENDS "")
    LIST(REMOVE_DUPLICATES ${target}_LIB_DEPENDS)
    FOREACH(lib ${${target}_LIB_DEPENDS})
      # Filter out "general", it is not a library, just CMake hint
      # Also, remove duplicates
      IF(NOT lib STREQUAL "general" AND NOT ${var}  MATCHES "-l${lib} ")
        IF (lib MATCHES "^\\-l")
          SET(${var} "${${var}} ${lib} ") 
        ELSEIF(lib MATCHES "^/")
          # Full path, convert to just filename, strip "lib" prefix and extension
          GET_FILENAME_COMPONENT(lib "${lib}" NAME_WE)
          STRING(REGEX REPLACE "^lib" "" lib "${lib}")
          SET(${var} "${${var}}-l${lib} " ) 
        ELSE()
          SET(${var} "${${var}}-l${lib} " ) 
        ENDIF()
      ENDIF()
    ENDFOREACH()
  ENDIF()
  IF(MSVC)
    STRING(REPLACE "-l" "" ${var} "${${var}}")
  ENDIF()
ENDMACRO()

EXTRACT_LINK_LIBRARIES(mysqlclient CLIENT_LIBS)
EXTRACT_LINK_LIBRARIES(mysqlserver LIBS)

# mysql_config evaluates ${LIBDL}, we want to avoid it
# as our CLIENT_LIBS and LIBS are already correct
SET(LIBDL)

SET(NON_THREADED_LIBS ${CLIENT_LIBS})
SET(mysql_config_COMPONENT COMPONENT Development)

IF(WIN32)
  # On Windows, some .sh and some .pl.in files are configured 
  # The resulting files will have .pl extension (those are perl scripts)

  # Input files with pl.in extension
  SET(PLIN_FILES mysql_config mysql_secure_installation)
  # Input files with .sh extension

  SET(SH_FILES mysql_convert_table_format mysqld_multi mysqldumpslow
    mysqlhotcopy)

  FOREACH(file ${PLIN_FILES})
    IF(NOT ${file}_COMPONENT)
      SET(${file}_COMPONENT Server_Scripts)
    ENDIF()
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${file}.pl.in
      ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl ESCAPE_QUOTES @ONLY)
    INSTALL_SCRIPT(${CMAKE_CURRENT_BINARY_DIR}/${file}.pl COMPONENT ${${file}_COMPONENT})
  ENDFOREACH()

  FOREACH(file ${SH_FILES})
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${file}.sh
       ${CMAKE_CURRENT_BINARY_DIR}/${file}.pl ESCAPE_QUOTES @ONLY)
    INSTALL_SCRIPT(${CMAKE_CURRENT_BINARY_DIR}/${file}.pl COMPONENT Server_Scripts)
  ENDFOREACH()
ELSE()
  # On Unix, most of the files end up in the bin directory
  SET(mysql_config_COMPONENT COMPONENT Development)
  SET(BIN_SCRIPTS
    msql2mysql
    mysql_config
    mysql_fix_extensions
    mysql_setpermission
    mysql_secure_installation
    mysql_zap
    mysqlaccess
    mysqlaccess.conf
    mysql_convert_table_format
    mysql_find_rows
    mysqlhotcopy
    mysqldumpslow
    mysqld_multi
    mysqld_safe
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
    EXECUTE_PROCESS(COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/${file})
    IF(NOT ${file}_COMPONENT)
      SET(${file}_COMPONENT Server)
    ENDIF()
    INSTALL_SCRIPT(
      ${CMAKE_CURRENT_BINARY_DIR}/${file} 
      DESTINATION ${INSTALL_BINDIR}
      COMPONENT ${${file}_COMPONENT}
     )
  ENDFOREACH()
ENDIF()

# Install libgcc as  mylibgcc.a
IF(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_FLAGS MATCHES "-static")
  EXECUTE_PROCESS (
   COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1}
     ${CMAKE_CXX_FLAGS} --print-libgcc
     OUTPUT_VARIABLE  LIBGCC_LOCATION
     RESULT_VARIABLE RESULT
     OUTPUT_STRIP_TRAILING_WHITESPACE
     ERROR_QUIET
  )
  IF(${RESULT} EQUAL 0 AND EXISTS ${LIBGCC_LOCATION})
    INSTALL(FILES "${LIBGCC_LOCATION}" DESTINATION ${INSTALL_LIBDIR}
    COMPONENT Development)
  ENDIF()
ENDIF()

