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

IF(MSVC)
    #Innodb plugin needs linker-generated map file to locate server exports
    #see bug#42001
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP /MAPINFO:EXPORTS")
ENDIF(MSVC)

INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}/include 
${CMAKE_SOURCE_DIR}/sql 
${CMAKE_SOURCE_DIR}/regex 
${ZLIB_INCLUDE_DIR}
${SSL_INCLUDE_DIRS}
${CMAKE_BINARY_DIR}/sql
)

SET(GEN_SOURCES
${CMAKE_BINARY_DIR}/sql/sql_yacc.h 
${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
${CMAKE_BINARY_DIR}/sql/sql_builtin.cc
${CMAKE_BINARY_DIR}/sql/lex_hash.h 
)

SET_SOURCE_FILES_PROPERTIES(${GEN_SOURCES} PROPERTIES GENERATED 1)

ADD_DEFINITIONS(-DMYSQL_SERVER -DHAVE_EVENT_SCHEDULER) 
IF(SSL_DEFINES)
 ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()



SET (SQL_SOURCE
               ../sql-common/client.c derror.cc des_key_file.cc
               discover.cc ../libmysql/errmsg.c field.cc  field_conv.cc 
               filesort.cc gstream.cc
               ha_partition.cc
               handler.cc hash_filo.cc hash_filo.h sql_plugin_services.h
               hostname.cc init.cc item.cc item_buff.cc item_cmpfunc.cc 
               item_create.cc item_func.cc item_geofunc.cc item_row.cc 
               item_strfunc.cc item_subselect.cc item_sum.cc item_timefunc.cc 
               key.cc log.cc lock.cc message.rc 
               log_event.cc rpl_record.cc rpl_reporting.cc
               log_event_old.cc rpl_record_old.cc
               message.h mf_iocache.cc my_decimal.cc ../sql-common/my_time.c
               mysqld.cc net_serv.cc 
               opt_range.cc opt_range.h opt_sum.cc 
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
               sql_profile.cc event_parse_data.cc
               sql_signal.cc rpl_handler.cc
			   ${GEN_SOURCES}
               ${MYSYS_LIBWRAP_SOURCE})


SET (MYSQLD_CORE_LIBS mysys ${ZLIB_LIBRARY} ${SSL_LIBRARIES}  vio regex )
IF(WIN32)
  SET(SQL_SOURCE ${SQL_SOURCE} nt_servc.cc nt_servc.h)
ENDIF()
IF(MSVC)
  ADD_LIBRARY(sql ${SQL_SOURCE})
  ADD_EXECUTABLE(mysqld mysqld_dummy.cc)
  ADD_CUSTOM_COMMAND( 
        OUTPUT  mysqld_dummy.cc
        COMMAND cmake ARGS -E touch mysqld_dummy.cc
        VERBATIM)
  SET(MYSQLD_CORE_LIBS ${MYSQLD_CORE_LIBS} sql)
  
ELSE()
  ADD_EXECUTABLE(mysqld ${SQL_SOURCE})
  DTRACE_INSTRUMENT(mysqld)
ENDIF()  

IF(NOT WITHOUT_DYNAMIC_PLUGINS)
  SET_TARGET_PROPERTIES(mysqld PROPERTIES ENABLE_EXPORTS TRUE)
  IF (MINGW OR CYGWIN)
    SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS "-Wl,--export-all-symbols")
  ENDIF()
  IF(MSVC)
    # Set module definition file. Also use non-incremental linker, 
    # incremental appears to crash from time to time,if used with /DEF option
    SET_TARGET_PROPERTIES(mysqld PROPERTIES LINK_FLAGS "/DEF:mysqld.def /INCREMENTAL:NO")

    FOREACH (CORELIB ${MYSQLD_CORE_LIBS}  dbug strings)
      GET_TARGET_PROPERTY(LOC ${CORELIB} LOCATION)
      FILE(TO_NATIVE_PATH ${LOC} LOC)
      SET (LIB_LOCATIONS ${LIB_LOCATIONS} ${LOC}) 
    ENDFOREACH (CORELIB ${MYSQLD_CORE_LIBS})
 
    ADD_CUSTOM_COMMAND(TARGET mysqld PRE_LINK
      COMMAND echo ${PLATFORM} && cscript ARGS //nologo ${PROJECT_SOURCE_DIR}/win/create_def_file.js
                  ${PLATFORM}  ${LIB_LOCATIONS} > mysqld.def 
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    ADD_DEPENDENCIES(sql GenError)
  ENDIF()
ENDIF()

SET_TARGET_PROPERTIES(mysqld PROPERTIES ENABLE_EXPORTS TRUE) 
SET_TARGET_PROPERTIES(mysqld PROPERTIES OUTPUT_NAME mysqld${MYSQLD_EXE_SUFFIX})  
TARGET_LINK_LIBRARIES(mysqld ${MYSQLD_STATIC_PLUGIN_LIBS} ${MYSQLD_CORE_LIBS} ${LIBWRAP_LIBRARY})
INSTALL(TARGETS mysqld DESTINATION bin)
INSTALL_DEBUG_SYMBOLS(mysqld)
  

FIND_PROGRAM(BISON_EXECUTABLE bison DOC "path to the bison executable")
MARK_AS_ADVANCED(BISON_EXECUTABLE "")


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
TARGET_LINK_LIBRARIES(gen_lex_hash mysys)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex_hash.h
  COMMAND gen_lex_hash ARGS > lex_hash.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_lex_hash.cc)
        
ADD_CUSTOM_TARGET( 
        GenServerSource
        DEPENDS ${GEN_SOURCES}
)

#Need this only for embedded
SET_TARGET_PROPERTIES(GenServerSource PROPERTIES EXCLUDE_FROM_ALL TRUE)

IF(WIN32 OR HAVE_DLOPEN)
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





# We need to create empty directories (data/test) the installation.  
# This does not work with current CPack due to http://www.cmake.org/Bug/view.php?id=8767
# Avoid completely empty directories and install dummy file instead.

SET(DUMMY_FILE ${CMAKE_CURRENT_BINARY_DIR}/.empty ) 
FILE(WRITE ${DUMMY_FILE} "")
INSTALL(FILES ${DUMMY_FILE} DESTINATION data/test)

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
  
  MAKE_DIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/data)
  ADD_CUSTOM_COMMAND(
     OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data/mysql/user.frm
     COMMAND ${CMAKE_COMMAND} 
	 -DCONFIG=${CMAKE_CFG_INTDIR} 
	 -P ${CMAKE_CURRENT_BINARY_DIR}/create_initial_db.cmake
     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data
     DEPENDS mysqld
  )
  ADD_CUSTOM_TARGET(initial_database  
    ALL
    DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/data/mysql/user.frm
  )
  INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/mysql DESTINATION data)
ELSE()
  # Not windows or cross compiling, just install an empty directory
  INSTALL(FILES ${DUMMY_FILE} DESTINATION data/mysql)
ENDIF()
