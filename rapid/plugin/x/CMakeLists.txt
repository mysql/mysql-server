# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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


INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

SET(XPLUGIN_LOG_PROTOBUF 1 CACHE STRING "Enable logging of protobuf messages")

INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIRS})
INCLUDE(mysqlx_protobuf.cmake)

IF(NOT PROTOBUF_FOUND)
  MESSAGE(WARNING "Disabling mysqlx plugin.")
ELSE()
  INCLUDE(mysqlx_variables.cmake)
  INCLUDE(source_files.cmake)
  INCLUDE(mysqlx_configure.cmake)

  SET(GENERATED_LITE_PROTO_FILES)
  # Generate protobuf-lite versions of proto files.
  FOREACH(PROTOBUF_MYSQLX_FILE ${PROTOBUF_MYSQLX_FILES})
    GET_FILENAME_COMPONENT(MYSQLX_NAME_WD ${PROTOBUF_MYSQLX_FILE} NAME)
    LIST(APPEND GENERATED_LITE_PROTO_FILES
      ${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${MYSQLX_NAME_WD})
    ADD_CUSTOM_COMMAND(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${MYSQLX_NAME_WD}"
      COMMAND ${CMAKE_COMMAND}
              -D CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
              -D PROTO_FILE="${PROTOBUF_MYSQLX_FILE}"
              -D PROTO_FILE_FLAGS="PROTOBUF_LITE"
              -P ${CMAKE_CURRENT_SOURCE_DIR}/process_protobuf_file.cmake
      DEPENDS ${PROTOBUF_MYSQLX_FILE})
    SET_SOURCE_FILES_PROPERTIES(
      "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite/${MYSQLX_NAME_WD}"
      PROPERTIES GENERATED TRUE)
  ENDFOREACH()

  # Be sure to convert all proto files to protobuf_lite before running protoc
  ADD_CUSTOM_TARGET(GenLiteProtos ALL DEPENDS ${GENERATED_LITE_PROTO_FILES})

  MYSQLX_PROTOBUF_GENERATE_CPP(
    PROTO_SRCS PROTO_HDRS
    PROTO_LITE_SRCS PROTO_LITE_HDRS
    ${PROTOBUF_MYSQLX_FILES})

  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    ADD_COMPILE_FLAGS(${PROTO_SRCS} COMPILE_FLAGS
      "-Wno-ignored-qualifiers -Wno-sign-compare -Wno-unused-variable")
  ENDIF()
  
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ngs/include
    ${CMAKE_SOURCE_DIR}/include #temporary
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${SSL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIR}
  )

  INCLUDE_DIRECTORIES(SYSTEM
    ${PROTOBUF_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIR}
  )

  IF(XPLUGIN_NO_LITE_PROTOBUF)
    SET(xplugin_PROTOBUF_SRC ${PROTO_SRCS} ${PROTO_HDRS})
    SET(xplugin_PROTOBUF_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf")
    SET(xplugin_PROTOBUF_LIB ${PROTOBUF_LIBRARY})
  ELSE()
    SET(xplugin_PROTOBUF_SRC ${PROTO_LITE_SRCS} ${PROTO_LITE_HDRS})
    SET(xplugin_PROTOBUF_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf_lite")
    SET(xplugin_PROTOBUF_LIB ${PROTOBUF_LITE_LIBRARY})
  ENDIF()

  # Do not LINK_LIBRARIES below, do TARGET_LINK_LIBRARIES instead.
  IF(WITH_PROTOBUF STREQUAL "system")
    SET(xplugin_PROTOBUF_LIB)
  ENDIF()

  IF(MSVC)
    ADD_COMPILE_FLAGS(${PROTO_SRCS} ${PROTO_LITE_SRCS} COMPILE_FLAGS "/wd4018")
  ENDIF(MSVC)

  ADD_COMPILE_FLAGS(
    src/xpl_plugin.cc
    src/xpl_performance_schema.cc
    ${xplugin_SRC} ${xplugin_PROTOBUF_SRC}
    COMPILE_FLAGS
    "-I${xplugin_PROTOBUF_DIR}")

  # assertion failed in function assert_with_dumps() @ ssa.c:621 when using -xO3
  IF(CMAKE_CXX_COMPILER_ID MATCHES "SunPro")
    ADD_COMPILE_FLAGS(
      ${MYSQLX_PROJECT_DIR}/ngs/src/client.cc
      COMPILE_FLAGS "-xO2")
  ENDIF()

  MYSQL_ADD_PLUGIN(mysqlx
    src/xpl_plugin.cc
    src/xpl_performance_schema.cc
    ${xplugin_SRC} ${xplugin_HDRS} ${xplugin_PROTOBUF_SRC}
    LINK_LIBRARIES ${xplugin_PROTOBUF_LIB} ${LIBEVENT_LIBRARY} ${LIBEVENT_LIBRARIES}
    MODULE_ONLY MODULE_OUTPUT_NAME "mysqlx")

  IF(WITH_PROTOBUF STREQUAL "system")
    TARGET_LINK_LIBRARIES(mysqlx "${PROTOBUF_LIBRARY}")
  ENDIF()

  ADD_DEPENDENCIES(mysqlx GenError)
  IF (WITH_LIBEVENT STREQUAL "bundled")
    ADD_DEPENDENCIES(mysqlx ${LIBEVENT_LIBRARY})
  ENDIF()

  IF(XPLUGIN_LOG_PROTOBUF)
    MESSAGE(STATUS "MYSQLX - Text log of protobuf messages enabled")
    ADD_DEFINITIONS(-DXPLUGIN_LOG_PROTOBUF)
  ENDIF()

  IF(XPLUGIN_LOG_DEBUG OR WITH_DEBUG)
    MESSAGE(STATUS "MYSQLX - Enabled debug traces")
    ADD_DEFINITIONS(-DXPLUGIN_LOG_DEBUG)
  ENDIF()

  IF(XPLUGIN_NO_LITE_PROTOBUF)
    MESSAGE(STATUS "MYSQLX - Using protobuf instead protobuf-litle")
    ADD_DEFINITIONS(-DUSE_MYSQLX_FULL_PROTO)
  ENDIF()

  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/mysqlxtest_src/common)

  FILE(GLOB mysqlxtest_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/mysqlxtest_src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/mysqlxtest_src/common/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ngs/include/ngs_common/*.h"
  )

  FILE(GLOB mysqlxtest_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/mysqlxtest_src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/mysqlxtest_src/common/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ngs/src/protocol_authentication.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/ngs/src/mysql41_hash.cc"
  )

  SET(mysqlxtest_SRC ${mysqlxtest_SRC} ${PROTO_SRCS})

  MYSQL_ADD_EXECUTABLE(mysqlxtest ${mysqlxtest_SRC})

  ADD_DEPENDENCIES(mysqlxtest GenError)

  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    ADD_COMPILE_FLAGS(${mysqlxtest_SRC} COMPILE_FLAGS "-Wno-unused-parameter")

    MY_CHECK_CXX_COMPILER_FLAG("-Wunused-but-set-parameter" HAVE_UNUSED_BUT_SET)
    IF(HAVE_UNUSED_BUT_SET)
      ADD_COMPILE_FLAGS(${mysqlxtest_SRC} COMPILE_FLAGS
        "-Wno-unused-but-set-parameter")
    ENDIF()
  ENDIF()

  GET_PROPERTY(OLD_COMPILE_FLAGS TARGET mysqlxtest PROPERTY COMPILE_FLAGS)
  SET_PROPERTY(TARGET mysqlxtest PROPERTY COMPILE_FLAGS
    "${OLD_COMPILE_FLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/generated/protobuf")

    GET_TARGET_PROPERTY(mysqlxtest_compile_definitions
      mysqlxtest COMPILE_DEFINITIONS)
  IF(mysqlxtest_compile_definitions)
    SET_TARGET_PROPERTIES(mysqlxtest PROPERTIES COMPILE_DEFINITIONS
      "${mysqlxtest_compile_definitions};NGS_DISABLE_LOGGING;USE_MYSQLX_FULL_PROTO")
  ELSE()
    SET_TARGET_PROPERTIES(mysqlxtest PROPERTIES COMPILE_DEFINITIONS
      "NGS_DISABLE_LOGGING;USE_MYSQLX_FULL_PROTO")
  ENDIF()

  TARGET_LINK_LIBRARIES(mysqlxtest mysys strings mysqlclient)
  TARGET_LINK_LIBRARIES(mysqlxtest ${PROTOBUF_LIBRARY})

  IF( WIN32 )
    ADD_DEFINITIONS(-DMYSQLXTEST_EXPORTS)
  ENDIF()

  IF(INSTALL_MYSQLTESTDIR)
    INSTALL(
      FILES tests/mtr/plugin.defs
      DESTINATION "${INSTALL_MYSQLTESTDIR}/suite/x"
      COMPONENT Test
    )
    INSTALL(
      DIRECTORY tests/mtr/t tests/mtr/r tests/mtr/include
      DESTINATION "${INSTALL_MYSQLTESTDIR}/suite/x"
      USE_SOURCE_PERMISSIONS
      COMPONENT Test
      PATTERN "collections" EXCLUDE
    )
  ENDIF()
ENDIF()

