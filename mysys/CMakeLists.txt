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

SET(MYSYS_SOURCES
  array.cc
  base64.cc
  charset.cc
  charset-def.cc
  checksum.cc
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
  mf_getdate.cc
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
  my_div.cc
  my_error.cc
  my_fallocator.cc
  my_file.cc
  my_fopen.cc
  my_fstream.cc
  my_gethwaddr.cc
  my_getsystime.cc
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
  my_time.cc
  my_timespec.cc
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
)

IF (WIN32)
  LIST(APPEND MYSYS_SOURCES
    my_conio.cc
    my_windac.cc
    my_winerr.cc
    my_winfile.cc
    win_timers.cc
  )
ENDIF()

IF(HAVE_POSIX_TIMERS)
 SET(MYSYS_SOURCES ${MYSYS_SOURCES} posix_timers.cc)
ENDIF()

IF(HAVE_KQUEUE_TIMERS)
  SET(MYSYS_SOURCES ${MYSYS_SOURCES} kqueue_timers.cc)
ENDIF()

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND CMAKE_C_COMPILER_ID MATCHES "SunPro")
  # Inline assembly template for rdtsc
  SET_SOURCE_FILES_PROPERTIES(my_rdtsc.cc
   PROPERTIES COMPILE_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/my_timer_cycles.il")
ENDIF()

IF(HAVE_LINUX_LARGE_PAGES)
 SET(MYSYS_SOURCES ${MYSYS_SOURCES} my_largepage.cc)
ENDIF()

ADD_CONVENIENCE_LIBRARY(mysys ${MYSYS_SOURCES})
TARGET_LINK_LIBRARIES(mysys dbug strings ${ZLIB_LIBRARY} 
 ${LIBNSL} ${LIBM} ${LIBRT} ${LIBEXECINFO})

# Need explicit pthread for gcc -fsanitize=address
IF(CMAKE_USE_PTHREADS_INIT AND CMAKE_C_FLAGS MATCHES "-fsanitize=")
  TARGET_LINK_LIBRARIES(mysys pthread)
ENDIF()

# Bug in std::call_once in optimized mode
IF(SOLARIS_SPARC AND CMAKE_COMPILER_IS_GNUCXX)
  ADD_COMPILE_FLAGS(charset.cc COMPILE_FLAGS "-O0")
ENDIF()

MYSQL_ADD_EXECUTABLE(thr_lock thr_lock.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(thr_lock mysys)
SET_TARGET_PROPERTIES(thr_lock PROPERTIES COMPILE_FLAGS "-DMAIN")
MYSQL_ADD_EXECUTABLE(mf_iocache_test mf_iocache.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(mf_iocache_test mysys)
SET_TARGET_PROPERTIES(mf_iocache_test PROPERTIES COMPILE_FLAGS "-DMAIN")
MYSQL_ADD_EXECUTABLE(base64_test base64.cc SKIP_INSTALL)
SET_TARGET_PROPERTIES(base64_test PROPERTIES COMPILE_FLAGS "-DMAIN")
TARGET_LINK_LIBRARIES(base64_test mysys)
ADD_DEPENDENCIES(base64_test GenError)
