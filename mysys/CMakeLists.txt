# Copyright (c) 2006, 2022, Oracle and/or its affiliates.
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

ADD_WSHADOW_WARNING()

SET(MY_TIME_SOURCES my_time.cc my_systime.cc)

ADD_CONVENIENCE_LIBRARY(mytime ${MY_TIME_SOURCES})

SET(MYSYS_SOURCES
  array.cc
  charset.cc
  charset-def.cc
  dbug.cc
  errors.cc
  lf_alloc-pin.cc
  lf_dynarray.cc
  lf_hash.cc
  list.cc
  mf_arr_appstr.cc
  mf_cache.cc
  mf_dirname.cc
  mf_fn_ext.cc
  mf_format.cc
  mf_iocache2.cc
  mf_iocache.cc
  mf_keycache.cc
  mf_keycaches.cc
  mf_loadpath.cc
  mf_pack.cc
  mf_path.cc
  mf_same.cc
  mf_tempdir.cc
  mf_tempfile.cc
  mf_unixpath.cc
  mf_wcomp.cc
  mulalloc.cc
  my_access.cc
  my_alloc.cc
  my_bit.cc
  my_bitmap.cc
  my_chmod.cc
  my_chsize.cc
  my_compare.cc
  my_compress.cc
  my_copy.cc
  my_create.cc
  my_delete.cc
  my_error.cc
  my_fallocator.cc
  my_file.cc
  my_fopen.cc
  my_fstream.cc
  my_gethwaddr.cc
  my_getwd.cc
  my_init.cc
  my_lib.cc
  my_malloc.cc
  my_mess.cc
  my_mkdir.cc
  my_mmap.cc
  my_murmur3.cc
  my_once.cc
  my_open.cc
  my_pread.cc
  my_rdtsc.cc
  my_read.cc
  my_rename.cc
  my_seek.cc
  my_static.cc
  my_string.cc
  my_symlink2.cc
  my_symlink.cc
  my_sync.cc
  my_syslog.cc
  my_thread.cc
  my_thr_init.cc
  my_user.cc
  my_write.cc
  pack.cc
  print_version.cc
  psi_noop.cc
  ptr_cmp.cc
  sql_chars.cc
  stacktrace.cc
  thr_cond.cc
  thr_lock.cc
  thr_mutex.cc
  thr_rwlock.cc
  tree.cc
  typelib.cc
  unhex.cc
  keyring_operations_helper.cc
  # From deleted mysys_ssl library
  crypt_genhash_impl.cc
  my_default.cc
  my_getopt.cc
  my_kdf.cc
  my_aes.cc
  my_sha1.cc
  my_sha2.cc
  my_md5.cc
  my_rnd.cc
  my_openssl_fips.cc
)
LIST(APPEND MYSYS_SOURCES my_aes_openssl.cc)

IF(BUILD_IS_SINGLE_CONFIG)
  GET_PROPERTY(CWD_DEFINITIONS DIRECTORY PROPERTY COMPILE_DEFINITIONS)
  IF(NOT CMAKE_CXX_FLAGS MATCHES "DSAFE_MUTEX" AND
      NOT CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER} MATCHES "DSAFE_MUTEX" AND
      NOT CWD_DEFINITIONS MATCHES "SAFE_MUTEX")
    # Both have ifdef SAFE_MUTEX
    LIST(REMOVE_ITEM MYSYS_SOURCES
      thr_cond.cc
      thr_mutex.cc
      )
  ENDIF()
  IF(CMAKE_CXX_FLAGS MATCHES "DNDEBUG" OR
      CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER} MATCHES "DNDEBUG" OR
      CWD_DEFINITIONS MATCHES "NDEBUG")
    # ifndef NDEBUG
    LIST(REMOVE_ITEM MYSYS_SOURCES
      dbug.cc
      )
  ENDIF()
ENDIF()

IF (WIN32)
  LIST(APPEND MYSYS_SOURCES
    my_conio.cc
    my_windac.cc
    my_winerr.cc
    my_winfile.cc
    win_timers.cc
    my_nojemalloc.cc
  )
ENDIF()

IF(HAVE_BUILD_ID_SUPPORT)
  LIST(APPEND MYSYS_SOURCES build_id.cc)
ENDIF()

IF(HAVE_POSIX_TIMERS)
  LIST(APPEND MYSYS_SOURCES posix_timers.cc)
ENDIF()

IF(HAVE_KQUEUE_TIMERS)
  LIST(APPEND MYSYS_SOURCES kqueue_timers.cc)
ENDIF()

IF(HAVE_GETPWNAM)
  SET(MYSYS_SOURCES ${MYSYS_SOURCES} my_getpwnam.cc)
ENDIF()

IF(HAVE_LINUX_LARGE_PAGES)
  LIST(APPEND MYSYS_SOURCES my_largepage.cc)
ENDIF()

IF(FREEBSD)
  ADD_COMPILE_FLAGS(stacktrace.cc COMPILE_FLAGS ${LIBUNWIND_CFLAGS})
ENDIF()

ADD_CONVENIENCE_LIBRARY(mysys ${MYSYS_SOURCES}
  LINK_LIBRARIES
  mytime
  strings
  ${ZLIB_LIBRARY}
  ${ZSTD_LIBRARY}
  ${LIBNSL}
  ${LIBM}
  ${LIBRT}
  ${LIBEXECINFO}
  ${LIBUNWIND_LDFLAGS}
  ${SSL_LIBRARIES}
  )

# For targets that link with mysys, and are independent of other targets.
IF(TARGET copy_openssl_dlls)
  ADD_DEPENDENCIES(mysys copy_openssl_dlls)
ENDIF()

# Need explicit pthread for gcc -fsanitize=address
IF(CMAKE_USE_PTHREADS_INIT AND CMAKE_C_FLAGS MATCHES "-fsanitize=")
  TARGET_LINK_LIBRARIES(mysys pthread)
ENDIF()
# Needed to resolve OpenSSL dependency on Windows
IF(WIN32)
  TARGET_LINK_LIBRARIES(mysys crypt32)
ENDIF()

# MD5_Init() and others are deprecated.
IF(SOLARIS AND MY_COMPILER_IS_GNU_OR_CLANG)
  ADD_COMPILE_FLAGS(my_md5.cc COMPILE_FLAGS "-Wno-deprecated-declarations")
ENDIF()

# We are not interesting in profiling tests.
DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_EXECUTABLE(thr_lock
  thr_lock.cc
  LINK_LIBRARIES mysys
  SKIP_INSTALL
  )
SET_TARGET_PROPERTIES(thr_lock PROPERTIES COMPILE_FLAGS "-DMAIN")

MYSQL_ADD_EXECUTABLE(mf_iocache_test
  mf_iocache.cc
  LINK_LIBRARIES mysys
  SKIP_INSTALL
  )
SET_TARGET_PROPERTIES(mf_iocache_test PROPERTIES COMPILE_FLAGS "-DMAIN")

IF(HAVE_BUILD_ID_SUPPORT)
  MYSQL_ADD_EXECUTABLE(build_id_test
    build_id_test.cc build_id.cc
    SKIP_INSTALL
    )
  # Verify that we can read our own build-id:
  ADD_CUSTOM_COMMAND(TARGET build_id_test POST_BUILD
#   COMMAND ./build_id_test
#   COMMAND ${READELF_EXECUTABLE} -n ./build_id_test | grep "Build ID:"
    COMMAND ./build_id_test      | egrep -o "[0-9a-f]{40}" > build_id_test_1
    COMMAND ${READELF_EXECUTABLE} -n ./build_id_test | grep "Build ID:" |
                                   egrep -o "[0-9a-f]{40}" > build_id_test_2
    COMMAND diff build_id_test_1 build_id_test_2
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
    COMMENT "Verifying build-id"
    VERBATIM
    )
ENDIF()
