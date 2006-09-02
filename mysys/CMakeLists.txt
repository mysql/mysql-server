SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# Need to set USE_TLS, since mysys is linked into libmysql.dll and
# libmysqld.dll, and __declspec(thread) approach to thread local storage does
# not work properly in DLLs.
# Currently, USE_TLS crashes in Debug builds, so until that is fixed Debug
# .dlls cannot be loaded at runtime.
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DUSE_TLS")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DUSE_TLS")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/zlib ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/mysys )
ADD_LIBRARY(mysys array.c charset-def.c charset.c checksum.c default.c default_modify.c
				errors.c hash.c list.c md5.c mf_brkhant.c mf_cache.c mf_dirname.c mf_fn_ext.c
				mf_format.c mf_getdate.c mf_iocache.c mf_iocache2.c mf_keycache.c 
				mf_keycaches.c mf_loadpath.c mf_pack.c mf_path.c mf_qsort.c mf_qsort2.c
				mf_radix.c mf_same.c mf_sort.c mf_soundex.c mf_strip.c mf_tempdir.c
				mf_tempfile.c mf_unixpath.c mf_wcomp.c mf_wfile.c mulalloc.c my_access.c
				my_aes.c my_alarm.c my_alloc.c my_append.c my_bit.c my_bitmap.c my_chsize.c
				my_clock.c my_compress.c my_conio.c my_copy.c my_crc32.c my_create.c my_delete.c
				my_div.c my_error.c my_file.c my_fopen.c my_fstream.c my_gethostbyname.c 
				my_gethwaddr.c my_getopt.c my_getsystime.c my_getwd.c my_handler.c my_init.c
				my_lib.c my_lock.c my_lockmem.c my_lread.c my_lwrite.c my_malloc.c my_messnc.c
				my_mkdir.c my_mmap.c my_net.c my_once.c my_open.c my_pread.c my_pthread.c 
				my_quick.c my_read.c my_realloc.c my_redel.c my_rename.c my_seek.c my_sleep.c
				my_static.c my_symlink.c my_symlink2.c my_sync.c my_thr_init.c my_wincond.c
				my_windac.c my_winsem.c my_winthread.c my_write.c ptr_cmp.c queues.c
				rijndael.c safemalloc.c sha1.c string.c thr_alarm.c thr_lock.c thr_mutex.c
				thr_rwlock.c tree.c typelib.c base64.c my_memmem.c)
