# Copyright (c) 2006, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

ADD_WSHADOW_WARNING()

SET(CLIENT_API_FUNCTIONS
  mysql_affected_rows
  mysql_autocommit
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
  mysql_fetch_field
  mysql_fetch_field_direct
  mysql_fetch_fields
  mysql_fetch_lengths
  mysql_fetch_row
  mysql_field_count
  mysql_field_seek
  mysql_field_tell
  mysql_free_result
  mysql_get_client_info
  mysql_get_client_version
  mysql_get_host_info
  mysql_get_proto_info
  mysql_get_server_info
  mysql_get_ssl_cipher
  mysql_hex_string
  mysql_info
  mysql_init
  mysql_insert_id
  mysql_kill
  mysql_list_dbs
  mysql_list_fields
  mysql_list_processes
  mysql_list_tables
  mysql_more_results
  mysql_next_result
  mysql_num_fields
  mysql_num_rows
  mysql_options
  mysql_ping
  mysql_query
  mysql_set_server_option
  mysql_stmt_bind_param
  mysql_stmt_bind_named_param
  mysql_stmt_bind_result
  mysql_stmt_execute
  mysql_stmt_fetch
  mysql_stmt_fetch_column
  mysql_stmt_param_count
  mysql_stmt_param_metadata
  mysql_stmt_result_metadata
  mysql_thread_end
  mysql_thread_init
  # We need to have document for this api
  mysql_read_query_result
  mysql_real_connect
  mysql_real_escape_string
  mysql_real_escape_string_quote
  mysql_real_query
  mysql_refresh
  mysql_rollback
  mysql_row_seek
  mysql_row_tell
  mysql_select_db
  mysql_stmt_send_long_data
  # We need to have document for this api
  mysql_client_find_plugin
  mysql_client_register_plugin
  mysql_get_character_set_info
  mysql_get_option
  mysql_get_server_version
  mysql_load_plugin
  mysql_load_plugin_v
  mysql_options4
  mysql_plugin_options
  mysql_reset_connection
  mysql_reset_server_public_key
  mysql_result_metadata
  mysql_send_query
  mysql_server_end
  mysql_server_init
  mysql_session_track_get_first
  mysql_session_track_get_next
  mysql_set_character_set
  mysql_set_local_infile_default
  mysql_set_local_infile_handler
  mysql_shutdown
  mysql_sqlstate
  mysql_ssl_set
  mysql_stat
  mysql_stmt_affected_rows
  mysql_stmt_attr_get
  mysql_stmt_attr_set
  mysql_stmt_close
  mysql_stmt_data_seek
  mysql_stmt_errno
  mysql_stmt_error
  mysql_stmt_field_count
  mysql_stmt_free_result
  mysql_stmt_init
  mysql_stmt_insert_id
  mysql_stmt_next_result
  mysql_stmt_num_rows
  mysql_stmt_prepare
  mysql_stmt_reset
  mysql_stmt_row_seek
  mysql_stmt_row_tell
  mysql_stmt_sqlstate
  mysql_stmt_store_result
  mysql_store_result
  mysql_thread_id
  mysql_thread_safe
  mysql_use_result
  mysql_warning_count
  mysql_real_connect_dns_srv
  mysql_bind_param
  mysql_plugin_get_option
  mysql_get_ssl_session_reused
  mysql_get_ssl_session_data
  mysql_free_ssl_session_data
  mysql_binlog_close
  mysql_binlog_fetch
  mysql_binlog_open
  CACHE INTERNAL "Functions exported by client API"
)

# Below list will have all the undocumented C API symbols but still exported.
# Once the decision is taken to have documentation we need to move them to
# CLIENT_API_FUNCTIONS list.
SET(CLIENT_API_FUNCTIONS_UNDOCUMENTED
  get_tty_password
  # my_load_defaults is a wrapper for load_defaults and it is not documented.
  # We will have a FR to replace this for decent name/functionality and
  # document it.
  my_load_defaults
  handle_options
  # pure-ftpd 1.0.42 needs either my_make_scrambled_password (preferred)
  # or make_scrambled_password.
  my_make_scrambled_password

  CACHE INTERNAL "Undocumented functions exported by client API"

)

# Below list will have all nonblocking C APIs
SET(CLIENT_API_NONBLOCKING_FUNCTIONS
  mysql_fetch_row_nonblocking
  mysql_free_result_nonblocking
  mysql_next_result_nonblocking
  mysql_real_connect_nonblocking
  mysql_real_query_nonblocking
  mysql_send_query_nonblocking
  mysql_store_result_nonblocking
  mysql_get_connect_nonblocking_stage
  mysql_reset_connection_nonblocking

  CACHE INTERNAL "Nonblocking functions exported by client API"
)

SET(CLIENT_SOURCES
  libmysql.cc
  errmsg.cc
  dns_srv.cc
  ../sql-common/client.cc
  ../sql-common/client_plugin.cc
  ../sql-common/client_authentication.cc
  ../sql-common/compression.cc
  ../sql-common/get_password.cc
  ../sql-common/net_serv.cc
  ../sql-common/bind_params.cc
  ../sql/auth/password.cc
  ../sql/auth/sha2_password_common.cc
)

IF (WIN32 AND OPENSSL_APPLINK_C)
  MY_ADD_COMPILE_DEFINITIONS(
    ../sql-common/client_authentication.cc
    COMPILE_DEFINITIONS HAVE_OPENSSL_APPLINK_C
  )
ENDIF()

#
# Include protocol tracing infrastructure and the test
# trace plugin if enabled by build options.
#
IF (WITH_CLIENT_PROTOCOL_TRACING)

  LIST(APPEND CLIENT_SOURCES mysql_trace.cc)
  ADD_DEFINITIONS(-D CLIENT_PROTOCOL_TRACING)

  IF (WITH_TEST_TRACE_PLUGIN)
    MESSAGE(STATUS "Client library contains the test trace plugin")
    LIST(APPEND CLIENT_SOURCES test_trace_plugin.cc)
    ADD_DEFINITIONS(-D TEST_TRACE_PLUGIN)
  ENDIF (WITH_TEST_TRACE_PLUGIN)

ENDIF (WITH_CLIENT_PROTOCOL_TRACING)

ADD_CONVENIENCE_LIBRARY(clientlib ${CLIENT_SOURCES}
  DEPENDENCIES GenError
  LINK_LIBRARIES ext::zlib
  OpenSSL::SSL OpenSSL::Crypto
  )

SET(LIBS_TO_MERGE
  clientlib mytime strings vio mysys
  )
SET(LIBS_TO_LINK ${LIBDL})

IF(WITH_ZLIB STREQUAL "bundled")
  LIST(APPEND LIBS_TO_MERGE zlib)
ELSE()
  LIST(APPEND LIBS_TO_LINK ext::zlib)
ENDIF()

IF(WITH_ZSTD STREQUAL "bundled")
  LIST(APPEND LIBS_TO_MERGE zstd)
ELSE()
  LIST(APPEND LIBS_TO_LINK ext::zstd)
ENDIF()

LIST(APPEND LIBS_TO_LINK OpenSSL::SSL OpenSSL::Crypto)

IF(WITH_EXT_BACKTRACE)
  LIST(APPEND LIBS_TO_MERGE backtrace)
ENDIF()

UNSET(HAVE_WIN32_DNS_SRV)
UNSET(HAVE_UNIX_DNS_SRV)
SET(HAVE_DNS_SRV 0)
IF(WIN32)
  LIST(APPEND LIBS_TO_LINK dnsapi)
  SET(HAVE_WIN32_DNS_SRV 1 PARENT_SCOPE)
  SET(HAVE_DNS_SRV 1)
  MESSAGE(STATUS "Found Win32 DNS SRV APIs")
ELSEIF(FREEBSD)
  SET(HAVE_DNS_SRV 1)
  SET(HAVE_UNIX_DNS_SRV 1 PARENT_SCOPE)
  MESSAGE(STATUS "BSD built in DNS SRV APIs")
ELSE()
  FIND_LIBRARY(RESOLV_LIBRARY NAMES resolv)
  IF (RESOLV_LIBRARY)
    LIST(APPEND LIBS_TO_LINK ${RESOLV_LIBRARY})
    SET(HAVE_UNIX_DNS_SRV 1 PARENT_SCOPE)
    SET(HAVE_DNS_SRV 1)
    MESSAGE(STATUS "Found Unix DNS SRV APIs")
  ENDIF()
ENDIF()

IF(HAVE_DNS_SRV EQUAL 0)
  MESSAGE(FATAL_ERROR "Can't find neither Win32 nor Unix DNS SRV APIs")
ENDIF()
#
# On Windows platform client library includes the client-side
# Windows Native Authentication plugin.
#
IF(WIN32)
  ADD_DEFINITIONS(-DAUTHENTICATION_WIN)
  ADD_SUBDIRECTORY(authentication_win)
  LIST(APPEND LIBS_TO_MERGE auth_win_client)
ENDIF()

# LDAP authentication SASL client plug-in
ADD_SUBDIRECTORY(authentication_ldap)

# authentication kerberos client plug-in
ADD_SUBDIRECTORY(authentication_kerberos)

# authentication IAM client plug-in
ADD_SUBDIRECTORY(authentication_oci_client)

# Fido and Webauthn clients
ADD_SUBDIRECTORY(fido_client)

# authentication mysql_native_password client plug-in
ADD_SUBDIRECTORY(authentication_native_password)

# authentication openid connect client plug-in
ADD_SUBDIRECTORY(authentication_openid_connect_client)

# Merge several convenience libraries into one big mysqlclient
MERGE_CONVENIENCE_LIBRARIES(mysqlclient ${LIBS_TO_MERGE}
  COMPONENT Development
  LINK_LIBRARIES ${LIBS_TO_LINK}
  )

# Visual Studio users need debug  static library for debug projects
IF(MSVC)
  INSTALL_DEBUG_TARGET(mysqlclient DESTINATION ${INSTALL_LIBDIR}/debug)

  INSTALL_DEBUG_TARGET(auth_win_client DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(clientlib DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(mysys DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(strings DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(vio DESTINATION ${INSTALL_LIBDIR}/debug)
  IF(WITH_ZLIB STREQUAL "bundled")
    INSTALL_DEBUG_TARGET(zlib DESTINATION ${INSTALL_LIBDIR}/debug)
  ENDIF()
  IF(WITH_ZSTD STREQUAL "bundled")
    INSTALL_DEBUG_TARGET(zstd DESTINATION ${INSTALL_LIBDIR}/debug)
  ENDIF()
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
ENDIF()

IF(UNIX)
  SET(OS_SHARED_LIB_VERSION "${SHARED_LIB_MAJOR_VERSION}")
  IF(NOT FREEBSD AND NOT APPLE)
    STRING_APPEND(OS_SHARED_LIB_VERSION
      ".${SHARED_LIB_MINOR_VERSION}.${SHARED_LIB_PATCH_VERSION}")
  ENDIF()
  # Name of shared library is libmysqlclient on Unix
  SET(UNIX_OUTPUT_NAME OUTPUT_NAME mysqlclient)
  SET(UNIX_VERSION     VERSION     "${OS_SHARED_LIB_VERSION}")
  SET(UNIX_SOVERSION   SOVERSION   "${SHARED_LIB_MAJOR_VERSION}")
ENDIF()

# Merge several convenience libraries into one big mysqlclient
# and link them together into shared library.
MERGE_LIBRARIES_SHARED(libmysql ${LIBS_TO_MERGE}
  EXPORTS
  ${CLIENT_API_FUNCTIONS}
  ${CLIENT_API_FUNCTIONS_UNDOCUMENTED}
  ${CLIENT_API_NONBLOCKING_FUNCTIONS}
  COMPONENT SharedLibraries
  LINK_LIBRARIES ${LIBS_TO_LINK}
  ${UNIX_OUTPUT_NAME}
  ${UNIX_SOVERSION}
  ${UNIX_VERSION}
  )

# Downgrade warning for strncat in my_crypt_genhash.
IF((WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO) AND MY_COMPILER_IS_GNU)
  TARGET_LINK_OPTIONS(libmysql PRIVATE -Wno-error=stringop-truncation)
ENDIF()

IF(LINUX_STANDALONE AND KERBEROS_CUSTOM_LIBRARY)
  ADD_DEPENDENCIES(libmysql ${kerberos_target})
  ADD_DEPENDENCIES(mysqlclient ${kerberos_target})
ENDIF()

IF(UNIX)
  IF(LINK_FLAG_Z_DEFS)
    TARGET_LINK_OPTIONS(libmysql PRIVATE LINKER:${LINK_FLAG_Z_DEFS})
  ENDIF()

  IF(LINUX)
    CONFIGURE_FILE(libmysql.ver.in ${CMAKE_CURRENT_BINARY_DIR}/libmysql.ver)
    TARGET_LINK_OPTIONS(libmysql PRIVATE
      LINKER:--version-script=${CMAKE_CURRENT_BINARY_DIR}/libmysql.ver)
  ENDIF()

  # clean direct output needs to be set several targets have the same name
  #(mysqlclient in this case)
  SET_TARGET_PROPERTIES(mysqlclient PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  SET_TARGET_PROPERTIES(libmysql PROPERTIES CLEAN_DIRECT_OUTPUT 1)
ENDIF()

#
#  Basic application for testing linking against dynamic client library.
#

#
# Generate a comma separated list of C API functions which is used
# to initialize api_calls[] array in api_test.c
#
SET(CLIENT_API_FUNCTION_LIST "")
FOREACH(api ${CLIENT_API_FUNCTIONS})
  SET(CLIENT_API_FUNCTION_LIST "${CLIENT_API_FUNCTION_LIST} ${api},")
ENDFOREACH()
FOREACH(api ${CLIENT_API_NONBLOCKING_FUNCTIONS})
  SET(CLIENT_API_FUNCTION_LIST "${CLIENT_API_FUNCTION_LIST} ${api},")
ENDFOREACH()

#
# Generate api_test.c source, substituting @CLIENT_API_FUNCTION_LIST@
# with the list of API functions.
#
CONFIGURE_FILE(api_test.c.in ${CMAKE_CURRENT_BINARY_DIR}/api_test.c)

#
# Note: Compiling this test application will fail if not all symbols
# from @CLIENT_API_FUNCTIONS@ are declared by <mysql.h>. It will fail
# to run if not all of these symbols are exported by the library.
#
MYSQL_ADD_EXECUTABLE(libmysql_api_test
  ${CMAKE_CURRENT_BINARY_DIR}/api_test.c
  LINK_LIBRARIES libmysql ${LIBRT}
  SKIP_INSTALL
  )
# Clang/UBSAN needs this on some platforms.
SET_TARGET_PROPERTIES(libmysql_api_test PROPERTIES LINKER_LANGUAGE CXX)

IF(MY_COMPILER_IS_GNU)
  ADD_COMPILE_FLAGS(
    ${CMAKE_CURRENT_BINARY_DIR}/api_test.c
    COMPILE_FLAGS -Wstrict-prototypes
    )
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(
    ${CMAKE_CURRENT_BINARY_DIR}/api_test.c
    COMPILE_FLAGS ${HAS_WARN_FLAG}
    )
ENDIF()

# Verify that libmysql_api_test runs OK
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libmysql_api_test.out
  DEPENDS libmysql_api_test
  COMMAND libmysql_api_test
  > ${CMAKE_CURRENT_BINARY_DIR}/libmysql_api_test.out
  )
MY_ADD_CUSTOM_TARGET(run_libmysql_api_test ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libmysql_api_test.out
  )
