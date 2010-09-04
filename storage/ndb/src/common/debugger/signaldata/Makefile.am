# Copyright (C) 2004 MySQL AB
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

noinst_LTLIBRARIES = libsignaldataprint.la

libsignaldataprint_la_SOURCES = \
          TcKeyReq.cpp TcKeyConf.cpp TcKeyRef.cpp \
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
	  UtilLock.cpp TuxMaint.cpp AccLock.cpp \
          LqhTrans.cpp ReadNodesConf.cpp CntrStart.cpp \
          ScanFrag.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

windoze-dsp: libsignaldataprint.dsp

libsignaldataprint.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libsignaldataprint_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
