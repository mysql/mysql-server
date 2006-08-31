# Need to set USE_TLS, since __declspec(thread) approach to thread local
# storage does not work properly in DLLs.
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX -DUSE_TLS")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX -DUSE_TLS")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/zlib 
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include 
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/strings)

ADD_LIBRARY(libmysql SHARED dll.c libmysql.def
                     ../mysys/array.c ../strings/bchange.c ../strings/bmove.c 
                     ../strings/bmove_upp.c ../mysys/charset-def.c ../mysys/charset.c
                     ../sql-common/client.c ../strings/ctype-big5.c ../strings/ctype-bin.c 
                     ../strings/ctype-cp932.c ../strings/ctype-czech.c ../strings/ctype-euc_kr.c 
                     ../strings/ctype-eucjpms.c ../strings/ctype-extra.c ../strings/ctype-gb2312.c
                     ../strings/ctype-gbk.c ../strings/ctype-latin1.c ../strings/ctype-mb.c 
                     ../strings/ctype-simple.c ../strings/ctype-sjis.c ../strings/ctype-tis620.c 
                     ../strings/ctype-uca.c ../strings/ctype-ucs2.c ../strings/ctype-ujis.c 
                     ../strings/ctype-utf8.c ../strings/ctype-win1250ch.c ../strings/ctype.c 
                     ../mysys/default.c ../libmysql/errmsg.c ../mysys/errors.c
                     ../libmysql/get_password.c ../strings/int2str.c ../strings/is_prefix.c 
                     ../libmysql/libmysql.c ../mysys/list.c ../strings/llstr.c
                     ../strings/longlong2str.c ../libmysql/manager.c ../mysys/mf_cache.c 
                     ../mysys/mf_dirname.c ../mysys/mf_fn_ext.c ../mysys/mf_format.c 
                     ../mysys/mf_iocache.c ../mysys/mf_iocache2.c ../mysys/mf_loadpath.c 
                     ../mysys/mf_pack.c ../mysys/mf_path.c ../mysys/mf_tempfile.c ../mysys/mf_unixpath.c 
                     ../mysys/mf_wcomp.c ../mysys/mulalloc.c ../mysys/my_access.c ../mysys/my_alloc.c 
                     ../mysys/my_chsize.c ../mysys/my_compress.c ../mysys/my_create.c
                     ../mysys/my_delete.c ../mysys/my_div.c ../mysys/my_error.c ../mysys/my_file.c 
                     ../mysys/my_fopen.c ../mysys/my_fstream.c  ../mysys/my_gethostbyname.c 
                     ../mysys/my_getopt.c ../mysys/my_getwd.c ../mysys/my_init.c ../mysys/my_lib.c
                     ../mysys/my_malloc.c ../mysys/my_messnc.c ../mysys/my_net.c ../mysys/my_once.c 
                     ../mysys/my_open.c ../mysys/my_pread.c  ../mysys/my_pthread.c ../mysys/my_read.c 
                     ../mysys/my_realloc.c ../mysys/my_rename.c ../mysys/my_seek.c  
                     ../mysys/my_static.c ../strings/my_strtoll10.c ../mysys/my_symlink.c 
                     ../mysys/my_symlink2.c ../mysys/my_thr_init.c  ../sql-common/my_time.c 
                     ../strings/my_vsnprintf.c ../mysys/my_wincond.c ../mysys/my_winthread.c 
                     ../mysys/my_write.c ../sql/net_serv.cc ../sql-common/pack.c ../sql/password.c 
                     ../mysys/safemalloc.c ../mysys/sha1.c ../strings/str2int.c
                     ../strings/str_alloc.c ../strings/strcend.c ../strings/strcont.c ../strings/strend.c 
                     ../strings/strfill.c ../mysys/string.c ../strings/strinstr.c ../strings/strmake.c 
                     ../strings/strmov.c ../strings/strnlen.c ../strings/strnmov.c ../strings/strtod.c
                     ../strings/strtoll.c ../strings/strtoull.c ../strings/strxmov.c ../strings/strxnmov.c 
                     ../mysys/thr_mutex.c ../mysys/typelib.c ../vio/vio.c ../vio/viosocket.c 
                     ../vio/viossl.c ../vio/viosslfactories.c ../strings/xml.c)
ADD_DEPENDENCIES(libmysql dbug vio mysys strings GenError zlib yassl taocrypt)
TARGET_LINK_LIBRARIES(libmysql mysys strings wsock32)

ADD_EXECUTABLE(myTest mytest.c)
TARGET_LINK_LIBRARIES(myTest libmysql)
