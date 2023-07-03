# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

ADD_SUBDIRECTORY(backup)
ADD_SUBDIRECTORY(dbdict)
ADD_SUBDIRECTORY(dbdih)
ADD_SUBDIRECTORY(dblqh)
ADD_SUBDIRECTORY(dbtup)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/error
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/kernel/vm)


IF(WIN32)
  SET(EXTRA_SRC ndbfs/Win32AsyncFile.cpp)
ELSE()
  SET(EXTRA_SRC ndbfs/PosixAsyncFile.cpp)
ENDIF()

ADD_STATIC_LIBRARY(ndbblocks
  LocalProxy.cpp
  PgmanProxy.cpp
  RestoreProxy.cpp
  backup/QBackup.cpp
  backup/QBackupProxy.cpp
  backup/Backup.cpp
  backup/BackupInit.cpp
  backup/BackupProxy.cpp
  cmvmi/Cmvmi.cpp
  dbacc/Dbqacc.cpp
  dbacc/DbqaccProxy.cpp
  dbacc/DbaccInit.cpp
  dbacc/DbaccMain.cpp
  dbacc/DbaccProxy.cpp
  dbdict/Dbdict.cpp
  dbdih/DbdihInit.cpp
  dbdih/DbdihMain.cpp
  dbdih/Sysfile.cpp
  dbgdm/DbgdmProxy.cpp
  dbinfo/Dbinfo.cpp
  dblqh/DblqhCommon.cpp
  dblqh/DblqhInit.cpp
  dblqh/DblqhMain.cpp
  dblqh/DblqhProxy.cpp
  dblqh/Dbqlqh.cpp
  dblqh/DbqlqhProxy.cpp
  dbspj/DbspjInit.cpp
  dbspj/DbspjMain.cpp
  dbspj/DbspjProxy.cpp
  dbtc/DbtcInit.cpp
  dbtc/DbtcMain.cpp
  dbtc/DbtcProxy.cpp
  dbtup/Dbqtup.cpp
  dbtup/DbqtupProxy.cpp
  dbtup/DbtupAbort.cpp
  dbtup/DbtupBuffer.cpp
  dbtup/DbtupClient.cpp
  dbtup/DbtupCommit.cpp
  dbtup/DbtupDebug.cpp
  dbtup/DbtupDiskAlloc.cpp
  dbtup/DbtupExecQuery.cpp
  dbtup/DbtupFixAlloc.cpp
  dbtup/DbtupGen.cpp
  dbtup/DbtupIndex.cpp
  dbtup/DbtupMeta.cpp
  dbtup/DbtupPagMan.cpp
  dbtup/DbtupPageMap.cpp
  dbtup/DbtupProxy.cpp
  dbtup/DbtupRoutines.cpp
  dbtup/DbtupScan.cpp
  dbtup/DbtupStoredProcDef.cpp
  dbtup/DbtupTabDesMan.cpp
  dbtup/DbtupTrigger.cpp
  dbtup/DbtupVarAlloc.cpp
  dbtup/Undo_buffer.cpp
  dbtup/tuppage.cpp
  dbtux/Dbqtux.cpp
  dbtux/DbqtuxProxy.cpp
  dbtux/DbtuxBuild.cpp
  dbtux/DbtuxDebug.cpp
  dbtux/DbtuxGen.cpp
  dbtux/DbtuxMaint.cpp
  dbtux/DbtuxMeta.cpp
  dbtux/DbtuxNode.cpp
  dbtux/DbtuxProxy.cpp
  dbtux/DbtuxScan.cpp
  dbtux/DbtuxSearch.cpp
  dbtux/DbtuxStat.cpp
  dbtux/DbtuxTree.cpp
  dbutil/DbUtil.cpp
  diskpage.cpp
  lgman.cpp
  ndbcntr/NdbcntrInit.cpp
  ndbcntr/NdbcntrMain.cpp
  ndbcntr/NdbcntrSysTable.cpp
  ndbfs/AsyncFile.cpp
  ndbfs/AsyncIoThread.cpp
  ndbfs/Filename.cpp
  ndbfs/Ndbfs.cpp
  ndbfs/VoidFs.cpp
  pgman.cpp
  qmgr/QmgrInit.cpp
  qmgr/QmgrMain.cpp
  QRestore.cpp
  QRestoreProxy.cpp
  restore.cpp
  suma/Suma.cpp
  suma/SumaInit.cpp
  thrman.cpp
  trix/Trix.cpp
  trpman.cpp
  tsman.cpp
  ${EXTRA_SRC}
  LINK_LIBRARIES ext::zlib
  )

MYSQL_ADD_EXECUTABLE(ndb_print_file
  print_file.cpp
  diskpage.cpp
  dbtup/tuppage.cpp
  COMPONENT ClusterTools
  ENABLE_EXPORTS
  LINK_LIBRARIES ndbclient_static ext::zlib
  )
