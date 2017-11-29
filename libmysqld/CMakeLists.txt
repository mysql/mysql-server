# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

ADD_DEFINITIONS(-DMYSQL_SERVER -DEMBEDDED_LIBRARY
 ${SSL_DEFINES})

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include 
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
  ${CMAKE_SOURCE_DIR}/libmysqld
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/sql/auth
  ${CMAKE_SOURCE_DIR}/sql/conn_handler
  ${CMAKE_BINARY_DIR}/libmysqld
  ${CMAKE_BINARY_DIR}/sql
  ${CMAKE_SOURCE_DIR}/regex
  ${ZLIB_INCLUDE_DIR}
  ${SSL_INCLUDE_DIRS}
  ${SSL_INTERNAL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/sql/backup
  ${LZ4_INCLUDE_DIR}
)

# We should generate these separately for libmysqld to avoid
# compiling them for libmysqld while they are generated for sql.
SET(GEN_YACC_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h 
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.h
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.cc
  ${CMAKE_CURRENT_BINARY_DIR}/lex_hash.h
)

SET(GEN_SOURCES
  ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc
)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex_hash.h
  COMMAND gen_lex_hash > lex_hash.h
  DEPENDS gen_lex_hash
)

SET_SOURCE_FILES_PROPERTIES(${GEN_YACC_SOURCES}
                            ${GEN_SOURCES}
                            PROPERTIES GENERATED TRUE)

FOREACH(file ${SQL_EXPORTED_SOURCES})
  LIST(APPEND IMPORTED_SOURCES "../sql/${file}")
ENDFOREACH()

SET(SQL_EMBEDDED_SOURCES
  emb_qcache.cc
  lib_sql.cc 
  libmysqld.c
  ${GEN_SOURCES}
  ${GEN_YACC_SOURCES}
  ../extra/lz4/xxhash.c
  ../client/get_password.c
  ../libmysql/errmsg.c
  ../libmysql/libmysql.c
  ../sql-common/client.c
  ../sql-common/client_plugin.c
  ../sql-common/my_time.c 
  ../sql-common/my_user.c
  ../sql-common/pack.c
  ../sql-common/sql_string.cc
  ../libbinlogevents/src/statement_events.cpp
  ../libbinlogevents/src/control_events.cpp
  ../libbinlogevents/src/load_data_events.cpp
  ../libbinlogevents/src/rows_event.cpp
  ../libbinlogevents/src/binlog_event.cpp
  ../libbinlogevents/src/binary_log_funcs.cpp
  ../libbinlogevents/src/uuid.cpp
  ../sql/binlog.cc 
  ../sql/event_parse_data.cc
  ../sql/log_event.cc
  ../sql/mysqld_thd_manager.cc
  ../sql/rpl_filter.cc
  ../sql/rpl_injector.cc
  ../sql/rpl_record.cc
  ../sql/rpl_reporting.cc
  ../sql/rpl_utility.cc
  ../sql/rpl_gtid_misc.cc
  ../sql/rpl_gtid_sid_map.cc
  ../sql/rpl_gtid_set.cc
  ../sql/rpl_gtid_specification.cc
  ../sql/rpl_gtid_state.cc
  ../sql/rpl_gtid_owned.cc
  ../sql/rpl_gtid_execution.cc
  ../sql/rpl_gtid_mutex_cond_array.cc
  ../sql/rpl_gtid_persist.cc
  ../sql/rpl_table_access.cc
  ../sql/rpl_context.cc
  ../sql/rpl_trx_boundary_parser.cc
  ${IMPORTED_SOURCES}
)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)
ADD_COMPILE_FLAGS(
  ../sql/debug_sync.cc
  ../sql/handler.cc
  ../sql/geometry_rtree.cc
  ../sql/item_geofunc.cc
  ../sql/item_geofunc_buffer.cc
  ../sql/item_geofunc_internal.cc
  ../sql/item_geofunc_relchecks.cc
  ../sql/item_geofunc_relchecks_bgwrap.cc
  ../sql/item_geofunc_setops.cc
  ../sql/item_json_func.cc
  ../sql/spatial.cc
  COMPILE_FLAGS -I${BOOST_PATCHES_DIR} -I${BOOST_INCLUDE_DIR}
)

ADD_COMPILE_FLAGS(
  ../extra/lz4/xxhash.c
  COMPILE_FLAGS -DXXH_NAMESPACE=MY_
)

# Fixes "C1128: number of sections exceeded object file format limit" in MSVC /MD
# The flag /bigobj is not added if the build is not /MD
IF(WIN32 AND CMAKE_SIZEOF_VOID_P MATCHES 8)
  FOREACH(flag 
     CMAKE_C_FLAGS_RELEASE    CMAKE_C_FLAGS_RELWITHDEBINFO 
     CMAKE_C_FLAGS_DEBUG      CMAKE_C_FLAGS_DEBUG_INIT 
     CMAKE_CXX_FLAGS_RELEASE  CMAKE_CXX_FLAGS_RELWITHDEBINFO
     CMAKE_CXX_FLAGS_DEBUG    CMAKE_CXX_FLAGS_DEBUG_INIT)
    
     STRING(FIND ${${flag}} "/MD" POS)
     IF(NOT (${POS} MATCHES "-1"))
       ADD_COMPILE_FLAGS(../sql/item_geofunc_setops.cc COMPILE_FLAGS "/bigobj")
       BREAK()
     ENDIF()  
  ENDFOREACH()
ENDIF()

# Boost source has unused local typedefs.
MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-local-typedefs" HAVE_NO_UNUSED_TYPEDEFS)
IF(HAVE_NO_UNUSED_TYPEDEFS)
  ADD_COMPILE_FLAGS(
    ../sql/debug_sync.cc
    ../sql/handler.cc
    ../sql/geometry_rtree.cc
    ../sql/item_geofunc.cc
    ../sql/item_geofunc_buffer.cc
    ../sql/item_geofunc_internal.cc
    ../sql/item_geofunc_relchecks.cc
    ../sql/item_geofunc_relchecks_bgwrap.cc
    ../sql/item_geofunc_setops.cc
    ../sql/item_json_func.cc
    ../sql/spatial.cc
    COMPILE_FLAGS "-Wno-unused-local-typedefs"
  )
ENDIF()

# Segfault in boost::geometry code at high optimization levels
IF(CMAKE_CXX_COMPILER_ID MATCHES "SunPro")
  STRING(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKEBT)
  IF(CMAKEBT MATCHES "^REL")
    ADD_COMPILE_FLAGS(../sql/item_geofunc_buffer.cc COMPILE_FLAGS "-xO2")
  ENDIF()
ENDIF()

# Boost source has misleading indentation
MY_CHECK_CXX_COMPILER_FLAG("-Wmisleading-indentation"
                           HAVE_MISLEADING_INDENTATION)
IF(HAVE_MISLEADING_INDENTATION)
  ADD_COMPILE_FLAGS(
    ../sql/geometry_rtree.cc
    ../sql/item_geofunc.cc
    ../sql/item_geofunc_buffer.cc
    ../sql/item_geofunc_internal.cc
    ../sql/item_geofunc_relchecks.cc
    ../sql/item_geofunc_relchecks_bgwrap.cc
    ../sql/item_geofunc_setops.cc    
    COMPILE_FLAGS "-Wno-misleading-indentation")
ENDIF()

# gcc 7.2.0 complains:
# boost/geometry/algorithms/detail/expand/indexed.hpp:81
# *((void*)& box +24) may be used uninitialized
MY_CHECK_CXX_COMPILER_FLAG("-Wmaybe-uninitialized" HAVE_MAYBE_UNINITIALIZED)
IF(HAVE_MAYBE_UNINITIALIZED)
  ADD_COMPILE_FLAGS(
    ../sql/geometry_rtree.cc
    COMPILE_FLAGS "-Wno-maybe-uninitialized")
ENDIF()

# Handle out-of-source build from source package with possibly broken 
# bison. Copy bison output to from source to build directory, if not already 
# there
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/bison.cmake)
COPY_BISON_OUTPUT(
  ${CMAKE_SOURCE_DIR}/sql/sql_yacc.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_yacc.h
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h
)

COPY_BISON_OUTPUT(
  ${CMAKE_SOURCE_DIR}/sql/sql_hints.yy.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_hints.yy.h
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.h
)

RUN_BISON(
  ${CMAKE_SOURCE_DIR}/sql/sql_yacc.yy 
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.h
  MYSQL
)

RUN_BISON(
  ${CMAKE_SOURCE_DIR}/sql/sql_hints.yy
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.cc
  ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.h
  HINT_PARSER_
)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex_token.h
  COMMAND gen_lex_token > lex_token.h
  DEPENDS gen_lex_token
)

SET_SOURCE_FILES_PROPERTIES(
  ${CMAKE_CURRENT_BINARY_DIR}/sql/lex_token.h
  PROPERTIES GENERATED 1)

SET_SOURCE_FILES_PROPERTIES(
  ${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
  ${CMAKE_SOURCE_DIR}/sql/sql_digest.cc
  PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex_token.h
)

ADD_CUSTOM_TARGET(GenYaccEmbeddedSource DEPENDS ${GEN_YACC_SOURCES})

ADD_CONVENIENCE_LIBRARY(sql_embedded ${SQL_EMBEDDED_SOURCES})
DTRACE_INSTRUMENT(sql_embedded)
ADD_DEPENDENCIES(sql_embedded GenError GenServerSource GenYaccEmbeddedSource)

# On Windows, static embedded server library is called mysqlserver.lib
# On Unix, it is libmysqld.a
IF(WIN32)
  SET(MYSQLSERVER_OUTPUT_NAME mysqlserver)
ELSE()
  SET(MYSQLSERVER_OUTPUT_NAME mysqld)
ENDIF()


SET(LIBS 
  dbug strings regex mysys mysys_ssl vio
  ${ZLIB_LIBRARY} ${SSL_LIBRARIES}
  ${LIBCRYPT} ${LIBDL}
  ${MYSQLD_STATIC_EMBEDDED_PLUGIN_LIBS}
  sql_embedded
)

# Some storage engine were compiled for embedded specifically
# (with corresponding target ${engine}_embedded)
SET(EMBEDDED_LIBS)
FOREACH(LIB ${LIBS})
  LIST(FIND KNOWN_CONVENIENCE_LIBRARIES ${LIB}_embedded FOUNDIT)
  IF(FOUNDIT GREATER -1)
    LIST(APPEND EMBEDDED_LIBS ${LIB}_embedded)
  ELSE()
    LIST(APPEND EMBEDDED_LIBS ${LIB})
  ENDIF()
ENDFOREACH()

LIST(REMOVE_DUPLICATES EMBEDDED_LIBS)

MERGE_CONVENIENCE_LIBRARIES(mysqlserver ${EMBEDDED_LIBS} 
  OUTPUT_NAME ${MYSQLSERVER_OUTPUT_NAME} COMPONENT Embedded)

# Visual Studio users need debug  static library 
IF(MSVC)
 INSTALL_DEBUG_TARGET(mysqlserver DESTINATION ${INSTALL_LIBDIR}/debug)
ENDIF()

IF(UNIX)
 INSTALL_DEBUG_TARGET(mysqlserver DESTINATION ${INSTALL_LIBDIR} RENAME
   libmysqld-debug.a)
ENDIF()

IF(MSVC AND NOT DISABLE_SHARED)
  MERGE_LIBRARIES_SHARED(libmysqld ${LIBS} EXPORTS ${CLIENT_API_FUNCTIONS}
  COMPONENT Embedded)
ENDIF()

OPTION(WITH_EMBEDDED_SHARED_LIBRARY
  "Generate shared version of embedded library (in addition to the static one)"
  OFF)
IF(WITH_EMBEDDED_SHARED_LIBRARY)
  MERGE_LIBRARIES_SHARED(libmysqld
    mysqlserver
    EXPORTS ${CLIENT_API_FUNCTIONS}
    OUTPUT_NAME "mysqld"
    COMPONENT Embedded)
  SET_TARGET_PROPERTIES(libmysqld PROPERTIES
    VERSION "${SHARED_LIB_MAJOR_VERSION}.${SHARED_LIB_MINOR_VERSION}.0"
    SOVERSION "${SHARED_LIB_MAJOR_VERSION}"
    )

  GET_TARGET_PROPERTY(libmysqld_link_flags libmysqld LINK_FLAGS)
  IF(NOT libmysqld_link_flags)
    SET(libmysqld_link_flags)
  ENDIF()
  SET(libmysqld_link_flags
    "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${libmysqld_link_flags}")
  SET_TARGET_PROPERTIES(libmysqld
    PROPERTIES LINK_FLAGS "${libmysqld_link_flags}")

  IF(LINK_FLAG_NO_UNDEFINED)
    SET(libmysqld_link_flags
      "${libmysqld_link_flags} ${LINK_FLAG_NO_UNDEFINED}")
    SET_TARGET_PROPERTIES(libmysqld
      PROPERTIES LINK_FLAGS "${libmysqld_link_flags}")
  ENDIF()

  SET_TARGET_PROPERTIES(libmysqld PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  SET_TARGET_PROPERTIES(mysqlserver PROPERTIES CLEAN_DIRECT_OUTPUT 1)
ENDIF()
