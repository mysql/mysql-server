# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

ADD_CONVENIENCE_LIBRARY(ndbsignaldata
  AccLock.cpp
  AllocNodeId.cpp
  AlterIndx.cpp
  AlterIndxImpl.cpp
  AlterTab.cpp
  AlterTable.cpp
  ApiVersion.cpp
  BackupImpl.cpp
  BackupSignalData.cpp
  BuildIndx.cpp
  BuildIndxImpl.cpp
  CloseComReqConf.cpp
  CntrStart.cpp
  ContinueB.cpp
  CopyGCI.cpp
  CreateEvnt.cpp
  CreateFK.cpp
  CreateFragmentation.cpp
  CreateIndx.cpp
  CreateIndxImpl.cpp
  CreateTab.cpp
  CreateTable.cpp
  CreateTrig.cpp
  CreateTrigImpl.cpp
  DbinfoScan.cpp
  DictTabInfo.cpp
  DihContinueB.cpp
# DihSwitchReplicaReq.cpp
  DisconnectRep.cpp
  DropFK.cpp
  DropIndx.cpp
  DropIndxImpl.cpp
  DropTab.cpp
  DropTable.cpp
  DropTrig.cpp
  DropTrigImpl.cpp
  FailRep.cpp
  FireTrigOrd.cpp
  FsAppendReq.cpp
  FsCloseReq.cpp
  FsConf.cpp
  FsOpenReq.cpp
  FsReadWriteReq.cpp
  FsRef.cpp
  GCP.cpp
  GetConfig.cpp
  GetTabInfo.cpp
  IndexStatSignal.cpp
  IndxAttrInfo.cpp
  IndxKeyInfo.cpp
  IsolateOrd.cpp
  LCP.cpp
  LocalRouteOrd.cpp
  LqhFrag.cpp
  LqhKey.cpp
  LqhTrans.cpp
  MasterLCP.cpp
  NFCompleteRep.cpp
  NdbSttor.cpp
  NdbfsContinueB.cpp
  NodePing.cpp
  PackedSignal.cpp
  PrepDropTab.cpp
  PrepFailReqRef.cpp
  ProcessInfoRep.cpp
  ReadNodesConf.cpp
  RestoreImpl.cpp
  ScanFrag.cpp
  ScanTab.cpp
  SchemaTrans.cpp
  SchemaTransImpl.cpp
  SignalDataPrint.cpp
  SignalDroppedRep.cpp
  SignalNames.cpp
  StartRec.cpp
  SumaImpl.cpp
  SystemError.cpp
  TcIndx.cpp
  TcKeyConf.cpp
  TcKeyRef.cpp
  TcKeyReq.cpp
  TcRollbackRep.cpp
  TrigAttrInfo.cpp
  TrpKeepAlive.cpp
  TupCommit.cpp
  TupKey.cpp
  TuxMaint.cpp
  UtilDelete.cpp
  UtilExecute.cpp
  UtilLock.cpp
  UtilPrepare.cpp
  UtilSequence.cpp
  )
