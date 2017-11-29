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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/strings
  ${SSL_INCLUDE_DIRS}
  ${SSL_INTERNAL_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR})
ADD_DEFINITIONS(${SSL_DEFINES})

SET(CLIENT_API_FUNCTIONS
mysql_thread_end
mysql_thread_init
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
mysql_server_init
mysql_server_end
mysql_set_character_set
mysql_get_character_set_info
mysql_stmt_next_result
# Since its already documented we will keep it. FR will be created to redesign.
my_init
mysql_client_find_plugin
mysql_client_register_plugin
mysql_load_plugin
mysql_load_plugin_v
mysql_options4
mysql_plugin_options
mysql_reset_connection
mysql_get_option
mysql_session_track_get_first
mysql_session_track_get_next

CACHE INTERNAL "Functions exported by client API"

)

# Below lists will have all the undocumented C API symbols but still exported.
# Once the decision is taken to have documentation we need to move them to
# CLIENT_API_FUNCTIONS list.
SET(CLIENT_API_FUNCTIONS_UNDOCUMENTED_20_0
get_tty_password
# my_load_defaults is a wrapper for load_defaults and it is not documented. We
# will have a FR to replace this for decent name/functionality and document it.
my_load_defaults
handle_options

CACHE INTERNAL "Undocumented functions exported by client API in version 20.0"

)

SET(CLIENT_API_FUNCTIONS_UNDOCUMENTED_20_3
  # pure-ftpd 1.0.42 needs either my_make_scrambled_password (preferred)
  # or make_scrambled_password.
  my_make_scrambled_password

  CACHE INTERNAL "Undocumented functions exported by client API in version 20.3"

)

SET(CLIENT_API_FUNCTIONS_UNDOCUMENTED
  ${CLIENT_API_FUNCTIONS_UNDOCUMENTED_20_0}
  ${CLIENT_API_FUNCTIONS_UNDOCUMENTED_20_3}

  CACHE INTERNAL "Undocumented functions exported by client API"

)

SET(CLIENT_SOURCES
  libmysql.c
  errmsg.c
  ../sql-common/client.c 
  ../sql-common/my_time.c 
  ../sql-common/client_plugin.c 
  ../sql-common/client_authentication.cc
  ../sql-common/get_password.c
  ../sql/net_serv.cc
  ../sql-common/pack.c 
  ../sql/auth/password.c
)

IF (WIN32 AND OPENSSL_APPLINK_C)
  SET_SOURCE_FILES_PROPERTIES(
    ../sql-common/client_authentication.cc
    PROPERTIES COMPILE_FLAGS "-DHAVE_OPENSSL_APPLINK_C"
  )
ENDIF()

#
# Include protocol tracing infrastructure and the test
# trace plugin if enabled by build options.
#
IF (WITH_CLIENT_PROTOCOL_TRACING)

  LIST(APPEND CLIENT_SOURCES mysql_trace.c)
  ADD_DEFINITIONS(-D CLIENT_PROTOCOL_TRACING)

  IF (WITH_TEST_TRACE_PLUGIN)
    MESSAGE(STATUS "Client library contains the test trace plugin")
    LIST(APPEND CLIENT_SOURCES test_trace_plugin.cc)
    ADD_DEFINITIONS(-D TEST_TRACE_PLUGIN)
  ENDIF (WITH_TEST_TRACE_PLUGIN)

ENDIF (WITH_CLIENT_PROTOCOL_TRACING)

ADD_CONVENIENCE_LIBRARY(clientlib ${CLIENT_SOURCES})
DTRACE_INSTRUMENT(clientlib)
ADD_DEPENDENCIES(clientlib GenError)

SET(LIBS clientlib dbug strings vio mysys mysys_ssl ${ZLIB_LIBRARY} ${SSL_LIBRARIES} ${LIBDL})

#
# On Windows platform client library includes the client-side 
# Windows Native Authentication plugin.
#
IF(WIN32)
  ADD_DEFINITIONS(-DAUTHENTICATION_WIN)
  ADD_SUBDIRECTORY(authentication_win)
  LIST(APPEND LIBS auth_win_client)
ENDIF()

# LDAP authentication SASL client plugin
MESSAGE(STATUS "Creating LDAP authentication SASL client library.")
ADD_SUBDIRECTORY(authentication_ldap)
  
# Merge several convenience libraries into one big mysqlclient
MERGE_CONVENIENCE_LIBRARIES(mysqlclient ${LIBS} COMPONENT Development)

# Visual Studio users need debug  static library for debug projects
IF(MSVC)
  INSTALL_DEBUG_TARGET(mysqlclient DESTINATION ${INSTALL_LIBDIR}/debug)

  INSTALL_DEBUG_TARGET(auth_win_client DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(clientlib DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(mysys DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(mysys_ssl DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(strings DESTINATION ${INSTALL_LIBDIR}/debug)
  INSTALL_DEBUG_TARGET(vio DESTINATION ${INSTALL_LIBDIR}/debug)
  IF(WITH_SSL STREQUAL "bundled")
    INSTALL_DEBUG_TARGET(taocrypt DESTINATION ${INSTALL_LIBDIR}/debug)
    INSTALL_DEBUG_TARGET(yassl DESTINATION ${INSTALL_LIBDIR}/debug)
  ENDIF()
  IF(WITH_ZLIB STREQUAL "bundled")
    INSTALL_DEBUG_TARGET(zlib DESTINATION ${INSTALL_LIBDIR}/debug)
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

# I we build client-only, we may be using libstlport which will
# be injected into the build tree by the Sun Studio part of the build.

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND DEFINED ENV{PB2WORKDIR})
  INSTALL(FILES ${CMAKE_BINARY_DIR}/libstlport.so
                ${CMAKE_BINARY_DIR}/libstlport.so.1
          DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries OPTIONAL)
ENDIF()

IF(NOT DISABLE_SHARED)
  # Merge several convenience libraries into one big mysqlclient
  # and link them together into shared library.
  MERGE_LIBRARIES_SHARED(libmysql ${LIBS}
    EXPORTS ${CLIENT_API_FUNCTIONS} ${CLIENT_API_FUNCTIONS_UNDOCUMENTED}
    COMPONENT SharedLibraries)
  IF(UNIX)
    # libtool compatability
    IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR APPLE)
      SET(OS_SHARED_LIB_VERSION "${SHARED_LIB_MAJOR_VERSION}")
    ELSE()
      SET(OS_SHARED_LIB_VERSION
        "${SHARED_LIB_MAJOR_VERSION}.${SHARED_LIB_MINOR_VERSION}.${SHARED_LIB_PATCH_VERSION}")
    ENDIF()
    # Name of shared library is mysqlclient on Unix
    SET_TARGET_PROPERTIES(libmysql PROPERTIES 
      OUTPUT_NAME mysqlclient 
      VERSION "${OS_SHARED_LIB_VERSION}" 
      SOVERSION "${SHARED_LIB_MAJOR_VERSION}")
    CONFIGURE_FILE(libmysql.ver.in ${CMAKE_CURRENT_BINARY_DIR}/libmysql.ver)

    IF(APPLE)
      SET_TARGET_PROPERTIES(libmysql PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIBDIR}"
      )
    ENDIF()

    GET_TARGET_PROPERTY(libmysql_link_flags libmysql LINK_FLAGS)
    IF(NOT libmysql_link_flags)
      SET(libmysql_link_flags)
    ENDIF()
    SET(libmysql_link_flags
      "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${libmysql_link_flags}")
    SET_TARGET_PROPERTIES(libmysql
      PROPERTIES LINK_FLAGS "${libmysql_link_flags}")

    IF(LINK_FLAG_NO_UNDEFINED)
      SET(libmysql_link_flags
        "${libmysql_link_flags} ${LINK_FLAG_NO_UNDEFINED}")
      SET(libmysql_link_flags
        "${libmysql_link_flags} -Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/libmysql.ver")
      SET_TARGET_PROPERTIES(libmysql
        PROPERTIES LINK_FLAGS "${libmysql_link_flags}")
    ENDIF() 
    # clean direct output needs to be set several targets have the same name
    #(mysqlclient in this case)
    SET_TARGET_PROPERTIES(mysqlclient PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    SET_TARGET_PROPERTIES(libmysql PROPERTIES CLEAN_DIRECT_OUTPUT 1)

  ENDIF()
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
ADD_EXECUTABLE(libmysql_api_test ${CMAKE_CURRENT_BINARY_DIR}/api_test.c)
SET_TARGET_PROPERTIES(libmysql_api_test PROPERTIES LINKER_LANGUAGE CXX)
IF(LIBRT)
  TARGET_LINK_LIBRARIES(libmysql_api_test ${LIBRT})
ENDIF()

IF(DISABLE_SHARED)
  TARGET_LINK_LIBRARIES(libmysql_api_test ${LIBS})
ELSE()
  TARGET_LINK_LIBRARIES(libmysql_api_test libmysql)
ENDIF()

# Verify that libmysql_api_test runs OK
ADD_CUSTOM_COMMAND(TARGET libmysql_api_test POST_BUILD
  COMMAND $<TARGET_FILE:libmysql_api_test>
  > ${CMAKE_CURRENT_BINARY_DIR}/libmysql_api_test.out
  )
