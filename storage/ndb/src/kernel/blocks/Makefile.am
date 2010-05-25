# Copyright (C) 2004-2006 MySQL AB
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

SUBDIRS = \
	dbdict		\
	dbdih		\
	dblqh		\
	dbtup		\
        backup          

noinst_LIBRARIES = libblocks.a

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/kernel/blocks/dblqh
libblocks_a_SOURCES = tsman.cpp lgman.cpp pgman.cpp diskpage.cpp restore.cpp\
  cmvmi/Cmvmi.cpp \
  dbacc/DbaccInit.cpp dbacc/DbaccMain.cpp \
  dbdict/Dbdict.cpp \
  dbdih/DbdihInit.cpp dbdih/DbdihMain.cpp \
  dblqh/DblqhInit.cpp dblqh/DblqhMain.cpp \
  dbtc/DbtcInit.cpp dbtc/DbtcMain.cpp \
  dbtup/DbtupExecQuery.cpp dbtup/DbtupBuffer.cpp \
  dbtup/DbtupRoutines.cpp dbtup/DbtupCommit.cpp \
  dbtup/DbtupFixAlloc.cpp dbtup/DbtupTrigger.cpp \
  dbtup/DbtupAbort.cpp dbtup/DbtupPageMap.cpp \
  dbtup/DbtupPagMan.cpp dbtup/DbtupStoredProcDef.cpp \
  dbtup/DbtupMeta.cpp dbtup/DbtupTabDesMan.cpp \
  dbtup/DbtupGen.cpp dbtup/DbtupIndex.cpp \
  dbtup/DbtupDebug.cpp dbtup/DbtupScan.cpp \
  dbtup/DbtupDiskAlloc.cpp dbtup/DbtupVarAlloc.cpp \
  dbtup/tuppage.cpp dbtup/Undo_buffer.cpp \
  ndbfs/AsyncFile.cpp ndbfs/Ndbfs.cpp ndbfs/VoidFs.cpp \
  ndbfs/Filename.cpp ndbfs/CircularIndex.cpp \
  ndbcntr/NdbcntrInit.cpp ndbcntr/NdbcntrSysTable.cpp ndbcntr/NdbcntrMain.cpp \
  qmgr/QmgrInit.cpp qmgr/QmgrMain.cpp \
  trix/Trix.cpp \
  backup/Backup.cpp backup/BackupInit.cpp \
  dbutil/DbUtil.cpp \
  suma/Suma.cpp suma/SumaInit.cpp \
  dbtux/DbtuxGen.cpp dbtux/DbtuxMeta.cpp dbtux/DbtuxMaint.cpp \
  dbtux/DbtuxNode.cpp dbtux/DbtuxTree.cpp dbtux/DbtuxScan.cpp \
  dbtux/DbtuxSearch.cpp dbtux/DbtuxCmp.cpp dbtux/DbtuxStat.cpp \
  dbtux/DbtuxDebug.cpp

EXTRA_PROGRAMS = ndb_print_file
ndb_print_file_SOURCES = print_file.cpp diskpage.cpp dbtup/tuppage.cpp
ndb_print_file_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

windoze-dsp:
