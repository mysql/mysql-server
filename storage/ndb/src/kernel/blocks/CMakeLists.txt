# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

IF(WIN32)
  SET(EXTRA_SRC ndbfs/Win32AsyncFile.cpp)
ELSE()
  SET(EXTRA_SRC ndbfs/PosixAsyncFile.cpp)
ENDIF()

ADD_CONVENIENCE_LIBRARY(ndbblocks
    tsman.cpp lgman.cpp pgman.cpp diskpage.cpp restore.cpp
    cmvmi/Cmvmi.cpp
    dbacc/DbaccInit.cpp dbacc/DbaccMain.cpp
    dbdict/Dbdict.cpp
    dbdih/DbdihInit.cpp dbdih/DbdihMain.cpp
    dblqh/DblqhInit.cpp dblqh/DblqhMain.cpp
    dbtc/DbtcInit.cpp dbtc/DbtcMain.cpp
    dbgdm/DbgdmProxy.cpp
    dbspj/DbspjInit.cpp dbspj/DbspjMain.cpp dbspj/DbspjProxy.cpp
    dbtup/DbtupExecQuery.cpp dbtup/DbtupBuffer.cpp
    dbtup/DbtupRoutines.cpp dbtup/DbtupCommit.cpp
    dbtup/DbtupFixAlloc.cpp dbtup/DbtupTrigger.cpp
    dbtup/DbtupAbort.cpp dbtup/DbtupPageMap.cpp
    dbtup/DbtupPagMan.cpp dbtup/DbtupStoredProcDef.cpp
    dbtup/DbtupMeta.cpp dbtup/DbtupTabDesMan.cpp
    dbtup/DbtupGen.cpp dbtup/DbtupIndex.cpp
    dbtup/DbtupDebug.cpp dbtup/DbtupScan.cpp
    dbtup/DbtupDiskAlloc.cpp dbtup/DbtupVarAlloc.cpp
    dbtup/tuppage.cpp dbtup/Undo_buffer.cpp
    ndbfs/AsyncIoThread.cpp ndbfs/AsyncFile.cpp
    ndbfs/Ndbfs.cpp
    ndbfs/VoidFs.cpp
    ndbfs/Filename.cpp
    ndbcntr/NdbcntrInit.cpp ndbcntr/NdbcntrSysTable.cpp ndbcntr/NdbcntrMain.cpp
    qmgr/QmgrInit.cpp qmgr/QmgrMain.cpp
    trix/Trix.cpp
    backup/Backup.cpp backup/BackupInit.cpp
    dbutil/DbUtil.cpp
    suma/Suma.cpp suma/SumaInit.cpp
    dbtux/DbtuxGen.cpp dbtux/DbtuxMeta.cpp dbtux/DbtuxMaint.cpp
    dbtux/DbtuxNode.cpp dbtux/DbtuxTree.cpp dbtux/DbtuxScan.cpp
    dbtux/DbtuxSearch.cpp dbtux/DbtuxCmp.cpp dbtux/DbtuxStat.cpp
    dbtux/DbtuxDebug.cpp dbtux/DbtuxBuild.cpp
    LocalProxy.cpp
    dblqh/DblqhProxy.cpp
    dbacc/DbaccProxy.cpp
    dbtup/DbtupProxy.cpp
    dbtux/DbtuxProxy.cpp
    backup/BackupProxy.cpp
    RestoreProxy.cpp
    dbtc/DbtcProxy.cpp
    dbinfo/Dbinfo.cpp
    dblqh/DblqhCommon.cpp
    PgmanProxy.cpp
    dbtup/DbtupClient.cpp
    ${EXTRA_SRC}
    thrman.cpp
    trpman.cpp)

MYSQL_ADD_EXECUTABLE(ndb_print_file
  print_file.cpp
  diskpage.cpp
  dbtup/tuppage.cpp
  COMPONENT ClusterTools)
TARGET_LINK_LIBRARIES(ndb_print_file ndbclient_static)
