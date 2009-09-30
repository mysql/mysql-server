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

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

ADD_DEFINITIONS(-DMYSQL_SERVER -DEMBEDDED_LIBRARY -DHAVE_DLOPEN)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/libmysqld
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include 
                    ${CMAKE_SOURCE_DIR}/zlib)

SET(GEN_SOURCES     ${CMAKE_SOURCE_DIR}/sql/sql_yacc.cc
                    ${CMAKE_SOURCE_DIR}/sql/sql_yacc.h
                    ${CMAKE_SOURCE_DIR}/sql/message.h
                    ${CMAKE_SOURCE_DIR}/sql/message.rc
                    ${CMAKE_SOURCE_DIR}/sql/sql_builtin.cc
                    ${CMAKE_SOURCE_DIR}/sql/lex_hash.h)

SET_SOURCE_FILES_PROPERTIES(${GEN_SOURCES} PROPERTIES GENERATED 1)

# Include and add the directory path
SET(SOURCE_SUBLIBS TRUE)
SET(LIB_SOURCES "")

INCLUDE(${CMAKE_SOURCE_DIR}/zlib/CMakeLists.txt)
FOREACH(rpath ${ZLIB_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../zlib/${rpath})
ENDFOREACH(rpath)

# FIXME only needed if build type is "Debug", but CMAKE_BUILD_TYPE is
# not set during configure time.
INCLUDE(${CMAKE_SOURCE_DIR}/dbug/CMakeLists.txt)
FOREACH(rpath ${DBUG_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../dbug/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/CMakeLists.txt)
FOREACH(rpath ${TAOCRYPT_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../extra/yassl/taocrypt/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/extra/yassl/CMakeLists.txt)
FOREACH(rpath ${YASSL_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../extra/yassl/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/strings/CMakeLists.txt)
FOREACH(rpath ${STRINGS_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../strings/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/regex/CMakeLists.txt)
FOREACH(rpath ${REGEX_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../regex/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/mysys/CMakeLists.txt)
FOREACH(rpath ${MYSYS_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../mysys/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/vio/CMakeLists.txt)
FOREACH(rpath ${VIO_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../vio/${rpath})
ENDFOREACH(rpath)



FOREACH (ENGINE_LIB ${MYSQLD_STATIC_ENGINE_LIBS})
  INCLUDE(${CMAKE_SOURCE_DIR}/storage/${ENGINE_LIB}/CMakeLists.txt)
  STRING(TOUPPER ${ENGINE_LIB} ENGINE_LIB_UPPER)
  FOREACH(rpath ${${ENGINE_LIB_UPPER}_SOURCES})
    SET(LIB_SOURCES ${LIB_SOURCES} ${CMAKE_SOURCE_DIR}/storage/${ENGINE_LIB}/${rpath})
  ENDFOREACH(rpath)
ENDFOREACH(ENGINE_LIB)


SET(SOURCE_SUBLIBS FALSE)
                            
SET(LIBMYSQLD_SOURCES emb_qcache.cc libmysqld.c lib_sql.cc 
           ../libmysql/libmysql.c ../libmysql/errmsg.c ../client/get_password.c
           ../sql-common/client.c ../sql-common/my_time.c 
           ../sql-common/my_user.c ../sql-common/pack.c
           ../sql/password.c ../sql/discover.cc ../sql/derror.cc 
           ../sql/field.cc ../sql/field_conv.cc
           ../sql/filesort.cc ../sql/gstream.cc ../sql/ha_partition.cc 
           ../sql/handler.cc ../sql/hash_filo.cc ../sql/hostname.cc 
           ../sql/init.cc ../sql/item_buff.cc ../sql/item_cmpfunc.cc 
           ../sql/item.cc ../sql/item_create.cc ../sql/item_func.cc 
           ../sql/item_geofunc.cc ../sql/item_row.cc ../sql/item_strfunc.cc 
           ../sql/item_subselect.cc ../sql/item_sum.cc ../sql/item_timefunc.cc 
           ../sql/item_xmlfunc.cc ../sql/key.cc ../sql/lock.cc ../sql/log.cc 
           ../sql/log_event.cc ../sql/mf_iocache.cc ../sql/my_decimal.cc 
           ../sql/net_serv.cc ../sql/opt_range.cc ../sql/opt_sum.cc 
           ../sql/parse_file.cc ../sql/procedure.cc ../sql/protocol.cc 
           ../sql/records.cc ../sql/repl_failsafe.cc ../sql/rpl_filter.cc
           ../sql/rpl_record.cc
           ../sql/rpl_injector.cc ../sql/set_var.cc ../sql/spatial.cc 
           ../sql/sp_cache.cc ../sql/sp.cc ../sql/sp_head.cc 
           ../sql/sp_pcontext.cc ../sql/sp_rcontext.cc ../sql/sql_acl.cc 
           ../sql/sql_analyse.cc ../sql/sql_base.cc ../sql/sql_cache.cc 
           ../sql/sql_class.cc ../sql/sql_crypt.cc ../sql/sql_cursor.cc 
           ../sql/sql_db.cc ../sql/sql_delete.cc ../sql/sql_derived.cc 
           ../sql/sql_do.cc ../sql/sql_error.cc ../sql/sql_handler.cc 
           ../sql/sql_help.cc ../sql/sql_insert.cc ../sql/sql_lex.cc 
           ../sql/sql_list.cc ../sql/sql_load.cc ../sql/sql_locale.cc 
           ../sql/sql_binlog.cc ../sql/sql_manager.cc ../sql/sql_map.cc 
           ../sql/sql_parse.cc ../sql/sql_partition.cc ../sql/sql_plugin.cc 
           ../sql/sql_prepare.cc ../sql/sql_rename.cc ../sql/sql_repl.cc 
           ../sql/sql_select.cc ../sql/sql_servers.cc
           ../sql/sql_show.cc ../sql/sql_state.c ../sql/sql_string.cc
           ../sql/sql_tablespace.cc ../sql/sql_table.cc ../sql/sql_test.cc
           ../sql/sql_trigger.cc ../sql/sql_udf.cc ../sql/sql_union.cc
           ../sql/sql_update.cc ../sql/sql_view.cc ../sql/sql_profile.cc
           ../sql/strfunc.cc ../sql/table.cc ../sql/thr_malloc.cc
           ../sql/time.cc ../sql/tztime.cc ../sql/uniques.cc ../sql/unireg.cc
           ../sql/partition_info.cc ../sql/sql_connect.cc 
           ../sql/scheduler.cc ../sql/event_parse_data.cc
           ./sql/sql_signal.cc
           ${GEN_SOURCES}
           ${LIB_SOURCES})

# Seems we cannot make a library without at least one source file. So use a
# dummy empty file
FILE(WRITE cmake_dummy.c " ")

# Tried use the correct ${GEN_SOURCES} as dependency, worked on Unix
# but not on Windows and Visual Studio generators. Likely because they
# are no real targets from the Visual Studio project files view. Added
# custom targets to "sql/CMakeLists.txt" and reference them here.
ADD_LIBRARY(mysqlserver STATIC ${LIBMYSQLD_SOURCES})
ADD_DEPENDENCIES(mysqlserver GenServerSource GenError)
TARGET_LINK_LIBRARIES(mysqlserver)

ADD_LIBRARY(libmysqld SHARED cmake_dummy.c libmysqld.def)
ADD_DEPENDENCIES(libmysqld mysqlserver)
TARGET_LINK_LIBRARIES(libmysqld mysqlserver wsock32)
