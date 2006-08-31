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

SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/sql/message.rc 
							${CMAKE_SOURCE_DIR}/sql/message.h 
							${CMAKE_SOURCE_DIR}/sql/sql_yacc.h 
							${CMAKE_SOURCE_DIR}/sql/sql_yacc.cc
                            ${CMAKE_SOURCE_DIR}/include/mysql_version.h
                            ${CMAKE_SOURCE_DIR}/sql/lex_hash.h 
                            ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
                            ${PROJECT_SOURCE_DIR}/include/mysqld_ername.h
                            ${PROJECT_SOURCE_DIR}/include/sql_state.h 
                              PROPERTIES GENERATED 1)

ADD_DEFINITIONS(-DHAVE_INNOBASE -DMYSQL_SERVER 
                -D_CONSOLE -DHAVE_DLOPEN)

ADD_EXECUTABLE(mysqld ../sql-common/client.c derror.cc des_key_file.cc
               discover.cc ../libmysql/errmsg.c field.cc field_conv.cc 
               filesort.cc gstream.cc ha_blackhole.cc 
               ha_archive.cc ha_heap.cc ha_myisam.cc ha_myisammrg.cc
               ha_innodb.cc ha_federated.cc ha_berkeley.cc ha_blackhole.cc
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
               ${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
  			   ${PROJECT_SOURCE_DIR}/sql/sql_yacc.h
			   ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
			   ${PROJECT_SOURCE_DIR}/include/mysqld_ername.h 
			   ${PROJECT_SOURCE_DIR}/include/sql_state.h
			   ${PROJECT_SOURCE_DIR}/include/mysql_version.h 
			   ${PROJECT_SOURCE_DIR}/sql/lex_hash.h)

TARGET_LINK_LIBRARIES(mysqld heap myisam myisammrg mysys yassl zlib dbug yassl 
                      taocrypt strings vio regex wsock32)

IF(WITH_EXAMPLE_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld example)
ENDIF(WITH_EXAMPLE_STORAGE_ENGINE)

IF(WITH_INNOBASE_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld innobase)
ENDIF(WITH_INNOBASE_STORAGE_ENGINE)

IF(WITH_BERKELEY_STORAGE_ENGINE)
  TARGET_LINK_LIBRARIES(mysqld bdb)
ENDIF(WITH_BERKELEY_STORAGE_ENGINE)


ADD_DEPENDENCIES(mysqld GenError)

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

# Windows message file
ADD_CUSTOM_COMMAND(
	SOURCE ${PROJECT_SOURCE_DIR}/sql/message.mc
	OUTPUT message.rc message.h
	COMMAND mc ARGS ${PROJECT_SOURCE_DIR}/sql/message.mc
	DEPENDS ${PROJECT_SOURCE_DIR}/sql/message.mc)

# Gen_lex_hash
ADD_EXECUTABLE(gen_lex_hash gen_lex_hash.cc)
TARGET_LINK_LIBRARIES(gen_lex_hash dbug mysqlclient wsock32)
GET_TARGET_PROPERTY(GEN_LEX_HASH_EXE gen_lex_hash LOCATION)
ADD_CUSTOM_COMMAND(
	OUTPUT ${PROJECT_SOURCE_DIR}/sql/lex_hash.h
	COMMAND ${GEN_LEX_HASH_EXE} ARGS > lex_hash.h
	DEPENDS ${GEN_LEX_HASH_EXE}
)

ADD_DEPENDENCIES(mysqld gen_lex_hash)
