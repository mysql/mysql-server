# Copyright (c) 2015, 2021, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# MS Visual C++ specifics
IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)  # bypass Warning C4996 (getenv)
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../include/mysqlrouter)

SET(lib_source_files
  auto_cleaner.cc
  certificate_generator.cc
  certificate_handler.cc
  config_files.cc
  config_generator.cc
  cluster_metadata.cc
  cluster_metadata_dynamic_state.cc
  router_app.cc
  utils.cc
  uri.cc
  keyring_info.cc
  plugin_config.cc
  common/log_filter.cc
  common/mysql_session.cc
  common/utils_sqlstring.cc
  )
IF(WIN32)
  LIST(APPEND lib_source_files
    windows/main-windows.cc
    windows/nt_servc.cc
    windows/password_vault.cc
    )
ENDIF()

SET(source_files main.cc)

IF(WIN32)
  LIST(APPEND source_files
    ../../harness/src/logging/eventlog_rc/message.rc
    ../../harness/src/logging/eventlog_rc/MSG00001.bin)
ENDIF()

IF(NOT WIN32)
  ADD_LIBRARY(router_lib SHARED ${lib_source_files} ${common_files})
ELSE()
  # on Windows, we statically link the router lib to otherwise avoid
  # exporting a bunch of symbols
  ADD_LIBRARY(router_lib STATIC ${lib_source_files})
ENDIF()
TARGET_LINK_LIBRARIES(router_lib
  ${CMAKE_DL_LIBS}
  harness-library
  harness_stdx
  mysqlclient
  ${SSL_LIBRARIES}
  io_component
  )
TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC ../include)

SET_TARGET_PROPERTIES(router_lib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
)

IF(NOT WIN32)
  SET_TARGET_PROPERTIES(router_lib PROPERTIES
    OUTPUT_NAME "mysqlrouter"
    SOVERSION 1
  )
ELSE()
  SET_TARGET_PROPERTIES(router_lib PROPERTIES
    OUTPUT_NAME "mysqlrouter_lib"
    SOVERSION 1
  )
ENDIF()

ADD_INSTALL_RPATH_FOR_OPENSSL(router_lib)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(router_lib)

MYSQL_ADD_EXECUTABLE(mysqlrouter
  ${source_files}
  COMPONENT Router
  LINK_LIBRARIES
  harness-library
  router_lib
  ${CMAKE_DL_LIBS}
  )

ADD_INSTALL_RPATH(mysqlrouter "${ROUTER_INSTALL_RPATH}")

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(mysqlrouter -lnsl -lsocket)
ENDIF()

IF(APPLE)
  SET_TARGET_PROPERTIES(mysqlrouter PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
ENDIF()

INSTALL(TARGETS router_lib
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

MYSQL_ADD_EXECUTABLE(mysqlrouter_keyring
  keyring_cli.cc
  keyring_frontend.cc
  COMPONENT Router
  LINK_LIBRARIES router_lib
  )
