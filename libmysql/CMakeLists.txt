# Copyright (c) 2006, 2013, Oracle and/or its affiliates.
# Copyright (c) 2009, 2013, SkySQL Ab.
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
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/strings
  ${SSL_INCLUDE_DIRS}
  ${SSL_INTERNAL_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR})
ADD_DEFINITIONS(${SSL_DEFINES})

SET(CLIENT_API_FUNCTIONS_5_1
get_tty_password
mysql_thread_end
mysql_thread_init
myodbc_remove_escape
mysql_affected_rows
mysql_autocommit
mysql_stmt_bind_param
mysql_stmt_bind_result
mysql_change_user
mysql_character_set_name
mysql_close
mysql_commit
mysql_data_seek
mysql_debug
mysql_dump_debug_info
mysql_eof
mysql_errno
mysql_error
mysql_escape_string
mysql_hex_string
mysql_stmt_execute
mysql_stmt_fetch
mysql_stmt_fetch_column
mysql_fetch_field
mysql_fetch_field_direct
mysql_fetch_fields
mysql_fetch_lengths
mysql_fetch_row
mysql_field_count
mysql_field_seek
mysql_field_tell
mysql_free_result
mysql_get_parameters
mysql_get_client_info
mysql_get_host_info
mysql_get_proto_info
mysql_get_server_info
mysql_get_client_version
mysql_get_ssl_cipher
mysql_info
mysql_init
mysql_insert_id
mysql_kill
mysql_set_server_option
mysql_list_dbs
mysql_list_fields
mysql_list_processes
mysql_list_tables
mysql_more_results
mysql_next_result
mysql_num_fields
mysql_num_rows
mysql_options
mysql_stmt_param_count
mysql_stmt_param_metadata
mysql_ping
mysql_stmt_result_metadata
mysql_query
mysql_read_query_result
mysql_real_connect
mysql_real_escape_string
mysql_real_query
mysql_refresh
mysql_rollback
mysql_row_seek
mysql_row_tell
mysql_select_db
mysql_stmt_send_long_data
mysql_send_query
mysql_shutdown
mysql_ssl_set
mysql_stat
mysql_stmt_affected_rows
mysql_stmt_close
mysql_stmt_reset
mysql_stmt_data_seek
mysql_stmt_errno
mysql_stmt_error
mysql_stmt_free_result
mysql_stmt_num_rows
mysql_stmt_row_seek
mysql_stmt_row_tell
mysql_stmt_store_result
mysql_store_result
mysql_thread_id
mysql_thread_safe
mysql_use_result
mysql_warning_count
mysql_stmt_sqlstate
mysql_sqlstate
mysql_get_server_version
mysql_stmt_prepare
mysql_stmt_init
mysql_stmt_insert_id
mysql_stmt_attr_get
mysql_stmt_attr_set
mysql_stmt_field_count
mysql_set_local_infile_default
mysql_set_local_infile_handler
mysql_embedded
mysql_server_init
mysql_server_end
mysql_set_character_set
mysql_get_character_set_info
# These are documented in Paul DuBois' MySQL book,
# so we treat them as part of the de-facto API.
handle_options
load_defaults
free_defaults
my_print_help
)

SET(CLIENT_API_FUNCTIONS_5_5
mysql_stmt_next_result
# Client plugins
mysql_client_find_plugin
mysql_client_register_plugin
mysql_load_plugin
mysql_load_plugin_v
mysql_plugin_options
# Async API
mysql_get_timeout_value
mysql_get_timeout_value_ms
mysql_get_socket
mysql_autocommit_cont
mysql_autocommit_start
mysql_change_user_cont
mysql_change_user_start
mysql_close_cont
mysql_close_start
mysql_commit_cont
mysql_commit_start
mysql_dump_debug_info_cont
mysql_dump_debug_info_start
mysql_fetch_row_cont
mysql_fetch_row_start
mysql_free_result_cont
mysql_free_result_start
mysql_kill_cont
mysql_kill_start
mysql_list_dbs_cont
mysql_list_dbs_start
mysql_list_fields_cont
mysql_list_fields_start
mysql_list_processes_cont
mysql_list_processes_start
mysql_list_tables_cont
mysql_list_tables_start
mysql_next_result_cont
mysql_next_result_start
mysql_ping_cont
mysql_ping_start
mysql_query_cont
mysql_query_start
mysql_read_query_result_cont
mysql_read_query_result_start
mysql_real_connect_cont
mysql_real_connect_start
mysql_real_query_cont
mysql_real_query_start
mysql_refresh_cont
mysql_refresh_start
mysql_rollback_cont
mysql_rollback_start
mysql_select_db_cont
mysql_select_db_start
mysql_send_query_cont
mysql_send_query_start
mysql_set_character_set_cont
mysql_set_character_set_start
mysql_set_server_option_cont
mysql_set_server_option_start
mysql_shutdown_cont
mysql_shutdown_start
mysql_stat_cont
mysql_stat_start
mysql_stmt_close_cont
mysql_stmt_close_start
mysql_stmt_execute_cont
mysql_stmt_execute_start
mysql_stmt_fetch_cont
mysql_stmt_fetch_start
mysql_stmt_free_result_cont
mysql_stmt_free_result_start
mysql_stmt_next_result_cont
mysql_stmt_next_result_start
mysql_stmt_prepare_cont
mysql_stmt_prepare_start
mysql_stmt_reset_cont
mysql_stmt_reset_start
mysql_stmt_send_long_data_cont
mysql_stmt_send_long_data_start
mysql_stmt_store_result_cont
mysql_stmt_store_result_start
mysql_store_result_cont
mysql_store_result_start
#dynamic columns api
dynamic_column_create
dynamic_column_create_many
dynamic_column_update
dynamic_column_update_many
dynamic_column_delete
dynamic_column_exists
dynamic_column_list
dynamic_column_get
dynamic_column_prepare_decimal
)

SET(CLIENT_API_FUNCTIONS
  ${CLIENT_API_FUNCTIONS_5_1}
  ${CLIENT_API_FUNCTIONS_5_5}
  CACHE INTERNAL
  "Client functions"
)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux") 
  # When building RPM, or DEB package on Debian, use ELF symbol versioning
  # for compatibility with distribution packages, so client shared library can 
  # painlessly replace the one supplied by the distribution.
  
  # Also list of exported symbols in distributions may differ from what is
  # considered official API. Define CLIENT_API_5_1_EXTRA for the set of
  # symbols, that required to be exported on different platforms.
  
  # Fedora & Co declared following functions as part of API 
  SET(CLIENT_API_5_1_EXTRA
    # why does Fedora export these?
    _fini
    _init
    my_init

    #  mysql-connector-odbc requires these
    mysql_default_charset_info
    mysql_get_charset
    mysql_get_charset_by_csname
    mysql_net_realloc

    # PHP's mysqli.so requires this (via the ER() macro)
    mysql_client_errors

    # Also export the non-renamed variants
    # (in case someone wants to rebuild mysqli-php or something similar)
    # See MDEV-4127
    default_charset_info
    get_charset
    get_charset_by_csname
    net_realloc
    client_errors

    # pure-ftpd requires this
    my_make_scrambled_password

    # hydra requires this
    scramble

    # ODB requires this: https://bugzilla.redhat.com/show_bug.cgi?id=846602
    THR_KEY_mysys

    # DBD::mysql requires this
    is_prefix
  )  

  # Linker script to version symbols in Fedora- and Debian- compatible way, MDEV-5529
  SET(VERSION_SCRIPT_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/libmysql_versions.ld.in)

  # Generate version script.  
  # Create semicolon separated lists of functions to export from 
  # Since RPM packages use separate versioning for 5.1 API
  # and 5.5 API (libmysqlclient_16 vs libmysqlclient_18), 
  # we need 2 lists.
  SET (CLIENT_API_5_1_LIST)
  SET (CLIENT_API_5_1_ALIASES)
  FOREACH (f ${CLIENT_API_FUNCTIONS_5_1} ${CLIENT_API_5_1_EXTRA})
    SET(CLIENT_API_5_1_LIST "${CLIENT_API_5_1_LIST}\t${f};\n")
    SET(CLIENT_API_5_1_ALIASES "${CLIENT_API_5_1_ALIASES}\"${f}@libmysqlclient_16\" = ${f};\n")
  ENDFOREACH()
      
  SET (CLIENT_API_5_5_LIST)
  FOREACH (f ${CLIENT_API_FUNCTIONS_5_5})
    SET(CLIENT_API_5_5_LIST "${CLIENT_API_5_5_LIST}\t${f};\n")
  ENDFOREACH()

  CONFIGURE_FILE(
    ${VERSION_SCRIPT_TEMPLATE}
    ${CMAKE_CURRENT_BINARY_DIR}/libmysql_versions.ld
    @ONLY@
  )
  SET(VERSION_SCRIPT_LINK_FLAGS 
    "-Wl,${CMAKE_CURRENT_BINARY_DIR}/libmysql_versions.ld")

ENDIF()


SET(CLIENT_SOURCES
  get_password.c 
  libmysql.c
  errmsg.c
  ../sql-common/client.c 
  ../sql-common/mysql_async.c
  ../sql-common/my_time.c 
  ../sql-common/client_plugin.c 
  ../sql/net_serv.cc
  ../sql-common/pack.c 
  ../sql/password.c
  ${CLIENT_SOURCES_EXTRA}
)
ADD_CONVENIENCE_LIBRARY(clientlib ${CLIENT_SOURCES})
DTRACE_INSTRUMENT(clientlib)
ADD_DEPENDENCIES(clientlib GenError)

SET(LIBS clientlib dbug strings vio mysys ${ZLIB_LIBRARY} ${SSL_LIBRARIES} ${LIBDL})

# Merge several convenience libraries into one big mysqlclient
# and link them together into shared library.
MERGE_LIBRARIES(mysqlclient STATIC ${LIBS} COMPONENT Development)

# Visual Studio users need debug  static library for debug projects
INSTALL_DEBUG_SYMBOLS(clientlib)
IF(MSVC)
 INSTALL_DEBUG_TARGET(mysqlclient DESTINATION ${INSTALL_LIBDIR}/debug)
 INSTALL_DEBUG_TARGET(clientlib DESTINATION ${INSTALL_LIBDIR}/debug)
ENDIF()

IF(UNIX)
  MACRO(GET_VERSIONED_LIBNAME LIBNAME EXTENSION VERSION OUTNAME)
    SET(DOT_VERSION ".${VERSION}")
    IF(DOT_VERSION STREQUAL ".") 
      SET(DOT_VERSION "")
    ENDIF()
    IF(APPLE)
      SET(${OUTNAME} ${LIBNAME}${DOT_VERSION}${EXTENSION})
    ELSE()
      SET(${OUTNAME} ${LIBNAME}${EXTENSION}${DOT_VERSION})
    ENDIF() 
  ENDMACRO()
  INSTALL_SYMLINK(${CMAKE_STATIC_LIBRARY_PREFIX}mysqlclient_r.a mysqlclient ${INSTALL_LIBDIR} Development)
ENDIF()

IF(NOT DISABLE_SHARED)
  MERGE_LIBRARIES(libmysql SHARED ${LIBS} EXPORTS ${CLIENT_API_FUNCTIONS} ${CLIENT_API_5_1_EXTRA} COMPONENT SharedLibraries)
  IF(UNIX)
    # libtool compatability
    IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR APPLE)
      SET(OS_SHARED_LIB_VERSION "${SHARED_LIB_MAJOR_VERSION}")
    ELSEIF(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
      SET(OS_SHARED_LIB_VERSION "${SHARED_LIB_MAJOR_VERSION}.0")
    ELSE()
      SET(OS_SHARED_LIB_VERSION "${SHARED_LIB_MAJOR_VERSION}.0.0")
    ENDIF()
    # Name of shared library is mysqlclient on Unix
    SET_TARGET_PROPERTIES(libmysql PROPERTIES 
      OUTPUT_NAME mysqlclient 
      VERSION "${OS_SHARED_LIB_VERSION}" 
      SOVERSION "${SHARED_LIB_MAJOR_VERSION}")
    IF(LINK_FLAG_NO_UNDEFINED OR VERSION_SCRIPT_LINK_FLAGS)
      GET_TARGET_PROPERTY(libmysql_link_flags libmysql LINK_FLAGS)
      SET_TARGET_PROPERTIES(libmysql PROPERTIES LINK_FLAGS 
        "${libmysql_link_flags} ${LINK_FLAG_NO_UNDEFINED} ${VERSION_SCRIPT_LINK_FLAGS}")
    ENDIF() 
    # clean direct output needs to be set several targets have the same name
    #(mysqlclient in this case)
    SET_TARGET_PROPERTIES(mysqlclient PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    SET_TARGET_PROPERTIES(libmysql PROPERTIES CLEAN_DIRECT_OUTPUT 1)

    # Install links to libmysqlclient.so (client_r)
    GET_VERSIONED_LIBNAME(
      "${CMAKE_SHARED_LIBRARY_PREFIX}mysqlclient_r"
      "${CMAKE_SHARED_LIBRARY_SUFFIX}"
      ""
      linkname)
    INSTALL_SYMLINK(${linkname} libmysql ${INSTALL_LIBDIR} SharedLibraries)
    SET(OS_SHARED_LIB_SYMLINKS "${SHARED_LIB_MAJOR_VERSION}" "${OS_SHARED_LIB_VERSION}")
    LIST(REMOVE_DUPLICATES OS_SHARED_LIB_SYMLINKS)
    FOREACH(ver ${OS_SHARED_LIB_SYMLINKS})
      GET_VERSIONED_LIBNAME(
        "${CMAKE_SHARED_LIBRARY_PREFIX}mysqlclient_r"
        "${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${ver}"
        linkname)
      INSTALL_SYMLINK(${linkname} libmysql ${INSTALL_LIBDIR} SharedLibraries)
    ENDFOREACH()
  ENDIF()
ENDIF()
