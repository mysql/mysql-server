include .defs.mk

TYPE := ndbapi

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := signaldataprint

SOURCES = TcKeyReq.cpp TcKeyConf.cpp TcKeyRef.cpp \
	  TcRollbackRep.cpp \
          TupKey.cpp TupCommit.cpp LqhKey.cpp \
          FsOpenReq.cpp FsCloseReq.cpp FsRef.cpp FsConf.cpp FsReadWriteReq.cpp\
          SignalDataPrint.cpp SignalNames.cpp \
          ContinueB.cpp DihContinueB.cpp NdbfsContinueB.cpp \
          CloseComReqConf.cpp PackedSignal.cpp PrepFailReqRef.cpp \
	  GCPSave.cpp DictTabInfo.cpp \
	  AlterTable.cpp AlterTab.cpp \
	  CreateTrig.cpp AlterTrig.cpp DropTrig.cpp \
	  FireTrigOrd.cpp TrigAttrInfo.cpp \
	  CreateIndx.cpp AlterIndx.cpp DropIndx.cpp TcIndx.cpp \
	  IndxKeyInfo.cpp IndxAttrInfo.cpp \
          FsAppendReq.cpp ScanTab.cpp \
          BackupImpl.cpp BackupSignalData.cpp \
          UtilSequence.cpp UtilPrepare.cpp UtilDelete.cpp UtilExecute.cpp \
          LqhFrag.cpp DropTab.cpp PrepDropTab.cpp LCP.cpp MasterLCP.cpp \
          CopyGCI.cpp SystemError.cpp StartRec.cpp NFCompleteRep.cpp \
          FailRep.cpp DisconnectRep.cpp SignalDroppedRep.cpp \
          SumaImpl.cpp NdbSttor.cpp CreateFragmentation.cpp \
	  CntrStart.cpp ReadNodesConf.cpp \
	  UtilLock.cpp TuxMaint.cpp TupAccess.cpp AccLock.cpp \
          LqhTrans.cpp

include $(NDB_TOP)/Epilogue.mk

