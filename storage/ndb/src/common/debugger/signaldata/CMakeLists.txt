# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

ADD_CONVENIENCE_LIBRARY(ndbsignaldata
        AlterIndxImpl.cpp BuildIndxImpl.cpp BuildIndx.cpp
        CreateIndxImpl.cpp CreateTab.cpp
        CreateTable.cpp CreateTrigImpl.cpp #DihSwitchReplicaReq.cpp
        DropIndxImpl.cpp DropTable.cpp DropTrigImpl.cpp
        GetTabInfo.cpp
        SchemaTrans.cpp SchemaTransImpl.cpp
        TcKeyReq.cpp TcKeyConf.cpp TcKeyRef.cpp
        TcRollbackRep.cpp
        TupKey.cpp TupCommit.cpp LqhKey.cpp
        FsOpenReq.cpp FsCloseReq.cpp FsRef.cpp FsConf.cpp FsReadWriteReq.cpp
        SignalDataPrint.cpp SignalNames.cpp
        ContinueB.cpp DihContinueB.cpp NdbfsContinueB.cpp
        CloseComReqConf.cpp PackedSignal.cpp PrepFailReqRef.cpp
        GCP.cpp DictTabInfo.cpp
        AlterTable.cpp AlterTab.cpp
        CreateTrig.cpp DropTrig.cpp
        FireTrigOrd.cpp TrigAttrInfo.cpp
        CreateIndx.cpp AlterIndx.cpp DropIndx.cpp TcIndx.cpp
        IndxKeyInfo.cpp IndxAttrInfo.cpp
        FsAppendReq.cpp ScanTab.cpp
        BackupImpl.cpp BackupSignalData.cpp
        UtilSequence.cpp UtilPrepare.cpp UtilDelete.cpp UtilExecute.cpp
        LqhFrag.cpp DropTab.cpp PrepDropTab.cpp LCP.cpp MasterLCP.cpp
        CopyGCI.cpp SystemError.cpp StartRec.cpp NFCompleteRep.cpp
        FailRep.cpp DisconnectRep.cpp SignalDroppedRep.cpp
        SumaImpl.cpp NdbSttor.cpp CreateFragmentation.cpp
        UtilLock.cpp TuxMaint.cpp AccLock.cpp
        LqhTrans.cpp ReadNodesConf.cpp CntrStart.cpp
        ScanFrag.cpp ApiVersion.cpp
        LocalRouteOrd.cpp
	DbinfoScan.cpp NodePing.cpp
	IndexStatSignal.cpp GetConfig.cpp)

