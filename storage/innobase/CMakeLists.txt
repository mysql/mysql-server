# Copyright (C) 2009 Oracle/Innobase Oy
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# This is the CMakeLists for InnoDB Plugin


# TODO: remove the two FLAGS_DEBUG settings when merging into
# 6.0-based trees, like is already the case for other engines in
# those trees.
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

# Starting at 5.1.38, MySQL CMake files are simplified. But the plugin
# CMakeLists.txt still needs to work with previous versions of MySQL.
IF (MYSQL_VERSION_ID GREATER "50137")
	INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
ENDIF (MYSQL_VERSION_ID GREATER "50137")

IF (CMAKE_SIZEOF_VOID_P MATCHES 8)
	SET(WIN64 TRUE)
ENDIF (CMAKE_SIZEOF_VOID_P MATCHES 8)

# Include directories under innobase
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/innobase/include
		    ${CMAKE_SOURCE_DIR}/storage/innobase/handler)

# Include directories under mysql
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
		    ${CMAKE_SOURCE_DIR}/sql
		    ${CMAKE_SOURCE_DIR}/regex
		    ${CMAKE_SOURCE_DIR}/zlib
		    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

# Removing compiler optimizations for innodb/mem/* files on 64-bit Windows
# due to 64-bit compiler error, See MySQL Bug #19424, #36366, #34297
IF (MSVC AND $(WIN64))
	SET_SOURCE_FILES_PROPERTIES(mem/mem0mem.c mem/mem0pool.c
				    PROPERTIES COMPILE_FLAGS -Od)
ENDIF (MSVC AND $(WIN64))

SET(INNOBASE_SOURCES	btr/btr0btr.c btr/btr0cur.c btr/btr0pcur.c btr/btr0sea.c
			buf/buf0buddy.c buf/buf0buf.c buf/buf0flu.c buf/buf0lru.c buf/buf0rea.c
			data/data0data.c data/data0type.c
			dict/dict0boot.c dict/dict0crea.c dict/dict0dict.c dict/dict0load.c dict/dict0mem.c
			dyn/dyn0dyn.c
			eval/eval0eval.c eval/eval0proc.c
			fil/fil0fil.c
			fsp/fsp0fsp.c
			fut/fut0fut.c fut/fut0lst.c
			ha/ha0ha.c ha/hash0hash.c ha/ha0storage.c
			ibuf/ibuf0ibuf.c
			pars/lexyy.c pars/pars0grm.c pars/pars0opt.c pars/pars0pars.c pars/pars0sym.c
			lock/lock0lock.c lock/lock0iter.c
			log/log0log.c log/log0recv.c
			mach/mach0data.c
			mem/mem0mem.c mem/mem0pool.c
			mtr/mtr0log.c mtr/mtr0mtr.c
			os/os0file.c os/os0proc.c os/os0sync.c os/os0thread.c
			page/page0cur.c page/page0page.c page/page0zip.c
			que/que0que.c
			handler/ha_innodb.cc handler/handler0alter.cc handler/i_s.cc handler/mysql_addons.cc
			read/read0read.c
			rem/rem0cmp.c rem/rem0rec.c
			row/row0ext.c row/row0ins.c row/row0merge.c row/row0mysql.c row/row0purge.c row/row0row.c
			row/row0sel.c row/row0uins.c row/row0umod.c row/row0undo.c row/row0upd.c row/row0vers.c
			srv/srv0que.c srv/srv0srv.c srv/srv0start.c
			sync/sync0arr.c sync/sync0rw.c sync/sync0sync.c
			thr/thr0loc.c
			trx/trx0i_s.c trx/trx0purge.c trx/trx0rec.c trx/trx0roll.c trx/trx0rseg.c
			trx/trx0sys.c trx/trx0trx.c trx/trx0undo.c
			usr/usr0sess.c
			ut/ut0byte.c ut/ut0dbg.c ut/ut0mem.c ut/ut0rnd.c ut/ut0ut.c ut/ut0vec.c
			ut/ut0list.c ut/ut0wqueue.c)
ADD_DEFINITIONS(-DHAVE_WINDOWS_ATOMICS -DIB_HAVE_PAUSE_INSTRUCTION)

IF (MYSQL_VERSION_ID GREATER "50137")
	IF (WITH_INNOBASE_STORAGE_ENGINE)
		MYSQL_STORAGE_ENGINE(INNOBASE)
	ELSE (WITH_INNOBASE_STORAGE_ENGINE)
		SET (INNODB_SOURCES ${INNOBASE_SOURCES})
		MYSQL_STORAGE_ENGINE(INNODB)
	ENDIF (WITH_INNOBASE_STORAGE_ENGINE)
ELSE (MYSQL_VERSION_ID GREATER "50137")
	IF (NOT SOURCE_SUBLIBS)
		ADD_DEFINITIONS(-D_WIN32 -DMYSQL_SERVER)
		ADD_LIBRARY(innobase STATIC ${INNOBASE_SOURCES})
		# Require mysqld_error.h, which is built as part of the GenError
		ADD_DEPENDENCIES(innobase GenError)
	ENDIF (NOT SOURCE_SUBLIBS)
ENDIF (MYSQL_VERSION_ID GREATER "50137")
MYSQL_STORAGE_ENGINE(INNOBASE)
