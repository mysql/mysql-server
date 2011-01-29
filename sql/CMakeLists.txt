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

ADD_DEFINITIONS(-DUSE_SYMDIR)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include 
                    ${CMAKE_SOURCE_DIR}/sql 
                    ${CMAKE_SOURCE_DIR}/regex 
                    ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/extra/libevent
                    ${CMAKE_BINARY_DIR}/extra/libevent
                    ${CMAKE_CURRENT_BINARY_DIR}
)

SET_SOURCE_FILES_PROPERTIES(${CMAKE_BINARY_DIR}/sql/sql_yacc.h 
                            ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
                            ${CMAKE_BINARY_DIR}/include/mysql_version.h
                            ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc
                            ${CMAKE_BINARY_DIR}/sql/lex_hash.h 
                            ${CMAKE_BINARY_DIR}/include/mysqld_error.h
                            ${CMAKE_BINARY_DIR}/include/mysqld_ername.h
                            ${CMAKE_BINARY_DIR}/include/sql_state.h 
                              PROPERTIES GENERATED 1)

ADD_DEFINITIONS(-DMYSQL_SERVER -D_CONSOLE -DHAVE_DLOPEN -DHAVE_EVENT_SCHEDULER)


SET (SQL_SOURCE
               ../sql-common/client.c derror.cc des_key_file.cc
               discover.cc ../libmysql/errmsg.c field.cc  field_conv.cc 
               filesort.cc gstream.cc
               ha_partition.cc
               handler.cc hash_filo.cc hash_filo.h sql_plugin_services.h
               hostname.cc init.cc item.cc item_buff.cc item_cmpfunc.cc 
               item_create.cc item_func.cc item_geofunc.cc item_row.cc 
               item_strfunc.cc item_subselect.cc item_sum.cc item_timefunc.cc 
               key.cc log.cc lock.cc  
               log_event.cc rpl_record.cc rpl_reporting.cc
               log_event_old.cc rpl_record_old.cc
               message.h mf_iocache.cc my_decimal.cc ../sql-common/my_time.c
               mysqld.cc net_serv.cc ../sql-common/client_plugin.c 
               nt_servc.cc nt_servc.h opt_range.cc opt_range.h opt_sum.cc 
               ../sql-common/pack.c parse_file.cc password.c procedure.cc 
               protocol.cc records.cc repl_failsafe.cc rpl_filter.cc set_var.cc 
               slave.cc sp.cc sp_cache.cc sp_head.cc sp_pcontext.cc 
               sp_rcontext.cc spatial.cc sql_acl.cc sql_analyse.cc sql_base.cc 
               sql_cache.cc sql_class.cc sql_client.cc sql_crypt.cc sql_crypt.h 
               sql_cursor.cc sql_db.cc sql_delete.cc sql_derived.cc sql_do.cc 
               sql_error.cc sql_handler.cc sql_help.cc sql_insert.cc sql_lex.cc 
               sql_list.cc sql_load.cc sql_manager.cc sql_map.cc sql_parse.cc 
               sql_partition.cc sql_plugin.cc sql_prepare.cc sql_rename.cc 
               debug_sync.cc debug_sync.h
               sql_repl.cc sql_select.cc sql_show.cc sql_state.c sql_string.cc 
               sql_table.cc sql_test.cc sql_trigger.cc sql_udf.cc sql_union.cc
               sql_update.cc sql_view.cc strfunc.cc table.cc thr_malloc.cc 
               time.cc tztime.cc uniques.cc unireg.cc item_xmlfunc.cc 
               rpl_tblmap.cc sql_binlog.cc event_scheduler.cc event_data_objects.cc
               event_queue.cc event_db_repository.cc 
               sql_tablespace.cc events.cc ../sql-common/my_user.c 
               partition_info.cc rpl_utility.cc rpl_injector.cc sql_locale.cc
               rpl_rli.cc rpl_mi.cc sql_servers.cc
               sql_connect.cc scheduler.cc 
               sql_profile.cc event_parse_data.cc opt_table_elimination.cc
               create_options.cc
               ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
               ${CMAKE_BINARY_DIR}/sql/sql_yacc.h
               ${CMAKE_BINARY_DIR}/include/mysqld_error.h
               ${CMAKE_BINARY_DIR}/include/mysqld_ername.h 
               ${CMAKE_BINARY_DIR}/include/sql_state.h
               ${CMAKE_BINARY_DIR}/include/mysql_version.h 
               ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc
               ${CMAKE_BINARY_DIR}/sql/lex_hash.h)
ADD_LIBRARY(sql ${SQL_SOURCE})

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/win/cmake/dummy.in cmake_dummy.cc COPYONLY)
MYSQL_ADD_EXECUTABLE(mysqld cmake_dummy.cc message.rc DESTINATION ${INSTALL_SBINDIR} COMPONENT Server)
INSTALL_DEBUG_TARGET(mysqld 
  DESTINATION ${INSTALL_SBINDIR} 
  PDB_DESTINATION ${INSTALL_SBINDIR}/debug
  RENAME mysqld-debug)

SET_TARGET_PROPERTIES(mysqld PROPERTIES OUTPUT_NAME mysqld${MYSQLD_EXE_SUFFIX})
SET_TARGET_PROPERTIES(mysqld PROPERTIES ENABLE_EXPORTS TRUE)

SET (MYSQLD_CORE_LIBS mysys zlib dbug strings yassl taocrypt vio regex sql libevent)
TARGET_LINK_LIBRARIES(mysqld ${MYSQLD_CORE_LIBS} ${MYSQLD_STATIC_ENGINE_LIBS})
TARGET_LINK_LIBRARIES(mysqld ws2_32.lib psapi.lib)


IF(MSVC AND NOT WITHOUT_DYNAMIC_PLUGINS)
  # Set module definition file. Also use non-incremental linker, 
  # incremental appears to crash from time to time,if used with /DEF option
  SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS "/DEF:mysqld.def /INCREMENTAL:NO")

  FOREACH (CORELIB ${MYSQLD_CORE_LIBS})
    GET_TARGET_PROPERTY(LOC ${CORELIB} LOCATION)
    FILE(TO_NATIVE_PATH ${LOC} LOC)
    SET (LIB_LOCATIONS ${LIB_LOCATIONS} ${LOC}) 
  ENDFOREACH (CORELIB ${MYSQLD_CORE_LIBS})
 
  ADD_CUSTOM_COMMAND(TARGET mysqld PRE_LINK
    COMMAND cscript ARGS //nologo ${PROJECT_SOURCE_DIR}/win/create_def_file.js
                  ${PLATFORM}  ${LIB_LOCATIONS} > mysqld.def 
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDIF(MSVC AND NOT WITHOUT_DYNAMIC_PLUGINS)

ADD_DEPENDENCIES(sql GenError)

# Sql Parser custom command
ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_BINARY_DIR}/sql/sql_yacc.h
               ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
        COMMAND bison ARGS -y -p MYSQL --defines=sql_yacc.h
                               --output=sql_yacc.cc "${CMAKE_CURRENT_SOURCE_DIR}/sql_yacc.yy"
        DEPENDS ${PROJECT_SOURCE_DIR}/sql/sql_yacc.yy)


# Gen_lex_hash
ADD_EXECUTABLE(gen_lex_hash gen_lex_hash.cc)
TARGET_LINK_LIBRARIES(gen_lex_hash debug dbug mysqlclient strings wsock32)
GET_TARGET_PROPERTY(GEN_LEX_HASH_EXE gen_lex_hash LOCATION)
ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_BINARY_DIR}/sql/lex_hash.h
        COMMAND ${GEN_LEX_HASH_EXE} ARGS > lex_hash.h
        DEPENDS ${GEN_LEX_HASH_EXE})

ADD_CUSTOM_TARGET(
        GenServerSource ALL
        DEPENDS ${CMAKE_BINARY_DIR}/sql/sql_yacc.h
                ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
                ${CMAKE_BINARY_DIR}/sql/lex_hash.h)

ADD_DEPENDENCIES(mysqld GenServerSource)

# Remove the auto-generated files as part of 'Clean Solution'
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES 
  "lex_hash.h;sql_yacc.h;sql_yacc.cc;mysqld.def")

ADD_LIBRARY(udf_example MODULE udf_example.c udf_example.def)
ADD_DEPENDENCIES(udf_example strings GenError)
TARGET_LINK_LIBRARIES(udf_example strings wsock32)

ADD_SUBDIRECTORY(share)

IF(WIN32)
  SET(my_bootstrap_sql ${CMAKE_CURRENT_BINARY_DIR}/my_bootstrap.sql)
  FILE(TO_NATIVE_PATH ${my_bootstrap_sql}  native_outfile)

  # Create bootstrapper SQL script
  ADD_CUSTOM_COMMAND(OUTPUT    
    ${my_bootstrap_sql}
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/scripts
    cmd /c copy mysql_system_tables.sql+mysql_system_tables_data.sql+fill_help_tables.sql ${native_outfile}
    DEPENDS
    ${CMAKE_SOURCE_DIR}/scripts/mysql_system_tables.sql
    ${CMAKE_SOURCE_DIR}/scripts/mysql_system_tables_data.sql
    ${CMAKE_SOURCE_DIR}/scripts/fill_help_tables.sql
   )
   
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mysql_bootstrap_sql.c
  COMMAND comp_sql
  mysql_bootstrap_sql
  ${CMAKE_CURRENT_BINARY_DIR}/my_bootstrap.sql
  mysql_bootstrap_sql.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS comp_sql ${my_bootstrap_sql})
  
  MYSQL_ADD_EXECUTABLE(mysql_install_db 
    mysql_install_db.cc
    ${CMAKE_CURRENT_BINARY_DIR}/mysql_bootstrap_sql.c
    COMPONENT Server)
  TARGET_LINK_LIBRARIES(mysql_install_db mysys strings dbug)

  MYSQL_ADD_EXECUTABLE(mysql_upgrade_service
    mysql_upgrade_service.cc
    COMPONENT Server)
  TARGET_LINK_LIBRARIES(mysql_upgrade_service mysys strings dbug)

  # mysql_install_db should be in the same directory as mysqld
  # to work correctly
  GET_TARGET_PROPERTY(MYSQLD_EXECUTABLE mysqld LOCATION)
  IF(NOT MYSQLD_EXECUTABLE)
    MESSAGE(FATAL_ERROR "Unexpected")
  ENDIF()
ENDIF()

# We need to create empty directories (data/test) the installation.  
# This does not work with current CPack due to http://www.cmake.org/Bug/view.php?id=8767
# Avoid completely empty directories and install dummy file instead.
SET(DUMMY_FILE ${CMAKE_CURRENT_BINARY_DIR}/.empty ) 
FILE(WRITE ${DUMMY_FILE} "")
INSTALL(FILES ${DUMMY_FILE} DESTINATION data/test COMPONENT DataFiles)

# Install initial database on windows
IF(NOT CMAKE_CROSSCOMPILING)
  GET_TARGET_PROPERTY(MYSQLD_EXECUTABLE mysqld LOCATION)
ENDIF()
IF(WIN32 AND MYSQLD_EXECUTABLE)
  CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/win/cmake/create_initial_db.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/create_initial_db.cmake
    @ONLY
  )
  
  IF(MSVC_IDE OR CMAKE_GENERATOR MATCHES "Xcode")
    SET (CONFIG_PARAM -DCONFIG=${CMAKE_CFG_INTDIR})
  ENDIF()
  MAKE_DIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/data)
  ADD_CUSTOM_COMMAND(
     OUTPUT initdb.dep
     COMMAND ${CMAKE_COMMAND}
     ${CONFIG_PARAM} -P ${CMAKE_CURRENT_BINARY_DIR}/create_initial_db.cmake
     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data
     COMMAND  ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/initdb.dep
     DEPENDS mysqld
  )
  ADD_CUSTOM_TARGET(initial_database  
    ALL
    DEPENDS  initdb.dep
  )
  INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data DESTINATION . 
  COMPONENT DataFiles PATTERN "initdb.dep" EXCLUDE  PATTERN "bootstrap.sql" EXCLUDE)
ELSE()
  # Not windows or cross compiling, just install an empty directory
  INSTALL(FILES ${DUMMY_FILE} DESTINATION data/mysql COMPONENT DataFiles)
ENDIF()
