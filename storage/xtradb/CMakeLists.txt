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

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
ADD_DEFINITIONS(-D_WIN32 -D_LIB)

# Bug 19424 - InnoDB: Possibly a memory overrun of the buffer being freed (64-bit Visual C)
# Removing Win64 compiler optimizations for all innodb/mem/* files.
IF(CMAKE_GENERATOR MATCHES "Visual Studio" AND CMAKE_SIZEOF_VOID_P MATCHES 8)
  SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/storage/xtradb/mem/mem0mem.c
                              ${CMAKE_SOURCE_DIR}/storage/xtradb/mem/mem0pool.c
                              PROPERTIES COMPILE_FLAGS -Od)
ENDIF(CMAKE_GENERATOR MATCHES "Visual Studio" AND CMAKE_SIZEOF_VOID_P MATCHES 8)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/storage/xtradb/include
                    ${CMAKE_SOURCE_DIR}/storage/xtradb/handler
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(INNOBASE_SOURCES  btr/btr0btr.c btr/btr0cur.c btr/btr0pcur.c btr/btr0sea.c 
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
					 row/row0ext.c row/row0ins.c row/row0merge.c row/row0mysql.c 
					 row/row0purge.c row/row0row.c row/row0sel.c row/row0uins.c 
					 row/row0umod.c row/row0undo.c row/row0upd.c row/row0vers.c 
					 srv/srv0que.c srv/srv0srv.c srv/srv0start.c 
					 sync/sync0arr.c sync/sync0rw.c sync/sync0sync.c 
					 thr/thr0loc.c 
					 trx/trx0i_s.c trx/trx0purge.c trx/trx0rec.c trx/trx0roll.c trx/trx0rseg.c 
					 trx/trx0sys.c trx/trx0trx.c trx/trx0undo.c 
					 usr/usr0sess.c 
					 ut/ut0byte.c ut/ut0dbg.c ut/ut0mem.c ut/ut0rnd.c ut/ut0ut.c ut/ut0vec.c ut/ut0list.c ut/ut0wqueue.c)

IF(NOT SOURCE_SUBLIBS)
  ADD_LIBRARY(innobase ${INNOBASE_SOURCES})
  ADD_DEPENDENCIES(innobase GenError)
  SET_TARGET_PROPERTIES(innobase PROPERTIES COMPILE_FLAGS "-DMYSQL_SERVER")

  IF(INNODB_DYNAMIC_PLUGIN)
    # The dynamic plugin requires CMake 2.6.0 or later. Otherwise, the /DELAYLOAD property
    # will not be set
    CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0 FATAL_ERROR)
    ADD_LIBRARY(ha_innodb SHARED ${INNOBASE_SOURCES} ha_innodb.def handler/win_delay_loader.cc)
    ADD_DEPENDENCIES(ha_innodb GenError mysqld)
    # If build type is not specified as Release, default to Debug
    # This is a workaround to a problem in CMake 2.6, which does not
    # set the path of mysqld.lib correctly
    IF(CMAKE_BUILD_TYPE MATCHES Release)
       SET(CMAKE_BUILD_TYPE "Release")
    ELSE(CMAKE_BUILD_TYPE MATCHES Release)
       SET(CMAKE_BUILD_TYPE "Debug") 
    ENDIF(CMAKE_BUILD_TYPE MATCHES Release)
    TARGET_LINK_LIBRARIES(ha_innodb strings zlib)
    TARGET_LINK_LIBRARIES(ha_innodb ${CMAKE_SOURCE_DIR}/sql/${CMAKE_BUILD_TYPE}/mysqld.lib)
    SET_TARGET_PROPERTIES(ha_innodb PROPERTIES OUTPUT_NAME ha_innodb)
    SET_TARGET_PROPERTIES(ha_innodb PROPERTIES LINK_FLAGS "/MAP /MAPINFO:EXPORTS")
    SET_TARGET_PROPERTIES(ha_innodb PROPERTIES LINK_FLAGS "/ENTRY:\"_DllMainCRTStartup@12\"")
    SET_TARGET_PROPERTIES(ha_innodb PROPERTIES COMPILE_FLAGS "-DMYSQL_DYNAMIC_PLUGIN")
    SET_TARGET_PROPERTIES(ha_innodb PROPERTIES LINK_FLAGS "/DELAYLOAD:mysqld.exe")
  ENDIF(INNODB_DYNAMIC_PLUGIN)

ENDIF(NOT SOURCE_SUBLIBS)
