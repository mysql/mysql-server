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

INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}/include 
${CMAKE_SOURCE_DIR}/sql 
${CMAKE_SOURCE_DIR}/regex 
${ZLIB_INCLUDE_DIR}
${SSL_INCLUDE_DIRS}
${CMAKE_BINARY_DIR}/sql
)

SET(GEN_SOURCES
${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h 
${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
${CMAKE_CURRENT_BINARY_DIR}/sql_builtin.cc
${CMAKE_CURRENT_BINARY_DIR}/lex_hash.h 
)

SET_SOURCE_FILES_PROPERTIES(${GEN_SOURCES} PROPERTIES GENERATED 1)

ADD_DEFINITIONS(-DMYSQL_SERVER -DHAVE_EVENT_SCHEDULER) 
IF(SSL_DEFINES)
 ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()


SET (SQL_SOURCE
               ../sql-common/client.c derror.cc abstract_query_plan.cc 
	       des_key_file.cc
               discover.cc ../libmysql/errmsg.c field.cc  field_conv.cc 
               filesort.cc gstream.cc sha2.cc
               signal_handler.cc
               handler.cc hash_filo.h sql_plugin_services.h
               hostname.cc init.cc item.cc item_buff.cc item_cmpfunc.cc 
               item_create.cc item_func.cc item_geofunc.cc item_row.cc 
               item_strfunc.cc item_subselect.cc item_sum.cc item_timefunc.cc 
               key.cc log.cc lock.cc
               log_event.cc rpl_record.cc rpl_reporting.cc
               log_event_old.cc rpl_record_old.cc
               message.h mf_iocache.cc my_decimal.cc ../sql-common/my_time.c
               mysqld.cc net_serv.cc  keycaches.cc
               ../sql-common/client_plugin.c
               opt_range.cc opt_range.h opt_sum.cc 
               ../sql-common/pack.c parse_file.cc password.c procedure.cc 
               protocol.cc records.cc repl_failsafe.cc rpl_filter.cc set_var.cc 
               slave.cc sp.cc sp_cache.cc sp_head.cc sp_pcontext.cc 
               sp_rcontext.cc spatial.cc sql_acl.cc sql_analyse.cc sql_base.cc 
               sql_cache.cc sql_class.cc sql_client.cc sql_crypt.cc sql_crypt.h 
               sql_cursor.cc sql_db.cc sql_delete.cc sql_derived.cc sql_do.cc 
               sql_error.cc sql_handler.cc sql_help.cc sql_insert.cc sql_lex.cc 
               sql_list.cc sql_load.cc sql_manager.cc sql_parse.cc
               sql_partition.cc sql_plugin.cc sql_prepare.cc sql_rename.cc 
               debug_sync.cc debug_sync.h
               sql_repl.cc sql_select.cc sql_show.cc sql_state.c sql_string.cc 
               sql_table.cc sql_test.cc sql_trigger.cc sql_udf.cc sql_union.cc
               sql_update.cc sql_view.cc strfunc.cc table.cc thr_malloc.cc 
               sql_time.cc tztime.cc uniques.cc unireg.cc item_xmlfunc.cc 
               rpl_tblmap.cc sql_binlog.cc event_scheduler.cc event_data_objects.cc
               event_queue.cc event_db_repository.cc 
               sql_tablespace.cc events.cc ../sql-common/my_user.c 
               partition_info.cc rpl_utility.cc rpl_injector.cc sql_locale.cc
               rpl_rli.cc rpl_mi.cc sql_servers.cc sql_audit.cc
               sql_connect.cc scheduler.cc sql_partition_admin.cc
               sql_profile.cc event_parse_data.cc sql_alter.cc
               sql_signal.cc rpl_handler.cc mdl.cc sql_admin.cc
               transaction.cc sys_vars.cc sql_truncate.cc datadict.cc
               sql_reload.cc
               ${GEN_SOURCES}
               ${MYSYS_LIBWRAP_SOURCE})

MYSQL_ADD_PLUGIN(partition ha_partition.cc STORAGE_ENGINE DEFAULT STATIC_ONLY
RECOMPILE_FOR_EMBEDDED)

ADD_LIBRARY(sql STATIC ${SQL_SOURCE})
ADD_DEPENDENCIES(sql GenServerSource)
DTRACE_INSTRUMENT(sql)
TARGET_LINK_LIBRARIES(sql ${MYSQLD_STATIC_PLUGIN_LIBS} 
  mysys dbug strings vio regex   
  ${LIBWRAP} ${LIBCRYPT} ${LIBDL}
  ${SSL_LIBRARIES})

#
# On Windows platform we compile in the clinet-side Windows Native Authentication
# plugin which is used by the client connection code included in the server.
#
IF(WIN32)
  ADD_DEFINITIONS(-DAUTHENTICATION_WIN)
  TARGET_LINK_LIBRARIES(sql auth_win_client)
ENDIF() 

IF(WIN32)
  SET(MYSQLD_SOURCE main.cc nt_servc.cc nt_servc.h message.rc)
ELSE()
  SET(MYSQLD_SOURCE main.cc ${DTRACE_PROBES_ALL})
ENDIF()

MYSQL_ADD_EXECUTABLE(mysqld ${MYSQLD_SOURCE} DESTINATION ${INSTALL_SBINDIR} COMPONENT Server)

IF(APPLE) 
  # Add CoreServices framework since some dloadable plugins may need it 
  FIND_LIBRARY(CORESERVICES NAMES CoreServices) 
  IF(CORESERVICES) 
    TARGET_LINK_LIBRARIES(mysqld ${CORESERVICES}) 
  ENDIF() 
ENDIF() 

IF(NOT WITHOUT_DYNAMIC_PLUGINS)
  SET_TARGET_PROPERTIES(mysqld PROPERTIES ENABLE_EXPORTS TRUE)
  GET_TARGET_PROPERTY(mysqld_link_flags mysqld LINK_FLAGS)
  IF(NOT mysqld_link_flags)
    SET(mysqld_link_flags)
  ENDIF()
  IF (MINGW OR CYGWIN)
    SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS "${mysqld_link_flags} -Wl,--export-all-symbols")
  ENDIF()
  IF(MSVC)
    # Set module definition file. Also use non-incremental linker, 
    # incremental appears to crash from time to time,if used with /DEF option
    SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS "${mysqld_link_flags} /DEF:mysqld.def /INCREMENTAL:NO")

    FOREACH (CORELIB sql mysys dbug strings)
      GET_TARGET_PROPERTY(LOC ${CORELIB} LOCATION)
      FILE(TO_NATIVE_PATH ${LOC} LOC)
      SET (LIB_LOCATIONS ${LIB_LOCATIONS} ${LOC}) 
    ENDFOREACH (CORELIB ${MYSQLD_CORE_LIBS})
    SET(_PLATFORM x86)
    IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET(_PLATFORM  x64)
    ENDIF()
    ADD_CUSTOM_COMMAND(TARGET mysqld PRE_LINK
      COMMAND echo ${_PLATFORM} && cscript ARGS //nologo ${PROJECT_SOURCE_DIR}/win/create_def_file.js
                  ${_PLATFORM}  ${LIB_LOCATIONS} > mysqld.def 
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    ADD_DEPENDENCIES(sql GenError)
  ENDIF()
ENDIF()

SET_TARGET_PROPERTIES(mysqld PROPERTIES ENABLE_EXPORTS TRUE) 
TARGET_LINK_LIBRARIES(mysqld sql mysys)

# Provide plugins with minimal set of libraries
SET(INTERFACE_LIBS ${LIBRT})
IF(INTERFACE_LIBS)
 SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_INTERFACE_LIBRARIES 
  "${INTERFACE_LIBS}")
ENDIF()

# On Solaris, some extra effort is required in order to get dtrace probes
# from static libraries
DTRACE_INSTRUMENT_STATIC_LIBS(mysqld 
 "sql;mysys;${MYSQLD_STATIC_PLUGIN_LIBS}")
 

SET(WITH_MYSQLD_LDFLAGS "" CACHE STRING "Additional linker flags for mysqld")
MARK_AS_ADVANCED(WITH_MYSQLD_LDFLAGS)
IF(WITH_MYSQLD_LDFLAGS)
  GET_TARGET_PROPERTY(mysqld LINK_FLAGS MYSQLD_LINK_FLAGS)
  IF(NOT MYSQLD_LINK_FLAGS)
    SET(MYSQLD_LINK_FLAGS) 
  ENDIF() 
  SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS 
     "${MYSQLD_LINK_FLAGS} ${WITH_MYSQLD_LDFLAGS}")
ENDIF()
INSTALL_DEBUG_TARGET(mysqld 
  DESTINATION ${INSTALL_SBINDIR} 
  PDB_DESTINATION ${INSTALL_SBINDIR}/debug
  RENAME mysqld-debug)

# Handle out-of-source build from source package with possibly broken 
# bison. Copy bison output to from source to build directory, if not already 
# there
IF (NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sql_yacc.cc)
    IF(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc)
      CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/sql_yacc.cc 
        ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc COPYONLY)
      CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/sql_yacc.h
        ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h COPYONLY)
    ENDIF()
  ENDIF()
ENDIF()


INCLUDE(${CMAKE_SOURCE_DIR}/cmake/bison.cmake)
RUN_BISON(
  ${CMAKE_CURRENT_SOURCE_DIR}/sql_yacc.yy 
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h
)

# Gen_lex_hash
ADD_EXECUTABLE(gen_lex_hash gen_lex_hash.cc)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex_hash.h
  COMMAND gen_lex_hash > lex_hash.h
  DEPENDS gen_lex_hash
)

MYSQL_ADD_EXECUTABLE(mysql_tzinfo_to_sql tztime.cc)
SET_TARGET_PROPERTIES(mysql_tzinfo_to_sql PROPERTIES COMPILE_FLAGS "-DTZINFO2SQL")
TARGET_LINK_LIBRARIES(mysql_tzinfo_to_sql mysys)

ADD_CUSTOM_TARGET( 
        GenServerSource
        DEPENDS ${GEN_SOURCES}
)

#Need this only for embedded
SET_TARGET_PROPERTIES(GenServerSource PROPERTIES EXCLUDE_FROM_ALL TRUE)

IF(WIN32 OR HAVE_DLOPEN AND NOT DISABLE_SHARED)
  ADD_LIBRARY(udf_example MODULE udf_example.c)
  SET_TARGET_PROPERTIES(udf_example PROPERTIES PREFIX "")
  # udf_example depends on strings 
  IF(WIN32)
    IF(MSVC)
     SET_TARGET_PROPERTIES(udf_example PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/udf_example.def")
    ENDIF()
    TARGET_LINK_LIBRARIES(udf_example strings)
  ELSE()
    # udf_example is using safemutex exported by mysqld
    TARGET_LINK_LIBRARIES(udf_example mysqld)
  ENDIF()
ENDIF()

FOREACH(tool glibtoolize libtoolize aclocal autoconf autoheader automake gtar 
 tar bzr)
 STRING(TOUPPER ${tool}  TOOL)
 FIND_PROGRAM(${TOOL}_EXECUTABLE ${tool} DOC "path to the executable")
 MARK_AS_ADVANCED(${TOOL}_EXECUTABLE)
ENDFOREACH()

CONFIGURE_FILE(
  ${CMAKE_SOURCE_DIR}/cmake/make_dist.cmake.in ${CMAKE_BINARY_DIR}/make_dist.cmake @ONLY)

ADD_CUSTOM_TARGET(dist 
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/make_dist.cmake
  DEPENDS ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc ${CMAKE_BINARY_DIR}/sql/sql_yacc.h
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

ADD_CUSTOM_TARGET(distclean
  COMMAND ${CMAKE_COMMAND} -E echo  WARNING: distclean target is not functional
  COMMAND ${CMAKE_COMMAND} -E echo  Use 'bzr clean-tree' with --unknown and/or
    --ignored parameter instead
  VERBATIM
  )

IF(INSTALL_LAYOUT STREQUAL "STANDALONE")

# We need to create empty directories (data/test) the installation.  
# This does not work with current CPack due to http://www.cmake.org/Bug/view.php?id=8767
# Avoid completely empty directories and install dummy file instead.
# Use a file extension so that it will be deleted in case someone does
#   'drop database test'
# See deletable_extentions.
SET(DUMMY_FILE ${CMAKE_CURRENT_BINARY_DIR}/dummy.bak ) 
FILE(WRITE ${DUMMY_FILE} "")
INSTALL(FILES ${DUMMY_FILE} DESTINATION data/test COMPONENT DataFiles)

# Install initial database on windows
IF(NOT CMAKE_CROSSCOMPILING)
  GET_TARGET_PROPERTY(MYSQLD_EXECUTABLE mysqld LOCATION)
ENDIF()
IF(WIN32 AND MYSQLD_EXECUTABLE)
  CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/cmake/create_initial_db.cmake.in
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
     DEPENDS mysqld
  )
  ADD_CUSTOM_TARGET(initial_database  
    ALL
    DEPENDS  initdb.dep
  )
  INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data
    DESTINATION . 
    COMPONENT DataFiles
    PATTERN "initdb.dep" EXCLUDE
    PATTERN "bootstrap.sql" EXCLUDE)
ELSE()
  # Not windows or cross compiling, just install an empty directory
  INSTALL(FILES ${DUMMY_FILE} DESTINATION data/mysql COMPONENT DataFiles)
ENDIF()
ENDIF()

ADD_CUSTOM_TARGET(show-dist-name
  COMMAND ${CMAKE_COMMAND} -E echo "${CPACK_PACKAGE_FILE_NAME}"
)

