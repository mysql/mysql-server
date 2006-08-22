SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# Need to set USE_TLS, since __declspec(thread) approach to thread local
# storage does not work properly in DLLs.
ADD_DEFINITIONS(-DUSE_TLS -DHAVE_ROW_BASED_REPLICATION -DMYSQL_SERVER
                -DEMBEDDED_LIBRARY)

# The old Windows build method used renamed (.cc -> .cpp) source files, fails
# in #include in lib_sql.cc. So disable that using the USING_CMAKE define.
ADD_DEFINITIONS(-DUSING_CMAKE)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
		    ${CMAKE_SOURCE_DIR}/libmysqld
		    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include 
                    ${CMAKE_SOURCE_DIR}/zlib 
)

SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
                            ${PROJECT_SOURCE_DIR}/sql/sql_yacc.h
                            PROPERTIES GENERATED 1)
                            
ADD_LIBRARY(mysqldemb emb_qcache.cc libmysqld.c lib_sql.cc
            ../client/get_password.c ../libmysql/errmsg.c
            ../libmysql/libmysql.c ../sql/password.c ../sql-common/client.c
            ../sql-common/my_time.c ../sql-common/my_user.c
            ../sql-common/pack.c ../sql/derror.cc ../sql/event_scheduler.cc
            ../sql/event_timed.cc ../sql/events.cc ../sql/discover.cc
            ../sql/field_conv.cc ../sql/field.cc ../sql/filesort.cc
            ../sql/gstream.cc ../sql/ha_heap.cc ../sql/ha_myisam.cc
            ../sql/ha_myisammrg.cc ${mysql_se_ha_src}
            ../sql/handler.cc ../sql/hash_filo.cc
            ../sql/hostname.cc ../sql/init.cc ../sql/item_buff.cc
            ../sql/item_cmpfunc.cc ../sql/item.cc ../sql/item_create.cc
            ../sql/item_func.cc ../sql/item_geofunc.cc ../sql/item_row.cc
            ../sql/item_strfunc.cc ../sql/item_subselect.cc ../sql/item_sum.cc
            ../sql/item_timefunc.cc ../sql/item_uniq.cc ../sql/item_xmlfunc.cc
            ../sql/key.cc ../sql/lock.cc ../sql/log.cc ../sql/log_event.cc
            ../sql/mf_iocache.cc ../sql/my_decimal.cc ../sql/net_serv.cc
            ../sql/opt_range.cc ../sql/opt_sum.cc ../sql/parse_file.cc
            ../sql/procedure.cc ../sql/protocol.cc ../sql/records.cc
            ../sql/repl_failsafe.cc ../sql/rpl_filter.cc ../sql/set_var.cc
            ../sql/spatial.cc ../sql/sp_cache.cc ../sql/sp.cc
            ../sql/sp_head.cc ../sql/sp_pcontext.cc ../sql/sp_rcontext.cc
            ../sql/sql_acl.cc ../sql/sql_analyse.cc ../sql/sql_base.cc
            ../sql/sql_cache.cc ../sql/sql_class.cc ../sql/sql_crypt.cc
            ../sql/sql_cursor.cc ../sql/sql_db.cc ../sql/sql_delete.cc
            ../sql/sql_derived.cc ../sql/sql_do.cc ../sql/sql_error.cc
            ../sql/sql_handler.cc ../sql/sql_help.cc ../sql/sql_insert.cc
            ../sql/sql_lex.cc ../sql/sql_list.cc ../sql/sql_load.cc
            ../sql/sql_manager.cc ../sql/sql_map.cc ../sql/sql_parse.cc
            ../sql/sql_partition.cc ../sql/sql_plugin.cc ../sql/sql_prepare.cc
            ../sql/sql_rename.cc ../sql/sql_repl.cc ../sql/sql_select.cc
            ../sql/sql_show.cc ../sql/sql_state.c ../sql/sql_string.cc
            ../sql/sql_tablespace.cc ../sql/sql_table.cc ../sql/sql_test.cc
            ../sql/sql_trigger.cc ../sql/sql_udf.cc ../sql/sql_union.cc
            ../sql/sql_update.cc ../sql/sql_view.cc 
            ../sql/strfunc.cc ../sql/table.cc ../sql/thr_malloc.cc
            ../sql/time.cc ../sql/tztime.cc ../sql/uniques.cc ../sql/unireg.cc
            ../sql/partition_info.cc ../sql/sql_locale.cc
            ../vio/vio.c ../vio/viosocket.c ../vio/viossl.c
            ../vio/viosslfactories.c
            ${PROJECT_SOURCE_DIR}/sql/sql_yacc.cc
            ${PROJECT_SOURCE_DIR}/sql/sql_yacc.h)

# Seems we cannot make a library without at least one source file. So use a
# dummy empty file
FILE(WRITE cmake_dummy.c " ")

ADD_LIBRARY(mysqlserver cmake_dummy.c)
TARGET_LINK_LIBRARIES(mysqlserver wsock32)
ADD_DEPENDENCIES(mysqlserver dbug mysys strings zlib mysqldemb regex myisam myisammrg
                 heap yassl taocrypt)
IF(WITH_ARCHIVE_STORAGE_ENGINE)
  ADD_DEPENDENCIES(mysqlserver archive)
ENDIF(WITH_ARCHIVE_STORAGE_ENGINE)
IF(WITH_EXAMPLE_STORAGE_ENGINE)
  ADD_DEPENDENCIES(mysqlserver example)
ENDIF(WITH_EXAMPLE_STORAGE_ENGINE)
IF(WITH_INNOBASE_STORAGE_ENGINE)
  ADD_DEPENDENCIES(mysqlserver innobase)
ENDIF(WITH_INNOBASE_STORAGE_ENGINE)

ADD_LIBRARY(libmysqld MODULE cmake_dummy.c libmysqld.def)
TARGET_LINK_LIBRARIES(libmysqld wsock32)
ADD_DEPENDENCIES(libmysqld mysqlserver)
