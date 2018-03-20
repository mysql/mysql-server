# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

# Default install component for the files is Server here
SET(MYSQL_INSTALL_COMPONENT Server)

IF(NOT CMAKE_CROSSCOMPILING)
  MYSQL_ADD_EXECUTABLE(comp_err comp_err.cc SKIP_INSTALL)
  TARGET_LINK_LIBRARIES(comp_err mysys mysys_ssl)
ENDIF()

MYSQL_ADD_EXECUTABLE(comp_client_err
  comp_client_err.cc ${CMAKE_SOURCE_DIR}/libmysql/errmsg.cc
  SKIP_INSTALL)
TARGET_LINK_LIBRARIES(comp_client_err mysys mysys_ssl)

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h
  COMMAND comp_client_err
  --in_file=${PROJECT_SOURCE_DIR}/include/errmsg.h
  --out_file=${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/comp_client_err.cc)

ADD_CUSTOM_TARGET(GenClientError
                  ALL
                  DEPENDS ${PROJECT_BINARY_DIR}/include/mysqlclient_ername.h)


IF (BUILD_BUNDLED_LZ4)
  ADD_CONVENIENCE_LIBRARY(lz4_lib
    ../extra/lz4/lz4.c
    ../extra/lz4/lz4frame.c
    ../extra/lz4/lz4hc.c
    ../extra/lz4/xxhash.c
  )
ENDIF()

FILE(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR})

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                   ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/english/errmsg.sys
                   COMMAND comp_err
                    --charset=${PROJECT_SOURCE_DIR}/share/charsets
                    --out-dir=${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/
                    --header_file=${PROJECT_BINARY_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_BINARY_DIR}/include/mysqld_ername.h
                    --in_file=${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt
                   DEPENDS ${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/comp_err.cc)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS 
                  ${PROJECT_BINARY_DIR}/include/mysqld_error.h
                  ${PROJECT_BINARY_DIR}/${INSTALL_MYSQLSHAREDIR}/english/errmsg.sys
                  ${PROJECT_SOURCE_DIR}/share/errmsg-utf8.txt)

# Set InnoDB mutex type
ADD_DEFINITIONS(-DMUTEX_EVENT)

## Most executables depend on libeay32.dll (through mysys_ssl).
## For 'comp_err' we need it to be available during build.
IF (WIN32 AND WITH_SSL_PATH AND HAVE_CRYPTO_DLL)
  ADD_DEPENDENCIES(GenError copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_client_err copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_err copy_openssl_dlls)
ENDIF()

IF(APPLE AND HAVE_CRYPTO_DYLIB AND HAVE_OPENSSL_DYLIB)
  ADD_DEPENDENCIES(GenError copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_client_err copy_openssl_dlls)
  ADD_DEPENDENCIES(comp_err copy_openssl_dlls)
ENDIF()

MYSQL_ADD_EXECUTABLE(my_print_defaults my_print_defaults.cc)
TARGET_LINK_LIBRARIES(my_print_defaults  mysys mysys_ssl)
SET_TARGET_PROPERTIES(my_print_defaults PROPERTIES LINKER_LANGUAGE CXX)

MYSQL_ADD_EXECUTABLE(perror perror.cc)
ADD_DEPENDENCIES(perror GenError)
IF(WITH_NDBCLUSTER)
  # In case NDB is compiled in.
  TARGET_INCLUDE_DIRECTORIES(perror PUBLIC
    ${CMAKE_SOURCE_DIR}/storage/ndb/include
    ${CMAKE_BINARY_DIR}/storage/ndb/include
    ${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
    ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi)
ENDIF()
TARGET_LINK_LIBRARIES(perror mysys mysys_ssl)
SET_TARGET_PROPERTIES(perror PROPERTIES LINKER_LANGUAGE CXX)

MYSQL_ADD_EXECUTABLE(resolveip resolveip.cc)
TARGET_LINK_LIBRARIES(resolveip mysys mysys_ssl)
SET_TARGET_PROPERTIES(resolveip PROPERTIES LINKER_LANGUAGE CXX)
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  INCLUDE(CheckFunctionExists)
  INCLUDE(CheckLibraryExists)
  MY_SEARCH_LIBS(inet_aton  "nsl;socket;resolv" SOLARIS_NSL)
  TARGET_LINK_LIBRARIES(resolveip ${SOLARIS_NSL})
ENDIF()


IF (BUILD_BUNDLED_LZ4)
  MYSQL_ADD_EXECUTABLE(lz4_decompress lz4_decompress.cc)
  TARGET_LINK_LIBRARIES(lz4_decompress ${LZ4_LIBRARY} mysys)
ENDIF()

IF (BUILD_BUNDLED_ZLIB OR NOT OPENSSL_EXECUTABLE_HAS_ZLIB)
  MYSQL_ADD_EXECUTABLE(zlib_decompress zlib_decompress.cc)
  TARGET_LINK_LIBRARIES(zlib_decompress ${ZLIB_LIBRARY} mysys)
ENDIF()

IF(WITH_INNOBASE_STORAGE_ENGINE)

  IF(LZ4_LIBRARY)
    ADD_DEFINITIONS(-DHAVE_LZ4=1)
  ENDIF()

  IF(NOT MSVC)
    IF(NOT CMAKE_CROSSCOMPILING)
    CHECK_C_SOURCE_RUNS(
    "#include<stdint.h>
    int main()
    {
      unsigned char       a = 0;
      unsigned char       b = 0;
      unsigned char       c = 1;

      __atomic_exchange(&a, &b,  &c, __ATOMIC_RELEASE);
      __atomic_compare_exchange(&a, &b, &c, 0,
                                __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
      return(0);
    }"
    HAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE
    )
    ENDIF()

    IF(HAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE)
      ADD_DEFINITIONS(-DHAVE_IB_GCC_ATOMIC_COMPARE_EXCHANGE=1)
    ENDIF()
  ENDIF(NOT MSVC)

  # Add path to the InnoDB headers
  INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/storage/innobase/
    ${CMAKE_SOURCE_DIR}/storage/innobase/include
  )

  # We use the InnoDB code directly in case the code changes.
  MYSQL_ADD_EXECUTABLE(innochecksum innochecksum.cc)
  TARGET_COMPILE_DEFINITIONS(innochecksum PRIVATE UNIV_NO_ERR_MSGS UNIV_LIBRARY)
  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    ADD_COMPILE_FLAGS(innochecksum.cc COMPILE_FLAGS "-Wno-unused-parameter")
  ENDIF()
  TARGET_LINK_LIBRARIES(innochecksum
    mysys_ssl innodb_zipdecompress ${LZ4_LIBRARY})
  ADD_DEPENDENCIES(innochecksum GenError)

  MYSQL_ADD_EXECUTABLE(ibd2sdi ibd2sdi.cc)
  TARGET_COMPILE_DEFINITIONS(ibd2sdi PRIVATE UNIV_NO_ERR_MSGS UNIV_LIBRARY)
  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    ADD_COMPILE_FLAGS(ibd2sdi.cc COMPILE_FLAGS "-Wno-unused-parameter")
  ENDIF()
  TARGET_LINK_LIBRARIES(ibd2sdi
    mysys_ssl innodb_zipdecompress ${LZ4_LIBRARY})
  ADD_DEPENDENCIES(ibd2sdi GenError)

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
