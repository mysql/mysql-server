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

# Build comp_sql - used for embedding SQL in C or C++ programs
IF(NOT CMAKE_CROSSCOMPILING)
 ADD_EXECUTABLE(comp_sql comp_sql.c)
 TARGET_LINK_LIBRARIES(comp_sql)
ENDIF()

# Add compiler information, for easier debugging of output from mysql_config.
SET(COMPILER_ID_AND_VERSION
  "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

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
  ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_sys_schema.h
)

# If we do not have performance schema compiled in then sys will not work,
# so only create an empty sys database with an auto-generated file,
# otherwise, load the full schema file normally.
IF(NOT WITH_PERFSCHEMA_STORAGE_ENGINE)
  FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/mysql_sys_schema_empty.sql "CREATE DATABASE sys;")

  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_sys_schema.h
    COMMAND comp_sql
    mysql_sys_schema
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_sys_schema_empty.sql
    sql_commands_sys_schema.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS comp_sql
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_sys_schema_empty.sql
  )
ELSE()
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_sys_schema.h
    COMMAND comp_sql
    mysql_sys_schema
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_sys_schema.sql
    sql_commands_sys_schema.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS comp_sql
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_sys_schema.sql
  )
ENDIF()

IF(NOT WITHOUT_SERVER)
  ADD_CUSTOM_TARGET(GenSysSchema
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_sys_schema.h
  )
ENDIF()

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_system_data.h
  COMMAND comp_sql
  mysql_system_data
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
  sql_commands_system_data.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS comp_sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_system_tables.h
  COMMAND comp_sql
  mysql_system_tables
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql
  sql_commands_system_tables.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS comp_sql
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql
)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_help_data.h
  COMMAND comp_sql
  fill_help_tables
  ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
  sql_commands_help_data.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS comp_sql
  ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
)

# Add target for the above to be built
IF(NOT WITHOUT_SERVER)
  ADD_CUSTOM_TARGET(GenBootstrapPriv
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_system_data.h
    ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_system_tables.h
    ${CMAKE_CURRENT_BINARY_DIR}/sql_commands_help_data.h
  )
ENDIF()

IF(UNIX)
 FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution 
   "cd ${CMAKE_BINARY_DIR} && '${CMAKE_CPACK_COMMAND}' -G TGZ --config CPackConfig.cmake\n" )
 EXECUTE_PROCESS(
  COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/make_binary_distribution
)
ENDIF()

IF(NOT WITHOUT_SERVER)
  INSTALL(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_system_tables_data.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/fill_help_tables.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_sys_schema.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_test_data_timezone.sql
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_security_commands.sql
    ${FIX_PRIVILEGES_SQL}
    DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Server
  )
ENDIF()

# TCMalloc hacks
IF(MALLOC_LIB)
  MESSAGE("Using tcmalloc '${MALLOC_LIB}'")
  INSTALL(FILES ${MALLOC_LIB} DESTINATION ${INSTALL_LIBDIR} OPTIONAL)
ENDIF()

SET(COMPILE_DEFINITIONS_WHITELIST
)

# In practice: only in use on Solaris
SET(CMAKE_C_FLAGS_WHITELIST
  "-m32"
  "-m64"
)

SET(CFLAGS "")
SET(CXXFLAGS "")

GET_PROPERTY(cwd_definitions DIRECTORY PROPERTY COMPILE_DEFINITIONS)
FOREACH(white_flag ${COMPILE_DEFINITIONS_WHITELIST})
  FOREACH(flag ${cwd_definitions})
    IF (${flag} MATCHES ${white_flag})
      SET(CFLAGS "-D${white_flag} ${CFLAGS}")
      SET(CXXFLAGS "-D${white_flag} ${CXXFLAGS}")
    ENDIF()
  ENDFOREACH()
ENDFOREACH()

IF(CMAKE_BUILD_TYPE AND BUILD_IS_SINGLE_CONFIG)
  STRING(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKEBT)
  SET(CMAKEBT_FLAGS "${CMAKE_C_FLAGS_${CMAKEBT}} ${CMAKE_C_FLAGS}")
  SEPARATE_ARGUMENTS(CMAKEBT_FLAGS)
ENDIF()

FOREACH(white_flag ${CMAKE_C_FLAGS_WHITELIST})
  FOREACH(flag ${CMAKEBT_FLAGS})
    IF (${flag} STREQUAL ${white_flag})
      SET(CFLAGS "${white_flag} ${CFLAGS}")
      SET(CXXFLAGS "${white_flag} ${CXXFLAGS}")
    ENDIF()
  ENDFOREACH()
ENDFOREACH()

# Do not remove -std=c++11 for Solaris Studio as it is required to get
# the correct libraries. Only remove for GCC/Clang
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  STRING(REGEX REPLACE "-std=c..[0-9]+" "" CXXFLAGS "${CXXFLAGS}")
ENDIF()

IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  SET (PERL_PATH "/usr/local/bin/perl")
ELSE()
  SET (PERL_PATH "/usr/bin/perl")
ENDIF()

IF(UNIX)
  # SHELL_PATH, FIND_PROC, CHECK_PID are used by mysqld_safe
IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
  SET (SHELL_PATH "/bin/bash")
ELSE()
  SET (SHELL_PATH "/bin/sh")
ENDIF()

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET (FIND_PROC 
    "ps wwwp $PID | grep -v mysqld_safe | grep -- $MYSQLD > /dev/null")
ENDIF()
IF(NOT FIND_PROC AND CMAKE_SYSTEM_NAME MATCHES "SunOS")
  SET (FIND_PROC 
    "ps -p $PID | grep -v mysqld_safe | grep -- $MYSQLD > /dev/null")
ENDIF()

IF(NOT FIND_PROC)
  # BSD styl
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

ENDIF(UNIX)

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

SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(sysconfdir ${prefix})
SET(bindir ${prefix}/${INSTALL_BINDIR})
SET(libexecdir ${prefix}/${INSTALL_SBINDIR})
SET(scriptdir ${prefix}/${INSTALL_BINDIR})
SET(datadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
SET(pkgdatadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
SET(libsubdir  ${INSTALL_LIBDIR})
SET(pkgincludedir ${prefix}/${INSTALL_INCLUDEDIR})
SET(pkglibdir ${prefix}/${INSTALL_LIBDIR})
SET(pkgplugindir ${prefix}/${INSTALL_PLUGINDIR})
SET(localstatedir ${MYSQL_DATADIR})

IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  SET(RPATH_OPTION " -R$pkglibdir")
ENDIF()

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

# Extract dependencies using CMake's internal ${target}_LIB_DEPENDS variable
# returned string in ${var} is can be passed to linker's command line
MACRO(EXTRACT_LINK_LIBRARIES target var)
  IF(${target}_LIB_DEPENDS)
    LIST(REMOVE_ITEM ${target}_LIB_DEPENDS "")
    LIST(REMOVE_DUPLICATES ${target}_LIB_DEPENDS)
    FOREACH(lib ${${target}_LIB_DEPENDS})
      # Filter out "general", it is not a library, just CMake hint
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
  STRING(REGEX REPLACE "^[ ]+" "" ${var} "${${var}}")
  STRING(REGEX REPLACE "[ ]+$" "" ${var} "${${var}}")
ENDMACRO()

EXTRACT_LINK_LIBRARIES(mysqlclient CLIENT_LIBS)
IF(WITH_EMBEDDED_SERVER)
  EXTRACT_LINK_LIBRARIES(mysqlserver EMBEDD_LIBS)
ENDIF()

IF(MSVC)
  GET_TARGET_PROPERTY(LIBMYSQL_OS_SHLIB_VERSION mysqlclient VERSION)
  GET_TARGET_PROPERTY(LIBMYSQL_OS_OUTPUT_NAME mysqlclient OUTPUT_NAME)
ELSE()
  GET_TARGET_PROPERTY(LIBMYSQL_OS_SHLIB_VERSION libmysql VERSION)
  GET_TARGET_PROPERTY(LIBMYSQL_OS_OUTPUT_NAME libmysql OUTPUT_NAME)
ENDIF()
# See WITH_EMBEDDED_SHARED_LIBRARY, "mysqld" is generated from mysqlserver
IF(WITH_EMBEDDED_SERVER)
  GET_TARGET_PROPERTY(LIBEMBED_OS_OUTPUT_NAME mysqlserver OUTPUT_NAME)
ENDIF()

SET(CONFIG_CLIENT_LIBS "${CLIENT_LIBS}")
SET(CONFIG_EMBEDD_LIBS "${EMBEDD_LIBS}")
SET(CONFIG_LIBS_PRIVATE "${CLIENT_LIBS}")
SET(CONFIG_REQUIRES_PRIVATE "")
# ssl.cmake should have used pkg-config rather than cmake to locate ssl
IF (WITH_SSL STREQUAL "system")
  SET(CONFIG_REQUIRES_PRIVATE "openssl")
  STRING(REPLACE "-lssl" "" CONFIG_LIBS_PRIVATE "${CONFIG_LIBS_PRIVATE}")
  STRING(REPLACE "-lcrypto" "" CONFIG_LIBS_PRIVATE "${CONFIG_LIBS_PRIVATE}")
  STRING(REGEX REPLACE "[ ]+" " " CONFIG_LIBS_PRIVATE "${CONFIG_LIBS_PRIVATE}")
ENDIF()

SET(LIBMYSQLD_LIBS_USAGE "
        --libmysqld-libs [$embedded_libs]")
SET(DISABLE_EMBEDDED_SH "")
SET(DISABLE_EMBEDDED_PL "")
IF(WITHOUT_SERVER)
  SET(LIBMYSQLD_LIBS_USAGE)
  SET(DISABLE_EMBEDDED_SH  "usage; ")
  SET(DISABLE_EMBEDDED_PL "usage(); ")
ENDIF()

SET(mysql_config_COMPONENT COMPONENT Development)

IF(WIN32)
  # On Windows, some .sh and some .pl.in files are configured 
  # The resulting files will have .pl extension (those are perl scripts)

  # Input files with pl.in extension
  SET(PLIN_FILES mysql_config)
  # Input files with .sh extension

  SET(SH_FILES mysqld_multi mysqldumpslow)

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
  # Configure this one, for testing, but do not install it.
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_config.pl.in
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_config.pl ESCAPE_QUOTES @ONLY)
  # On Unix, most of the files end up in the bin directory
  SET(mysql_config_COMPONENT COMPONENT Development)

  IF(WITH_SYSTEMD)
    SET(BIN_SCRIPTS
      mysql_config
      mysqldumpslow
      )
  ELSE()
    SET(BIN_SCRIPTS
      mysql_config
      mysqldumpslow
      mysqld_multi
      mysqld_safe
      )
  ENDIF()

  SET(PKGCONFIG_FILE ${LIBMYSQL_OS_OUTPUT_NAME}.pc)
  STRING(REGEX REPLACE "/mysql$" "" PKGCONFIG_DIR "${INSTALL_LIBDIR}")
  SET(PKGCONFIG_DIR "${PKGCONFIG_DIR}/pkgconfig")
  # Update the cache, to make it visible in cmake-gui.
  SET(INSTALL_PKGCONFIGDIR ${INSTALL_PKGCONFIGDIR} CACHE PATH
    "Where to install ${PKGCONFIG_FILE}, defaults to ${PKGCONFIG_DIR}")
  IF(NOT INSTALL_PKGCONFIGDIR)
    SET(INSTALL_PKGCONFIGDIR "${PKGCONFIG_DIR}")
  ENDIF()

  CONFIGURE_FILE(mysqlclient.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PKGCONFIG_FILE}
    ESCAPE_QUOTES @ONLY)

  IF(INSTALL_PKGCONFIGDIR)
    MESSAGE(STATUS "INSTALL ${PKGCONFIG_FILE} ${INSTALL_PKGCONFIGDIR}")
    INSTALL(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${PKGCONFIG_FILE}
      DESTINATION ${INSTALL_PKGCONFIGDIR}
      PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
      COMPONENT Development
      )
  ENDIF()

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

  # Systemd files
  IF(WITH_SYSTEMD)
    CONFIGURE_FILE(systemd/mysqld_pre_systemd.in
      ${CMAKE_CURRENT_BINARY_DIR}/mysqld_pre_systemd ESCAPE_QUOTES @ONLY)

    CONFIGURE_FILE(systemd/mysqld.service.in
      ${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_SERVICE_NAME}.service ESCAPE_QUOTES @ONLY)

    CONFIGURE_FILE(systemd/mysqld@.service.in
      ${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_SERVICE_NAME}@.service ESCAPE_QUOTES @ONLY)

    CONFIGURE_FILE(systemd/mysqld.tmpfiles.d.in
      ${CMAKE_CURRENT_BINARY_DIR}/mysql.conf ESCAPE_QUOTES @ONLY)

    INSTALL_SCRIPT(
      ${CMAKE_CURRENT_BINARY_DIR}/mysqld_pre_systemd
      DESTINATION ${INSTALL_BINDIR}
      COMPONENT Server
    )

    IF(INSTALL_LAYOUT MATCHES "STANDALONE")
      SET(SYSTEMD_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/")
    ELSE()
      SET(SYSTEMD_INSTALL_PREFIX "")
    ENDIF()

    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_SERVICE_NAME}.service
       DESTINATION ${SYSTEMD_INSTALL_PREFIX}${SYSTEMD_SERVICES_DIR} COMPONENT Server
       PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_SERVICE_NAME}@.service
       DESTINATION ${SYSTEMD_INSTALL_PREFIX}${SYSTEMD_SERVICES_DIR} COMPONENT Server
       PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql.conf
       DESTINATION ${SYSTEMD_INSTALL_PREFIX}${SYSTEMD_TMPFILES_DIR} COMPONENT Server
       PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
  ENDIF()
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

