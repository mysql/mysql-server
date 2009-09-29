# Copyright (C) 2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# Only the server link with this library, the client libraries and the client
# executables all link with recompiles of source found in the "mysys" directory.
# So we only need to create one version of this library, with the "static"
# Thread Local Storage model.
#
# Exception is the embedded server that needs this library compiled with
# dynamic TLS, i.e. define USE_TLS

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/zlib ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/mysys)

SET(MYSYS_SOURCES  array.c charset-def.c charset.c checksum.c default.c default_modify.c
				errors.c hash.c list.c md5.c mf_brkhant.c mf_cache.c mf_dirname.c mf_fn_ext.c
				mf_format.c mf_getdate.c mf_iocache.c mf_iocache2.c mf_keycache.c 
				mf_keycaches.c mf_loadpath.c mf_pack.c mf_path.c mf_qsort.c mf_qsort2.c
				mf_radix.c mf_same.c mf_sort.c mf_soundex.c mf_strip.c mf_arr_appstr.c mf_tempdir.c
				mf_tempfile.c mf_unixpath.c mf_wcomp.c mf_wfile.c mulalloc.c my_access.c
				my_aes.c my_alarm.c my_alloc.c my_append.c my_bit.c my_bitmap.c my_chsize.c
				my_clock.c my_compress.c my_conio.c my_copy.c my_crc32.c my_create.c my_delete.c
				my_div.c my_error.c my_file.c my_fopen.c my_fstream.c my_gethostbyname.c 
				my_gethwaddr.c my_getopt.c my_getsystime.c my_getwd.c my_handler.c my_init.c
				my_lib.c my_lock.c my_lockmem.c my_malloc.c my_messnc.c
				my_mkdir.c my_mmap.c my_net.c my_once.c my_open.c my_pread.c my_pthread.c 
				my_quick.c my_read.c my_realloc.c my_redel.c my_rename.c my_seek.c my_sleep.c
				my_static.c my_symlink.c my_symlink2.c my_sync.c my_thr_init.c my_wincond.c
				my_winerr.c my_winfile.c my_windac.c my_winthread.c my_write.c ptr_cmp.c queues.c stacktrace.c
				rijndael.c safemalloc.c sha1.c string.c thr_alarm.c thr_lock.c thr_mutex.c
				thr_rwlock.c tree.c typelib.c my_vle.c base64.c my_memmem.c my_getpagesize.c)

IF(NOT SOURCE_SUBLIBS)
  ADD_LIBRARY(mysys ${MYSYS_SOURCES})
ENDIF(NOT SOURCE_SUBLIBS)
