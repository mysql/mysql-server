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

SET(CMAKE_CXX_FLAGS_DEBUG 
    "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX -DUSE_SYMDIR /Zi")
SET(CMAKE_C_FLAGS_DEBUG 
    "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX -DUSE_SYMDIR /Zi")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /MAP /MAPINFO:EXPORTS") 

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include 
                    ${CMAKE_SOURCE_DIR}/sql 
                    ${CMAKE_SOURCE_DIR}/regex 
                    ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/bdb/build_win32
                    ${CMAKE_SOURCE_DIR}/bdb/dbinc)

SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/sql/sql_yacc.h 
                            ${CMAKE_SOURCE_DIR}/sql/sql_yacc.cc
                            ${CMAKE_SOURCE_DIR}/include/mysql_version.h
                            ${CMAKE_SOURCE_DIR}/sql/lex_hash.h 
                            ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
                            ${PROJECT_SOURCE_DIR}/include/mysqld_ername.h
                            ${PROJECT_SOURCE_DIR}/include/sql_state.h 
                              PROPERTIES GENERATED 1)

ADD_DEFINITIONS(-DHAVE_INNOBASE -DMYSQL_SERVER 
                -D_CONSOLE -DHAVE_DLOPEN)
                
IF(DISABLE_GRANT_OPTIONS)                
  ADD_DEFINITIONS(-DDISABLE_GRANT_OPTIONS)
ENDIF(DISABLE_GRANT_OPTIONS)

ADD_EXECUTABLE(mysqld${MYSQLD_EXE_SUFFIX}
	       ../sql-common/client.c derror.cc des_key_file.cc
               discover.cc ../libmysql/errmsg.c field.cc  stacktrace.c stacktrace.h field_conv.cc 
               filesort.cc gstream.cc ha_blackhole.cc 
               ha_archive.cc ha_heap.cc ha_myisam.cc ha_myisammrg.cc
               ha_innodb.cc ha_federated.cc ha_berkeley.cc
               handler.cc hash_filo.cc hash_filo.h 
               hostname.cc init.cc item.cc item_buff.cc item_cmpfunc.cc 
               item_create.cc item_func.cc item_geofunc.cc item_row.cc 
               item_strfunc.cc item_subselect.cc item_sum.cc item_timefunc.cc 
               item_uniq.cc key.cc log.cc lock.cc log_event.cc message.rc 
               message.h mf_iocache.cc my_decimal.cc ../sql-common/my_time.c
               ../myisammrg/myrg_rnext_same.c mysqld.cc net_serv.cc 
               nt_servc.cc nt_servc.h opt_range.cc opt_range.h opt_sum.cc 
               ../sql-common/pack.c parse_file.cc password.c procedure.cc 
               protocol.cc records.cc repl_failsafe.cc set_var.cc 
               slave.cc sp.cc sp_cache.cc sp_head.cc sp_pcontext.cc 
               sp_rcontext.cc spatial.cc sql_acl.cc sql_analyse.cc sql_base.cc 
               sql_cache.cc sql_class.cc sql_client.cc sql_crypt.cc sql_crypt.h 
               sql_cursor.cc sql_db.cc sql_delete.cc sql_derived.cc sql_do.cc 
               sql_error.cc sql_handler.cc sql_help.cc sql_insert.cc sql_lex.cc 
               sql_list.cc sql_load.cc sql_manager.cc sql_map.cc sql_parse.cc 
               sql_prepare.cc sql_rename.cc 
               sql_repl.cc sql_select.cc sql_show.cc sql_state.c sql_string.cc 
               sql_table.cc sql_test.cc sql_trigger.cc sql_udf.cc sql_union.cc
               sql_update.cc sql_view.cc strfunc.cc table.cc thr_malloc.cc 
               time.cc tztime.cc uniques.cc unireg.cc 
               ../sql-common/my_user.c 
               sql_locale.cc
               sql_profile.cc
               ${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
  			   ${PROJECT_SOURCE_DIR}/sql/sql_yacc.h
			   ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
			   ${PROJECT_SOURCE_DIR}/include/mysqld_ername.h 
			   ${PROJECT_SOURCE_DIR}/include/sql_state.h
			   ${PROJECT_SOURCE_DIR}/include/mysql_version.h 
			   ${PROJECT_SOURCE_DIR}/sql/lex_hash.h)

TARGET_LINK_LIBRARIES(mysqld${MYSQLD_EXE_SUFFIX}
		      heap myisam myisammrg mysys yassl zlib debug dbug yassl 
                      taocrypt strings vio regex wsock32)

IF(EMBED_MANIFESTS)
  MYSQL_EMBED_MANIFEST("mysqld" "asInvoker")
ENDIF(EMBED_MANIFESTS)

IF(WITH_EXAMPLE_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld${MYSQLD_EXE_SUFFIX} example)
ENDIF(WITH_EXAMPLE_STORAGE_ENGINE)

IF(WITH_INNOBASE_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld${MYSQLD_EXE_SUFFIX} innobase)
ENDIF(WITH_INNOBASE_STORAGE_ENGINE)

IF(WITH_BERKELEY_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld${MYSQLD_EXE_SUFFIX} bdb)
ENDIF(WITH_BERKELEY_STORAGE_ENGINE)


ADD_DEPENDENCIES(mysqld${MYSQLD_EXE_SUFFIX} GenError)

# Sql Parser custom command
ADD_CUSTOM_COMMAND(
	SOURCE ${PROJECT_SOURCE_DIR}/sql/sql_yacc.yy 
	OUTPUT ${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
	COMMAND bison.exe ARGS -y -p MYSQL --defines=sql_yacc.h	
	                       --output=sql_yacc.cc sql_yacc.yy
	DEPENDS ${PROJECT_SOURCE_DIR}/sql/sql_yacc.yy)

ADD_CUSTOM_COMMAND(
	OUTPUT ${PROJECT_SOURCE_DIR}/sql/sql_yacc.h
	COMMAND echo
	DEPENDS ${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
)

# Gen_lex_hash
# About "mysqlclient_notls", see note in "client/CMakeLists.txt"
ADD_EXECUTABLE(gen_lex_hash gen_lex_hash.cc)
TARGET_LINK_LIBRARIES(gen_lex_hash mysqlclient_notls wsock32)
GET_TARGET_PROPERTY(GEN_LEX_HASH_EXE gen_lex_hash LOCATION)
ADD_CUSTOM_COMMAND(
	OUTPUT ${PROJECT_SOURCE_DIR}/sql/lex_hash.h
	COMMAND ${GEN_LEX_HASH_EXE} ARGS > lex_hash.h
	DEPENDS ${GEN_LEX_HASH_EXE}
)
ADD_DEPENDENCIES(mysqld${MYSQLD_EXE_SUFFIX} gen_lex_hash)

# Remove the auto-generated files as part of 'Clean Solution'
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES 
                         "lex_hash.h;sql_yacc.h;sql_yacc.cc")

ADD_LIBRARY(udf_example MODULE udf_example.c udf_example.def)
ADD_DEPENDENCIES(udf_example strings)
TARGET_LINK_LIBRARIES(udf_example strings wsock32)
