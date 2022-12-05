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

# This is the CMakeLists for InnoDB

INCLUDE(innodb.cmake)

ADD_DEFINITIONS(-DLOG_SUBSYSTEM_TAG="InnoDB")

# Expand PSI_XXX_CALL into direct calls to the performance_schema.
ADD_DEFINITIONS(-DPFS_DIRECT_CALL)

IF(MSVC AND NOT WIN32_CLANG)
  # Treat warnings as errors on MSVC for InnoDB
  ADD_COMPILE_OPTIONS("/WX")
  # Add warnings from level 4
  ADD_COMPILE_OPTIONS("/W4")
  # Disable C4324: structure was padded due to __declspec(align())
  ADD_COMPILE_OPTIONS("/wd4324")
  # Disable C4589: Constructor of abstract class 'dd::Entity_object_impl'
  # ignores initializer for virtual base class 'dd::Weak_object'
  ADD_COMPILE_OPTIONS("/wd4589")
ENDIF()

# TODO: Fix up all non-canonical include paths and remove this.
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/sql/auth
)

# Conflicting YYSTYPE, because we have multiple Bison grammars.
# WL#11100 Migrate to Bison 3.x should fix this.
#
# STRING(REPLACE "-flto" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
# STRING(REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

SET(INNOBASE_ZIP_DECOMPRESS_SOURCES
  buf/buf.cc
  buf/checksum.cc
  dict/dict.cc
  dict/mem.cc
  mem/memory.cc
  os/file.cc
  os/os0thread.cc
  page/zipdecompress.cc
  rem/rec.cc
  rem/rem0wrec.cc
  ut/crc32.cc
  ut/ut.cc
  ut/ut0new.cc)

SET(INNOBASE_SOURCES
  api/api0api.cc
  api/api0misc.cc
  arch/arch0arch.cc
  arch/arch0page.cc
  arch/arch0log.cc
  arch/arch0recv.cc
  btr/btr0btr.cc
  btr/btr0cur.cc
  btr/btr0pcur.cc
  btr/btr0sea.cc
  buf/buf0block_hint.cc
  buf/buf0buddy.cc
  buf/buf0buf.cc
  buf/buf0dblwr.cc
  buf/buf0dump.cc
  buf/buf0flu.cc
  btr/btr0load.cc
  buf/buf0lru.cc
  buf/buf0rea.cc
  clone/clone0api.cc
  clone/clone0clone.cc
  clone/clone0copy.cc
  clone/clone0apply.cc
  clone/clone0desc.cc
  clone/clone0snapshot.cc
  clone/clone0repl.cc
  data/data0data.cc
  data/data0type.cc
  dict/dict0boot.cc
  dict/dict0crea.cc
  dict/dict0dict.cc
  dict/dict0dd.cc
  dict/dict0inst.cc
  dict/dict0load.cc
  dict/dict0mem.cc
  dict/dict0sdi.cc
  dict/dict0stats.cc
  dict/dict0stats_bg.cc
  dict/dict0upgrade.cc
  ddl/ddl0buffer.cc
  ddl/ddl0builder.cc
  ddl/ddl0ctx.cc
  ddl/ddl0ddl.cc
  ddl/ddl0fts.cc
  ddl/ddl0file-reader.cc
  ddl/ddl0loader.cc
  ddl/ddl0merge.cc
  ddl/ddl0rtree.cc
  ddl/ddl0par-scan.cc
  eval/eval0eval.cc
  eval/eval0proc.cc
  fil/fil0fil.cc
  fsp/fsp0fsp.cc
  fsp/fsp0file.cc
  fsp/fsp0space.cc
  fsp/fsp0sysspace.cc
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
  handler/p_s.cc
  ibuf/ibuf0ibuf.cc
  lob/lob0first.cc
  lob/lob0index.cc
  lob/lob0ins.cc
  lob/lob0pages.cc
  lob/lob0purge.cc
  lob/lob0update.cc
  lob/lob0impl.cc
  lob/lob0lob.cc
  lob/lob0del.cc
  lob/lob0undo.cc
  lob/lob0util.cc
  lob/zlob0index.cc
  lob/zlob0ins.cc
  lob/zlob0update.cc
  lob/zlob0first.cc
  lob/zlob0read.cc
  lock/lock0guards.cc
  lock/lock0iter.cc
  lock/lock0prdt.cc
  lock/lock0latches.cc
  lock/lock0lock.cc
  lock/lock0wait.cc
  log/log0buf.cc
  log/log0chkp.cc
  log/log0consumer.cc
  log/log0ddl.cc
  log/log0encryption.cc
  log/log0files_capacity.cc
  log/log0files_dict.cc
  log/log0files_finder.cc
  log/log0files_governor.cc
  log/log0files_io.cc
  log/log0log.cc
  log/log0meb.cc
  log/log0pfs.cc
  log/log0pre_8_0_30.cc
  log/log0recv.cc
  log/log0test.cc
  log/log0write.cc
  mach/mach0data.cc
  mtr/mtr0log.cc
  mtr/mtr0mtr.cc
  os/os0file.cc
  os/os0enc.cc
  os/os0proc.cc
  os/os0event.cc
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
  rem/rem0wrec.cc
  row/row0ext.cc
  row/row0import.cc
  row/row0ins.cc
  row/row0mysql.cc
  row/row0log.cc
  row/row0pread.cc
  row/row0purge.cc
  row/row0pread-adapter.cc
  row/row0pread-histogram.cc
  row/row0row.cc
  row/row0sel.cc
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
  srv/srv0tmp.cc
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
  ut/ut0dbg.cc
  ut/ut0list.cc
  ut/ut0math.cc
  ut/ut0mem.cc
  ut/ut0new.cc
  ut/ut0rbt.cc
  ut/ut0rnd.cc
  ut/ut0test.cc
  ut/ut0ut.cc
  ut/ut0vec.cc
  ut/ut0wqueue.cc)

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=98753
IF(MY_COMPILER_IS_GNU AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  ADD_COMPILE_FLAGS(
    pars/pars0grm.cc
    COMPILE_FLAGS "-Wno-free-nonheap-object")
ENDIF()

# The combination of
# ut_ad(strlen(dir_path) + 1 + strlen(dir_name) + 1 < sizeof(path))
# and
# snprintf(path, sizeof(path), "%s%c%s", dir_path, OS_PATH_SEPARATOR, dir_name);
# gives a warning: -Werror=format-truncation=
IF(MY_COMPILER_IS_GNU AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11 AND
    CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG" AND
    CMAKE_CXX_FLAGS MATCHES " -O2 ")
  ADD_COMPILE_FLAGS(arch/arch0arch.cc COMPILE_FLAGS "-Wformat-truncation=0")
ENDIF()

IF(MSVC)
  # TODO(Bug#33985941): update the code and remove the disabled warnings
  MY_ADD_COMPILE_DEFINITIONS(os/os0file.cc
    COMPILE_DEFINITIONS "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING"
    )
ENDIF()

IF(MSVC AND NOT WIN32_CLANG)
  # Disable C4505: unreferenced local function has been removed
  ADD_COMPILE_FLAGS(pars/lexyy.cc COMPILE_FLAGS "/wd4505")
  # Disable C4702: unreachable code
  ADD_COMPILE_FLAGS(pars/lexyy.cc pars/pars0grm.cc COMPILE_FLAGS "/wd4702")
  # Disable C4100: unreferenced formal parameter
  # Disable C4701: potentially uninitialized local variable used
  # Disable C4702: unreachable code
  ADD_COMPILE_FLAGS(fts/fts0pars.cc COMPILE_FLAGS "/wd4100 /wd4701 /wd4702")

  # Temporary disables for warnings that are not yet fixed:
  # C4456: declaration of 'X' hides local declaration
  ADD_COMPILE_OPTIONS("/wd4456")
  # C4457: declaration of 'X' hides function parameter
  ADD_COMPILE_OPTIONS("/wd4457")
  # C4458: declaration of 'X' hides class member
  ADD_COMPILE_OPTIONS("/wd4458")
  # C4459: declaration of 'X' hides global declaration
  ADD_COMPILE_OPTIONS("/wd4459")
ENDIF()

# There seems to be a bug in gcc for Oracle Linux 8 on ARM.
# storage/innobase/ut/ut0test.cc:213:76:
# warning: requested alignment 512 is larger than 16 [-Wattributes]
# alignas(OS_FILE_LOG_BLOCK_SIZE) ....
# We downgrade -Werror to warning to avoid broken build:
IF(MY_COMPILER_IS_GNU AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  MY_CHECK_CXX_COMPILER_WARNING("error=attributes" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    ADD_COMPILE_FLAGS(
      ut/ut0test.cc
      COMPILE_FLAGS ${HAS_WARN_FLAG}
      )
  ENDIF()
ENDIF()

ADD_LIBRARY(innodb_zipdecompress STATIC ${INNOBASE_ZIP_DECOMPRESS_SOURCES})
TARGET_COMPILE_DEFINITIONS(
    innodb_zipdecompress PRIVATE DISABLE_PSI_MEMORY UNIV_LIBRARY UNIV_NO_ERR_MSGS)
ADD_DEPENDENCIES(innodb_zipdecompress GenError)

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  TARGET_COMPILE_OPTIONS(innodb_zipdecompress PRIVATE ${HAS_MISSING_PROFILE})
ENDIF()


UNSET(NUMA_LIBRARY)
IF(HAVE_LIBNUMA)
  SET(NUMA_LIBRARY "numa")
ENDIF()

MYSQL_ADD_PLUGIN(innobase
  ${INNOBASE_SOURCES} ${INNOBASE_ZIP_DECOMPRESS_SOURCES} STORAGE_ENGINE
  MANDATORY
  MODULE_OUTPUT_NAME ha_innodb
  LINK_LIBRARIES sql_dd sql_gis ${ZLIB_LIBRARY} ${LZ4_LIBRARY} ${NUMA_LIBRARY})

# On linux: /usr/include/stdio.h:#define BUFSIZ 8192
# On Solaris: /usr/include/iso/stdio_iso.h:#define    BUFSIZ  1024
IF(SOLARIS AND MY_COMPILER_IS_GNU)
  ADD_COMPILE_FLAGS(
    row/row0import.cc
    row/row0quiesce.cc
    COMPILE_FLAGS "-Wno-format-truncation"
    )
ENDIF()

# Generated code with logical-op warnings
MY_CHECK_CXX_COMPILER_WARNING("-Wlogical-op" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(fts/fts0pars.cc
    COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wcast-function-type" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(fts/fts0pars.cc
    COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(fts/fts0pars.cc
    COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

IF(MSVC AND NOT WIN32_CLANG)
  # Disable C4100: unreferenced formal parameter
  # for release builds and other non-innobase targets, as many parameters are
  # server-only and debug-only.
  FOREACH(flag
     CMAKE_CXX_FLAGS_MINSIZEREL
     CMAKE_CXX_FLAGS_RELEASE  CMAKE_CXX_FLAGS_RELWITHDEBINFO)
   SET("${flag}" "${${flag}} /wd4100")
  ENDFOREACH()
  TARGET_COMPILE_OPTIONS(innodb_zipdecompress PRIVATE "/wd4100")
ENDIF()
