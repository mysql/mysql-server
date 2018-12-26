# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# This is the CMakeLists for InnoDB

INCLUDE(innodb.cmake)

SET(INNOBASE_SOURCES
	api/api0api.cc
	api/api0misc.cc
	btr/btr0btr.cc
	btr/btr0cur.cc
	btr/btr0pcur.cc
	btr/btr0sea.cc
	btr/btr0bulk.cc
	buf/buf0buddy.cc
	buf/buf0buf.cc
	buf/buf0dblwr.cc
	buf/buf0checksum.cc
	buf/buf0dump.cc
	buf/buf0flu.cc
	buf/buf0lru.cc
	buf/buf0rea.cc
	data/data0data.cc
	data/data0type.cc
	dict/dict0boot.cc
	dict/dict0crea.cc
	dict/dict0dict.cc
	dict/dict0load.cc
	dict/dict0mem.cc
	dict/dict0stats.cc
	dict/dict0stats_bg.cc
	eval/eval0eval.cc
	eval/eval0proc.cc
	fil/fil0fil.cc
	fsp/fsp0fsp.cc
	fsp/fsp0file.cc
	fsp/fsp0space.cc
	fsp/fsp0sysspace.cc
	fut/fut0fut.cc
	fut/fut0lst.cc
	ha/ha0ha.cc
	ha/ha0storage.cc
	ha/hash0hash.cc
	fts/fts0fts.cc
	fts/fts0ast.cc
	fts/fts0blex.cc
	fts/fts0config.cc
	fts/fts0opt.cc
	fts/fts0pars.cc
	fts/fts0que.cc
	fts/fts0sql.cc
	fts/fts0tlex.cc
	gis/gis0geo.cc
	gis/gis0rtree.cc
	gis/gis0sea.cc
	fts/fts0plugin.cc
	handler/ha_innodb.cc
	handler/ha_innopart.cc
	handler/handler0alter.cc
	handler/i_s.cc
	ibuf/ibuf0ibuf.cc
	lock/lock0iter.cc
	lock/lock0prdt.cc
	lock/lock0lock.cc
	lock/lock0wait.cc
	log/log0log.cc
	log/log0recv.cc
	mach/mach0data.cc
	mem/mem0mem.cc
	mtr/mtr0log.cc
	mtr/mtr0mtr.cc
	os/os0file.cc
	os/os0proc.cc
	os/os0event.cc
	os/os0thread.cc
	page/page0cur.cc
	page/page0page.cc
	page/page0zip.cc
	pars/lexyy.cc
	pars/pars0grm.cc
	pars/pars0opt.cc
	pars/pars0pars.cc
	pars/pars0sym.cc
	que/que0que.cc
	read/read0read.cc
	rem/rem0cmp.cc
	rem/rem0rec.cc
	row/row0ext.cc
	row/row0ftsort.cc
	row/row0import.cc
	row/row0ins.cc
	row/row0merge.cc
	row/row0mysql.cc
	row/row0log.cc
	row/row0purge.cc
	row/row0row.cc
	row/row0sel.cc
	row/row0trunc.cc
	row/row0uins.cc
	row/row0umod.cc
	row/row0undo.cc
	row/row0upd.cc
	row/row0quiesce.cc
	row/row0vers.cc
	srv/srv0conc.cc
	srv/srv0mon.cc
	srv/srv0srv.cc
	srv/srv0start.cc
	sync/sync0arr.cc
	sync/sync0rw.cc
	sync/sync0debug.cc
	sync/sync0sync.cc
	trx/trx0i_s.cc
	trx/trx0purge.cc
	trx/trx0rec.cc
	trx/trx0roll.cc
	trx/trx0rseg.cc
	trx/trx0sys.cc
	trx/trx0trx.cc
	trx/trx0undo.cc
	usr/usr0sess.cc
	ut/ut0byte.cc
	ut/ut0crc32.cc
	ut/ut0dbg.cc
	ut/ut0list.cc
	ut/ut0mem.cc
	ut/ut0new.cc
	ut/ut0rbt.cc
	ut/ut0rnd.cc
	ut/ut0ut.cc
	ut/ut0vec.cc
	ut/ut0wqueue.cc)

IF(WITH_INNODB)
  # Legacy option
  SET(WITH_INNOBASE_STORAGE_ENGINE TRUE)
ENDIF()

UNSET(NUMA_LIBRARY)
IF(HAVE_LIBNUMA)
   SET(NUMA_LIBRARY "numa")
ENDIF()

MYSQL_ADD_PLUGIN(innobase ${INNOBASE_SOURCES} STORAGE_ENGINE
  MANDATORY
  MODULE_OUTPUT_NAME ha_innodb
  LINK_LIBRARIES ${ZLIB_LIBRARY} ${LZ4_LIBRARY} ${NUMA_LIBRARY})

# Remove -DMYSQL_SERVER, it breaks embedded build
SET_TARGET_PROPERTIES(innobase PROPERTIES COMPILE_DEFINITIONS "")

IF(WITH_INNOBASE_STORAGE_ENGINE)
  ADD_DEPENDENCIES(innobase GenError)
ENDIF()

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)
# Avoid generating Hardware Capabilities due to crc32 instructions
IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  MY_CHECK_CXX_COMPILER_FLAG("-Wa,-nH" HAVE_WA_NH)
  IF(HAVE_WA_NH)
    ADD_COMPILE_FLAGS(
      ut/ut0crc32.cc
      COMPILE_FLAGS "-Wa,-nH"
    )
  ENDIF()
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wcast-function-type" HAVE_CAST_FUNCTION_TYPE)
IF(HAVE_CAST_FUNCTION_TYPE)
  ADD_COMPILE_FLAGS(fts/fts0pars.cc
    COMPILE_FLAGS "-Wno-cast-function-type")
ENDIF()

# A GCC bug causes crash when compiling these files on ARM64 with -O1+
# Compile them with -O0 as a workaround.
IF(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  # Bug was fixed in GCC 5.2, so workaround only needed < 5.2
  EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)
  IF(GCC_VERSION VERSION_LESS 5.2)
    INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)
    ADD_COMPILE_FLAGS(
      btr/btr0btr.cc
      btr/btr0cur.cc
      buf/buf0buf.cc
      gis/gis0sea.cc
      COMPILE_FLAGS "-O0"
      )
  ENDIF()
ENDIF()
