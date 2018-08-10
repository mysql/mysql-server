# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/compile_flags.cmake)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../include/mysqlrouter)
INCLUDE_DIRECTORIES(SYSTEM ${SSL_INCLUDE_DIRS})

IF(WITH_SSL STREQUAL "wolfssl")
  SET(MY_AES_IMPL ${CMAKE_SOURCE_DIR}/mysys_ssl/my_aes_wolfssl.cc)
  #ADD_DEFINITIONS(-DSIZEOF_LONG=${SIZEOF_LONG} -DSIZEOF_LONG_LONG=${SIZEOF_LONG_LONG})
ELSE()
  SET(MY_AES_IMPL ${CMAKE_SOURCE_DIR}/mysys_ssl/my_aes_openssl.cc)
ENDIF()

ADD_DEFINITIONS(${SSL_DEFINES})


SET(lib_source_files
  config_files.cc
  config_generator.cc
  cluster_metadata.cc
  router_app.cc
  utils.cc
  uri.cc
  keyring_info.cc
  plugin_config.cc
  common/log_filter.cc
  # common/my_aes.cc
  common/my_sha1.cc
  common/mysql_session.cc
  common/utils_sqlstring.cc
  # ${MY_AES_IMPL}
  )
IF(WIN32)
  LIST(APPEND lib_source_files windows/nt_servc.cc windows/main-windows.cc windows/password_vault.cc)
ENDIF()

SET(source_files main.cc)

IF(NOT WIN32)
  ADD_LIBRARY(router_lib SHARED ${lib_source_files} ${common_files})
ELSE()
  # on Windows, we statically link the router lib to otherwise avoid exporting a bunch of symbols
  ADD_LIBRARY(router_lib STATIC ${lib_source_files})
ENDIF()
TARGET_LINK_LIBRARIES(router_lib ${CMAKE_DL_LIBS} harness-library ${MySQL_CLIENT_LIB} ${SSL_LIBRARIES})
IF(WIN32)
  TARGET_LINK_LIBRARIES(router_lib crypt32)
ENDIF()
TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC ../include ${MySQL_INCLUDE_DIRS})
IF(WITH_HARNESS)
  TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC "${PROJECT_BINARY_DIR}")
  TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC "${PROJECT_BINARY_DIR}/include/mysql/harness")
  TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC "${PROJECT_BINARY_DIR}/include")
  TARGET_INCLUDE_DIRECTORIES(router_lib PUBLIC "${PROJECT_BINARY_DIR}/harness")
ENDIF()

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

SET_PATH_TO_SSL(router_lib ${CMAKE_BINARY_DIR}/library_output_directory)

MYSQL_ADD_EXECUTABLE(${MYSQL_ROUTER_TARGET} ${source_files} COMPONENT Router)

SET_PROPERTY(TARGET ${MYSQL_ROUTER_TARGET} PROPERTY INSTALL_RPATH "${ROUTER_INSTALL_RPATH}")

TARGET_LINK_LIBRARIES(${MYSQL_ROUTER_TARGET} ${CMAKE_DL_LIBS} router_lib harness-library)
IF(WIN32)
  TARGET_LINK_LIBRARIES(${MYSQL_ROUTER_TARGET} crypt32)
ENDIF()
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  TARGET_LINK_LIBRARIES(${MYSQL_ROUTER_TARGET} -lnsl -lsocket)
ENDIF()

IF(APPLE)
  SET_TARGET_PROPERTIES(${MYSQL_ROUTER_TARGET} PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
ELSEIF(CMAKE_COMPILER_IS_GNUCXX)
ENDIF()


INSTALL(TARGETS router_lib
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR}
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR}
  COMPONENT Router)
