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
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -DSAFEMALLOC -DSAFE_MUTEX")

# Note that we don't link with the libraries "strings" or "mysys"
# here, instead we recompile the files needed and include them
# directly. This means we don't have to worry here about if these
# libraries are compiled defining USE_TLS or not. Not that it *should*
# have been a problem anyway, they don't use thread local storage.

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/libmysql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/strings)

# We include the source file listing instead of referencing the
# libraries. At least with CMake 2.4 and Visual Studio 2005 a static
# library created from other static libraries would not be complete,
# i.e. the libraries listed in TARGET_LINK_LIBRARIES() were just
# ignored.


# Include and add the directory path
SET(SOURCE_SUBLIBS TRUE)
SET(LIB_SOURCES "")

INCLUDE(${CMAKE_SOURCE_DIR}/zlib/CMakeLists.txt)
FOREACH(rpath ${ZLIB_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../zlib/${rpath})
ENDFOREACH(rpath)

# FIXME only needed if build type is "Debug", but CMAKE_BUILD_TYPE is
# not set during configure time.
INCLUDE(${CMAKE_SOURCE_DIR}/dbug/CMakeLists.txt)
FOREACH(rpath ${DBUG_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../dbug/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/extra/yassl/taocrypt/CMakeLists.txt)
FOREACH(rpath ${TAOCRYPT_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../extra/yassl/taocrypt/${rpath})
ENDFOREACH(rpath)

INCLUDE(${CMAKE_SOURCE_DIR}/extra/yassl/CMakeLists.txt)
FOREACH(rpath ${YASSL_SOURCES})
  SET(LIB_SOURCES ${LIB_SOURCES} ../extra/yassl/${rpath})
ENDFOREACH(rpath)

SET(CLIENT_SOURCES   ../mysys/array.c ../strings/bchange.c ../strings/bmove.c 
                     ../strings/bmove_upp.c ../mysys/charset-def.c ../mysys/charset.c
                     ../sql-common/client.c ../strings/ctype-big5.c ../strings/ctype-bin.c 
                     ../strings/ctype-cp932.c ../strings/ctype-czech.c ../strings/ctype-euc_kr.c 
                     ../strings/ctype-eucjpms.c ../strings/ctype-extra.c ../strings/ctype-gb2312.c
                     ../strings/ctype-gbk.c ../strings/ctype-latin1.c ../strings/ctype-mb.c 
                     ../strings/ctype-simple.c ../strings/ctype-sjis.c ../strings/ctype-tis620.c 
                     ../strings/ctype-uca.c ../strings/ctype-ucs2.c ../strings/ctype-ujis.c 
                     ../strings/ctype-utf8.c ../strings/ctype-win1250ch.c ../strings/ctype.c 
                     ../mysys/default.c errmsg.c ../mysys/errors.c
                     ../mysys/hash.c ../mysys/my_sleep.c ../mysys/default_modify.c
                     get_password.c ../strings/int2str.c ../strings/is_prefix.c 
                     libmysql.c ../mysys/list.c ../strings/llstr.c
                     ../strings/longlong2str.c manager.c ../mysys/mf_arr_appstr.c ../mysys/mf_cache.c
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
		     ../mysys/my_getsystime.c ${LIB_SOURCES})

# Need to set USE_TLS for building the DLL, since __declspec(thread)
# approach to thread local storage does not work properly in DLLs.
#
# The static library might be used to form another DLL, as is the case
# with the ODBC driver, so it has to be compiled with USE_TLS as well.
#
# We create a third library without USE_TLS for internal use. We can't
# be sure that some client application part of this build doesn't go
# beond the documented API, and try access the Thread Local Storage.
# The "_notls" means no Tls*() functions used, i.e. "static" TLS.

ADD_LIBRARY(mysqlclient       STATIC ${CLIENT_SOURCES})
ADD_DEPENDENCIES(mysqlclient GenError)
TARGET_LINK_LIBRARIES(mysqlclient)

ADD_LIBRARY(mysqlclient_notls STATIC ${CLIENT_SOURCES})
ADD_DEPENDENCIES(mysqlclient_notls GenError)
TARGET_LINK_LIBRARIES(mysqlclient_notls)

ADD_LIBRARY(libmysql          SHARED ${CLIENT_SOURCES} dll.c libmysql.def)
IF(WIN32)
  SET_TARGET_PROPERTIES(libmysql mysqlclient PROPERTIES COMPILE_FLAGS "-DUSE_TLS")
ENDIF(WIN32)
ADD_DEPENDENCIES(libmysql GenError)
TARGET_LINK_LIBRARIES(libmysql wsock32)

ADD_EXECUTABLE(myTest mytest.c)
TARGET_LINK_LIBRARIES(myTest libmysql)

IF(EMBED_MANIFESTS)
  MYSQL_EMBED_MANIFEST("myTest" "asInvoker")
ENDIF(EMBED_MANIFESTS)
