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
INCLUDE("${PROJECT_SOURCE_DIR}/win/mysql_manifest.cmake")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# The old Windows build method used renamed (.cc -> .cpp) source files, fails
# in #include in mysqlbinlog.cc. So disable that using the USING_CMAKE define.
ADD_DEFINITIONS(-DUSING_CMAKE -DYASSL_PREFIX -DUSE_TLS)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/include
                    ${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/mySTL
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/mysys
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/strings)

SET(YASSL_SOURCES ../extra/yassl/src/buffer.cpp
                  ../extra/yassl/src/cert_wrapper.cpp
                  ../extra/yassl/src/crypto_wrapper.cpp
                  ../extra/yassl/src/handshake.cpp
                  ../extra/yassl/src/lock.cpp 
                  ../extra/yassl/src/log.cpp 
                  ../extra/yassl/src/socket_wrapper.cpp
                  ../extra/yassl/src/ssl.cpp
                  ../extra/yassl/src/timer.cpp
                  ../extra/yassl/src/yassl_error.cpp
                  ../extra/yassl/src/yassl_imp.cpp
                  ../extra/yassl/src/yassl_int.cpp)
				  
SET(TAOCRYPT_SOURCES ../extra/yassl/taocrypt/src/aes.cpp
                     ../extra/yassl/taocrypt/src/aestables.cpp
                     ../extra/yassl/taocrypt/src/algebra.cpp
                     ../extra/yassl/taocrypt/src/arc4.cpp
                     ../extra/yassl/taocrypt/src/asn.cpp
                     ../extra/yassl/taocrypt/src/coding.cpp
                     ../extra/yassl/taocrypt/src/des.cpp
                     ../extra/yassl/taocrypt/src/dh.cpp
                     ../extra/yassl/taocrypt/src/dsa.cpp
                     ../extra/yassl/taocrypt/src/file.cpp
                     ../extra/yassl/taocrypt/src/hash.cpp
                     ../extra/yassl/taocrypt/src/integer.cpp
                     ../extra/yassl/taocrypt/src/md2.cpp
                     ../extra/yassl/taocrypt/src/md4.cpp
                     ../extra/yassl/taocrypt/src/md5.cpp
                     ../extra/yassl/taocrypt/src/misc.cpp
                     ../extra/yassl/taocrypt/src/random.cpp
                     ../extra/yassl/taocrypt/src/ripemd.cpp
                     ../extra/yassl/taocrypt/src/rsa.cpp
                     ../extra/yassl/taocrypt/src/sha.cpp)

ADD_LIBRARY(mysqlclient ../mysys/array.c ../strings/bchange.c ../strings/bmove.c 
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
                        ../vio/viossl.c ../vio/viosslfactories.c ../strings/xml.c
                        ${YASSL_SOURCES} ${TAOCRYPT_SOURCES}       
                        )
                        

ADD_DEPENDENCIES(mysqlclient GenError)
ADD_EXECUTABLE(mysql completion_hash.cc mysql.cc readline.cc sql_string.cc)
LINK_DIRECTORIES(${MYSQL_BINARY_DIR}/mysys ${MYSQL_BINARY_DIR}/zlib)
TARGET_LINK_LIBRARIES(mysql mysqlclient mysys zlib dbug wsock32)

ADD_EXECUTABLE(mysqltest mysqltest.c)
TARGET_LINK_LIBRARIES(mysqltest mysqlclient mysys zlib dbug regex wsock32)

ADD_EXECUTABLE(mysqlcheck mysqlcheck.c)
TARGET_LINK_LIBRARIES(mysqlcheck mysqlclient dbug zlib wsock32)

ADD_EXECUTABLE(mysqldump mysqldump.c ../sql-common/my_user.c)
TARGET_LINK_LIBRARIES(mysqldump mysqlclient mysys dbug zlib wsock32)

ADD_EXECUTABLE(mysqlimport mysqlimport.c)
TARGET_LINK_LIBRARIES(mysqlimport mysqlclient mysys dbug zlib wsock32)

ADD_EXECUTABLE(mysql_upgrade mysql_upgrade.c ../mysys/my_getpagesize.c)
TARGET_LINK_LIBRARIES(mysql_upgrade mysqlclient mysys dbug zlib wsock32)
ADD_DEPENDENCIES(mysql_upgrade GenFixPrivs)

ADD_EXECUTABLE(mysqlshow mysqlshow.c)
TARGET_LINK_LIBRARIES(mysqlshow mysqlclient mysys dbug zlib wsock32)

ADD_EXECUTABLE(mysqlbinlog mysqlbinlog.cc ../mysys/mf_tempdir.c ../mysys/my_new.cc
                           ../mysys/my_bit.c ../mysys/my_bitmap.c 
                           ../mysys/base64.c)
TARGET_LINK_LIBRARIES(mysqlbinlog mysqlclient dbug zlib wsock32)

ADD_EXECUTABLE(mysqladmin mysqladmin.cc)
TARGET_LINK_LIBRARIES(mysqladmin mysqlclient mysys dbug zlib wsock32)

ADD_EXECUTABLE(echo echo.c)

IF(EMBED_MANIFESTS)
  MYSQL_EMBED_MANIFEST("mysql" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqltest" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlcheck" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqldump" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlimport" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysql_upgrade" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlshow" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqlbinlog" "asInvoker")
  MYSQL_EMBED_MANIFEST("mysqladmin" "asInvoker")
  MYSQL_EMBED_MANIFEST("echo" "asInvoker")
ENDIF(EMBED_MANIFESTS)

