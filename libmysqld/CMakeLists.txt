# Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.
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
  ${CMAKE_SOURCE_DIR}/libmysqld
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_BINARY_DIR}/sql 
  ${CMAKE_SOURCE_DIR}/regex
  ${ZLIB_INCLUDE_DIR}
  ${SSL_INCLUDE_DIRS}
  ${SSL_INTERNAL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/sql/backup
)

SET(GEN_SOURCES
  ${CMAKE_BINARY_DIR}/sql/sql_yacc.h 
  ${CMAKE_BINARY_DIR}/sql/sql_yacc.cc
  ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc
  ${CMAKE_BINARY_DIR}/sql/lex_hash.h 
)

SET_SOURCE_FILES_PROPERTIES(${GEN_SOURCES} PROPERTIES GENERATED TRUE)

FOREACH(file ${SQL_EXPORTED_SOURCES})
  LIST(APPEND IMPORTED_SOURCES "../sql/${file}")
ENDFOREACH()

SET(SQL_EMBEDDED_SOURCES
  emb_qcache.cc
  lib_sql.cc 
  libmysqld.c
  ${GEN_SOURCES}
  ${MYSYS_LIBWRAP_SOURCE}
  ../client/get_password.c
  ../libmysql/errmsg.c
  ../libmysql/libmysql.c
  ../sql-common/client.c
  ../sql-common/client_plugin.c
  ../sql-common/my_time.c 
  ../sql-common/my_user.c
  ../sql-common/pack.c
  ../sql/binlog.cc 
  ../sql/event_parse_data.cc
  ../sql/hash_filo.cc
  ../sql/log_event.cc
  ../sql/rpl_filter.cc
  ../sql/rpl_injector.cc
  ../sql/rpl_record.cc
  ../sql/rpl_reporting.cc
  ../sql/rpl_utility.cc
  ../sql/uuid.cc
  ../sql/rpl_gtid_misc.cc
  ../sql/rpl_gtid_sid_map.cc
  ../sql/rpl_gtid_set.cc
  ../sql/rpl_gtid_specification.cc
  ../sql/rpl_gtid_state.cc
  ../sql/rpl_gtid_owned.cc
  ../sql/rpl_gtid_cache.cc
  ../sql/rpl_gtid_execution.cc
  ../sql/rpl_gtid_mutex_cond_array.cc
  ${IMPORTED_SOURCES}
)


ADD_CONVENIENCE_LIBRARY(sql_embedded ${SQL_EMBEDDED_SOURCES})
DTRACE_INSTRUMENT(sql_embedded)
ADD_DEPENDENCIES(sql_embedded GenError GenServerSource)

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
  ${LIBWRAP} ${LIBCRYPT} ${LIBDL}
  ${MYSQLD_STATIC_PLUGIN_LIBS}
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
    mysqlserver
    EXPORTS ${CLIENT_API_FUNCTIONS}
    OUTPUT_NAME "mysqld"
    COMPONENT Embedded)
  SET_TARGET_PROPERTIES(libmysqld PROPERTIES
    VERSION "${SHARED_LIB_MAJOR_VERSION}.${SHARED_LIB_MINOR_VERSION}.0"
    SOVERSION "${SHARED_LIB_MAJOR_VERSION}"
    )

  GET_TARGET_PROPERTY(libmysqld_link_flags libmysqld LINK_FLAGS)
  IF(NOT libmysqld_link_flag)
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
