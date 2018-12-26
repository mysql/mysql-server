# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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
${LZ4_INCLUDE_DIR}
# Following is for perror, in case NDB is compiled in.
${CMAKE_SOURCE_DIR}/storage/ndb/include
${CMAKE_BINARY_DIR}/storage/ndb/include
${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi)

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

# Default install component for the files is Server here
SET(MYSQL_INSTALL_COMPONENT Server)

IF(NOT CMAKE_CROSSCOMPILING)
 ADD_EXECUTABLE(comp_err comp_err.c)
 TARGET_LINK_LIBRARIES(comp_err mysys mysys_ssl)
 SET_TARGET_PROPERTIES(comp_err PROPERTIES LINKER_LANGUAGE CXX)
ENDIF()

IF (BUILD_BUNDLED_LZ4)
  ADD_CONVENIENCE_LIBRARY(lz4_lib
    lz4/lz4.c
    lz4/lz4frame.c
    lz4/lz4hc.c
    lz4/xxhash.c
  )
ENDIF()

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                   ${PROJECT_BINARY_DIR}/sql/share/english/errmsg.sys
                   COMMAND comp_err
                    --charset=${PROJECT_SOURCE_DIR}/sql/share/charsets 
                    --out-dir=${PROJECT_BINARY_DIR}/sql/share/
                    --header_file=${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_BINARY_DIR}/include/mysqld_ername.h
                    --state_file=${PROJECT_BINARY_DIR}/include/sql_state.h
                    --in_file=${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt
                   DEPENDS ${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/comp_err.c)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS 
                  ${PROJECT_BINARY_DIR}/include/mysqld_error.h
                  ${PROJECT_BINARY_DIR}/sql/share/english/errmsg.sys 
                  ${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt)

# Set InnoDB mutex type
ADD_DEFINITIONS(-DMUTEX_EVENT)

## Most executables depend on libeay32.dll (through mysys_ssl).
## For 'comp_err' we need it to be available during build.
COPY_OPENSSL_DLLS(copy_openssl_extra)
IF (WIN32 AND WITH_SSL_PATH AND HAVE_CRYPTO_DLL)
  ADD_DEPENDENCIES(GenError copy_openssl_extra)
ENDIF()


MYSQL_ADD_EXECUTABLE(my_print_defaults my_print_defaults.c)
TARGET_LINK_LIBRARIES(my_print_defaults  mysys mysys_ssl)
SET_TARGET_PROPERTIES(my_print_defaults PROPERTIES LINKER_LANGUAGE CXX)

MYSQL_ADD_EXECUTABLE(perror perror.c)
ADD_DEPENDENCIES(perror GenError)
TARGET_LINK_LIBRARIES(perror mysys mysys_ssl)
SET_TARGET_PROPERTIES(perror PROPERTIES LINKER_LANGUAGE CXX)

MYSQL_ADD_EXECUTABLE(resolveip resolveip.c)
TARGET_LINK_LIBRARIES(resolveip mysys mysys_ssl)
SET_TARGET_PROPERTIES(resolveip PROPERTIES LINKER_LANGUAGE CXX)
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  INCLUDE(CheckFunctionExists)
  INCLUDE(CheckLibraryExists)
  MY_SEARCH_LIBS(inet_aton  "nsl;socket;resolv" SOLARIS_NSL)
  TARGET_LINK_LIBRARIES(resolveip ${SOLARIS_NSL})
ENDIF()


MYSQL_ADD_EXECUTABLE(replace replace.c)
TARGET_LINK_LIBRARIES(replace mysys)

MYSQL_ADD_EXECUTABLE(lz4_decompress lz4_decompress.cc)
TARGET_LINK_LIBRARIES(lz4_decompress ${LZ4_LIBRARY})

MYSQL_ADD_EXECUTABLE(zlib_decompress zlib_decompress.cc)
TARGET_LINK_LIBRARIES(zlib_decompress ${ZLIB_LIBRARY})

IF(WITH_INNOBASE_STORAGE_ENGINE)

  IF(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
    ADD_DEFINITIONS(-DHAVE_LZ4=1)
    INCLUDE_DIRECTORIES(${LZ4_INCLUDE_DIR})
  ENDIF()

  # Add path to the InnoDB headers
  INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/storage/innobase/include
    ${CMAKE_SOURCE_DIR}/sql
  )
  # We use the InnoDB code directly in case the code changes.
  ADD_DEFINITIONS("-DUNIV_INNOCHECKSUM")
  SET(INNOBASE_SOURCES
      ../storage/innobase/buf/buf0checksum.cc
      ../storage/innobase/ut/ut0crc32.cc
      ../storage/innobase/ut/ut0ut.cc
      ../storage/innobase/buf/buf0buf.cc
      ../storage/innobase/page/page0zip.cc
      ../storage/innobase/os/os0file.cc
     )

  # Avoid generating Hardware Capabilities due to crc32 instructions
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND
     CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)
    MY_CHECK_CXX_COMPILER_FLAG("-Wa,-nH" HAVE_WA_NH)
    IF(HAVE_WA_NH)
      ADD_COMPILE_FLAGS(
        ../storage/innobase/ut/ut0crc32.cc
        COMPILE_FLAGS "-Wa,-nH"
      )
    ENDIF()
  ENDIF()

  MYSQL_ADD_EXECUTABLE(innochecksum innochecksum.cc ${INNOBASE_SOURCES})
  TARGET_LINK_LIBRARIES(innochecksum mysys mysys_ssl ${LZ4_LIBRARY})
  ADD_DEPENDENCIES(innochecksum GenError)
ENDIF()

IF(UNIX)
  MYSQL_ADD_EXECUTABLE(resolve_stack_dump resolve_stack_dump.cc)
  TARGET_LINK_LIBRARIES(resolve_stack_dump mysys mysys_ssl)
ENDIF()

# In published release builds on Solaris, we need to bundle gcc source.
# PB2 will take care of putting it in extra/ when needed
IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gcc-4.8.1.tar.bz2)
  INSTALL(FILES gcc-4.8.1.tar.bz2 
          DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Development)
ENDIF()
