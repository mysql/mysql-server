# Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.
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

ADD_DEFINITIONS(-DMYSQL_SERVER -DEMBEDDED_LIBRARY -DNO_EMBEDDED_ACCESS_CHECKS)

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
  libmysqld.cc
  ${GEN_SOURCES}
  ${GEN_YACC_SOURCES}
  ../extra/lz4/xxhash.c
  ../client/get_password.c
  ../libmysql/errmsg.cc
  ../libmysql/libmysql.cc
  ../sql-common/client.cc
  ../sql-common/client_plugin.c
  ../sql-common/sql_string.cc
  ../libbinlogevents/src/statement_events.cpp
  ../libbinlogevents/src/control_events.cpp
  ../libbinlogevents/src/load_data_events.cpp
  ../libbinlogevents/src/rows_event.cpp
  ../libbinlogevents/src/binlog_event.cpp
  ../libbinlogevents/src/binary_log_funcs.cpp
  ../libbinlogevents/src/uuid.cpp
  ../sql/binlog.cc 
  ../sql/log_event.cc
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

# Source files that use Boost
SET(LIBMYSQLD_BOOST_SOURCES
  ../sql/debug_sync.cc
  ../sql/handler.cc
  ../sql/geometry_rtree.cc
  ../sql/gis/srs/srs.cc
  ../sql/gis/srs/wkt_parser.cc
  ../sql/item_geofunc.cc
  ../sql/item_geofunc_buffer.cc
  ../sql/item_geofunc_internal.cc
  ../sql/item_geofunc_relchecks.cc
  ../sql/item_geofunc_relchecks_bgwrap.cc
  ../sql/item_geofunc_setops.cc
  ../sql/item_json_func.cc
  ../sql/spatial.cc
)  

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)
ADD_COMPILE_FLAGS(${LIBMYSQLD_BOOST_SOURCES}
  COMPILE_FLAGS -I${BOOST_PATCHES_DIR} -I${BOOST_INCLUDE_DIR}
)

# warning C4348
# boost::spirit::terminal<boost::spirit::tag::lit>::result_helper': 
#       redefinition of default parameter
IF(WIN32)
  ADD_COMPILE_FLAGS(
    ../sql/gis/srs/srs.cc
    ../sql/gis/srs/wkt_parser.cc
    COMPILE_FLAGS "/wd4348")
ENDIF()

# warning 4244 4267
# conversion from 'coordinate_type' to 'const promoted_type
# conversion from 'boost::geometry::signed_size_type' to 'int'
# etc. etc.
IF(WIN32)
  ADD_COMPILE_FLAGS(
    ../sql/item_geofunc.cc
    ../sql/item_geofunc_buffer.cc
    ../sql/item_geofunc_internal.cc
    ../sql/item_geofunc_setops.cc
    COMPILE_FLAGS "/wd4244" "/wd4267")
ENDIF()

ADD_COMPILE_FLAGS(
  ../extra/lz4/xxhash.c
  COMPILE_FLAGS -DXXH_NAMESPACE=MY_
)

# Fixes "C1128: number of sections exceeded object file format limit" in MSVC
IF(WIN32)
  ADD_COMPILE_FLAGS(
    ../sql/item_geofunc.cc
    ../sql/item_geofunc_setops.cc
    COMPILE_FLAGS "/bigobj")
ENDIF()

#
# Disabling of various compiler warnings for GCC/Clang
#

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Common for all versions of GCC/Clang

  # Bison output and Boost source use undefined symbols in #if checks
  ADD_COMPILE_FLAGS(${CMAKE_CURRENT_BINARY_DIR}/sql_yacc.cc
                    ${CMAKE_CURRENT_BINARY_DIR}/sql_hints.yy.cc
                    ${LIBMYSQLD_BOOST_SOURCES}
                    COMPILE_FLAGS "-Wno-undef")
  # Boost source compares signed and unsigned integer expressions
  ADD_COMPILE_FLAGS(../sql/gis/srs/wkt_parser.cc
                    COMPILE_FLAGS "-Wno-sign-compare")
  # Boost source use auto_ptr, deprecated in C++11
  ADD_COMPILE_FLAGS(${LIBMYSQLD_BOOST_SOURCES}
                    COMPILE_FLAGS "-Wno-deprecated-declarations")

  # Only for some versions of GCC/Clang

  MY_CHECK_CXX_COMPILER_FLAG("-Wunused-local-typedefs" HAVE_UNUSED_TYPEDEFS)
  IF(HAVE_UNUSED_TYPEDEFS)
    # Boost source has unused local typedefs.
    ADD_COMPILE_FLAGS(${LIBMYSQLD_BOOST_SOURCES}
                      COMPILE_FLAGS "-Wno-unused-local-typedefs")
  ENDIF()
  
  MY_CHECK_CXX_COMPILER_FLAG("-Wlogical-op" HAVE_LOGICAL_OP)
  IF(HAVE_LOGICAL_OP)
    # Boost source has logical && with non-zero constant
    ADD_COMPILE_FLAGS(${LIBMYSQLD_BOOST_SOURCES}
                      COMPILE_FLAGS "-Wno-logical-op")
  ENDIF()

  MY_CHECK_CXX_COMPILER_FLAG("-Wplacement-new" HAVE_PLACEMENT_NEW_WARNING)
  IF(HAVE_PLACEMENT_NEW_WARNING)
    # Boost.Spirit causes a warning about placement new on GCC 6. Expected
    # to be fixed upstream by https://github.com/boostorg/function/pull/9.
    ADD_COMPILE_FLAGS(../sql/gis/srs/wkt_parser.cc
                      COMPILE_FLAGS "-Wno-placement-new")
  ENDIF()

  MY_CHECK_CXX_COMPILER_FLAG("-Wmisleading-indentation"
                             HAVE_MISLEADING_INDENTATION)
  IF(HAVE_MISLEADING_INDENTATION)
    # Boost source has misleading indentation
    ADD_COMPILE_FLAGS(${LIBMYSQLD_BOOST_SOURCES}
                      COMPILE_FLAGS "-Wno-misleading-indentation")
  ENDIF()
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
  ${MYSQLD_STATIC_EMBEDDED_COMPONENT_LIBS}
  sql_embedded
)

# Some storage engine were compiled for embedded specifically
# (with corresponding target ${engine}_embedded)
SET(EMBEDDED_LIBS)
FOREACH(LIB ${LIBS})
  GET_TARGET_PROPERTY(EMBEDDED_LOCATION ${LIB}_embedded LOCATION)
  IF(EMBEDDED_LOCATION)
    LIST(APPEND EMBEDDED_LIBS ${LIB}_embedded)
  ELSE()
    LIST(APPEND EMBEDDED_LIBS ${LIB})
  ENDIF()
ENDFOREACH()

MERGE_LIBRARIES(mysqlserver STATIC ${EMBEDDED_LIBS} 
  OUTPUT_NAME ${MYSQLSERVER_OUTPUT_NAME} COMPONENT Embedded)

# Visual Studio users need debug  static library 
IF(MSVC)
 INSTALL_DEBUG_TARGET(mysqlserver DESTINATION ${INSTALL_LIBDIR}/debug)
ENDIF()

IF(UNIX)
 INSTALL_DEBUG_TARGET(mysqlserver DESTINATION ${INSTALL_LIBDIR} RENAME
  ${CMAKE_STATIC_LIBRARY_PREFIX}mysqld-debug)
ENDIF()

IF(MSVC AND NOT DISABLE_SHARED)
  MERGE_LIBRARIES(libmysqld SHARED ${LIBS} EXPORTS ${CLIENT_API_FUNCTIONS}
  COMPONENT Embedded)
ENDIF()

OPTION(WITH_EMBEDDED_SHARED_LIBRARY
  "Generate shared version of embedded library (in addition to the static one)"
  OFF)
IF(WITH_EMBEDDED_SHARED_LIBRARY)
  MERGE_LIBRARIES(libmysqld SHARED
    ${EMBEDDED_LIBS} ${EMBEDDED_LIBS}
    EXPORTS ${CLIENT_API_FUNCTIONS}
    OUTPUT_NAME "mysqld"
    COMPONENT Embedded)
  SET_TARGET_PROPERTIES(libmysqld PROPERTIES
    VERSION "${SHARED_LIB_MAJOR_VERSION}.${SHARED_LIB_MINOR_VERSION}.0"
    SOVERSION "${SHARED_LIB_MAJOR_VERSION}"
    )

  IF(LINK_FLAG_NO_UNDEFINED)
    SET(libmysqld_link_flags
      "${libmysqld_link_flags} ${LINK_FLAG_NO_UNDEFINED}")
    SET_TARGET_PROPERTIES(libmysqld
      PROPERTIES LINK_FLAGS "${libmysqld_link_flags}")
  ENDIF()

  SET_TARGET_PROPERTIES(libmysqld PROPERTIES CLEAN_DIRECT_OUTPUT 1)
ENDIF()
